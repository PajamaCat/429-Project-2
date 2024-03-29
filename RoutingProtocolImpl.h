#ifndef ROUTINGPROTOCOLIMPL_H
#define ROUTINGPROTOCOLIMPL_H

#include "RoutingProtocol.h"

struct port_status_entry {
    unsigned short neighbor_id;
    unsigned short cost;
    unsigned int last_update;
    unsigned short port;
};

struct forwarding_table_entry {
    unsigned short dest_id;
    unsigned short next_hop_id;
    unsigned short port;
};

struct dv_entry {
    unsigned short dest_id;
    unsigned short cost;
    unsigned short next_hop_id;
    unsigned short port;
    unsigned int last_update;
};

struct ls_body {
    unsigned int seq_num;
    char *node_cost;
};

typedef pair <ls_body *, unsigned int> lsTable_val;

typedef pair <unsigned short, lsTable_val> ls_map_pair;

typedef pair <unsigned short, unsigned short> djk_map_pair;

struct ls_entry {
    unsigned short node_id;
    unsigned short cost;
    unsigned int last_update;
};

struct node_cost {
    unsigned short node_id;
    unsigned short cost;
};

struct msg_header {
  unsigned char type;
  unsigned char reserved;
  unsigned short size;
  unsigned short src;
  unsigned short dst;
};

enum instruction {
  SEND_PING,
  CHECK_ENTRY,
  SEND_DV,
  SEND_LS
};

class RoutingProtocolImpl : public RoutingProtocol {
  public:
    RoutingProtocolImpl(Node *n);
    ~RoutingProtocolImpl();

    void init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type);
    // As discussed in the assignment document, your RoutingProtocolImpl is
    // first initialized with the total number of ports on the router,
    // the router's ID, and the protocol type (P_DV or P_LS) that
    // should be used. See global.h for definitions of constants P_DV
    // and P_LS.

    void handle_alarm(void *data);
    // As discussed in the assignment document, when an alarm scheduled by your
    // RoutingProtoclImpl fires, your RoutingProtocolImpl's
    // handle_alarm() function will be called, with the original piece
    // of "data" memory supplied to set_alarm() provided. After you
    // handle an alarm, the memory pointed to by "data" is under your
    // ownership and you should free it if appropriate.

    void recv(unsigned short port, void *packet, unsigned short size);
    // When a packet is received, your recv() function will be called
    // with the port number on which the packet arrives from, the
    // pointer to the packet memory, and the size of the packet in
    // bytes. When you receive a packet, the packet memory is under
    // your ownership and you should free it if appropriate. When a
    // DATA packet is created at a router by the simulator, your
    // recv() function will be called for such DATA packet, but with a
    // special port number of SPECIAL_PORT (see global.h) to indicate
    // that the packet is generated locally and not received from 
    // a neighbor router.

    void send_ping_msg();
    // send ping messages

    void check_entries();
    // check expired entries in port_status_table and dv table

    void schedule_dv_update();
    void schedule_ls_update();

    void updateDV_from_cost_change(unsigned short neighbor_id, unsigned short update_val);
    void updateLS_from_cost_change(unsigned short neighbor_id, unsigned short update_val);
    void updateDV_from_DV_msg(unsigned short port, unsigned short neighbor_id, char *body_start, int pair_count);
    void updateLSTable_from_LSP(unsigned short port, unsigned short neighbor_id, char *body_start, int pair_count);

    void delete_from_ft(unsigned short neighbor_id, unsigned short update_val);
    // delete entry whose next hop is neighbor_id in forwarding table

    void send_DV_msg();
    void send_LS_msg(unsigned short dont_send_to_this_port);

    port_status_entry* get_nbr_port_status_entry(unsigned short neighbor_id);

    bool dv_contains_dest(unsigned short node_id);
    bool ls_contains_dest(unsigned short node_id);
    bool ft_contains_dest(unsigned short node_id);

    dv_entry* get_dv_entry_by_dest(unsigned short node_id);

    forwarding_table_entry* get_ft_entry_by_dest(unsigned short node_id);

    void remove_ft_entry_by_port(unsigned short port);
    void remove_ft_entry_by_dest(unsigned short dest);

    // For LS
    void compute_dijsktra();
    bool pq_contains_node(vector<djk_map_pair> input_vec, unsigned short input_node_id);
    void update_pq_cost_map_and_parent_map(hash_map<unsigned short, unsigned short> node_to_cost,
    hash_map<unsigned short, unsigned short> node_to_parent, vector<djk_map_pair> pq, unsigned short node_id);
    unsigned short get_min_node(vector<djk_map_pair> pq, vector<unsigned short> visited);
    bool visited_contains_node(vector<unsigned short> input_vec, unsigned short node_id);

 private:
    Node *sys; // To store Node object; used to access GSR9999 interfaces 
    unsigned short num_ports;
    unsigned short router_id;
    eProtocolType protocol_type;
    unsigned int ls_seq_num;
    vector<struct port_status_entry*> port_status_table;
    vector<struct dv_entry*> dv_table;
    vector<struct forwarding_table_entry*> forwarding_table;
    hash_map<unsigned short, lsTable_val> ls_table;
    vector<struct ls_entry*> ls_neighbor_info;
};

#endif

