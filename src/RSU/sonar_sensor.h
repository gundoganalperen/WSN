#ifndef SONAR_SENSOR_H
#define SONAR_SENSOR_H


int IR_sensor_dist(uint16_t adc_value);
int SONAR_sensor_dist(uint16_t adc_value);
int get_sensor_dist(uint16_t adc_value, uint8_t sensor_type);
uint32_t iabs(uint32_t x);
float sqrt_emb(float num);
uint16_t sqrt_emb2(uint32_t x);


#endif
