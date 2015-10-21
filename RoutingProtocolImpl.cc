#include <netinet/in.h>
#include <string.h>
#include "Node.h"
#include "RoutingProtocolImpl.h"

#define PING_INTERVAL 10000  // 10 secs
#define PORT_STATUS_TIMEOUT 15	// 15 secs

RoutingProtocolImpl::RoutingProtocolImpl(Node *n) : RoutingProtocol(n) {
  sys = n;
  instr = {SEND_PING, SEND_DV};
  // add your own code
}

RoutingProtocolImpl::~RoutingProtocolImpl() {
  // add your own code (if needed)
}

void RoutingProtocolImpl::init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type) {
  // add your own code
	this->num_ports = num_ports;
	this->router_id = router_id;
	this->protocol_type = protocol_type;

	std:cout<<"My Router ID is "<<router_id<<"\n";
	this->send_ping_msg();
}

void RoutingProtocolImpl::handle_alarm(void *data) {
  // add your own code

	if (memcmp(data, &instr[SEND_PING], sizeof(instruction)) == 0) {
		this->send_ping_msg();
	}

}

void RoutingProtocolImpl::recv(unsigned short port, void *packet, unsigned short size) {
  // add your own code

	std::cout<<"received a ping\n";

	unsigned int pkt_type = *((unsigned char *)packet);
	msg_header *header;
	if (pkt_type == PING) {
		header = (msg_header *)packet;
//		header.dst = header.src;
//		header.src = router_id;
		memset(&header->type, PONG, sizeof(header->type));
		memcpy(&header->dst, &header->src, sizeof(header->dst));
		int htons_id = htons(router_id);
		memcpy(&header->src, &htons_id, sizeof(header->src));

		unsigned int newbla = *((unsigned char *)packet);
		std::cout<<"header.dst "<<ntohs(header->dst)<<" header.src "<<ntohs(header->src)<<"\n";
		sys->send(port, packet, size);

		std::cout<<"PONGPONGPONG"<<"\n";
	} else if (pkt_type == PONG) {
		std::cout<<"RECEIVED PONG"<<"\n";
		header = (msg_header *)packet;

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


		unsigned int current_time = sys->time();
		unsigned int neighbor_time = *((unsigned int *)packet + sizeof(header));
		en->last_update = current_time;
		en->cost = current_time - ntohs(neighbor_time);
		en->port = port;
		std::cout<<"ENTRY: "<<en->last_update<<" "<<en->neighbor_id<<"\n";
	} else if (pkt_type == DATA){

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
    std::cout<<"SENT A PING\n";

    delete pkt;

}

void RoutingProtocolImpl::check_entries() {
	unsigned int cur_time = sys->time();

	vector<struct port_status_entry*>::iterator iter = port_status_table.begin();
	while (iter != port_status_table.end()) {
		if (cur_time - (*iter)->last_update > PORT_STATUS_TIMEOUT) {
			updateDV_from_cost_change((*iter)->neighbor_id, std::numeric_limits<int>::max());
			iter = port_status_table.erase(iter);

		} else {
			++iter;
		}
	}

	for (unsigned int i = 0; i < port_status_table.size(); i++) {
		if (cur_time - port_status_table[i]->last_update > PORT_STATUS_TIMEOUT) {
			// do something
		}
	}
}

void RoutingProtocolImpl::updateDV_from_cost_change(
		unsigned short neighbor_id, unsigned int update_val) {
	if (update_val == std::numeric_limits<int>::max()) {
		// update all costs in entries with nextHop as neighbor_id to infinity
	} else {
		// update all costs in entries with nextHop as neighbor_id to update_val
		// if this triggers new DV (nextHop changes to directly routing to neighbor,
		// or general min_cost change[where dest is not in the neighbor list], broad-
		// cast new DV to neighbors)
	}
}

// add more of your own code
