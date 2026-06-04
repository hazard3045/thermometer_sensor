/*
 * task_TIME.c
 *
 * Created on: May 25, 2023
 * Author: pperez
 */

#include <stdint.h>
#include "FreeRTOS.h"
#include <stdio.h>
#include "cmsis_os.h"
#include <stdlib.h>
#include "semphr.h"
#include "tareas.h"
#include <string.h>
#include <task.h>
#include <task_COMM.h>
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

				COMM_request.dst_port = 5000;
				strcpy((char *)COMM_request.dst_address, "pperezs-sec.disca.upv.es");

				memset(COMM_request.HTTP_request, 0, sizeof(COMM_request.HTTP_request));
				strcpy((char *)COMM_request.HTTP_request, "GET /tiempo HTTP/1.1\r\n\r\n");

				signal=0;
				xSemaphoreGive(COMM_xSem);
			}
			else {
			      xSemaphoreGive(COMM_xSem);
			      vTaskDelay((500 + (rand()%500)) / portTICK_RATE_MS);
			}

		} while(signal);

		while (COMM_request.result == 0) {
			vTaskDelay(10/portTICK_RATE_MS);
		}

		// Verifica se la chiamata ha avuto successo E se c'è davvero il JSON del tempo
		if (COMM_request.result == 1 && COMM_request.HTTP_response != NULL && strstr((char *)COMM_request.HTTP_response, "tiempo_actual")) {
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
			}
		} else {
			// In caso di server DOWN abbiamo l'orario di DEFAULT
			bprintf("TIME: Server down.\r\n");
			tm_offset = 1772635620;
			ticks_since_start_ts = xTaskGetTickCount();
			time_available = 1;
		}

		if (xSemaphoreTake(COMM_xSem, portMAX_DELAY) == pdTRUE) {
			COMM_request.result=0;
			COMM_request.command=0;
			xSemaphoreGive(COMM_xSem);
		}

		global_time_it++;

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
