#include <netinet/in.h>
#include <string.h>
#include "Node.h"
#include "RoutingProtocolImpl.h"
#include <limits>

#define CHECK_ENTRY_INTERVAL 1000 // 1 sec
#define PING_INTERVAL 10000  // 10 secs
#define DV_UPDATE_INTERVAL 30000	// 30 secs
#define LS_UPDATE_INTERVAL 30000	// 30 secs
#define PORT_STATUS_TIMEOUT 15000	// 15 secs
#define DV_TIMEOUT 45000	//45 secs
#define LS_TIMEOUT 45000	//45 secs


instruction instr[] = {SEND_PING, CHECK_ENTRY, SEND_DV, SEND_LS};


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
	if(protocol_type == P_LS) {
		this->ls_seq_num = 0; // Initialize ls sequence number to be 0;
	}

	std::cout<<"My Router ID is "<<router_id<<"\n";
	send_ping_msg();
	check_entries();
	if(protocol_type == P_DV) {
		sys->set_alarm(this, DV_UPDATE_INTERVAL, &instr[SEND_DV]);
	} else {
		sys->set_alarm(this, LS_UPDATE_INTERVAL, &instr[SEND_LS]);
	}
}

void RoutingProtocolImpl::handle_alarm(void *data) {
  // add your own code

	if (memcmp(data, &instr[SEND_PING], sizeof(instruction)) == 0) {
		send_ping_msg();
	} else if (memcmp(data, &instr[CHECK_ENTRY], sizeof(instruction)) == 0) {
		check_entries();
	} else if (memcmp(data, &instr[SEND_DV], sizeof(instruction)) == 0) {
		schedule_dv_update();
	} else if (memcmp(data, &instr[SEND_LS], sizeof(instruction)) == 0) {
		schedule_ls_update();
	}

}

void RoutingProtocolImpl::recv(unsigned short port, void *packet, unsigned short size) {
  // add your own code

	unsigned char pkt_type = *((unsigned char *)packet);
	msg_header *header;

	if (pkt_type == PING) {
		header = (msg_header *)packet;
		memset(&header->type, PONG, sizeof(header->type));
		memcpy(&header->dst, &header->src, sizeof(header->dst));
		unsigned short htons_id = htons(router_id);
		memcpy(&header->src, &htons_id, sizeof(header->src));

		sys->send(port, packet, size);

	} else if (pkt_type == PONG) {
		header = (msg_header *)packet;
		unsigned int current_time = sys->time();
		void *nbr_time_pt = (unsigned char *)packet + sizeof(struct msg_header);
		unsigned int neighbor_time = *((unsigned int *)nbr_time_pt);
		unsigned short new_cost = current_time - ntohl(neighbor_time);
		std::cout<<"current_time "<<current_time<<" neighbor_time "<<ntohl(neighbor_time)<<" new cost "<<new_cost<<"\n";

		port_status_entry *en = get_nbr_port_status_entry(ntohs(header->src));

		if (en == NULL) {
			std::cout<<"en is null \n";
			en = (struct port_status_entry *) malloc(sizeof(struct port_status_entry));
			en->neighbor_id = ntohs(header->src);
			en->port = port;
			en->cost = 0;
			en->last_update = current_time;
			port_status_table.push_back(en);
		}
		std::cout<<"en_cost "<<en->cost<<"\n";

		if (protocol_type == P_DV) {
			// Initializing for distance vector
			if (!dv_contains_dest(en->neighbor_id)) {
				dv_entry* dv_en = (struct dv_entry*) malloc(sizeof(struct dv_entry));
				dv_en->dest_id = en->neighbor_id;
				dv_en->port = port;
				dv_en->next_hop_id = en->neighbor_id;
				dv_en->last_update = current_time;
				dv_en->cost = numeric_limits<unsigned short>::max(); //TODO: ask Jiafang why? why not use new_cost?
				dv_table.push_back(dv_en);
				std::cout<<"dv_en's next hop id "<<dv_en->next_hop_id<<"\n";
				//TODO: why not send DV_msg here?
			}
		} else { // protocol LS
			// Initializing for ls_neighbor_info for myself
			if (!ls_contains_dest(en->neighbor_id)) {
				ls_entry* ls_en = (struct ls_entry*) malloc(sizeof(struct ls_entry));
				ls_en->node_id = en->neighbor_id;
				ls_en->cost = new_cost;
				ls_en->last_update = current_time;
				ls_neighbor_info.push_back(ls_en);
			}
		}

		// Initializing for forwarding table
		if (!ft_contains_dest(en->neighbor_id)) {
			forwarding_table_entry* ft_en = (struct forwarding_table_entry*) malloc(sizeof(struct forwarding_table_entry));
			ft_en->dest_id = en->neighbor_id;
			ft_en->next_hop_id = en->neighbor_id;
			ft_en->port = port;
			forwarding_table.push_back(ft_en);
		}

		// DEBUG print
		for (unsigned int i = 0; i < dv_table.size(); i++) {
			std::cout<<"dest_id "<<dv_table[i]->dest_id<<" "<<"cost "<<dv_table[i]->cost<<" "
					<<"next_hop "<<dv_table[i]->next_hop_id<<"\n";
		}


		// update DV || LS & FT if cost with neighbor changes
		if (new_cost != en->cost) {
			std::cout<<"en->cost "<<en->cost<<" new_cost "<<new_cost<<" diff "<<new_cost - en->cost<<"\n";
			if(protocol_type == P_DV) {
				// TODO: ask jiangjiafang, should have if statment, like below.
				updateDV_from_cost_change(en->neighbor_id, new_cost - en->cost);
				send_DV_msg();
			} else {	// protocol LS
				if(en->cost != numeric_limits<unsigned short>::max()) {
					// If there was a LS value with cost, that is not infinity,
					// pass in the delta, not the value to update to.
					updateLS_from_cost_change(en->neighbor_id, new_cost - en->cost);
				} else {
					// If the LS cost was originally infinity, meaning no connection,
					// then pass in the cost value to be updated to, directly.
					updateLS_from_cost_change(en->neighbor_id, new_cost);
				}
				send_LS_msg(port);
			}
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
			unsigned short dst = htons(router_id);
			memcpy(&header->src, &dst, sizeof(header->src)); // Make sure source is this router
		}

		unsigned short pkt_dst = ntohs(header->dst);
		if(pkt_dst == router_id) {
			// Free packet memory when it is at destination.
			free(packet);
		} else {
			forwarding_table_entry *ft_en = get_ft_entry_by_dest(pkt_dst);
			if (ft_en == NULL) {
				std::cout << "No forwarding table entry for destination router " << pkt_dst << "\n";
			} else {
				sys->send(ft_en->port, packet, ntohs(header->size));
			}
		}
	} else if (pkt_type == DV) {
		header = (msg_header *)packet;
		unsigned short neighbor_id = ntohs(header->src);

		updateDV_from_DV_msg(port, neighbor_id, (char *)packet + sizeof(struct msg_header),
			(ntohs(header->size) - sizeof(struct msg_header))/sizeof(struct node_cost));
		free(packet);
	} else if (pkt_type == LS) {
		header = (msg_header *)packet;
		unsigned short source_id = ntohs(header->src);

		updateLSTable_from_LSP(port, source_id, (char *)packet + sizeof(struct msg_header),
					(ntohs(header->size) - sizeof(struct msg_header) - sizeof(unsigned int))/sizeof(struct ls_entry));
		free(packet);
	}
}

void RoutingProtocolImpl::updateLSTable_from_LSP(
	unsigned short port, unsigned short source_id, char *body_start, int pair_count) {

	unsigned int current_time = sys->time();

	hash_map<unsigned short, struct ls_body*>::iterator i = ls_table.find(source_id);
	if(i == ls_table.end()) {
		// Did not find LSP for this source inside the LS table;
		char *LSP_data = (char *)malloc(sizeof(struct ls_entry) * pair_count);
		unsigned int seq_num;
		memcpy(&seq_num, body_start, sizeof(unsigned int));

		ls_body *this_ls_body = (ls_body *)malloc(sizeof(struct ls_body));
		this_ls_body->seq_num = seq_num;
		this_ls_body->node_cost = LSP_data;

		ls_table[source_id] = this_ls_body;

	} else {

	}

}

void RoutingProtocolImpl::updateDV_from_DV_msg(
	unsigned short port, unsigned short neighbor_id, char *body_start, int pair_count) {

	unsigned int current_time = sys->time();

	// update dv_entry of neighbor
	dv_entry *nbr = get_dv_entry_by_dest(neighbor_id);
	nbr->last_update = current_time;

	bool broadcast_dv_msg = false;
	int index = 0;

	while(index < pair_count) {
		node_cost *pair = (node_cost *)body_start + index * sizeof(struct node_cost);

		unsigned short node_id = ntohs(pair->node_id);
		unsigned short neighbor_cost = ntohs(pair->cost);

		index++;

		std::cout<<"DV_msg id: "<<node_id<<" updated cost: "<<neighbor_cost<<"\n";

		if (node_id == router_id) {
			continue;
		}

		if(!dv_contains_dest(node_id)) {
			// add new distance vector entry (with new destination)
			std::cout<<"build new entry\n";
			dv_entry* dv_en = (dv_entry*) malloc(sizeof(struct dv_entry));
			dv_en->dest_id = node_id;
			dv_en->port = port;
			dv_en->next_hop_id = neighbor_id;
			dv_en->last_update = current_time;
			dv_table.push_back(dv_en);

			// add ft entry
			forwarding_table_entry *ft_entry = (forwarding_table_entry *) malloc(sizeof(struct forwarding_table_entry));
			ft_entry->dest_id = node_id;
			ft_entry->next_hop_id = neighbor_id;
			ft_entry->port = port;
			forwarding_table.push_back(ft_entry);

			broadcast_dv_msg = true;
		} else {
			// try to compare and update the existing distance vector entry
			unsigned short new_cost = neighbor_cost + get_nbr_port_status_entry(neighbor_id)->cost;
			dv_entry *old_dv_entry = get_dv_entry_by_dest(node_id);
			std::cout<<"old cost is: "<<old_dv_entry->cost<<" new cost is: "<<new_cost<<"\n";

			if (old_dv_entry->cost > new_cost) {
				// make path to dest have this neighbor as next hop.
				old_dv_entry->cost = new_cost;
				old_dv_entry->next_hop_id = neighbor_id;
				old_dv_entry->port = port;
				broadcast_dv_msg = true;

				// update ft entry
				forwarding_table_entry *ft_entry = get_ft_entry_by_dest(node_id);
				ft_entry->next_hop_id = neighbor_id;
				ft_entry->port = port;
			}
			old_dv_entry->last_update = current_time;
		}
	}
	if (broadcast_dv_msg) {
		send_DV_msg();
	}
}

void RoutingProtocolImpl::send_ping_msg() {

    unsigned int cur_time = htonl(sys->time());
    char *msg = (char *) malloc(sizeof(struct msg_header) + sizeof(cur_time));
    struct msg_header *pkt  = (struct msg_header *) msg;

    pkt->type = (unsigned char) PING;
    pkt->size = htons(sizeof(struct msg_header) + sizeof(cur_time));
    pkt->src = htons(this->router_id);

    std::cout<<"cur_time in ping "<<ntohl(cur_time)<<" after htonl "<<cur_time<<"\n";

    void *time_addr = msg + sizeof(struct msg_header);
    memcpy(time_addr, &cur_time, sizeof(cur_time));

    std::cout<<"time after set "<<ntohl((unsigned int)(*((unsigned int *)time_addr)))<<"\n";

    for (unsigned short i = 0; i < num_ports; i++) {
    	sys->send(i, msg, sizeof(struct msg_header) + sizeof(cur_time));
    }

    sys->set_alarm(this, PING_INTERVAL, &instr[SEND_PING]);


}

void RoutingProtocolImpl::check_entries() {

	unsigned int cur_time = sys->time();

	vector<struct port_status_entry*>::iterator port_iter = port_status_table.begin();
	while (port_iter != port_status_table.end()) {
//		std::cout<<"[CE]cur_time "<<cur_time<<"\n";
//		std::cout<<"[CE]last update "<<(*port_iter)->last_update<<"\n";
		std::cout<<"[CE]diff "<<cur_time - (*port_iter)->last_update<<"\n";
		if (cur_time - (*port_iter)->last_update > PORT_STATUS_TIMEOUT) {
			updateDV_from_cost_change(
					(*port_iter)->neighbor_id, std::numeric_limits<unsigned short>::max());
			remove_ft_entry_by_port((*port_iter)->port);
			port_iter = port_status_table.erase(port_iter);
		} else {
			++port_iter;
		}
	}

	if(protocol_type == P_LS) {
		// check Local LSP (should we check LS table, or Local LSP)
		vector<struct ls_entry*>::iterator ls_iter = ls_neighbor_info.begin();
		while (ls_iter != ls_neighbor_info.end()) {
			if (cur_time - (*ls_iter)->last_update > LS_TIMEOUT) {
				ls_iter = ls_neighbor_info.erase(ls_iter);
				remove_ft_entry_by_dest((*ls_iter)->node_id); // is this needed?
			} else {
				++ls_iter;
			}
		}
	} else { // protocol_type == P_DV
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
	}

	sys->set_alarm(this, CHECK_ENTRY_INTERVAL, &instr[CHECK_ENTRY]);
}

void RoutingProtocolImpl::updateDV_from_cost_change(
		unsigned short neighbor_id, unsigned short delta) {

	std::cout<<"update from cost change "<<delta<<"\n";
	unsigned int cur_time = sys->time();

	if (delta == std::numeric_limits<unsigned short>::max()) {
		// update all costs in entries with nextHop as neighbor_id to infinity
		for (unsigned int i = 0; i < dv_table.size(); i++) {
			if (dv_table[i]->next_hop_id == neighbor_id) {
				dv_table[i]->cost = delta;
				dv_table[i]->last_update = cur_time;
			}
		}
	} else {
		// update all costs in entries with nextHop as neighbor_id to update_val
		// if this triggers new DV (nextHop changes to directly routing to neighbor,
		// or general min_cost change[where dest is not in the neighbor list], broad-
		// cast new DV to neighbors)
		for (unsigned int i = 0; i < dv_table.size(); i++) {
			if (dv_table[i]->next_hop_id == neighbor_id) {
				if (dv_table[i]->cost == numeric_limits<unsigned short>::max()) {
					dv_table[i]->cost = delta;
				} else {
					dv_table[i]->cost = dv_table[i]->cost + delta;
				}
				dv_table[i]->last_update = cur_time;
			}
		}
	}
}

void RoutingProtocolImpl::updateLS_from_cost_change(
		unsigned short neighbor_id, unsigned short delta) {

	std::cout<<"update LS from cost change "<<delta<<"\n";
	unsigned int cur_time = sys->time();

	if (delta == std::numeric_limits<unsigned short>::max()) {
		// update ls body entry with node_id, and infinity cost
		for (unsigned int i = 0; i < ls_neighbor_info.size(); i++) {
			if (ls_neighbor_info[i]->node_id == neighbor_id) {
				ls_neighbor_info[i]->cost = std::numeric_limits<unsigned short>::max();
				ls_neighbor_info[i]->last_update = cur_time;
				break; // there would only be one entry with this node id.
			}
		}
	} else {
		for (unsigned int i = 0; i < ls_neighbor_info.size(); i++) {
			if (ls_neighbor_info[i]->node_id == neighbor_id) {
				if (ls_neighbor_info[i]->cost == numeric_limits<unsigned short>::max()) {
					ls_neighbor_info[i]->cost = delta;
				} else {
					ls_neighbor_info[i]->cost = ls_neighbor_info[i]->cost + delta;
				}
				ls_neighbor_info[i]->last_update = cur_time;
				break; // there would only be one entry with this node id.
			}
		}
	}

}

port_status_entry* RoutingProtocolImpl::get_nbr_port_status_entry(unsigned short neighbor_id) {
	for (unsigned int i = 0; i < port_status_table.size(); i++) {
		if (port_status_table[i]->neighbor_id == neighbor_id)
			return port_status_table[i];
	}
	return NULL;
}

bool RoutingProtocolImpl::dv_contains_dest(unsigned short node_id) {
	std::cout<<"node_id in check: "<<node_id<<"\n";
	std::cout<<"dv table contains: \n";

	for (unsigned int i = 0; i < dv_table.size(); i++) {
		std::cout<<"\t"<<dv_table[i]->dest_id<<" "<<dv_table[i]->cost<<"\n";
		if (dv_table[i]->dest_id == node_id)
			return true;
	}
	return false;
}


bool RoutingProtocolImpl::ls_contains_dest(unsigned short node_id) {

	for (unsigned int i = 0; i < ls_neighbor_info.size(); i++) {
		std::cout<<"\t"<<ls_neighbor_info[i]->node_id<<" "<<ls_neighbor_info[i]->cost<<"\n";
		if (ls_neighbor_info[i]->node_id == node_id)
			return true;
	}
	return false;
}

bool RoutingProtocolImpl::ft_contains_dest(unsigned short node_id) {
	for (unsigned int i = 0; i < forwarding_table.size(); i++) {
		if (forwarding_table[i]->dest_id == node_id)
			return true;
	}
	return false;
}

dv_entry* RoutingProtocolImpl::get_dv_entry_by_dest(unsigned short node_id) {
	for (unsigned int i = 0; i < dv_table.size(); i++) {
		if (dv_table[i]->dest_id == node_id)
			return dv_table[i];
	}
	return NULL;
}

forwarding_table_entry* RoutingProtocolImpl::get_ft_entry_by_dest(unsigned short node_id) {
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
			++ft_iter;
		}
	}
}

void RoutingProtocolImpl::send_DV_msg() { // TODO: ask jiangjiafang, when flood, you send this DV to where it is from as well?
	// Build msg body

	for (unsigned int i = 0; i < port_status_table.size(); i++) {

		// Get count of node cost pair to send
		int count = 0;
		for (unsigned int j = 0; j < dv_table.size(); j++) {
			if (dv_table[j]->cost == std::numeric_limits<unsigned short>::max()) {
				continue;
			}
			count++;
		}
		msg_header *dv_header = (msg_header *) malloc(sizeof(struct msg_header));
		dv_header->src = htons(router_id);
		dv_header->dst = htons(port_status_table[i]->neighbor_id);
		dv_header->type = DV;
		unsigned short msg_size = sizeof(struct msg_header) + sizeof(struct node_cost) * count;
		dv_header->size = htons(msg_size);

		char *msg = (char *) malloc(msg_size);
		memcpy(msg, dv_header, sizeof(struct msg_header));
		free(dv_header);

		count = 0; // offset in dv msg body
		for (unsigned int j = 0; j < dv_table.size(); j++) {
			if (dv_table[j]->cost == std::numeric_limits<unsigned short>::max()) {
				continue;
			}

			node_cost *cost_pair = (node_cost *) malloc(sizeof(struct node_cost));
			cost_pair->node_id = htons(dv_table[j]->dest_id);
			if (dv_table[j]->next_hop_id == port_status_table[i]->neighbor_id
					&& dv_table[j]->dest_id != port_status_table[i]->neighbor_id) {
				// poison reverse
				cost_pair->cost = htons(numeric_limits<unsigned short>::max());
			} else {
				// normal case
				cost_pair->cost = htons(dv_table[j]->cost);
			}
			memcpy(msg + sizeof(struct msg_header) + count * sizeof(struct node_cost), cost_pair, sizeof(struct node_cost));
			free(cost_pair);
			count++;
		}

		sys->send(port_status_table[i]->port, msg, msg_size);
	}
}


void RoutingProtocolImpl::send_LS_msg(unsigned short dont_send_to_this_port) {
	// Build LSP msg body

	 for (unsigned int i = 0; i < port_status_table.size(); i++) {

	 	// Get count of node cost pair to send
	 	int count = 0;
	 	for (unsigned int j = 0; j < ls_neighbor_info.size(); j++) {
	 		if (ls_neighbor_info[j]->cost == std::numeric_limits<unsigned short>::max()) {
	 			continue;
	 		}
	 		count++;
	 	}
	 	msg_header *ls_header = (msg_header *) malloc(sizeof(struct msg_header));
	 	ls_header->src = htons(router_id);
	 	ls_header->type = LS;
	 	unsigned short msg_size = sizeof(struct msg_header) + sizeof(struct node_cost) * count + sizeof(unsigned int);
	 	ls_header->size = htons(msg_size);

	 	char *msg = (char *) malloc(msg_size);
	 	memcpy(msg, ls_header, sizeof(struct msg_header));
	 	free(ls_header);

	 	// put seq num in packet, and increment it for the use of next round.
	 	memcpy(msg + sizeof(struct msg_header), &ls_seq_num, sizeof(unsigned int));
	 	ls_seq_num++;

	 	count = 0; // offset in ls msg body
	 	for (unsigned int j = 0; j < ls_neighbor_info.size(); j++) {
	 		if (ls_neighbor_info[j]->cost == std::numeric_limits<unsigned short>::max()) {
	 			continue;
	 		}

	 		node_cost *cost_pair = (node_cost *) malloc(sizeof(struct node_cost));
	 		cost_pair->node_id = htons(ls_neighbor_info[j]->node_id);
	 		cost_pair->cost = htons(ls_neighbor_info[j]->cost);

	 		memcpy(msg + sizeof(struct msg_header) + count * sizeof(struct node_cost), cost_pair, sizeof(struct node_cost));
	 		free(cost_pair);
	 		count++;
	 	}

	 	sys->send(port_status_table[i]->port, msg, msg_size);
	 }
}

void RoutingProtocolImpl::schedule_ls_update() {
	send_LS_msg(0xffff); // 0xffff is a special value for port that will not be in the port table for any router.
	sys->set_alarm(this, LS_UPDATE_INTERVAL, &instr[SEND_LS]);
}

void RoutingProtocolImpl::schedule_dv_update() {
	send_DV_msg();
	sys->set_alarm(this, DV_UPDATE_INTERVAL, &instr[SEND_DV]);
}
// add more of your own code
