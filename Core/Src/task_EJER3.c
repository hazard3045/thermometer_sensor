#include <stdint.h>
#include "FreeRTOS.h"
#include <stdio.h>
#include "cmsis_os.h"
#include <stdlib.h>
#include "semphr.h"
#include "tareas.h"
#include <string.h>
#include <task.h>

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

	int signal;

	//ALUMNO Rellenar Ejercicio 3

	while (1) {
		global_ejer3_it++;
		//ALUMNO Rellenar Ejercicio 3
	}
}
