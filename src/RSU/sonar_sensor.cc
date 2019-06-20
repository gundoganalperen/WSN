#include <stdint.h>
#include "sonar_sensor.h"
typedef float float32_t;
//two seperate functions may be obsolete if the combined function works
int IR_sensor_dist(uint16_t adc_value){
	/*
	 * Distance in cm = 4.8 / (Voltage ratio - 0.02)
	 * Voltage ratio = Sensor output voltage / sensor VCC voltage
	 * eg. in case of ADC3 you have 5VDC VCC voltage or 1795
	 * ADC3 RAW ouput 2 -1795 | 0VDC = 2 | 5VDC or VCC = 1795
	 * */
	float v_r = adc_value / 1795.0; // maximum value tested manually by connecting Vcc to Ain
	float result = 4.8;
	result /= v_r - 0.02;
	return result;
}
int SONAR_sensor_dist(uint16_t adc_value){
	/*
	 * Distance in cm = Voltage ratio *1296;
	 * Voltage ratio = Sensor output voltage / sensor VCC voltage
	 * eg. in case of ADC3 you have 5VDC VCC voltage or 1795
	 * ADC3 RAW ouput 2 -1795 | 0VDC = 2 | 5VDC or VCC = 1795
	 * */
	float v_r = adc_value / 1795.0;
	float result = v_r * 1296;
	return result;

}

//combined function to get the distance
int get_sensor_dist(uint16_t adc_value, uint8_t sensor_type){
	float result;
	float v_r = adc_value / 1795.0;
	/* IR Sensor
	 * Distance in cm = 4.8 / (Voltage ratio - 0.02)
	 * Voltage ratio = Sensor output voltage / sensor VCC voltage
	 * eg. in case of ADC3 you have 5VDC VCC voltage or 1795
	 * ADC3 RAW ouput 2 -1795 | 0VDC = 2 | 5VDC or VCC = 1795
	 * */
	if(sensor_type == 0){
		result = 4.8;
		result /= v_r - 0.02;
		return result;
	}
	/* SONAR sensor distance
		 * Distance in cm = Voltage ratio *1296;
		 * Voltage ratio = Sensor output voltage / sensor VCC voltage
		 * eg. in case of ADC3 you have 5VDC VCC voltage or 1795
		 * ADC3 RAW ouput 2 -1795 | 0VDC = 2 | 5VDC or VCC = 1795
		 * */
		if(sensor_type == 1){
			result = v_r * 1296;
			return result;
		}

	return 0;
}

uint32_t iabs(uint32_t x){
	if(x>0){
		return x;
	}
	else if (x<0){
		return -1*x;
	}
	else return 0;
}

float sqrt_emb(float num)
{
    float guess, e, upperbound;
    guess = 1;
    e = 0.1;
    do
    {
        upperbound = num / guess;
        guess = (upperbound + guess) / 2;
    } while (!(guess * guess >= num - e &&
               guess * guess <= num + e));
    return guess;
}


uint16_t sqrt_emb2(uint32_t x)
{
	/*
	 * Calculate the square root of the argument using the iterative Babylonian method.
	 * Details of the algorithm are online at Wikipedia, I tweaked it to output an integer answer.
	 */
	uint32_t fa,sa,ta;
	uint32_t error,error_last;

	// Choose an (arbitrary) first approach as the given number divided by 2
	// The closer this number is to the final answer the faster this routines completes.
	//fa = x/2;
	fa = x>>1;		// Divide number by 2

	// Divide the argument number by the first approach
	sa = x/fa;

	// Get the mean between the two previous numbers (add them and divide by 2).
	ta = (fa+sa)>>1;

	error_last=-1;
	error=0;

	/*
	 * Repeat this routine until the integer output value is no longer changing.
	 */
	do {
		error_last=error;
		fa = ta;
		sa = x/fa;
		ta = (fa+sa)>>1;
		error=iabs(x - ta*ta);
	} while (error_last!=error);

	// Return the integer result: square root of the input argument x.
	return ta;
}
