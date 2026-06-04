#include <stdint.h>
#include "FreeRTOS.h"
#include <stdio.h>
#include "cmsis_os.h"
#include <stdlib.h>
#include "semphr.h"
#include "tareas.h"
#include <string.h>
#include <task.h>
#include <time.h>
#include "task_TIME.h"

#include "task_CONSOLE.h"
#include "main.h"
#include "task_EJER3.h"

//uint32_t global_ejer3_it;

void Task_EJER3_init(void){
	BaseType_t res_task;


	//global_ejer3_it=0;

	res_task=xTaskCreate(Task_EJER3,"EJER3",2048,NULL,	LOW_PRIORITY ,NULL);
 	if( res_task != pdPASS ){
 	 				printf("PANIC: Error al crear Tarea Ejer3\r\n");
 	 				fflush(NULL);
 	 				while(1);
 	}
}

void Task_EJER3( void *pvParameters ){
    static int compteur = 0;
    time_t raw_time;
    struct tm *time_info;

    while (1) {
        //global_ejer3_it++;
        compteur++;

        bprintf("\r\n--- %s (Build: %s) ---\r\n", PPB_PRJ, __TIME__);

        if (task_TIME_timeAvailable()) { // Se il WiFi ha sincronizzato l'ora
            raw_time = task_TIME_getTime(); // Ottiene i secondi Epoch
            time_info = localtime(&raw_time); // Converte in ore/minuti/secondi

            // Stampa l'ora formattata
            bprintf("Horario actual (Valencia): %02d:%02d:%02d\r\n",
                    time_info->tm_hour, time_info->tm_min, time_info->tm_sec);
        } else {
            bprintf("Hora no disponible\r\n");
        }

        vTaskDelay(10000 / portTICK_RATE_MS);
    }
}
