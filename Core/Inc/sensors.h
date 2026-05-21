/*
 * sensors.h
 *
 *  Created on: Mar 23, 2026
 *      Author: giuseppe
 */

#ifndef INC_SENSORS_H_
#define INC_SENSORS_H_
#include <stdbool.h>


struct sensor_t {

	float valor;
	float minimo;
	float maximo;
	int nivel_alarma;
	float record_max;
	float record_min;
};

#endif /* INC_SENSORS_H_ */
