#ifndef PACKET_STRUCTURES_H
#define PACKET_STRUCTURES_H
/*******************STRUCTURES**********************/

// This structure is used routing table entries.
// Total byte = 8 byte.
typedef struct{
	uint8_t type;// Standard C includes:
	uint8_t dest;
	uint8_t next_hop;
	uint8_t tot_hop;		// Total hop number for this destination.
	int16_t metric;
	uint16_t seq_no;
	// This structure can be extended with some additional flags.
}rt_entry;

typedef struct{

	uint8_t type;// Standard C includes:
	uint8_t no_entries;
	rt_entry table[MAX_NODES];

}routing_table;
// This structure will be used to transmit sensor information of the RSU.
typedef struct{
	uint8_t type;			// Indicates the type of unicast packet.
	uint8_t src_add;		// Source address of the generated unicast message.
	// Inside parameters will be filled later.
	int data;

}rsu_sensor_p;


typedef struct{

	uint8_t type;			// Indicates the type of unicast packet.
	uint8_t src_add;		// Source address of the generated unicast message.
	// Inside parameters will be filled later.
	uint16_t temp;
	uint16_t batt;
	uint16_t count;

}data_logger_p;



// This structure will be used transmit data with a destination address
typedef struct{
	uint8_t type;			// Indicates the type of unicast packet.
	uint8_t src_add;		// Source address of the generated unicast message.
	uint8_t dest_add;
	// Inside parameters will be filled later.
	int data;

}GWmessage;

// This is a linked list for periodic routing table entries.
struct permanent_rt_entry{
	struct permanent_rt_entry *next;
	rt_entry entry;
	// This ctimer will be used to detect the stale entries in the routing table.
	// If a mote does not receive any routing entry for a destination address for a certain interval,
	// It will delete the routing entry from its table.
	struct ctimer ctimer_stale_entry;
	// This ctimer will be used to broadcast the triggered updates.
	// If a mote receives a routing entry which should be broadcasted without waiting the periodic update,
	// It will call the required function.
	struct ctimer ctimer_advertise_entry;
};

// This routing entry is used for controlled rebroadcast mechanism to inform other MUs in the network.
struct MU_routing_entry{
	struct MU_routing_entry *next;
	// We can use the same routing entry struct to hold information about MU.
	rt_entry entry;
	// We will have a timer in each RSU to discard the packets after some time.
	struct ctimer ctimer_stale_entry;
	// Since we should advertise the received message immediately, we dont need to use advertise timer.
};

/*********************************************************/

/********************ROUTING LIST*************************/
// Linked list for periodic routing table.
// I used the term periodic because, this table is periodically broadcasted for routing table updates.
// Static routing tables can be used which can be accessed faster than linked list.
// But for now, this is not our priority.
LIST(permanent_rt_list);
MEMB(permanent_rt_mem, struct permanent_rt_entry, MAX_NODES);		// Maybe we can use this later for different purposes.

LIST(MU_rt_list);
MEMB(MU_rt_mem, struct MU_routing_entry, MAX_NODES);		// Maybe we can use this later for different purposes.

LIST(advertise_rt_list);
MEMB(advertise_rt_mem, struct permanent_rt_entry, MAX_NODES);	// advertise routing entry has the same structure as permanent routing entry.
/********************************************************/

#endif
