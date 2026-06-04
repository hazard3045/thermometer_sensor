#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include "semphr.h"

// Inclusioni specifiche del tuo progetto
#include "task_COMM.h"
#include "task_CONSOLE.h"
#include "tareas.h"
#include "main.h"
#include "sensors.h"
#include "task_ORION.h"

// --- CONFIGURAZIONE ENDPOINT DA SPECIFICHE ---
#define IoT_NAME "SensorSEU_03"

// --- VARIABILI ESTERNE (Riferite a main.c e task_COMM.c) ---
extern int g_mode;
extern bool alarm_triggered;
extern osMutexId_t sensorMutex;
extern struct sensor_t sensor_ntc;
extern struct sensor_t sensor_ldr;
extern SemaphoreHandle_t COMM_xSem;
extern scomm_request_t COMM_request;
extern int clone_target_id;
extern bool clone_alarm_inhibited;
extern uint32_t alarm_off_timestamp;

// Buffer statici per la richiesta (evitano di riempire lo stack della task)
static char json_body[512];
static char full_http_post[1024];

static bool g_request_alarm_clear = false;
static int g_clon_sequence = 0;


void Task_ORION(void *pvParameters){
    int signal;
    int local_mode;
    uint32_t delay_task_ms;

    float t_val, t_al, t_min, t_max;
    float l_val, l_al, l_min, l_max;

    bool last_remote_alarm;

    // Timer statico per scaglionare l'invio dei sensori (PATCH) ogni 30 secondi
    static uint32_t t_last_patch = 0;

    while (1) {
        osMutexAcquire(sensorMutex, osWaitForever);
        local_mode = g_mode;
        osMutexRelease(sensorMutex);

        if (local_mode == 0) {
            delay_task_ms = 10000; // Controlla il server OGNI SECONDO per massima reattività

            memset(full_http_post, 0, sizeof(full_http_post));
            sprintf(full_http_post,
                    "GET /v2/entities/" IoT_NAME "/attrs HTTP/1.1\r\n"
                    "Host: pperezs-sec.disca.upv.es:1026\r\n"
                    "Connection: close\r\n"
                    "Accept: application/json\r\n"
                    "\r\n");

            signal = 1;
            do {
                if (xSemaphoreTake(COMM_xSem, 20000 / portTICK_RATE_MS) != pdTRUE ){
                    bprintf("\r\n[ORION] HARAKIRI SEMAFORO GET M0!!\n");
                    HAL_NVIC_SystemReset();
                }
                if (COMM_request.command == 0){
                    COMM_request.command = 1;
                    COMM_request.result = 0;
                    COMM_request.dst_port = 1026;
                    strcpy((char *)COMM_request.dst_address, "pperezs-sec.disca.upv.es");
                    strcpy((char *)COMM_request.HTTP_request, full_http_post);
                    signal = 0;
                    xSemaphoreGive(COMM_xSem);
                } else {
                    xSemaphoreGive(COMM_xSem);
                    vTaskDelay(20 / portTICK_RATE_MS);
                }
            } while(signal);

            while (COMM_request.result == 0) {
                vTaskDelay(20 / portTICK_RATE_MS);
            }

            if (xSemaphoreTake(COMM_xSem, portMAX_DELAY) == pdTRUE) {
                if (COMM_request.result == 1 && strlen((char *)COMM_request.HTTP_response) > 0) {
                    char *response_ptr = (char *)COMM_request.HTTP_response;
                    char *alarm_block = strstr(response_ptr, "\"Alarma\"");

                    if (alarm_block != NULL) {
                        char *value_ptr = strstr(alarm_block, "\"value\":\"");
                        if (value_ptr != NULL) {
                            value_ptr += 9;
                            char remote_alarm_status = *value_ptr;
                            bool current_remote_alarm = (remote_alarm_status == 'T');

                            osMutexAcquire(sensorMutex, osWaitForever);
//                            if (last_remote_alarm == true && current_remote_alarm == false) {
//                                bprintf("[ORION] Rilevato reset sul server (Server=OFF, Scheda=ON).\r\n");
//                                alarm_triggered = false;
//                                alarm_off_timestamp = xTaskGetTickCount();
//                            }
//                            osMutexRelease(sensorMutex);
//
                            if (last_remote_alarm == true && current_remote_alarm == false) {
                                // Leggi chi ha spento l'allarme
                                char who_cleared[32] = "sconosciuto";
                                char *clear_block = strstr(response_ptr, "\"AlarmClearBy\"");
                                if (clear_block != NULL) {
                                    char *clear_val = strstr(clear_block, "\"value\":\"");
                                    if (clear_val != NULL) {
                                        clear_val += 9;
                                        sscanf(clear_val, "%31[^\"]", who_cleared);
                                    }
                                }
                                bprintf("[ORION] Allarme spento da: %s\r\n", who_cleared);
                                alarm_triggered = false;
                                alarm_off_timestamp = xTaskGetTickCount();
                            }
                            osMutexRelease(sensorMutex);
                        }
                    }
                }
                memset((char *)COMM_request.HTTP_response, 0, sizeof(COMM_request.HTTP_response));
                COMM_request.result = 0;
                COMM_request.command = 0;
                xSemaphoreGive(COMM_xSem);
            }

            if (xTaskGetTickCount() - t_last_patch >= (15000 / portTICK_RATE_MS) || t_last_patch == 0) {
                t_last_patch = xTaskGetTickCount(); // Aggiorna il timestamp dell'ultimo invio

                osMutexAcquire(sensorMutex, osWaitForever);
                t_val = sensor_ntc.valor;
                t_min = sensor_ntc.minimo;
                t_max = sensor_ntc.maximo;
                t_al  = t_min + (((float)sensor_ntc.nivel_alarma / 8.0f) * (t_max - t_min));

                l_val = sensor_ldr.valor;
                l_min = sensor_ldr.minimo;
                l_max = sensor_ldr.maximo;
                l_al  = l_min + (((float)sensor_ldr.nivel_alarma / 8.0f) * (l_max - l_min));

                bool current_alarm_state = alarm_triggered;
                osMutexRelease(sensorMutex);

//                memset(json_body, 0, sizeof(json_body));
//                sprintf(json_body,
//                    "{"
//                    "\"Temperatura\":{\"type\":\"floatArray\",\"value\":\"%.2f,%.2f,%.2f,%.2f\"},"
//                    "\"IntensidadLuz\":{\"type\":\"floatArray\",\"value\":\"%.2f,%.2f,%.2f,%.2f\"},"
//                    "\"Alarma\":{\"type\":\"boolean\",\"value\":\"%s\"},"
//                    "\"modo\":{\"type\":\"string\",\"value\":\"SensorSEU_03\"}"
//                    "}",
//                    t_val, t_max, t_min, t_al,
//                    l_val, l_max, l_min, l_al,
//                    current_alarm_state ? "T" : "F");

                memset(json_body, 0, sizeof(json_body));
                sprintf(json_body,
                    "{"
                    "\"Temperatura\":{\"type\":\"floatArray\",\"value\":\"%.2f,%.2f,%.2f,%.2f\"},"
                    "\"IntensidadLuz\":{\"type\":\"floatArray\",\"value\":\"%.2f,%.2f,%.2f,%.2f\"},"
                    "\"Alarma\":{\"type\":\"boolean\",\"value\":\"%s\"},"
                    "\"AlarmSetBy\":{\"type\":\"string\",\"value\":\"%s\"},"
                    "\"modo\":{\"type\":\"string\",\"value\":\"SensorSEU_18\"}"
                    "}",
                    t_val, t_max, t_min, t_al,
                    l_val, l_max, l_min, l_al,
                    current_alarm_state ? "T" : "F",
                    current_alarm_state ? IoT_NAME : "none");

                memset(full_http_post, 0, sizeof(full_http_post));
                sprintf(full_http_post,
                        "PATCH /v2/entities/" IoT_NAME "/attrs HTTP/1.1\r\n"
                        "Host: pperezs-sec.disca.upv.es:1026\r\n"
                        "Connection: close\r\n"
                        "Content-Type: application/json\r\n"
                        "Accept: application/json\r\n"
                        "Content-Length: %d\r\n"
                        "\r\n"
                        "%s",
                        (int)strlen(json_body), json_body);

                if (xSemaphoreTake(COMM_xSem, portMAX_DELAY) == pdTRUE) {
                    bprintf("\r\n================ RICHIESTA GENERATA (M0: PATCH SENSORI) ================\r\n");
                    bprintf("%s", full_http_post);
                    bprintf("\r\n========================================================================\r\n");
                    last_remote_alarm = current_alarm_state;
                    xSemaphoreGive(COMM_xSem);
                }

                signal = 1;
                do {
                    if (xSemaphoreTake(COMM_xSem, 20000 / portTICK_RATE_MS) != pdTRUE ){
                        bprintf("\r\n[ORION] HARAKIRI SEMAFORO PATCH M0!!\n");
                        HAL_NVIC_SystemReset();
                    }
                    if (COMM_request.command == 0){
                        COMM_request.command = 1;
                        COMM_request.result = 0;
                        COMM_request.dst_port = 1026;
                        strcpy((char *)COMM_request.dst_address, "pperezs-sec.disca.upv.es");
                        strcpy((char *)COMM_request.HTTP_request, full_http_post);
                        signal = 0;
                        xSemaphoreGive(COMM_xSem);
                    } else {
                        xSemaphoreGive(COMM_xSem);
                        vTaskDelay(20 / portTICK_RATE_MS);
                    }
                } while(signal);

                while (COMM_request.result == 0) {
                    vTaskDelay(20 / portTICK_RATE_MS);
                }

                if (xSemaphoreTake(COMM_xSem, portMAX_DELAY) == pdTRUE) {
                    memset((char *)COMM_request.HTTP_response, 0, sizeof(COMM_request.HTTP_response));
                    COMM_request.result = 0;
                    COMM_request.command = 0;
                    xSemaphoreGive(COMM_xSem);
                }
            }
        }
        // =================================================================
        // MODALITÀ CLONE (MODO 1)
        // =================================================================
        else if (local_mode == 1) {
            delay_task_ms = 1000;

            char target_clone_name[32];
            sprintf(target_clone_name, "SensorSEU_%02d", clone_target_id);

            bool was_clear_request = g_request_alarm_clear;

            if (was_clear_request) {
                bprintf("[CLONE] Invio PATCH di spegnimento a %s...\r\n", target_clone_name);

//                memset(json_body, 0, sizeof(json_body));
//                sprintf(json_body, "{\"Alarma\":{\"type\":\"boolean\",\"value\":\"F\"}}");
//
                memset(json_body, 0, sizeof(json_body));
                sprintf(json_body,
                    "{"
                    "\"Alarma\":{\"type\":\"boolean\",\"value\":\"F\"},"
                    "\"AlarmClearBy\":{\"type\":\"string\",\"value\":\"" IoT_NAME "\"}"
                    "}");

                memset(full_http_post, 0, sizeof(full_http_post));
                sprintf(full_http_post,
                        "PATCH /v2/entities/%s/attrs HTTP/1.1\r\n"
                        "Host: pperezs-sec.disca.upv.es:1026\r\n"
                        "Connection: close\r\n"
                        "Content-Type: application/json\r\n"
                        "Content-Length: %d\r\n"
                        "\r\n"
                        "%s",
                        target_clone_name, (int)strlen(json_body), json_body);
            }
            else {
                memset(full_http_post, 0, sizeof(full_http_post));
                sprintf(full_http_post,
                        "GET /v2/entities/%s/attrs HTTP/1.1\r\n"
                        "Host: pperezs-sec.disca.upv.es:1026\r\n"
                        "Connection: close\r\n"
                        "Accept: application/json\r\n"
                        "\r\n",
                        target_clone_name);
            }

            signal = 1;
            do {
                if (xSemaphoreTake(COMM_xSem, 20000 / portTICK_RATE_MS) != pdTRUE ){
                    bprintf("\r\n[CLONE] HARAKIRI SEMAFORO NEGOZIAZIONE!!\n");
                    HAL_NVIC_SystemReset();
                }

                if (COMM_request.command == 0){
                    COMM_request.command = 1;
                    COMM_request.result = 0;
                    COMM_request.dst_port = 1026;

                    memset(COMM_request.dst_address, 0, sizeof(COMM_request.dst_address));
                    strcpy((char *)COMM_request.dst_address, "pperezs-sec.disca.upv.es");

                    memset(COMM_request.HTTP_request, 0, sizeof(COMM_request.HTTP_request));
                    strcpy((char *)COMM_request.HTTP_request, full_http_post);

                    signal = 0;
                    xSemaphoreGive(COMM_xSem);
                }
                else {
                    xSemaphoreGive(COMM_xSem);
                    vTaskDelay(20 / portTICK_RATE_MS);
                }
            } while(signal);

            while (COMM_request.result == 0) {
                vTaskDelay(20 / portTICK_RATE_MS);
            }

            if (xSemaphoreTake(COMM_xSem, portMAX_DELAY) == pdTRUE) {
                if (COMM_request.result == 1) {

                    if (was_clear_request) {
                        bprintf("[CLONE] Server Orion aggiornato ad 'F'. Rilascio inibizione hardware.\r\n");

                        osMutexAcquire(sensorMutex, osWaitForever);
                        clone_alarm_inhibited = false;
                        osMutexRelease(sensorMutex);

                        g_request_alarm_clear = false;
                    }
                    else {
                        if (strlen((char *)COMM_request.HTTP_response) > 0) {

                            char *response_ptr = (char *)COMM_request.HTTP_response;
                            char *temp_block = strstr(response_ptr, "\"Temperatura\"");
                            char *luz_block  = strstr(response_ptr, "\"IntensidadLuz\"");
                            char *alarm_block = strstr(response_ptr, "\"Alarma\"");

                            float parsed_t_val = 0.0f, parsed_t_max = 0.0f, parsed_t_min = 0.0f, parsed_t_al = 0.0f;
                            float parsed_l_val = 0.0f, parsed_l_max = 0.0f, parsed_l_min = 0.0f, parsed_l_al = 0.0f;
                            char parsed_alarm_status = 'F';
                            bool parsing_ok = true;

                            if (temp_block != NULL) {
                                char *value_ptr = strstr(temp_block, "\"value\":\"");
                                if (value_ptr != NULL) {
                                    value_ptr += 9;
                                    if (sscanf(value_ptr, "%f,%f,%f,%f", &parsed_t_val, &parsed_t_max, &parsed_t_min, &parsed_t_al) != 4) {
                                        parsing_ok = false;
                                    }
                                } else { parsing_ok = false; }
                            } else { parsing_ok = false; }

                            if (luz_block != NULL) {
                                char *value_ptr = strstr(luz_block, "\"value\":\"");
                                if (value_ptr != NULL) {
                                    value_ptr += 9;
                                    if (sscanf(value_ptr, "%f,%f,%f,%f", &parsed_l_val, &parsed_l_max, &parsed_l_min, &parsed_l_al) != 4) {
                                        parsing_ok = false;
                                    }
                                } else { parsing_ok = false; }
                            } else { parsing_ok = false; }

//                            if (alarm_block != NULL) {
//                                char *value_ptr = strstr(alarm_block, "\"value\":\"");
//                                if (value_ptr != NULL) {
//                                    value_ptr += 9;
//                                    parsed_alarm_status = *value_ptr;
//                                }
//                            }

                            if (alarm_block != NULL) {
                                char *value_ptr = strstr(alarm_block, "\"value\":\"");
                                if (value_ptr != NULL) {
                                    value_ptr += 9;
                                    parsed_alarm_status = *value_ptr;

                                    // Se l'allarme è acceso, leggi chi lo ha settato
                                    if (parsed_alarm_status == 'T') {
                                        char who_set[32] = "sconosciuto";
                                        char *setby_block = strstr(response_ptr, "\"AlarmSetBy\"");
                                        if (setby_block != NULL) {
                                            char *setby_val = strstr(setby_block, "\"value\":\"");
                                            if (setby_val != NULL) {
                                                setby_val += 9;
                                                sscanf(setby_val, "%31[^\"]", who_set);
                                            }
                                        }
                                        bprintf("[CLONE] Allarme attivo su %s, settato da: %s\r\n",
                                                target_clone_name, who_set);
                                    }
                                }
                            }

                            if (parsing_ok) {
                                osMutexAcquire(sensorMutex, osWaitForever);

                                sensor_ntc.valor        = parsed_t_val;
                                sensor_ntc.maximo       = parsed_t_max;
                                sensor_ntc.minimo       = parsed_t_min;
                                sensor_ntc.nivel_alarma = (int)(((parsed_t_al - parsed_t_min) / (parsed_t_max - parsed_t_min)) * 8.0f);

                                sensor_ldr.valor        = parsed_l_val;
                                sensor_ldr.maximo       = parsed_l_max;
                                sensor_ldr.minimo       = parsed_l_min;
                                sensor_ldr.nivel_alarma = (int)(((parsed_l_al - parsed_l_min) / (parsed_l_max - parsed_l_min)) * 8.0f);

                                if (clone_alarm_inhibited) {
                                    alarm_triggered = false;
                                }
                                else {
                                    if (parsed_alarm_status == 'T') {
                                        alarm_triggered = true;
                                    } else {
                                        alarm_triggered = false;
                                    }
                                }

                                osMutexRelease(sensorMutex);
                            }
                        }
                    }
                } else {
                    bprintf("[CLONE] Errore di rete nella transazione.\r\n");
                    if (was_clear_request) {
                        osMutexAcquire(sensorMutex, osWaitForever);
                        clone_alarm_inhibited = false;
                        osMutexRelease(sensorMutex);
                        g_request_alarm_clear = false;
                    }
                }

                memset((char *)COMM_request.HTTP_response, 0, sizeof(COMM_request.HTTP_response));
                COMM_request.result = 0;
                COMM_request.command = 0;
                xSemaphoreGive(COMM_xSem);
            }
        }

        vTaskDelay(delay_task_ms / portTICK_RATE_MS);
    }
}

void ORION_SignalAlarmClear(void){
    if (!g_request_alarm_clear) {
        g_clon_sequence++;
        g_request_alarm_clear = true;
    }
}

void Task_ORION_init(void){
    BaseType_t res_task;
    res_task = xTaskCreate(Task_ORION, "ORION_PUB", 2048, NULL, LOW_PRIORITY, NULL);
    if(res_task != pdPASS){
        bprintf("PANIC: Error al crear Tarea ORION\r\n");
        fflush(NULL);
        while(1);
    }
}
