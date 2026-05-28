/*
 * task_ORION.h
 *
 * Created on: May 25, 2026
 * Author: Chiara Capaccioni
 */

#ifndef INC_TASK_ORION_H_
#define INC_TASK_ORION_H_

#include <stdint.h>

// If you need to keep track of the iteration count for the Orion task globally
extern uint32_t global_orion_it;

void Task_ORION_init(void);
void Task_ORION( void *pvParameters );

#endif /* INC_TASK_ORION_H_ */
