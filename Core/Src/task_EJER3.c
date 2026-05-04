#include <stdint.h>
#include "FreeRTOS.h"
#include <stdio.h>
#include "cmsis_os.h"
#include <stdlib.h>
#include "semphr.h"
#include "tareas.h"
#include <string.h>
#include <task.h>
#include <time.h>        // Per struct tm e time_t
#include "task_TIME.h"   // Per le funzioni dell'orologio

#include "task_CONSOLE.h"
#include "main.h"
#include "task_EJER3.h"

uint32_t global_ejer3_it;

void Task_EJER3_init(void){
	BaseType_t res_task;

	global_ejer3_it=0;

	res_task=xTaskCreate(Task_EJER3,"EJER3",2048,NULL,	NORMAL_PRIORITY,NULL);
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
        // Incrementa il contatore globale e locale
        global_ejer3_it++;
        compteur++;

        // Stampa identificazione progetto
        bprintf("\r\n--- %s (Build: %s) ---\r\n", PPB_PRJ, __TIME__);
        bprintf("Survival count: %d\r\n", compteur);

        // CONTROLLO ORA REALE (Esercizio 5.2)
        if (task_TIME_timeAvailable()) { // Se il WiFi ha sincronizzato l'ora
            raw_time = task_TIME_getTime(); // Ottiene i secondi Epoch[cite: 1]
            time_info = localtime(&raw_time); // Converte in ore/minuti/secondi[cite: 1]

            // Stampa l'ora formattata
            bprintf("Ora attuale (Valencia): %02d:%02d:%02d\r\n",
                    time_info->tm_hour, time_info->tm_min, time_info->tm_sec);
        } else {
            bprintf("Ora non ancora disponibile (Sincronizzazione WiFi in corso...)\r\n");
        }

        // Attesa di 10 secondi come richiesto[cite: 1]
        vTaskDelay(10000 / portTICK_RATE_MS);
    }
}
