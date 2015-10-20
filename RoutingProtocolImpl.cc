#include <netinet/in.h>
#include <string.h>
#include "Node.h"
#include "RoutingProtocolImpl.h"

#define PING_INTERVAL 10000  // 10 secs

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

	this->sendPingMsg();
}

void RoutingProtocolImpl::handle_alarm(void *data) {
  // add your own code

	if (memcmp(data, &instr[SEND_PING], sizeof(instruction)) == 0) {
		this->sendPingMsg();
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
		memset(&header->dst, header->src, sizeof(header->dst));
		memset(&header->src, htons(router_id), sizeof(header->src));

		unsigned int newbla = *((unsigned char *)packet);
		std::cout<<"packet type"<<newbla<<"\n";
		sys->send(port, packet, size);

		std::cout<<"PONGPONGPONG"<<"\n";
	} else if (pkt_type == PONG) {
		std::cout<<"RECEIVED PONG"<<"\n";
//		header = (msg_header *)packet;
//
//		port_status_entry *en;
//
//		if (port_status_table.find(header->src) == port_status_table.end()) { // If this neighbor ID is not in port_status_table
//			en = (struct port_status_entry *) malloc(sizeof(struct port_status_entry));
//			en->neighbor_id = header->src;
//			port_status_table[header->src] = en;
//		} else {
//			en = port_status_table[header->src];
//		}
//		unsigned int current_time = sys->time();
//		unsigned int neighbor_time = *((unsigned int *)packet + sizeof(header));
//		en->last_update = current_time;
//		en->cost = current_time - ntohs(neighbor_time);
//		en->port = port;
//		std::cout<<"ENTRY: "<<en->last_update<<" "<<en->neighbor_id<<"\n";
	}
}

void RoutingProtocolImpl::sendPingMsg() {

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

// add more of your own code
