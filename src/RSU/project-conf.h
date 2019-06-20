#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

// PHY LAYER PARAMETERS
#define CHANNEL 13
// Use the lowest value to create multi-hop network.
#define TX_POWER 7
#define TX_POWER_MAX 7
//#define PACKETBUF_SIZE 256

// MAC LAYER PARAMETERS   default: checkrate are csma, contikimac and 8
#define NETSTACK_CONF_MAC csma_driver    //*nullmac_driver, or csma_driver
#define NETSTACK_CONF_RDC nullrdc_driver	 // *nullrdc_driver, or contikimac_driver
#define NETSTACK_CONF_RDC_CHANNEL_CHECK_RATE 64 // *16 the checkrate in Hz. It should be a power of 2!

// Max number of nodes in the network.
#define MAX_NODES 9
#define PERIODIC_UPDATE_INTERVAL 16
#define ADVERTISED_UPDATE_INTERVAL 1
#define ADVERTISED_UPDATE 4
#define BROADCAST_WAIT	5
#define HOLD_TIMES 3
#define GATEWAY 1		// Node id of the gateway
#define ADVERTISE_BROKEN_LINK 12
#define BROADCAST_RSSI_TH -60

/************WEIGHTED_RSSI******************/
#define WEIGHT_a 7
#define WEIGHT_b 3

/************PACKET TYPES *****************/
/*TYPE MAPPING
 * 0- Broadcast Routing Entries
 * 1- Broadcast Collision Message from MU only
 * 2- Collision Rebroadcasting Message
 * 3- Broadcast to every node
 * 5- Unicast sensor value
 * 6- Unicast object detection
 * 7- Unicast Collision Message (converted from type 1)
 * 8- Unicast temperature sensor value
 * 9- Unicast battery voltage value
 * 10- Unicast emergency message GTW-MU
 * */
#define ROUTING_ENTRY 0				// This data type will be used by RSU to broadcast its routing table entry.
#define MU_COLLISION_DETECTION	1	// This data type will be used by MU.
#define REBROADCAST_COLLISION 2		// This data type is used by RSU to rebroadcast the received collided packet.
#define GW_RETURN 3					// This data type is used by GW to send crash message to everyone in the network.
#define ROUTING_TABLE 4
/* **UNICAST TYPES **/
#define SENSOR_VALUE 5				// This message type is used by RSU to share its sensor information to the GUI.
#define OBJECT_DETECTION 6			// This is a unicast message type used by RSUs
#define COLLISION_MU 7				// If a RSU detects collision, then it send this message to the gateway with this type.
#define DATA_LOGGER 8

#define GW_MESSAGE 10				// This data type is used by GW and collided MU to communicate each other.
#define DISABLE_CONNECTION 11		// If GW sends a message to collided unit with this value, then MU disables the routing.

#define SENSOR_DELAY_DIV	2			//defined as (SENSOR_DELAY_DIV * 1second)^-1
#define SENSOR_SAMPLING_DELAY_DIV 10 	//defined as (SENSOR_SAMPLING_DELAY_DIV * 1second)^-1

#define DATA_LOGGER_DELAY 10				// period of data packets defined as 1s * DATA_LOGGER

#endif /* PROJECT_CONF_H_ */
