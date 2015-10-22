#include <netinet/in.h>
#include <string.h>
#include "Node.h"
#include "RoutingProtocolImpl.h"
#include <limits>

#define PING_INTERVAL 10000  // 10 secs
#define PORT_STATUS_TIMEOUT 15	// 15 secs
#define DV_TIMEOUT 45	//45 secs

instruction instr[] = {SEND_PING, CHECK_ENTRY, SEND_DV};


RoutingProtocolImpl::RoutingProtocolImpl(Node *n) : RoutingProtocol(n) {
  sys = n;
  // add your own code
}

RoutingProtocolImpl::~RoutingProtocolImpl() {
  // add your own code (if needed)
}

void RoutingProtocolImpl::init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type) {
	this->num_ports = num_ports;
	this->router_id = router_id;
	this->protocol_type = protocol_type;

	std::cout<<"My Router ID is "<<router_id<<"\n";
	send_ping_msg();
	check_entries();
}

void RoutingProtocolImpl::handle_alarm(void *data) {
  // add your own code

	if (memcmp(data, &instr[SEND_PING], sizeof(instruction)) == 0) {
		send_ping_msg();
	} else if (memcmp(data, &instr[CHECK_ENTRY], sizeof(instruction)) == 0) {
		check_entries();
	}

}

void RoutingProtocolImpl::recv(unsigned short port, void *packet, unsigned short size) {
  // add your own code

	unsigned int pkt_type = *((unsigned char *)packet);
	msg_header *header;
	if (pkt_type == PING) {
		header = (msg_header *)packet;
		memset(&header->type, PONG, sizeof(header->type));
		memcpy(&header->dst, &header->src, sizeof(header->dst));
		int htons_id = htons(router_id);
		memcpy(&header->src, &htons_id, sizeof(header->src));

		sys->send(port, packet, size);

	} else if (pkt_type == PONG) {
		header = (msg_header *)packet;
		unsigned int current_time = sys->time();
		unsigned int neighbor_time = *((unsigned int *)packet + sizeof(header));
		unsigned int new_cost = current_time - ntohs(neighbor_time);

		port_status_entry *en = NULL;
		for (unsigned int i = 0; i < port_status_table.size(); i++) {
			if(port_status_table[i]->neighbor_id == header->src) {
				en = port_status_table[i];
			}
		}

		if (en == NULL) {
			en = (struct port_status_entry *) malloc(sizeof(struct port_status_entry));
			en->neighbor_id = ntohs(header->src);
			port_status_table.push_back(en);
		}

		// Initializing for distance vector
		if (!dv_contains_dest(en->neighbor_id)) {
			dv_entry* dv_en = (struct dv_entry*) malloc(sizeof(struct dv_entry));
			memcpy(&dv_en->dest_id, &en->neighbor_id, sizeof(en->neighbor_id));
			memcpy(&dv_en->port, &port, sizeof(port));
			memcpy(&dv_en->next_hop_id, &en->neighbor_id, sizeof(en->neighbor_id));
			memcpy(&dv_en->last_update, &current_time, sizeof(current_time));
			dv_table.push_back(dv_en);
			std::cout<<"dv_en's nexthopid "<<dv_en->next_hop_id<<"\n";
		}


//		for (unsigned int i = 0; i < dv_table.size(); i++) {
//			std::cout<<"dest_id "<<dv_table[i]->dest_id<<" "<<"cost "<<dv_table[i]->cost<<" "
//					<<"next_hop "<<dv_table[i]->next_hop_id<<"\n";
//		}
		// update DV if cost with neighbor changes
		if (new_cost != en->cost) {
			updateDV_from_cost_change(en->neighbor_id, new_cost);
		}

		// Initializing for forwarding table from DV
		forwarding_table_entry* ft_en;
		dv_entry* dv_en = get_dv_entry_by_dest(en->neighbor_id);
		if (!ft_contains_dest(en->neighbor_id)) {
			ft_en = (struct forwarding_table_entry*) malloc(sizeof(struct forwarding_table_entry));
			memcpy(&ft_en->dest_id, &en->neighbor_id, sizeof(en->neighbor_id));
			memcpy(&ft_en->port, &dv_en->port, sizeof(port));
			memcpy(&ft_en->next_hop_id, &dv_en->next_hop_id, sizeof(en->neighbor_id));
			forwarding_table.push_back(ft_en);
		} else {
			ft_en = get_ft_entry_by_dest(en->neighbor_id);
			memcpy(&ft_en->port, &dv_en->port, sizeof(port));
			memcpy(&ft_en->next_hop_id, &dv_en->next_hop_id, sizeof(en->neighbor_id));
		}

		en->last_update = current_time;
		en->cost = new_cost;
		en->port = port;

		free(packet);	// free packet
		std::cout<<"ENTRY: "<<en->last_update<<" "<<en->neighbor_id<<"\n";
	} else if (pkt_type == DATA){

		header = (msg_header *)packet;
		if (port == 0xffff) {
			// The package originated from this router
			header->src = router_id; // Make sure source is this router
		}

		if(header->dst == router_id) {
			// Free packet memory when it is at destination.
			free(packet);
		} else {
			forwarding_table_entry *ft_en = get_ft_entry_by_dest(header->dst);
			if (ft_en == NULL) {
				std::cout << "No forwarding table entry for destination router " << header->dst << "\n";
			} else {
				sys->send(ft_en->port, packet, header->size);
			}
		}

	}
}

void RoutingProtocolImpl::send_ping_msg() {

    struct msg_header *pkt = (struct msg_header *) malloc(sizeof(struct msg_header));
    pkt->type = (unsigned char) PING;
    pkt->size = htons(sizeof(struct msg_header));
    pkt->src = htons(this->router_id);

    unsigned int cur_time = sys->time();

    char *msg = (char *) malloc(sizeof(pkt) + sizeof(cur_time));
    memcpy(msg, pkt, sizeof(pkt));
    memcpy(msg + sizeof(pkt), &cur_time, sizeof(cur_time));

    for (unsigned short i = 0; i < num_ports; i++) {
    	sys->send(i, msg, sizeof(msg));
    }

    sys->set_alarm(this, 10000, &instr[SEND_PING]);

    delete pkt;

}

void RoutingProtocolImpl::check_entries() {

	unsigned int cur_time = sys->time();

	vector<struct port_status_entry*>::iterator port_iter = port_status_table.begin();
	while (port_iter != port_status_table.end()) {
		if (cur_time - (*port_iter)->last_update > PORT_STATUS_TIMEOUT) {
			updateDV_from_cost_change(
					(*port_iter)->neighbor_id, std::numeric_limits<int>::max());
			remove_ft_entry_by_port((*port_iter)->port);
			port_iter = port_status_table.erase(port_iter);
		} else {
			++port_iter;
		}
	}

	// check DV table
	vector<struct dv_entry*>::iterator dv_iter = dv_table.begin();
	while (dv_iter != dv_table.end()) {
		if (cur_time - (*dv_iter)->last_update > DV_TIMEOUT) {
			dv_iter = dv_table.erase(dv_iter);
			remove_ft_entry_by_dest((*dv_iter)->dest_id); // is this needed?
		} else {
			++dv_iter;
		}
	}

	sys->set_alarm(this, 1000, &instr[CHECK_ENTRY]);
}

void RoutingProtocolImpl::updateDV_from_cost_change(
		unsigned short neighbor_id, unsigned int update_val) {

	unsigned int cur_time = sys->time();

	if (update_val == (unsigned int)std::numeric_limits<int>::max()) {
		// update all costs in entries with nextHop as neighbor_id to infinity
		for (unsigned int i = 0; i < dv_table.size(); i++) {
			if (dv_table[i]->next_hop_id == neighbor_id) {
				memcpy(&dv_table[i]->cost, &update_val, sizeof(update_val));
				memcpy(&dv_table[i]->last_update, &cur_time, sizeof(cur_time));
			}
		}
	} else {
		// update all costs in entries with nextHop as neighbor_id to update_val
		// if this triggers new DV (nextHop changes to directly routing to neighbor,
		// or general min_cost change[where dest is not in the neighbor list], broad-
		// cast new DV to neighbors)
		bool update_dv = false;
		for (unsigned int i = 0; i < dv_table.size(); i++) {
			if (dv_table[i]->next_hop_id == neighbor_id) {
				update_dv = true;
				port_status_entry* nbr_entry = get_nbr_port_status_entry(dv_table[i]->dest_id);

				// re-route directly to nbr
				if (nbr_entry != NULL) {
					if (nbr_entry->cost < update_val && nbr_entry->cost != 0) {
						memcpy(&dv_table[i]->next_hop_id,
								&nbr_entry->neighbor_id,
								sizeof(nbr_entry->neighbor_id));
						memcpy(&dv_table[i]->cost, &nbr_entry->cost, sizeof(nbr_entry->cost));
						memcpy(&dv_table[i]->port, &nbr_entry->port, sizeof(nbr_entry->port));
						memcpy(&dv_table[i]->last_update, &cur_time, sizeof(cur_time));
						continue;
					}
				}
				memcpy(&dv_table[i]->cost, &update_val, sizeof(update_val));
				memcpy(&dv_table[i]->last_update, &cur_time, sizeof(cur_time));
			}
			send_DV_msg();
		}
	}
}

void RoutingProtocolImpl::updateDV_from_DV_msg(
		unsigned short neighbor_id, struct dv_msg_body* dv_msg) {
	//TODO:
}

port_status_entry* RoutingProtocolImpl::get_nbr_port_status_entry(unsigned int neighbor_id) {
	for (unsigned int i = 0; i < port_status_table.size(); i++) {
		if (port_status_table[i]->neighbor_id == neighbor_id)
			return port_status_table[i];
	}
	return NULL;
}

bool RoutingProtocolImpl::dv_contains_dest(unsigned int node_id) {
	for (unsigned int i = 0; i < dv_table.size(); i++) {
		if (dv_table[i]->dest_id == node_id)
			return true;
	}
	return false;
}

bool RoutingProtocolImpl::ft_contains_dest(unsigned int node_id) {
	for (unsigned int i = 0; i < forwarding_table.size(); i++) {
		if (forwarding_table[i]->dest_id == node_id)
			return true;
	}
	return false;
}

dv_entry* RoutingProtocolImpl::get_dv_entry_by_dest(unsigned int node_id) {
	for (unsigned int i = 0; i < dv_table.size(); i++) {
		if (dv_table[i]->dest_id == node_id)
			return dv_table[i];
	}
	return NULL;
}

forwarding_table_entry* RoutingProtocolImpl::get_ft_entry_by_dest(unsigned int node_id) {
	for (unsigned int i = 0; i < forwarding_table.size(); i++) {
		if (forwarding_table[i]->dest_id == node_id)
			return forwarding_table[i];
	}
	return NULL;
}

void RoutingProtocolImpl::remove_ft_entry_by_port(unsigned short port) {
	vector<struct forwarding_table_entry*>::iterator ft_iter = forwarding_table.begin();
	while(ft_iter != forwarding_table.end()) {
		if((*ft_iter)->port == port) {
			ft_iter = forwarding_table.erase(ft_iter);
		} else {
			ft_iter++;
		}
	}
}

void RoutingProtocolImpl::remove_ft_entry_by_dest(unsigned short dest) {
	vector<struct forwarding_table_entry*>::iterator ft_iter = forwarding_table.begin();
	while(ft_iter != forwarding_table.end()) {
		if((*ft_iter)->dest_id == dest) {
			ft_iter = forwarding_table.erase(ft_iter);
		} else {
			ft_iter++;
		}
	}
}

void RoutingProtocolImpl::send_DV_msg() {
	// Build msg body
	std::cout<<"send DV\n";
	dv_msg_body *msg_body = (dv_msg_body *) malloc(sizeof(struct dv_msg_body));
	for (unsigned int i = 0; i < dv_table.size(); i++) {
		std::cout<<"A\n";
		std::cout<<"dest id"<<dv_table[i]->dest_id<<"\n";
		node_cost *cost = (node_cost *) malloc(sizeof(struct node_cost));
		cost->node_id = htons(dv_table[i]->dest_id);

		std::cout<<"B\n";
		std::cout<<"next hop is: "<<dv_table[i]->next_hop_id<<"\n";
		if (dv_table[i]->next_hop_id != dv_table[i]->dest_id
				&& dv_contains_dest(dv_table[i]->dest_id)) {
			// poison reverse
			unsigned short infinity = std::numeric_limits<unsigned short>::max();
			cost->cost = htons(infinity);
		} else {
			std::cout<<"cost? " <<dv_table[i]->cost<<"\n";
			cost->cost = htons(dv_table[i]->cost);
//			memcpy(&cost->cost, &dv_table[i]->cost, sizeof(dv_table[i]->cost));
		}

		std::cout<<"C\n";
		std::cout<<"DV entry: id: "<<cost->node_id<<" cost: "<<cost->cost;
		msg_body->id_cost_pair.push_back(cost);
	}

	unsigned short msg_size = sizeof(struct msg_header) + sizeof(msg_body);

	for (unsigned int i = 0; i < port_status_table.size(); i++) {
		std::cout<<"D\n";

		msg_header *header = (msg_header *) malloc(sizeof(struct msg_header));
		header->dst = htons(port_status_table[i]->neighbor_id);
		header->src = htons(router_id);
		header->size = htons(msg_size);
		header->type = DV;

		// copy content
		char *msg = (char *) malloc(msg_size);
		memcpy(msg, header, sizeof(header));
		memcpy(msg + sizeof(header), msg_body, sizeof(msg_body));

		std::cout<<"E"<<" port: "<<port_status_table[i]->port<<"\n";
		sys->send(port_status_table[i]->port, msg, msg_size);

		std::cout<<"F\n";
		delete header;

		std::cout<<"G\n";
	}
}

// add more of your own code
