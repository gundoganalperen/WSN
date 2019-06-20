#ifndef TIMERS_H
#define TIMERS_H

/********************TIMERS*************************/
// Callback timer for periodic routing table updates.
static struct ctimer periodic_rt_timer;
// Callback timer for Crash messages produced by GW
static struct ctimer stateGC_timer;
// Timer for sampling sensor data in RSU
static struct etimer sensor_timer;
// Timer for RSU to avoid that one MU is detected twice
static struct etimer dedection_delay_timer;
// Timer for RSU to send temperature and battery data
static struct etimer data_logger_timer;
/***************************************************/

#endif
