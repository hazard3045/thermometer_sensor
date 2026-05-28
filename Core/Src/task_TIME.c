/*
 * task_TIME.c
 *
 * Created on: May 25, 2023
 * Author: pperez
 */

#include <stdint.h>
#include "task_COMM.h"
#include "FreeRTOS.h"
#include <stdio.h>
#include "cmsis_os.h"
#include <stdlib.h>
#include "semphr.h"
#include "tareas.h"
#include <string.h>
#include <task.h>
#include "task_CONSOLE.h"
#include "task_TIME.h"
#include "cJSON.h"
#include <time.h>

uint32_t global_time_it;
int time_available = 0;
time_t tm_offset;
time_t ticks_since_start_ts;

void Task_TIME_init(void){
	BaseType_t res_task;
	global_time_it=0;
	global_wifi_ready=0;
	res_task=xTaskCreate( Task_TIME,"TIME",2048,NULL, LOW_PRIORITY,NULL);
	if( res_task != pdPASS ){
		bprintf("PANIC: Error al crear Tarea TIME\r\n");
		fflush(NULL);
		while(1);
	}
}

void Task_TIME( void *pvParameters ){
	int signal;
	cJSON *jsons1;
 	cJSON *name = NULL;

	while (1) {
		signal=1;
		do {
			if (xSemaphoreTake(COMM_xSem, 20000/portTICK_RATE_MS) != pdTRUE ){
				bprintf("\r\n\n\nHARAKIRI!!\n\n\n");
		   		HAL_NVIC_SystemReset();
			}

			// Critical section secured by COMM_xSem Mutex
			if (COMM_request.command==0){ // Communication channel is completely free
				COMM_request.command=1;
				COMM_request.result=0;

				// Original university server network parameters
				COMM_request.dst_port=5000;
				COMM_request.dst_address=(uint8_t *)"pperez2.disca.upv.es";
				COMM_request.HTTP_request=(uint8_t *)"GET /tiempo HTTP/1.1\r\n\r\n";

				signal=0;
				xSemaphoreGive(COMM_xSem); // Safely exit critical section
			}
			else {
			      xSemaphoreGive(COMM_xSem); // Release mutex immediately if busy
			      // Increased backoff delay to let Task_ORION negotiate the channel smoothly
			      vTaskDelay((500 + (rand()%500)) / portTICK_RATE_MS);
			}

		} while(signal);

  		// ROBUSTNESS UPDATE: Wait until communication finishes (either 1 for success or -1 for error)
		while (COMM_request.result == 0) {
			vTaskDelay(10/portTICK_RATE_MS);
		}

		// Only parse JSON if the communication cycle returned absolute success (1)
		if (COMM_request.result == 1 && COMM_request.HTTP_response != NULL) {
			jsons1 = cJSON_Parse((const char *)COMM_request.HTTP_response);
			if (jsons1) {
				name = cJSON_GetObjectItem(jsons1, "tiempo_actual");
				if (name != NULL && name->valuestring != NULL) {
					struct tm tm_date = {0};
					int year, month, day, hour, minute, second;
					sscanf(name->valuestring, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second);
					tm_date.tm_year = year - 1900;
					tm_date.tm_mon = month - 1;
					tm_date.tm_mday = day;
					tm_date.tm_hour = hour;
					tm_date.tm_min = minute;
					tm_date.tm_sec = second;
					tm_date.tm_isdst = -1;

					tm_offset = mktime(&tm_date);
					ticks_since_start_ts = xTaskGetTickCount();
					time_available=1;
				}
				cJSON_Delete(jsons1);
			} else {
				bprintf("TIME: JSON Parsing error \r\n");
			}
		} else {
			bprintf("TIME: Network timeout or connection response error \r\n");
		}

		// --- CANAL CLEANUP AREA ---
		// Re-acquire the mutex to clean up shared request variables safely
		if (xSemaphoreTake(COMM_xSem, portMAX_DELAY) == pdTRUE) {
			COMM_request.result=0;
			COMM_request.command=0; // Free the shared structure for Task_ORION
			xSemaphoreGive(COMM_xSem);
		}

		global_time_it++;

		// Forced Yielding Delay: Gives complete structural freedom to Task_ORION
		// If an error occurred (e.g. server closed), wait 10 seconds instead of hammering the ESP module immediately
		if (time_available == 1) {
			vTaskDelay(100000/portTICK_RATE_MS);
		} else {
			vTaskDelay(10000/portTICK_RATE_MS);
		}
	}
}

int task_TIME_timeAvailable(void){
	return time_available;
}

time_t task_TIME_getTime(void){
	return tm_offset + (xTaskGetTickCount() - ticks_since_start_ts)/portTICK_RATE_MS/1000;
}
