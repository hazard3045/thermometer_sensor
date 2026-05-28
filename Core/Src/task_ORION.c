#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include "semphr.h"

// Inclusioni specifiche del tuo progetto
#include "task_COMM.h"
#include "tareas.h"
#include "main.h"
#include "sensors.h"

// --- CONFIGURAZIONE RETE LOCALE ---
//#define SERVER_IP "192.168.1.145"

// --- VARIABILI ESTERNE (Riferite a main.c e task_COMM.c) ---
extern int g_mode;
extern osMutexId_t sensorMutex;
extern struct sensor_t sensor_ntc;
extern struct sensor_t sensor_ldr;
extern SemaphoreHandle_t COMM_xSem;
extern scomm_request_t COMM_request;

// Buffer statici per la richiesta (evitano di riempire lo stack della task)
static char json_body[512];
static char full_http_post[1024];

// --- CORPO DELLA TASK ---
void Task_ORION(void *pvParameters){
    int signal;
    int local_mode; // Local variable to hold the safe snapshot of g_mode

    // Variabili locali per salvare i dati dei sensori al riparo dal mutex
    float t_val, t_al, t_min, t_max;
    float l_val, l_al, l_min, l_max;

    while (1) {
        // SECURE READ: Take a safe snapshot of the shared g_mode variable under Mutex protection
        osMutexAcquire(sensorMutex, osWaitForever);
        local_mode = g_mode;
        osMutexRelease(sensorMutex);

        // Execute only if system is in connected mode (Modo normal / conectado)
        if (local_mode == 0) {

            // 1. ACQUISIZIONE DATI SICURA
            // Protect shared sensor resources with a mutex to avoid race conditions
            osMutexAcquire(sensorMutex, osWaitForever);
            t_val = sensor_ntc.valor;
            t_al  = (float)sensor_ntc.nivel_alarma;
            t_min = sensor_ntc.minimo;
            t_max = sensor_ntc.maximo;

            l_val = sensor_ldr.valor;
            l_al  = (float)sensor_ldr.nivel_alarma;
            l_min = sensor_ldr.minimo;
            l_max = sensor_ldr.maximo;
            osMutexRelease(sensorMutex);

            // 2. COSTRUZIONE DEL CORPO JSON (SINTASSI API V2)
            // Flattened structure complying with local Orion strict v2 parser
            memset(json_body, 0, sizeof(json_body));
            sprintf(json_body,
                "{"
                "\"actionType\":\"APPEND\","
                "\"entities\":[{"
                    "\"type\":\"Sensor\",\"id\":\"SensorSEU_03\","
                    "\"Alarma\":{\"type\":\"boolean\",\"value\":\"F\"},"
                    "\"Alarma_src\":{\"type\":\"string\",\"value\":\"SensorSEU_0300\"},"
                    "\"modo\":{\"type\":\"string\",\"value\":\"SensorSEU_03\"},"
                    "\"Temperatura\":{\"type\":\"floatarray\",\"value\":\"%.2f,%.2f,%.2f,%.2f\"},"
                    "\"IntensidadLuz\":{\"type\":\"floatarray\",\"value\":\"%.2f,%.2f,%.2f,%.2f\"}"
                "}]"
                "}",
                t_val, t_max, t_min, t_al,   // current, max, min, threshold
                l_val, l_max, l_min, l_al);

            // 3. COSTRUZIONE COMPLETA DELLA RICHIESTA HTTP POST V2
            // Endpoint updated to /v2/op/update to match the local working docker schema
            memset(full_http_post, 0, sizeof(full_http_post));
            sprintf(full_http_post,
                "POST /v2/op/update HTTP/1.1\r\n"
                "Host: pperezs-sec.disca.upv.es:1026\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: %d\r\n"
                "\r\n"
                "%s",
                (int)strlen(json_body), json_body);

            // Secure print of the generated request using COMM mutex to prevent console data corruption
            if (xSemaphoreTake(COMM_xSem, portMAX_DELAY) == pdTRUE) {
                bprintf("--- RICHIESTA GENERATA ---\r\n%s\r\n-------------------------\r\n", full_http_post);
                xSemaphoreGive(COMM_xSem);
            }

            // 4. NEGOZIAZIONE CON LA TASK DI COMUNICAZIONE (MUTEX COMM)
            signal = 1;
            do {
                if (xSemaphoreTake(COMM_xSem, 20000/portTICK_RATE_MS) != pdTRUE ){
                    bprintf("\r\n\n\nHARAKIRI!!\n\n\n");
                    HAL_NVIC_SystemReset();
                }

                if (COMM_request.command == 0){ // Se il canale è libero
                    COMM_request.command = 1;
                    COMM_request.result = 0;

                    // Configura i parametri per Orion Context Broker
                    COMM_request.dst_port = 1026;
                    COMM_request.dst_address = (uint8_t *)"pperezs-sec.disca.upv.es";
                    COMM_request.HTTP_request = (uint8_t *)full_http_post;

                    signal = 0;
                    xSemaphoreGive(COMM_xSem);
                }
                else {
                    xSemaphoreGive(COMM_xSem);
                    vTaskDelay((1 + (rand() % 100)) / portTICK_RATE_MS);
                }
            } while(signal);

            // 5. ATTESA DEL RISULTATO DALL'ESP
            // Wait until the communication cycle fully terminates (1 or -1)
            while (COMM_request.result == 0) {
                vTaskDelay(10 / portTICK_RATE_MS);
            }

            // --- DEBUG PRINT OF THE SERVER RESPONSE ---
            // Protected critical section to output the final response from local Orion
            if (xSemaphoreTake(COMM_xSem, portMAX_DELAY) == pdTRUE) {
                if (COMM_request.result == 1 && COMM_request.HTTP_response != NULL) {
                    bprintf("--- RISPOSTA SERVER RAW ---\r\n%s\r\n---------------------------\r\n", COMM_request.HTTP_response);
                } else {
                    bprintf("[ORION] Errore di rete: La comunicazione con l'ESP ha fallito o è andata in TIMEOUT\r\n");
                }

                // Clear the request structure inside the protected context to release the channel
                COMM_request.result = 0;
                COMM_request.command = 0;
                xSemaphoreGive(COMM_xSem);
            }

            bprintf("[ORION] Ciclo di pubblicazione completato!\r\n");
        }

        // 6. PAUSA DI PUBBLICAZIONE (Ogni 30 secondi)
        vTaskDelay(30000 / portTICK_RATE_MS);
    }
}

// --- INIZIALIZZAZIONE DELLA TASK ---
void Task_ORION_init(void){
    BaseType_t res_task;
    res_task = xTaskCreate(Task_ORION, "ORION_PUB", 2048, NULL, LOW_PRIORITY, NULL);
    if(res_task != pdPASS){
        bprintf("PANIC: Error al crear Tarea ORION\r\n");
        fflush(NULL);
        while(1);
    }
}
