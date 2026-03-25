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
	float nivel_alarma;
	int activated;
	long int time_activation;
	int value_flashing; // valor del led de nivel de alarma (0 o 1) se puede obviar y leer el pin.
	long int flashing_last_time_activation; // instante en el que se actualizo el estado del led de alarma obtenido mediante la función xTaskGetTickCount()
 };

bool button_pressed_left = false; // false for off and true for on, set to true in inputs tasks and off in the outputs tasks
bool button_pressed_right = false;

#endif /* INC_SENSORS_H_ */
