/*
 * c2x_v0.c
 *
 *  Created on: Jan 8, 2019
 *      Author: group1
 */

// Contiki-specific includes:
#include "contiki.h"
#include "dev/leds.h"
#include "dev/button-sensor.h"
#include "dev/leds.h"
#include "dev/serial-line.h"
#include "core/net/linkaddr.h"
#include "lib/list.h"
#include "lib/memb.h"
#include "lib/random.h"
#include "net/rime/rime.h"
#include "sys/ctimer.h"			// Callback function
#include "dev/cc2538-rf.h"
#include "dev/zoul-sensors.h"  // Sensor functions
#include "dev/adc-zoul.h"      // ADC
#include "dev/sys-ctrl.h"
#include "net/netstack.h"		// Wireless-stack definitions
#include "sonar_sensor.h"
// Standard C includes:
#include <string.h>
#include <stdio.h>				// For prints.
#include <stdlib.h>				// For atoi() function
#include "project-conf.h"
#include "packet_structure.h"
#include "timers.h"

static int16_t rssi_weight[MAX_NODES+1];

/*******************CHANNELS***********************/
// Creates an instance of a unicast connection.
static struct unicast_conn unicast;
static struct broadcast_conn broadcast;
/***************************************************/


/******************HELPER_FUNCTIONS********************/
/**
 * @brief 		Print the given routing table entry.
 */
static void print_rt_entry(rt_entry *e)
{
	//GUI friendly printing
	printf("\rType_%d Dest_Node %d Next_hop %d Total_Hop %d Metric %d Seq_No %lu\n", e->type,
				 	 (e->dest),(e->next_hop), e->tot_hop,
					 e->metric, e->seq_no);
	/*
	printf("\r Type %d Dest. Node %d: Next_hop %d  Total Hop %d Metric %d, Seq. No %d\n", e->type,
			 	 (e->dest),(e->next_hop), e->tot_hop,
				 e->metric, e->seq_no);
*/
}

/**
 * @brief				This function is used to convert given node id value to
 * 						linkaddr_t type which is exploited for unicast messages.
 *
 * @param dest_id		Given node id.
 * @return linkaddr_t
 *
 */
static linkaddr_t convert_to_link(uint8_t dest_id){

	linkaddr_t dest_link;
	dest_link.u8[0] = (dest_id >> 8) & 0xFF;
	dest_link.u8[1] = dest_id & 0xFF;
	return dest_link;
}

/**
 * @brief		This function will remove MU from the routing table.
 *
 *
 */
static void remove_MU_from_list(void *n)
{

	struct MU_routing_entry *e = n;
	printf("\r Remove MU routing from the list.\n");
	//print_rt_entry(&e->entry);
	list_remove(MU_rt_list, e);
	memb_free(&MU_rt_mem, e);
	leds_off(LEDS_BLUE);
}

/**
 * @brief 		This function checks whether given destination id is rebroadcasted.
 * 				Basically, it checks the routing table of MUs and if it finds an destination for given id
 * 				with type REBROADCAST_COLLISION, then it has already broadcasted this entry. Because we add an entry with this type
 * 				when we broadcast the message.
 * @param id	Destination id.
 * @return 		Returns 1 if the given id is found in the list with type REBROADCAST_COLLISION, otherwise 0.
 */
static uint8_t is_rebroadcasted(uint8_t id)
{
	struct MU_routing_entry *e;
	uint8_t in_list = 0;		// Flag is used to determine whether transmitter is in the routing list.
	for(e = list_head(MU_rt_list); e != NULL; e = e->next)
	{
		if((e->entry.dest == id) && (e->entry.type == REBROADCAST_COLLISION))
		{
			in_list = 1;
		}
	}
	return in_list;
}
/******************************************************/


/***********************BROADCAST*CRASH*************************/
/**
 * @brief			This function is called by handle_bc_t1 to broadcast the collision message received from the MU.
 *
 * @param c_entry	Created entry to be broadcasted.
 *
 */
static void rebroadcast_collision(rt_entry c_entry)
{
	struct MU_routing_entry *e;
	rt_entry bc_collision_inform = c_entry;
	bc_collision_inform.type = REBROADCAST_COLLISION;
	e = memb_alloc(&MU_rt_mem);
	if(e != NULL)
	{
		e->entry = bc_collision_inform;
		list_add(MU_rt_list, e);
		// Set the timer for stale entries
		ctimer_set(&e->ctimer_stale_entry, CLOCK_SECOND*5, remove_MU_from_list, e);

		leds_on(LEDS_RED);
		packetbuf_copyfrom(&bc_collision_inform, sizeof(bc_collision_inform));
		leds_on(LEDS_BLUE);
		broadcast_send(&broadcast);
		leds_off(LEDS_RED);
		// Only print the valid entries.
		printf("\rRouting entry for MU %d is rebroadcasted!\n", bc_collision_inform.dest);

		print_rt_entry(&bc_collision_inform);

	}
}

/**
 * @brief               This function handles the type 2 broadcast packets.
 *
 * @param rx_packet     Pointer of the received routing entry.
 * @param tx_id         Transmitter id of the packet.
 */
static void handle_bc_t2(rt_entry *rx_packet, uint8_t tx_id)
{
	if(is_rebroadcasted(rx_packet->dest)==0)
	{
		// Then, we dont have any entry for the given destination.
		// add this mote into the routing table and rebroadcast the message.
		struct MU_routing_entry *e;
		e = memb_alloc(&MU_rt_mem);
		if(e != NULL)
		{
			// Adds the routing entry to the routing table.
			e->entry.type = REBROADCAST_COLLISION;
			e->entry.dest = rx_packet->dest;
			e->entry.next_hop = tx_id;
			//The bitmask 0xFF makes sure that only the last byte of the value is kept.
			e->entry.tot_hop= (rx_packet->tot_hop + 1);// & 0xFF;	// Increase the total hop for this entry + 1.
			// We are not using this part yet.
			e->entry.metric = 0;	// the metric of the transmitter + rssi value.
			e->entry.seq_no = rx_packet->seq_no;
			list_add(MU_rt_list, e);
			// Set the timer for stale entries
			ctimer_set(&e->ctimer_stale_entry, CLOCK_SECOND*PERIODIC_UPDATE_INTERVAL*HOLD_TIMES, remove_MU_from_list, e);
			// Since this is a new received entry, it should be broadcasted without waiting the periodic timer.

			leds_on(LEDS_RED);
			packetbuf_copyfrom(&e->entry, sizeof(e->entry));
			leds_on(LEDS_BLUE);

			broadcast_send(&broadcast);
			leds_off(LEDS_RED);

			printf("\rREBROADCAST is received from %d for MU %d.\n", tx_id, rx_packet->dest);
			print_rt_entry(&e->entry);
		}
	}
}


/***************************************************************/

/**********************ROUTING************************/

// To keep the information about broken motes in the network
static uint8_t broken_motes[MAX_NODES+1] =  { 0 }; // Zero-fills.


/**
 * @brief		Callback function for periodic_rt timer.
 * 				This function is called when the periodic_rt timer expires,
 * 				and broadcasts the routing table of the mode.
 *
 */
static void callback_periodic_rt_update(void *ptr )
{
	ctimer_reset(&periodic_rt_timer);
	struct permanent_rt_entry *e;
	// Packet for broadcasting routing table entry.
	rt_entry bc_rt_packet;
	// TODO: instead of sending the each entry seperately,
	// We can create a packet with routing entries that fits into the packetbuf.
	// Then we can send this packet. Receiver parse this packet and extract each entry.
	// Then routing algorithm is called.

//	int l_len = list_length(permanent_rt_list);
//	rt_entry *tx_rt_packet = malloc(sizeof(rt_entry)*l_len);
	routing_table bc_table;
	bc_table.type = ROUTING_TABLE;
	uint8_t i = 0;
	printf("\rMOTE ROUTING TABLE \n");
	for(e = list_head(permanent_rt_list); e != NULL; e = e->next)
	{

		// if the destination address is the address of the mote.
		// Then increase the sequence number.
		if(e->entry.dest == (linkaddr_node_addr.u8[1] & 0xFF))
		{
			e->entry.seq_no += 2;
		}
		bc_rt_packet = e->entry;
		 //Only print the valid entries.
		//if(e->entry.seq_no % 2 == 0)
		{
			print_rt_entry(&bc_rt_packet);
		}
//		leds_on(LEDS_RED);
//		packetbuf_copyfrom(&bc_rt_packet, sizeof(bc_rt_packet));
//		// TODO: Maybe we can use e.g. 5ms delay to make sure that we avoid collision in packet buffer.
//		// clock_wait(BROADCAST_WAIT)	 // receives an argument in ticks
//		broadcast_send(&broadcast);
//		leds_off(LEDS_RED);
		bc_table.table[i] = e->entry;
		i++;
	}
	bc_table.no_entries = i;
	leds_on(LEDS_RED);
	packetbuf_copyfrom(&bc_table, sizeof(bc_table));
	printf("\r ROUTING TABLE SENT no entries %d\n", bc_table.no_entries);
//TODO: Maybe we can use e.g. 5ms delay to make sure that we avoid collision in packet buffer.
//	clock_wait(BROADCAST_WAIT)	 // receives an argument in ticks
	broadcast_send(&broadcast);
	leds_off(LEDS_RED);
}

/**
 * @brief		Initialize the routing table of the mode.
 * 				The function is called when the mode is first started.
 *
 */
void initialize_routing_tables()
{
	 list_init(permanent_rt_list);
	 memb_init(&permanent_rt_mem);

	 list_init(advertise_rt_list);
	 memb_init(&advertise_rt_mem);

	 ctimer_set(&periodic_rt_timer,CLOCK_SECOND * PERIODIC_UPDATE_INTERVAL, callback_periodic_rt_update, NULL);
	 //ctimer_set(&advertised_rt_timer,CLOCK_SECOND*PERIODIC_UPDATE_INTERVAL, callback_periodic_rt_update, NULL);


	 // Adds its own routing entry to the routing table.
	 struct permanent_rt_entry *e;
	 e = memb_alloc(&permanent_rt_mem);
	 e->entry.dest = linkaddr_node_addr.u8[1] & 0xFF;
	 e->entry.type = 0;
	 e->entry.next_hop = 0;				// Since its own route, it should be zero.
	 e->entry.tot_hop = 0;				// Since its own route, it should be zero.
	 e->entry.metric = 0;
	 e->entry.seq_no = 0;//random_rand();		// Change this later and select reasonable number.

	  /* New entry goes first. */
	 list_push(permanent_rt_list, e);

	 printf("\rFirst routing entry is set\n ");
	 //print_rt_entry(e);

}

/**
 * @brief 		Checks routing table for the given node id.
 * 				This function return 0 if the given node id is not in the
 * 				list of the permanent_rt_list.
 * @param id	Given node id.
 * @return 		0 or 1.
 */
static uint8_t check_permanent_rt(uint8_t id)
{

	struct permanent_rt_entry *e;
	uint8_t in_list = 0;		// Flag is used to determine whether transmitter is in the routing list.
	for(e = list_head(permanent_rt_list); e != NULL; e = e->next)
	{
		if(e->entry.dest == id)
		{
			in_list = 1;
		}
	}
	return in_list;
}

/**
 * @brief		This function will be called when the ctimer_advertise_entry is expired.
 *
 *
 */
static void advertise_rt_entry(void *n)
{
	leds_on(LEDS_RED);
	struct permanent_rt_entry *e = n;

	rt_entry bc_rt_packet = e->entry;

	printf("\rADVERTISE the routing entry\n");
	//print_rt_entry(&bc_rt_packet);

	packetbuf_copyfrom(&bc_rt_packet, sizeof(bc_rt_packet));
	// TODO: Maybe we can use e.g. 5ms delay to make sure that we avoid collision in packet buffer.
	// clock_wait(BROADCAST_WAIT)	 // receives an argument in ticks
	broadcast_send(&broadcast);
	// Since we have advertised the routing entry once,
	// We do not need to call it periodically, so we should stop the timer.
	ctimer_stop(&e->ctimer_advertise_entry);
	// update the permanent routing table.

	// remove from the advertise table.
	leds_off(LEDS_RED);

}

/**
 * @brief 		Removes the routing entry from routing table. We will actually not remove the entry from routing table.
 * 				Only increase the sequence number by 1 which indicates broken links.
 * 				This function will be called by ctimer_stale_entry timer. If a mote does not receive any
 * 				update message from a mote, then we assume that mote is not in the coverage(or failed).
 * 				Therefore, entry related with this mote should be removed from the routing table.
 *
 * @param *n	Pointer belonging to the entry which will be removed.
 *
 */
static void remove_rt_entry(void *n)
{
	// This can also be improved more.
	// Deleted entry can be advertised to other motes in the network.

	struct permanent_rt_entry *e = n;
	printf("\rSTALE entry will be removed \n");
	// increase the seq no.
	e->entry.seq_no = e->entry.seq_no +1;
	//print_rt_entry(&e->entry);

	// Set the mote as a broken.
	broken_motes[e->entry.dest] = 1;
	// Reset the weighted rssi value for this mote.
	rssi_weight[e->entry.dest] = 0;
	// Now, advertise this entry to other motes immediately e.g. 0.1sec
	ctimer_set(&e->ctimer_advertise_entry, ADVERTISE_BROKEN_LINK, advertise_rt_entry, e);
	// Stop the since stale timer for this entry since we have advertised it.
	ctimer_stop(&e->ctimer_stale_entry);
//	list_remove(permanent_rt_list, e);
//	memb_free(&permanent_rt_mem, e);
}

/**
 * @brief 				This function will be called when a mote adds an entry to its routing table.
 *
 * @param rx_packet		received entry.
 * @param tx_node_id	TX node id.
 * @param rssi			RSSI value of received packet.
 *
 */
static void permanent_rt_add_entry(rt_entry * rx_packet, uint8_t tx_node_id, int16_t w_rssi)
{
	struct permanent_rt_entry *e;
	e = memb_alloc(&permanent_rt_mem);
	if(e != NULL)
	{
		// Adds the routing entry to the routing table.
		e->entry.type = ROUTING_ENTRY;
		e->entry.dest = rx_packet->dest;
		e->entry.next_hop = tx_node_id;
		//The bitmask 0xFF makes sure that only the last byte of the value is kept.
		e->entry.tot_hop= (rx_packet->tot_hop + 1);// & 0xFF;	// Increase the total hop for this entry +1.
		e->entry.metric = e->entry.metric + w_rssi;	// the metric of the transmitter + rssi value.
		e->entry.seq_no = rx_packet->seq_no;
		list_add(permanent_rt_list, e);
		// Set the timer for stale entries
		ctimer_set(&e->ctimer_stale_entry, CLOCK_SECOND*PERIODIC_UPDATE_INTERVAL*HOLD_TIMES, remove_rt_entry, e);
		// Since this is a new received entry, it should be broadcasted without waiting the periodic timer.
		ctimer_set(&e->ctimer_advertise_entry, CLOCK_SECOND*ADVERTISED_UPDATE_INTERVAL, advertise_rt_entry, e);
		printf("\r New routing entry is added from %d\n", tx_node_id);
		print_rt_entry(&e->entry);
	}
}
/**
 * @brief 			This function checks the given id is in the advertise routing table list.
 *
 * @param dest_id	User id that needs to be checked.
 *
 * @return 			Return 1 if the given id is in the list, otherwise 0.
 */

static uint8_t check_advertise_rt(uint8_t dest_id)
{
	struct permanent_rt_entry *e;
	uint8_t in_list = 0;		// Flag is used to determine whether transmitter is in the routing list.
	for(e = list_head(advertise_rt_list); e != NULL; e = e->next)
	{
		if(e->entry.dest == dest_id)
		{
			in_list = 1;
		}
	}
	return in_list;
}

/**
 * @brief				This function is called by advertise_stable_route_callback()function and updates the permanent
 * 						routing table when after the advertisement of the received entry.
 *
 * @param rx_packet		Routing entry which should be advertised and updates the permanent routing table.
 *
 */
static void update_permanent_rt(rt_entry rx_packet)
{
	struct permanent_rt_entry *e;
		// Flag is used to determine whether transmitter is in the routing list.
	for(e = list_head(permanent_rt_list); e != NULL; e = e->next)
	{
		if(rx_packet.dest == e->entry.dest)
		{
			e->entry = rx_packet;
			ctimer_set(&e->ctimer_stale_entry, CLOCK_SECOND*PERIODIC_UPDATE_INTERVAL*HOLD_TIMES, remove_rt_entry, e);
			printf("\n\r permanent routing table is updated\n");
		}
	}
}

/**
 * @brief			Callback function which is the called after the expiration of the routing entry
 * 					in the advertisement routing table of the mote.
 *
 * @param *n		Pointer which is passed indicates the permanent routing entry type.
 *
 */
static void advertise_stable_route_callback(void *n)
{

	leds_on(LEDS_RED);
	struct permanent_rt_entry *e = n;

	rt_entry bc_rt_packet = e->entry;

	printf("\rADVERTISE the routing entry\n");
	//print_rt_entry(&bc_rt_packet);

	packetbuf_copyfrom(&bc_rt_packet, sizeof(bc_rt_packet));
	// TODO: Maybe we can use e.g. 5ms delay to make sure that we avoid collision in packet buffer.
	// clock_wait(BROADCAST_WAIT)	 // receives an argument in ticks
	broadcast_send(&broadcast);
	// Since we have advertised the routing entry once,
	// We do not need to call it periodically, so we should stop the timer.
	ctimer_stop(&e->ctimer_advertise_entry);

	// update the permanent routing table.
	update_permanent_rt(bc_rt_packet);
	// remove from the advertise table.
	printf("\rADVERTISE entry is REMOVED\n");
	//print_rt_entry(&e->entry);
	list_remove(advertise_rt_list, e);
	memb_free(&advertise_rt_mem, e);

	leds_off(LEDS_RED);
}

/**
 * @brief				Adds the given entry to advertise routing table and set the timer.
 *
 * @param rx_packet		Routing entry which will be added into advertise routing table.
 * @param tx_node_id	Transmitter of the received entry.
 *
 */
static void add_advertise_rt(rt_entry *rx_packet, uint8_t tx_node_id)
{
	struct permanent_rt_entry *e;
	e = memb_alloc(&advertise_rt_mem);
	if(e != NULL)
	{
		// Adds the routing entry to the routing table.
		e->entry.type = ROUTING_ENTRY;
		e->entry.dest = rx_packet->dest;
		e->entry.next_hop = tx_node_id;
		//The bitmask 0xFF makes sure that only the last byte of the value is kept.
		e->entry.tot_hop= (rx_packet->tot_hop + 1);// & 0xFF;	// Increase the total hop for this entry +1.
		//e->entry.metric = e->entry.metric;	// the metric of the transmitter + rssi value.
		e->entry.seq_no = rx_packet->seq_no;
		e->entry.metric = rx_packet->metric;
		list_add(advertise_rt_list, e);
		// Since this is a new received entry, it should be broadcasted without waiting the periodic timer.
		ctimer_set(&e->ctimer_advertise_entry, CLOCK_SECOND*(ADVERTISED_UPDATE), advertise_stable_route_callback, e);
		printf("\radd_advertise_rt New routing entry is added\n");
		//print_rt_entry(&e->entry);
	}
}

/**
 * @brief 				This function is called to process the entry which will be advertised.
 *
 * @param rx_packet		Routing entry which will be added into advertise routing table.
 * @param tx_node_id	Transmitter of the received entry.
 *
 */
static void process_advertisement(rt_entry *rx_packet, uint8_t tx_node_id)
{
	// If there is no entry for this destination in the advertise routing table
	// add this entry into advertise routing table and start the timer.
	// When the timer expires, the entry will be advertised the other mote and
	// permanent routing table will be updated.
	if(check_advertise_rt(rx_packet->dest) == 0)
	{
		add_advertise_rt(rx_packet, tx_node_id);
	}
	else
	{
		struct permanent_rt_entry *e;
		for(e = list_head(advertise_rt_list); e != NULL; e = e->next)
		{
			if(rx_packet->dest == e->entry.dest)
			{
				// update the advertisement routing table entry.
				if((rx_packet->seq_no) > (e->entry.seq_no))
				{
					e->entry.next_hop = tx_node_id;
					e->entry.seq_no = rx_packet->seq_no;
					e->entry.tot_hop = rx_packet->tot_hop + 1;
					e->entry.metric = rx_packet->metric;
				}
				else if ((rx_packet->seq_no) == (e->entry.seq_no))
				{
					// update the advertisement routing table entry.
					if((rx_packet->tot_hop + 1) < e->entry.tot_hop)
					{
						e->entry.next_hop = tx_node_id;
						e->entry.tot_hop = rx_packet->tot_hop + 1;
						e->entry.metric = rx_packet->metric;
					}
					else if((rx_packet->tot_hop + 1) == e->entry.tot_hop)
					{
						// compare the metric.
						if((rx_packet->metric) > (e->entry.metric))
						{
							// update the entry in the advertisement routing table.
							e->entry.next_hop = tx_node_id;
							e->entry.metric = rx_packet->metric;
						}
					}
					else
					{
						// do nothing
					}
				}
				else
				{
					printf("\rThe code should not come here!!!!!!!!\n");
					return;
				}
				printf("\rprocess_advertisement routing entry is updated.\n");
			}
		}
	}
}

/**
 * @brief 				This function is called by broadcast_recv.
 * 						If the received entry is not in the list of routing table, then we call it to handle DSDV routing.
 *
 * @param rx_packet		Received routing entry.
 * @param tx_node_id	Transmitter node id.
 */
static void handle_received_entry(rt_entry* rx_packet, uint8_t tx_node_id)
{
	struct permanent_rt_entry *e;
	for(e = list_head(permanent_rt_list); e != NULL; e = e->next)
	{
		// if received entry is not in the list
		// find the routing entry location in the permanent rt for received dest. id.
		if(rx_packet->dest == e->entry.dest)
		{
			// Since we receive a routing entry update message for a specified destination address,
			// We update the timeout.
			//ctimer_set(&e->ctimer_stale_entry, CLOCK_SECOND*PERIODIC_UPDATE_INTERVAL*HOLD_TIMES, remove_rt_entry, e);
			//ctimer_reset(&e->ctimer_stale_entry);
			// Verify the sequence number of the received entry.
			// If it is odd that represent a broken link.
			// For now, I skip the the actions for the odd case.
			if((rx_packet->seq_no % 2) == 1)
			{
				// I think, we need this part to when a node fails.
				// In the stale case, where a mote remove the routing entry, when it
				// did not get any update message from the failed mote for a period of time
				// e.g. PERIODIC_UPDATE_INTERVAL*HOLD_TIMES, it will immediately inform the other motes
				// with odd sequence number. When a mote receives an update with odd seq. num, it will automatically
				// delete the entry for the destination address if the node from which this update was received is the next hop
				// neighbor in the permanent table. I think, this feature will help the network to converge faster in case of failure.
				if((e->entry.next_hop == tx_node_id))
				{

					// Updates its routing table and advertise the broken link.
					// Since we broadcast stale entry also if we want the broken node to rejoin the network.
					// We should not advertise every time when we receive broken link message.
					// If the timer is expired then we allready assign that this message is broadcasted since ctimer_stop() function
					// expires the timer when we call.
					if(ctimer_expired(&e->ctimer_stale_entry)==0)
					{
						e->entry.seq_no = rx_packet->seq_no;
						printf("\rSTALE entry will be removed \n");
						//print_rt_entry(&e->entry);
						// Set the destination address as broken.
						broken_motes[e->entry.dest] = 1;
						// reset the weighted rssi value for this mote.
						rssi_weight[e->entry.dest] = 0;
						// Advertise the broken link IN 0.1sec
						ctimer_set(&e->ctimer_advertise_entry, ADVERTISE_BROKEN_LINK, advertise_rt_entry, e);
						// Since the entry is tagged as broken, we can stop the stale entry timer.
						ctimer_stop(&e->ctimer_stale_entry);
					}
				}

				//
			}
			// Sequence no of the received entry is even.
			else
			{
				// Now we should compare the sequence number of the received entry with
				// the sequence number of our permanent rt entry.
				if((rx_packet->seq_no) > (e->entry.seq_no))
				{
					// Set this mote as valid mote in the network.
					broken_motes[rx_packet->dest] = 0;
					if((rx_packet->tot_hop + 1) != e->entry.tot_hop)
					{
						// update for unstable route, call process
						process_advertisement(rx_packet, tx_node_id);
					}
					else
					{
						// this is an stable update no need to advertise.
						if(e->entry.next_hop == tx_node_id)
						{
							// This is an update for a stable route.
							ctimer_set(&e->ctimer_stale_entry, CLOCK_SECOND*PERIODIC_UPDATE_INTERVAL*HOLD_TIMES, remove_rt_entry, e);
							// Update the permanent routing table entry.
							//e->entry.next_hop = tx_node_id;
							e->entry.seq_no = rx_packet->seq_no;
							e->entry.metric = rx_packet->metric;
						}
						else
						{
							// Compare the metrics, if the received metric is better than local one, then trigger advertisement.
							if(rx_packet->metric > (e->entry.metric))
							{
								process_advertisement(rx_packet, tx_node_id);
							}
						}
					}

					// Check the hop count for this destination.
					// If the hop count is not equal to the hop count of the permanent rt entry.
					// Then, this should be an update about new route which should be added into
					// the advertising rt and broadcasted.

					// I increase +1 since we need to send the packet to transmitter.
					// If the following equation in the if statement is equal,
					// Then, mote does not advertise this entry since this is an update for the stable route.
//					if((rx_packet->tot_hop + 1) != e->entry.tot_hop)
//					{
//						e->entry.tot_hop = rx_packet->tot_hop + 1;
//						// this packet should be broadcasted to the other motes now.
//						// Since a received routing entry has less hop count.
//						// process_advertisement(e);
//						ctimer_set(&e->ctimer_advertise_entry, CLOCK_SECOND*ADVERTISED_UPDATE_INTERVAL, advertise_rt_entry, e);
//					}
				}
				else if ((rx_packet->seq_no) == (e->entry.seq_no))
				{
					// If the received routing entry for that destination is less than
					// the routing entry in the permanent table for this destination
					// update the permanent routing table for this destination.
					if((rx_packet->tot_hop + 1) < e->entry.tot_hop)
					{
						process_advertisement(rx_packet, tx_node_id);
						//e->entry.next_hop = tx_node_id;
						//e->entry.tot_hop = rx_packet->tot_hop + 1;
						// Again find a way to implement the metric parameter.
						// Also advertise this new routing entry to the other motes.
						// process_advertisement(e);
						//ctimer_set(&e->ctimer_advertise_entry, CLOCK_SECOND*ADVERTISED_UPDATE_INTERVAL, advertise_rt_entry, e);
					}
					// For the case where the hop count of the received entry is higher or
					// less than the local hop count, we can discard the entry without any update.
					else
					{
						return;
					}
				}
				// Seq number is smaller than the local one.
				else
				{
					return;
				}
			}
		}
	}
}


/**
 * @brief 			This function creates an entry to add to MT_rt_list.
 * 					Then rebroadcast this message.
 *
 * @param src_add 	Source if of MU.
 *
 */
static void handle_bc_t1(uint8_t src_add)
{

	if(is_rebroadcasted(src_add) == 0)
	{
		struct MU_routing_entry *e;
		e = memb_alloc(&MU_rt_mem);
		if(e != NULL)
		{
			e->entry.type = MU_COLLISION_DETECTION;
			e->entry.dest = src_add;
			e->entry.next_hop = src_add;		// Since this message is received by neighbor RSUs, then we should have direct communication to MU.
			e->entry.tot_hop= 1;
			// We are not actually using the metric and seq_no as a part of this functionality
			// Therefore we define them as 0.
			e->entry.metric = 0;
			e->entry.seq_no = 0;
			list_add(MU_rt_list, e);
			// Set an expiration time for the routing entry to MU.
			ctimer_set(&e->ctimer_stale_entry, CLOCK_SECOND*5, remove_MU_from_list, e);

			// rebroadcast this message
			rebroadcast_collision(e->entry);
		}
	}
}
/***************************************************************/


/****************************UNICAST****************************/
/**
 * @brief			    This function is used to send the sensor value to the gateway.
 *
 * @param value	    Sensor value to sent the gateway
 * @param src_add	    Source address of the unicast packet.
 *
 */
static void send_sensor_value(uint8_t type, uint8_t src_add, int value)
{
	struct permanent_rt_entry *e;

	for(e = list_head(permanent_rt_list); e != NULL; e = e->next)
	{
		// If we find and entry for the destination.
		// If the next hop is not broken.
		//TODO the gateway himself shall not resend this message!!!
		if(e->entry.dest == GATEWAY && (broken_motes[e->entry.next_hop] != 1) )
		{
			leds_on(LEDS_RED);
			rsu_sensor_p tx_packet;
			linkaddr_t 	next = convert_to_link(e->entry.next_hop);
			// Since this is a sensor packet set the type to 0.


			tx_packet.type = (uint8_t)type;
			tx_packet.src_add = src_add;
			tx_packet.data = value;

			//change tx power to max power before unicast send
			//NETSTACK_CONF_RADIO.set_value(RADIO_PARAM_TXPOWER, TX_POWER_MAX);
			packetbuf_copyfrom(&tx_packet, sizeof(rsu_sensor_p));
			unicast_send(&unicast, &next);
			printf("\rUNICAST Sensor value is sent to the Gateway via NODE %d.\n", e->entry.next_hop );
			//radio_value_t value;
			//NETSTACK_CONF_RADIO.get_value(RADIO_PARAM_TXPOWER, &value);
			//printf("\rPower Value changed to %d\n", value);
			leds_off(LEDS_RED);

			//change tx power to the nominal value
			//NETSTACK_CONF_RADIO.set_value(RADIO_PARAM_TXPOWER, TX_POWER);
			//radio_value_t value2;
			//NETSTACK_CONF_RADIO.get_value(RADIO_PARAM_TXPOWER, &value2);
			//printf("\rPower Value changed to %d\n", value2);

		}
		if(e->entry.dest == GATEWAY && (broken_motes[e->entry.next_hop] == 1))
		{
			printf("\rUNICAST, next hop is broken %d \n", e->entry.next_hop);
		}
	}
}

/**
 * @brief            This function is used to send the sensor a message to emergency channel.
 *
 * @param type       Type of the created packet.
 * @param src_add    Source address of the unicast packet.
 * @param dest_add   Destination address of the unicast packet.
 * @param value      Value that should be sent.
 */
static void send_MU_message(uint8_t type, uint8_t src_add, uint8_t dest_add, int value)
{
	struct permanent_rt_entry *e;

	for(e = list_head(permanent_rt_list); e != NULL; e = e->next)
	{
		// If we find and entry for the destination.
		// If the next hop is not broken.
		//TODO the gateway himself shall not resend this message!!!
		if(e->entry.dest == dest_add && (broken_motes[e->entry.next_hop] != 1) )
		{
			leds_on(LEDS_RED);
			GWmessage tx_packet;
			linkaddr_t 	next = convert_to_link(e->entry.next_hop);
			// Since this is a sensor packet set the type to 0.


			tx_packet.type = (uint8_t)type;
			tx_packet.src_add = src_add;
			tx_packet.dest_add = dest_add;
			tx_packet.data = value;

			//change tx power to max power before unicast send
			NETSTACK_CONF_RADIO.set_value(RADIO_PARAM_TXPOWER, TX_POWER_MAX);
			packetbuf_copyfrom(&tx_packet, sizeof(tx_packet));
			unicast_send(&unicast, &next);
			printf("\rUNICAST Sensor value is sent to the Gateway via NODE %d.\n", e->entry.next_hop );
			//radio_value_t value;
			//NETSTACK_CONF_RADIO.get_value(RADIO_PARAM_TXPOWER, &value);
			//printf("\rPower Value changed to %d\n", value);
			leds_off(LEDS_RED);

			//change tx power to the nominal value
			//NETSTACK_CONF_RADIO.set_value(RADIO_PARAM_TXPOWER, TX_POWER);
			//radio_value_t value2;
			//NETSTACK_CONF_RADIO.get_value(RADIO_PARAM_TXPOWER, &value2);
			//printf("\rPower Value changed to %d\n", value2);

		}
		if(broken_motes[e->entry.next_hop] == 1)
		{
			printf("\rUNICAST, next hop is broken %d \n", e->entry.next_hop);
		}
	}
}

/****************************UNICAST****************************/
/**
 * @brief			    This function is used to send multiple data to the gateway.
 *
 * @param temp		    Temperature sensor value to sent the gateway
 * @param batt		    Battery voltage sensor value to sent the gateway
 * @param count		    Count value to sent the gateway
 * @param src_add	    Source address of the unicast packet.
 *
 */
static void send_datalogger_packet(uint8_t type, uint8_t src_add, uint16_t temp, uint16_t batt, uint16_t count)
{
	struct permanent_rt_entry *e;

	for(e = list_head(permanent_rt_list); e != NULL; e = e->next)
	{
		// If we find and entry for the destination.
		// If the next hop is not broken.
		//TODO the gateway himself shall not resend this message!!!
		if(e->entry.dest == GATEWAY && (broken_motes[e->entry.next_hop] != 1) )
		{
			leds_on(LEDS_RED);
			data_logger_p tx_packet;
			linkaddr_t 	next = convert_to_link(e->entry.next_hop);
			// Since this is a sensor packet set the type to 0.

			tx_packet.type = (uint8_t)type;
			tx_packet.src_add = src_add;
			tx_packet.temp = temp;
			tx_packet.batt = batt;
			tx_packet.count = count;

			//change tx power to max power before unicast send
			//NETSTACK_CONF_RADIO.set_value(RADIO_PARAM_TXPOWER, TX_POWER_MAX);
			packetbuf_copyfrom(&tx_packet, sizeof(data_logger_p));
			unicast_send(&unicast, &next);
			printf("\rUNICAST DATA logger is sent to the Gateway via NODE %d.\n", e->entry.next_hop );
			//radio_value_t value;
			//NETSTACK_CONF_RADIO.get_value(RADIO_PARAM_TXPOWER, &value);
			//printf("\rPower Value changed to %d\n", value);
			leds_off(LEDS_RED);

			//change tx power to the nominal value
			//NETSTACK_CONF_RADIO.set_value(RADIO_PARAM_TXPOWER, TX_POWER);
			//radio_value_t value2;
			//NETSTACK_CONF_RADIO.get_value(RADIO_PARAM_TXPOWER, &value2);
			//printf("\rPower Value changed to %d\n", value2);

		}
		if(e->entry.dest == GATEWAY && (broken_motes[e->entry.next_hop] == 1))
		{
			printf("\rUNICAST, next hop is broken %d \n", e->entry.next_hop);
		}
	}
}


/***************************************************************/


/************************CHANNEL_CALLBACKS**********************/

/**
 * @brief 		This function is used for the crash message produced by GW
 *				to converge in the network
 */
int stateGC=0;
static void callback_stateGC(){
	stateGC=0;
	ctimer_reset(&stateGC_timer);
}

//to check if collided MU exists or not
static uint8_t collided_MU=0;

/**
 * @brief 		Broadcast receive function.
 *
 */
static void broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{

	leds_on(LEDS_GREEN);

	uint8_t len = strlen((char *)packetbuf_dataptr());
	int16_t rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);
	uint8_t tx_node_id = from->u8[1] & 0xFF;
	if(rssi_weight[tx_node_id] == 0)
	{
		printf("\rFIRST TIME RECEIVED from: %d, RSSI: %d \n",tx_node_id ,rssi);
		rssi_weight[tx_node_id] = rssi;
	}

	int16_t w_rssi = (WEIGHT_a*rssi_weight[tx_node_id] + WEIGHT_b*rssi)/10;
	rssi_weight[tx_node_id] = w_rssi;
	// Discard the message with less power than -80dB.
	printf("\rGot RX packet (broadcast) from: %d, RSSI: %d W_RSSI %d\n",tx_node_id , rssi, w_rssi);
	if(w_rssi < (int16_t)BROADCAST_RSSI_TH )
	{
		printf("\rDISCARD PACKET from: %d, RSSI: %d W_RSSI %d\n",tx_node_id ,rssi, w_rssi);
		leds_off(LEDS_GREEN);
		return;
	}

	// Experiment: to see the difference between two approach.
	// I could not read the type(try different value than 0) when I copy the content
	// to an external memory using packetbuf_copyto.
	// However, I reached the type value when I get the pointer from packetbuf(not copying some other location and reading).
//	void *rx_ext;		// did not work.
//	packetbuf_copyto(rx_ext);
//	printf("\r(broadcast) first byte EXTERNAL %d \n",(uint8_t *)rx_ext);

	uint8_t *rx_buf = packetbuf_dataptr();		// worked


	if(*rx_buf == ROUTING_ENTRY )
	{
//		if(w_rssi < (int16_t)BROADCAST_RSSI_TH )
//		{
//			printf("\rGot RX packet (broadcast) from: %d, len: %d, RSSI: %d DISCARDED \n",tx_node_id ,len,rssi);
//			leds_off(LEDS_GREEN);
//			return;
//		}

		//printf("\rBROADCAST Received message for ROUTING, type %d\n", *rx_buf);

		// Copy the buffer to an external memory to make sure packetbuf is allocated by some other broadcast messages.
		rt_entry *rx_packet =(rt_entry*)rx_buf;


		if(rx_packet->dest == (linkaddr_node_addr.u8[1] & 0xFF))
		{
			// This function can be modifies in case of failure.
			// For example; if a mote restarts, then the sequence number will be restarted,
			// Since the network has already a sequence number higher then the restarted mote,
			// It will discard the messages. And it will take time for a network to converge.
			// instead we can check the seq number for this mote of received entries, if we find a higher number
			// we set our seq number to that number and start to increase from this number.
			struct permanent_rt_entry *e;
			e = list_head(permanent_rt_list);

			// Robust for unexpected REstarts.
			if((rx_packet->seq_no) % 2 == 0)
			{
				if((rx_packet->seq_no) > (e->entry.seq_no))
				{
					e->entry.seq_no = rx_packet->seq_no;
				}
			}
			// If a mote receives a broken link message for itself, Then It needs to say other motes that I come back!
			else
			{
				// Since the received message is odd, we should add 1 to make it worked.
				e->entry.seq_no = rx_packet->seq_no + 1;
			}

			packetbuf_clear();
			leds_off(LEDS_GREEN);
			return;
		}

		// Do we have any entry in the permanent rt for this destination?
		if(check_permanent_rt(rx_packet->dest) == 0)	// This is a new entry, adds this one into the permanent routing table.
		{

			// Add this routing entry to its
			permanent_rt_add_entry(rx_packet, tx_node_id, w_rssi);
			//print_rt_entry(rx_packet);
			// Add this entry to be advertised now.(Actually,I think we should immediately broadcast this packet, since we are still receiving some broadcast packets.
			// This will be probably cause some collisions in the packetbuf. One possible solution would be use external memory for outgoing packets.?)
			// For now, this entry will be added into the advertised_rt_table with some settling time feature.
			// After the expiration of this timer entries inside the advertised_rt_table will be broadcasted.

		}
		// Compare the received routing entry with the entry found in the local permanent routing table.
		else
		{
			rx_packet->metric = rx_packet->metric + w_rssi;
			handle_received_entry(rx_packet, tx_node_id);
		}
	}
	else if (*rx_buf == ROUTING_TABLE)
	{
		routing_table *rx_packet = (routing_table*)rx_buf;
		uint8_t no_entries = rx_packet->no_entries;
		printf("\rROUTING TABLE RECEVIED TYPE %d from: %d, TOTAL entry %d, RSSI: %d  \n",*rx_buf, tx_node_id ,no_entries,rssi);
		uint8_t i;
		for(i=0; i<no_entries; i++)
		{
			rt_entry *rx_en = &rx_packet->table[i];
			//print_rt_entry(rx_en);
			if(rx_en->dest == (linkaddr_node_addr.u8[1] & 0xFF))
			{
				// This function can be modifies in case of failure.
				// For example; if a mote restarts, then the sequence number will be restarted,
				// Since the network has already a sequence number higher then the restarted mote,
				// It will discard the messages. And it will take time for a network to converge.
				// instead we can check the seq number for this mote of received entries, if we find a higher number
				// we set our seq number to that number and start to increase from this number.
				struct permanent_rt_entry *e;
				e = list_head(permanent_rt_list);

				// Robust for unexpected REstarts.
				if((rx_en->seq_no) % 2 == 0)
				{
					if((rx_en->seq_no) > (e->entry.seq_no))
					{
						e->entry.seq_no = rx_en->seq_no;
					}
				}
				// If a mote receives a broken link message for itself, Then It needs to say other motes that I come back!
				else
				{
					// Since the received message is odd, we should add 1 to make it worked.
					e->entry.seq_no = rx_en->seq_no + 1;
				}

				packetbuf_clear();
				leds_off(LEDS_GREEN);
				//return;
			}

			// Do we have any entry in the permanent rt for this destination?
			if(check_permanent_rt(rx_en->dest) == 0)	// This is a new entry, adds this one into the permanent routing table.
			{

				// Add this routing entry to its
				permanent_rt_add_entry(rx_en, tx_node_id, w_rssi);
				//print_rt_entry(rx_en);
				// Add this entry to be advertised now.(Actually,I think we should immediately broadcast this packet, since we are still receiving some broadcast packets.
				// This will be probably cause some collisions in the packetbuf. One possible solution would be use external memory for outgoing packets.?)
				// For now, this entry will be added into the advertised_rt_table with some settling time feature.
				// After the expiration of this timer entries inside the advertised_rt_table will be broadcasted.

			}
			// Compare the received routing entry with the entry found in the local permanent routing table.
			else
			{
				rx_en->metric = rx_en->metric + w_rssi;
				handle_received_entry(rx_en, tx_node_id);
			}
		}
	}
	// Inside of this will be filled later.
	// Data packet.
	else if(*rx_buf == MU_COLLISION_DETECTION)
	{
		// Create a packet for type 1.
		// data packet.
		printf("\rReceived broadcast message from MU SENSOR VALUE, type %d\n", *rx_buf);
		// Copy the buffer to an external memory to make sure packetbuf is allocated by some other broadcast messages.
		rsu_sensor_p *MUmessage =(rsu_sensor_p*)rx_buf;
		leds_off(LEDS_GREEN);
		printf("\rMU ADRESS: %d MU DATA: %d\n",MUmessage->src_add,MUmessage->data);
		// Send unicast multi-Broadcast receive functionhop traffic to GW.
		send_sensor_value(COLLISION_MU, MUmessage->src_add, MUmessage->data);
		// Now we should check the MU_rt_list to make sure we had only broadcast at once.
		// This function will be used to handle received message for controlled rebroadcast mechanism for received type 1 packets..
		handle_bc_t1(MUmessage->src_add);
			// TODO: check_MU_rt_list(MUmessage->src_add)(NOTE: this is for received type 2 messages.)
				// TODO: if not in the list, create and entry and add this to list and rebroadcast with the type 2.
		// Rebroadcast the received message to other RSU to inform other MUs in the network.
		collided_MU = MUmessage->src_add;

	}
	else if(*rx_buf == REBROADCAST_COLLISION)
	{
		rt_entry *rx_packet =(rt_entry*)rx_buf;
		// Check first the MU routing table, if we have an entry for this MU with type REBROADCAST_COLLISION
		// Then discard the packet since we have already broadcasted this information. We only have a type REBROADCAST_COLLISION
		// entry when we broadcast the message.
		handle_bc_t2(rx_packet, tx_node_id);
		collided_MU = rx_packet->dest;

	}
	else if(*rx_buf == GW_RETURN){
		//If the received message is "C" from gateway
		rsu_sensor_p *GatewayCrash =(rsu_sensor_p*)rx_buf;
		leds_off(LEDS_GREEN);
		printf("\rGateway Crash message received! %d\n", GatewayCrash->type);
		if((stateGC == 0) && (linkaddr_node_addr.u8[1] & 0xFF)!= GATEWAY ){
			//rebroadcast it if you are not gateway and received first time
			packetbuf_copyfrom(GatewayCrash, sizeof(rsu_sensor_p));
			broadcast_send(&broadcast);
			ctimer_set(&stateGC_timer,CLOCK_SECOND * 45, callback_stateGC, NULL);
			printf("\rGateway Crash message directed!\n");
			stateGC = 1;
		}
	}
	packetbuf_clear();
	leds_off(LEDS_GREEN);

}

// Defines the behavior of a connection upon receiving data.
/**
 * @brief 		Unicast receive function.
 *
 */
static void
unicast_recv(struct unicast_conn *c, const linkaddr_t *from)
{
	//int16_t rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);
	uint8_t tx_node_id = from->u8[1] & 0xFF;

	uint8_t *rx_buf = packetbuf_dataptr();		// worked

	//printf("\rUNICAST packet from: %d, len: %d, RSSI: %d type %d \n",tx_node_id ,len,rssi, *rx_buf);

	// This is a unicast packet that is generated by the RSU.
	if(*rx_buf == 0)
	{
		// If I am not the gateway then I should forwarded to the gateway.
		// For now I will skip this feature just to test.
		// Maybe, we can add a feature to freeze the periodic update timers to avoid collision
		// When we have an unicast message to send.

		rsu_sensor_p *rx_packet =(rsu_sensor_p*)rx_buf;
		//If the receiver is Gateway, print the message
		//Otherwise, direct the message
		//TODO:Code should be updated to send the data to GUI
		if((linkaddr_node_addr.u8[1] & 0xFF) == GATEWAY)
		{
			//GUI friendly version:
			printf("\rType_%d source_adr %d prev_hop %d counter %d OBJECT DETECTED!!!!\n", rx_packet->type, rx_packet->src_add, tx_node_id, rx_packet->data);
			//printf("\rUNICAST received SOURCE %d via node %d Unicast value %d\n", rx_packet->src_add, tx_node_id, rx_packet->data);
		}
		else{
			send_sensor_value(rx_packet->type, rx_packet->src_add, rx_packet->data);
		}
	}
	else if(*rx_buf == OBJECT_DETECTION)
	{
		rsu_sensor_p *rx_packet =(rsu_sensor_p*)rx_buf;
		if((linkaddr_node_addr.u8[1] & 0xFF) == GATEWAY)
		{
			//GUI friendly version:
			printf("\rType_%d source_adr %d prev_hop %d counter %d OBJECT DETECTED!!!!\n", rx_packet->type, rx_packet->src_add, tx_node_id, rx_packet->data);
			//printf("\rUNICAST received SOURCE %d via node %d Unicast value %d\n", rx_packet->src_add, tx_node_id, rx_packet->data);
		}
		else
		{
			send_sensor_value(rx_packet->type, rx_packet->src_add, rx_packet->data);
		}
	}
	else if(*rx_buf == COLLISION_MU)
	{
		rsu_sensor_p *rx_packet =(rsu_sensor_p*)rx_buf;
		if((linkaddr_node_addr.u8[1] & 0xFF) == GATEWAY)
		{
			//GUI friendly version:
			printf("\rType_%d source_adr %d prev_hop %d Collision DETECTED!!!!\n", rx_packet->type, rx_packet->src_add, tx_node_id);
			//printf("\rUNICAST received SOURCE %d via node %d Unicast value %d\n", rx_packet->src_add, tx_node_id, rx_packet->data);
			collided_MU =rx_packet->src_add;
		}
		else
		{
			send_sensor_value(rx_packet->type, rx_packet->src_add, rx_packet->data);
		}
	}
	else if(*rx_buf == DATA_LOGGER)
		{
			data_logger_p *rx_packet =(data_logger_p*)rx_buf;
			if((linkaddr_node_addr.u8[1] & 0xFF) == GATEWAY)
			{
				//GUI friendly version:
				printf("\rType_%d src_adr %d p_hop %d temp %d batt %d count %d Datalogger Report!\n", rx_packet->type, rx_packet->src_add, tx_node_id, rx_packet->temp, rx_packet->batt, rx_packet->count);
				//printf("\rUNICAST received SOURCE %d via node %d Unicast value %d\n", rx_packet->src_add, tx_node_id, rx_packet->data);
			}
			else
			{
				send_datalogger_packet(rx_packet->type, rx_packet->src_add, rx_packet->temp, rx_packet->batt, rx_packet->count);
			}
		}
	else if(*rx_buf == GW_MESSAGE){
		GWmessage *gateway_packet=(GWmessage*)rx_buf;
		if(collided_MU !=0)
		{
			//Redirect received message to collided MU
			//printf("\rMessage for MU received: %d\n", gateway_packet->data);
			printf("\rType_10 MU_id %d MU_data %d\n",collided_MU,gateway_packet->data);
			send_MU_message(gateway_packet->type, gateway_packet->src_add, gateway_packet->dest_add, gateway_packet->data);
		}
	}


}

static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
// Defines the functions used as callbacks for a unicast connection.
static const struct unicast_callbacks unicast_call = {unicast_recv};
/***************************************************************/


/*** CONNECTION DEFINITION END ***/

//--------------------- PROCESS CONTROL BLOCK -------------------------------/
PROCESS(first_process, "Initialize Routing and opening broadcast channel");
PROCESS(unicast_traffic, "Send sensor values to GW with the help of unicast channel");
PROCESS(emergency_channel, "Send message from gateway");
PROCESS(data_logger, "Send data periodically to the gateway using UNICAST messages");

AUTOSTART_PROCESSES(&first_process, &unicast_traffic, &emergency_channel, &data_logger);

//------------------------ INITIALIZE ROUTING PROCESS THREAD --------------------------------------------/
PROCESS_THREAD(first_process, ev, data)
{
	PROCESS_EXITHANDLER(broadcast_close(&broadcast);)
	PROCESS_BEGIN();

	NETSTACK_CONF_RADIO.set_value(RADIO_PARAM_CHANNEL, CHANNEL);

	NETSTACK_CONF_RADIO.set_value(RADIO_PARAM_TXPOWER, TX_POWER); // 5,3,1,-1 ... int value form table

	initialize_routing_tables();


	// Open broadcast connection.
	broadcast_open(&broadcast, 129, &broadcast_call);

	while(1)
	{
		PROCESS_YIELD();
	}

	PROCESS_END();
}
//-------------------------------------------------------------------------------------------------------/

//------------------------ SENDING SENSOR VALUES IN RSU PROCESS THREAD ----------------------------------/
uint16_t adc3_value;	//var to store the value from adc
uint16_t adc1_value;	//var to store the value from adc
uint8_t obj_det;	//bool if the object is detected

uint16_t counter;	//holds how many detection events are triggered
uint16_t temperature;
uint16_t battery;

PROCESS_THREAD(unicast_traffic, ev, data)
{
	PROCESS_EXITHANDLER(unicast_close(&unicast);)
	PROCESS_BEGIN();

	// Open unicast connection.
	// Channel should be different for unicast.
	unicast_open(&unicast, 100, &unicast_call);
	adc_zoul.configure(SENSORS_HW_INIT, ZOUL_SENSORS_ADC1 | ZOUL_SENSORS_ADC3);
	obj_det = 0; //set state to default state
	etimer_set(&sensor_timer, CLOCK_SECOND/SENSOR_SAMPLING_DELAY_DIV); // 1/20 second delay
	//while the mode is not the gateway

	while ((linkaddr_node_addr.u8[1] & 0xFF) != GATEWAY) {
			PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&sensor_timer));
			adc3_value = adc_zoul.value(ZOUL_SENSORS_ADC3)>>4;
			//triggering based on raw values, more robust then complex moving average triggering
			if(adc3_value > 800 && obj_det == 0){
				leds_on(LEDS_BLUE);
				//printf("Object Detected\n");
				counter++;
				obj_det = 1;
			}
			else if(adc3_value < 700 && obj_det == 1){
				//printf("Object Away\n");
				send_sensor_value(OBJECT_DETECTION,(linkaddr_node_addr.u8[1] & 0xFF), counter);
				etimer_set(&dedection_delay_timer, CLOCK_SECOND/SENSOR_DELAY_DIV);//timmer to set a maximum freq for the detection event
				obj_det = 0;
				//printf("Event Armed\n");
				PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&dedection_delay_timer));
				leds_off(LEDS_BLUE);
				//printf("Event Disarmed\n");
			}
		etimer_reset(&sensor_timer);
	}

	PROCESS_END();
}
//-------------------------------------------------------------------------------------------------------/

//----------------------------------- CRASH MESSAGE PROCESS THREAD --------------------------------------/
PROCESS_THREAD(emergency_channel, ev, data)
{
	PROCESS_BEGIN();

	rsu_sensor_p crash;
	GWmessage gateway_packet;
	while(1)
	{
		 PROCESS_WAIT_EVENT();
		 if(ev == serial_line_event_message){
			 if(strncmp(data, "C", 1)==0){
				  //Serial message "C"
				  crash.type= GW_RETURN;
				  crash.src_add=GATEWAY;
				  crash.data=0;
				  printf("\rCRASH MESSAGE CREATED\n");
				  packetbuf_copyfrom(&crash, sizeof(crash));
				  broadcast_send(&broadcast);
			 }
			 else if(strncmp(data, "MU", 2)==0){
				 //prepare a packet to collided MU
				 printf("\rCollided MU: %d\n",collided_MU);
				 if(collided_MU !=0)
				 {
					 gateway_packet.type = GW_MESSAGE;
					 gateway_packet.src_add = GATEWAY;
					 gateway_packet.dest_add = collided_MU;
					 gateway_packet.data = atoi(&data[2]);
					 printf("\rMessage for MU created: %d\n", gateway_packet.data);
					 //send the packet to collided_MU
					 send_MU_message(gateway_packet.type, gateway_packet.src_add, gateway_packet.dest_add,gateway_packet.data);
				 }
			 }
		 }
	}

	PROCESS_END();
}
//-------------------------------------------------------------------------------------------------------/

//----------------------------------- DATA LOGGER PROCESS THREAD ----------------------------------------/

PROCESS_THREAD(data_logger, ev, data){
	PROCESS_BEGIN();


	etimer_set(&data_logger_timer, CLOCK_SECOND * DATA_LOGGER_DELAY); //  second delay for sending
	temperature = cc2538_temp_sensor.value(CC2538_SENSORS_VALUE_TYPE_CONVERTED) / 10;
	battery = vdd3_sensor.value(CC2538_SENSORS_VALUE_TYPE_CONVERTED) / 10;
	//while the mode is not the gateway
	while((linkaddr_node_addr.u8[1] & 0xFF) != GATEWAY){
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&data_logger_timer));

		send_datalogger_packet(DATA_LOGGER, (linkaddr_node_addr.u8[1] & 0xFF), temperature, battery, counter);
		//printf("\r\nMy Battery Voltage [VDD] = %d mV\n", vdd3_sensor.value(CC2538_SENSORS_VALUE_TYPE_CONVERTED));
		//printf("\r\nMy Temperature = %d mC\n", cc2538_temp_sensor.value(CC2538_SENSORS_VALUE_TYPE_CONVERTED));
		etimer_reset(&data_logger_timer);
	}

	PROCESS_END();
}
/*-----------------------------------------------------------------------------------------------------*/
