/*
 * MU.c
 *
 *  Created on: Jan 21, 2019
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
#include "dev/zoul-sensors.h"  	// Sensor functions
#include "dev/adc-zoul.h"      	// ADC
#include "dev/sys-ctrl.h"
#include "net/netstack.h"		// Wireless-stack definitions
// Standard C includes:
#include <stdio.h>      		// For prints.
#include <stdint.h>

#include "project-conf.h"
#include "packet_structure.h"

#define RECEIVED_COLLISION 2

static int16_t rssi_weight[MAX_NODES+1];
/*******************CHANNELS***********************/
// Creates an instance of a unicast connection.
static struct unicast_conn unicast;
/**
 * Connection information
 */
static struct broadcast_conn broadcast;
/***************************************************/

/********************TIMERS*************************/
// Callback timer for periodic routing table updates.
static struct ctimer periodic_rt_timer;
static struct ctimer collision_timer;
/***************************************************/

/*******************STRUCTURES**********************/

// This structure is used to send and receive collision broadcast messages.
typedef struct{
	uint8_t type;			// Indicates the type of unicast packet.
	uint8_t src_add;		// Source address of the generated unicast message.
	int data;
}sensor_p;

/******************************************************/

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
 * @brief		This function is called when the collision timer expired to turn off the LED.
 *
 */
static void OFF_LED(void *ptr ){
	leds_off(LEDS_BLUE);
	//ctimer_reset(&collision_timer);
	ctimer_stop(&collision_timer);
}

/*****************************************************/


/**********************ROUTING************************/

static uint8_t enable_routing = 0;			// If an accident happens this value turns into 1.

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

	 // We can send each entry of the routing table individually.
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
	 enable_routing = 1;

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
		printf("\r New routing entry is added\n");
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

/***************************************************************/

/****************************UNICAST****************************/

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
			//NETSTACK_CONF_RADIO.set_value(RADIO_PARAM_TXPOWER, TX_POWER_MAX);
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

static void handle_received_message(GWmessage *rx_packet)
{
	if(rx_packet->data == DISABLE_CONNECTION)
	{
		// Disable routing enable, so dont received routing update messages.
		enable_routing = 0;
		// Stop the periodic routing table timer.
		ctimer_stop(&periodic_rt_timer);
		// Clean the permanent routing list.
		//list_init(permanent_rt_list);
		//memb_init(&permanent_rt_mem);
		while(1){
			//to restart
		}
	}
	else if(rx_packet->data != DISABLE_CONNECTION)
	{
		// received a msg packet, increase the data by 5 and send unicast to gateway
		send_MU_message(GW_MESSAGE,(linkaddr_node_addr.u8[1] & 0xFF),GATEWAY,rx_packet->data + 5);
	}
}

/***********************************************************************/


/**************************CHANNEL_CALLBACKS****************************/

static void broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from) {

	leds_on(LEDS_GREEN);

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


	uint8_t *rx_buf = packetbuf_dataptr();
	printf("\rGot RX packet (broadcast) TYPE %d from: %d, RSSI: %d ROUTING enabled %d \n",*rx_buf, tx_node_id,rssi, enable_routing);
	// Only receives broadcast messages when the routing for MU is enabled.
	if((*rx_buf == ROUTING_ENTRY)&& (enable_routing == 1))
	{
		printf("\rBROADCAST Received message for ROUTING, type %d\n", *rx_buf);

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

	else if (*rx_buf == ROUTING_TABLE && (enable_routing == 1))
	{
		routing_table *rx_packet = (routing_table*)rx_buf;
		uint8_t no_entries = rx_packet->no_entries;
		printf("\rROUTING TABLE RECEVIED TYPE %d from: %d, TOTAL entry %d, RSSI: %d  \n",*rx_buf, tx_node_id ,no_entries,rssi);

		uint8_t i;
		for(i=0; i<no_entries; i++)
		{
			rt_entry *rx_en = &rx_packet->table[i];

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
				print_rt_entry(rx_en);
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
	else if(*rx_buf == RECEIVED_COLLISION )
	{
		sensor_p *rx_packet =(sensor_p*)rx_buf;
		printf("\r Received CRASH message from MU %d!!!\n", rx_packet->src_add);
		// Only receives message if the timer is already expired to avoid activating the led many times.
		if(ctimer_expired(&collision_timer)!=0)
		{
			leds_on(LEDS_BLUE);
			ctimer_set(&collision_timer,CLOCK_SECOND * 5, OFF_LED, NULL);
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
	int16_t rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);
	uint8_t tx_node_id = from->u8[1] & 0xFF;


	uint8_t *rx_buf = packetbuf_dataptr();

	if(*rx_buf == GW_MESSAGE)
	{
		GWmessage *rx_packet =(GWmessage*)rx_buf;
		printf("\rGateway message received from %d RSSI %d Value %d !!!\n", tx_node_id, rssi, rx_packet->data);
		handle_received_message(rx_packet);
	}
}

/**
 * Assign callback functions to the connection
 */
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};

/**
 * Assign callback functions to the connection
 */
// Defines the functions used as callbacks for a unicast connection.
static const struct unicast_callbacks unicast_call = {unicast_recv};

/*****************************************************************************/




/*** CONNECTION DEFINITION END ***/

//--------------------- PROCESS CONTROL BLOCK -------------------------------/
PROCESS (MU_process, "Mobile Unit Broadcast");
AUTOSTART_PROCESSES (&MU_process);


uint16_t adc3_value;	//var to store the value from adc

int state=0;			//1 for crash


//------------------------ PROCESS' THREAD ----------------------------------/
PROCESS_THREAD(MU_process, ev, data)
{
	PROCESS_EXITHANDLER(broadcast_close(&broadcast);)
	PROCESS_BEGIN();

	NETSTACK_CONF_RADIO.set_value(RADIO_PARAM_CHANNEL, CHANNEL);

	NETSTACK_CONF_RADIO.set_value(RADIO_PARAM_TXPOWER, TX_POWER); // 5,3,1,-1 ... int value form table


	broadcast_open(&broadcast, 129, &broadcast_call);
	adc_zoul.configure(SENSORS_HW_INIT, ZOUL_SENSORS_ADC1 | ZOUL_SENSORS_ADC3);
	static struct etimer sensor_timer;
	etimer_set(&sensor_timer, CLOCK_SECOND/10); // 1/20 second delay

	// Open unicast connection.
	// Channel should be different for unicast.
	unicast_open(&unicast, 100, &unicast_call);

	while(1)
	{
		//PROCESS_YIELD();
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&sensor_timer));
		adc3_value = (adc_zoul.value(ZOUL_SENSORS_ADC3)>>4);
	    //printf("\r ADCvalue: %d \n",adc3_value);
	    if(adc3_value > 100 && state==0){
	    	state=1;
	    	//Send crash message
	    	printf("\r Crash!!\n");
	    	sensor_p message;
	    	message.type = MU_COLLISION_DETECTION;
	    	message.src_add = (linkaddr_node_addr.u8[1] & 0xFF);
	    	message.data = 1;
	    	leds_on(LEDS_BLUE);
	    	packetbuf_copyfrom(&message, sizeof(message));
	    	broadcast_send(&broadcast);
	    	ctimer_set(&collision_timer,CLOCK_SECOND * 5, OFF_LED, NULL);
	    	// Initiate routing algorithm.
	    	initialize_routing_tables();

	    }else if(adc3_value < 100 && state==1){
	    	state = 0;
	    }
	    etimer_reset(&sensor_timer);
	}

	PROCESS_END();
}
