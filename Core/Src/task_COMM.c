#include <stdint.h>
#include "task_COMM.h"
#include "FreeRTOS.h"
#include <stdio.h>
#include "cmsis_os.h"
#include <stdlib.h>
#include "semphr.h"
#include <string.h>
#include <task.h>
#include "main.h"
#include "task_CONSOLE.h"
#include "tareas.h"

scomm_request_t COMM_request;
SemaphoreHandle_t COMM_xSem = NULL;
SemaphoreHandle_t COMM_WAIT_xSem = NULL;
uint32_t global_comm_it;

uint8_t buff_recv[2048];

uint32_t global_wifi_it;
uint32_t global_wifi_ready;

uint8_t aux_buff_WIFI[2048];
uint8_t buff_WIFI_response[2048];

void Task_COMM_init(void){
	BaseType_t res_task;

	global_comm_it=0;

	COMM_xSem=xSemaphoreCreateMutex();

	if( COMM_xSem == NULL ){
		printf("PANIC: Error al crear el Semáforo ORION\r\n");
		fflush(NULL);
		while(1);
	}

	COMM_WAIT_xSem= xSemaphoreCreateBinary();

	if( COMM_WAIT_xSem == NULL ){
		printf("PANIC: Error al crear el Semáforo ORION 2\r\n");
		fflush(NULL);
		while(1);
	}

 	res_task=xTaskCreate(Task_COMM,"COMMUNICATION",2048,NULL, NORMAL_PRIORITY,NULL);
 	if( res_task != pdPASS ){
 		printf("PANIC: Error al crear Tarea Comunicaciones\r\n");
 		fflush(NULL);
 		while(1);
 	}
}

void Task_COMM( void *pvParameters ){

	int signal;

	WIFI_Boot();

	while (1) {
	    signal=1;
		do {
			if (xSemaphoreTake(COMM_xSem, 20000/portTICK_RATE_MS  ) != pdTRUE ){
				bprintf("\r\n\n\nHARAKIRI!!\n\n\n");
		   		HAL_NVIC_SystemReset();
			}

			// Check if any thread has registered a request
			if (COMM_request.command==1) {
				signal=0;
			} else {
				xSemaphoreGive(COMM_xSem); // Leave critical section
				osDelay(10);
			}
		} while(signal);

		COMM_request.command=2; // Status: Busy
		xSemaphoreGive(COMM_xSem); // Leave critical section

		// --- MUTEX PROTECTION FOR HARDWARE AND RESPONSE CLEANING ---
		// Protect the ESP8266 Wi-Fi module from being accessed by other tasks concurrently
		if (xSemaphoreTake(COMM_xSem, portMAX_DELAY) == pdTRUE) {

			COMM_request.HTTP_response = ESP_Send_Request(COMM_request.dst_address, COMM_request.dst_port, COMM_request.HTTP_request);

			// Secure parsing: Only clean if a valid string response was received
			if (COMM_request.HTTP_response != NULL && strlen((char *)COMM_request.HTTP_response) > 0) {
				cleanResponse(COMM_request.HTTP_response, strlen((char *)COMM_request.HTTP_response));
				COMM_request.result = 1; // Execution successfully completed
			} else {
				COMM_request.result = -1; // Network error or connection failed
			}

			xSemaphoreGive(COMM_xSem); // Safe to release now
		}

		global_comm_it++;
	}
}

void cleanResponse(uint8_t * data, int maxlen)
{
	int t,i;
	uint8_t * j,*from, *pc;

	// Defensive check against unexpected empty parameters
	if (data == NULL || maxlen == 0) return;

	i=0;
	while ((j=(uint8_t *)strstr((char *)data,"+IPD"))){
		from=(uint8_t *)strstr((char *)j,":");
		if (from == NULL) break; // Avoid parsing corrupt frames
		from++;

		for (pc=from; pc<(data+2048); pc++) {
			*(j++)=*(from++);
		}
	};

	// Protection against responses that don't contain a valid JSON payload
	if (strstr((char *)data, "{") == NULL) return;

	i = 0;
	while (data[i] != '\0' && data[i] != '{' && i < 2048) {
		i++;
	}

	if (i >= 2048 || data[i] == '\0') return;

	for (t=0; t<(2048-i); t++) {
		data[t]=data[t+i];
	}

	i=0;
    for (t=0; t<2048; t++) {
    	if (data[t]=='}') {
    		i=t;
    	}
    }

    if (i > 0) {
    	data[i+1]=0;
    }
}

///////////////////////////////////////////////////////////////////////
// WIFI RELATED functions
///////////////////////////////////////////////////////////////////////

void WIFI_RESET(void){
	unsigned int ct;
	HAL_GPIO_WritePin(ESP8266_RESET_GPIO_Port, ESP8266_RESET_Pin, GPIO_PIN_RESET);
	for (ct=0; ct<1000000; ct++);
	HAL_UART_Init(&huart1);
	for (ct=0; ct<2048; ct++) buff_recv[ct]=0;
	HAL_UART_Receive_DMA(&huart1, buff_recv,2048);

	HAL_GPIO_WritePin(ESP8266_RESET_GPIO_Port, ESP8266_RESET_Pin, GPIO_PIN_SET);
	vTaskDelay(1000/portTICK_RATE_MS );
   	HAL_UART_DMAStop(&huart1);
}

void WIFI_Boot_TEST(void)
{
	unsigned int ct;

 	bprintf("Reseting...\r\n");
 	WIFI_RESET();
 	bprintf("Initializing...\r\n");
 	HAL_UART_Init(&huart1);

 	for (ct=0; ct<2048; ct++) buff_recv[ct]=0;
 	HAL_UART_Receive_DMA(&huart1, buff_recv,2048);
 	HAL_UART_Transmit(&huart1, ( unsigned char *)"AT\r\n",strlen("AT\r\n"),10000);

	vTaskDelay(100/portTICK_RATE_MS );
 	HAL_UART_DMAStop(&huart1);

	bprintf("1: %s",buff_recv);
}

void WIFI_Boot(void)
{
	unsigned int ct;

 	bprintf("Reseting...\r\n");
 	WIFI_RESET();
 	bprintf("Initializing...\r\n");
 	HAL_UART_Init(&huart1);

	// Current Wi-Fi Connection Settings
	for (ct=0; ct<2048; ct++) buff_recv[ct]=0;
	HAL_UART_Receive_DMA(&huart1, buff_recv,2048);
	HAL_UART_Transmit(&huart1, (unsigned char *)"AT+CWJAP_CUR=\"routerSEU\",\"00000000\"\r\n", strlen("AT+CWJAP_CUR=\"routerSEU-CcHX\",\"00000000\"\r\n"), 10000);

	vTaskDelay(500/portTICK_RATE_MS );
	HAL_UART_DMAStop(&huart1);

	// Put ESP in Station Mode (1)
	for (ct=0; ct<2048; ct++) buff_recv[ct]=0;
	HAL_UART_Receive_DMA(&huart1, buff_recv,2048);
	HAL_UART_Transmit(&huart1, ( unsigned char *) "AT+CWMODE=1\r\n",strlen("AT+CWMODE=1\r\n"),100000);
	vTaskDelay(200/portTICK_RATE_MS );
	HAL_UART_DMAStop(&huart1);

	// Connect to Access Point permanently
	for (ct=0; ct<2048; ct++) buff_recv[ct]=0;
	HAL_UART_Receive_DMA(&huart1, buff_recv,2048);

	bprintf("sending request\n");
	HAL_UART_Transmit(&huart1, (unsigned char *)"AT+CWJAP=\"routerSEU\",\"00000000\"\r\n", strlen("AT+CWJAP=\"routerSEU\",\"00000000\"\r\n"), 10000);

	vTaskDelay(5000/portTICK_RATE_MS );
	HAL_UART_DMAStop(&huart1);

	bprintf("Initialized: %s \r\n",buff_recv);
}

int ESP_TimeOut_tworesponses(TickType_t timeout,char *src,char * resp,char * resp2, char *msg1,char * msg){
	TickType_t localtimeout=xTaskGetTickCount();

	while (((strstr(src,resp)==NULL) && (strstr(src,resp2)==NULL)) && ((xTaskGetTickCount()-localtimeout)<(timeout/portTICK_RATE_MS)))
		{};

	if ((xTaskGetTickCount()-localtimeout)>=(timeout/portTICK_RATE_MS)){
		 bprintf("%s: %s\r\n", msg1, "TIMEOUT2");
		 bprintf("***********\r\n%s:\r\n*******", msg );
  		 return 1;
	}
	else return 0;
}

int ESP_TimeOut(TickType_t timeout,char *src,char * resp, char *msg1,char * msg){
	TickType_t localtimeout=xTaskGetTickCount();

	while ((strstr(src,resp)==NULL)&&((xTaskGetTickCount()-localtimeout)<(timeout/portTICK_RATE_MS)))
		{};

	if ((xTaskGetTickCount()-localtimeout)>=(timeout/portTICK_RATE_MS)){
		 bprintf("%s: %s\r\n", msg1, "TIMEOUT");
		 bprintf("***********\r\n%s:\r\n*******", msg );
  		 return 1;
	}
	else return 0;
}

uint8_t * ESP_Send_Request(uint8_t * dst_address, uint32_t dst_port, uint8_t * request){
    int ct;
    int st;
    int lc;

    st=1;

    while(st!=5){
    switch (st){
    				case 1: // TCP Connection
    						for (ct=0; ct<2048; ct++) buff_recv[ct]=0;
    						HAL_UART_Receive_DMA(&huart1, buff_recv,2048);
    						sprintf(( char *)aux_buff_WIFI,"AT+CIPSTART=\"TCP\",\"%s\",%d\r\n",dst_address,(int)dst_port);
    						HAL_UART_Transmit(&huart1, ( unsigned char *) aux_buff_WIFI,strlen((const char *)aux_buff_WIFI),10000);

    						// 5000ms timeout to handle network latency gracefully
    						if (ESP_TimeOut(5000,buff_recv,"CONNECT\r\n", "CONNECT",buff_recv)) {
    							HAL_UART_DMAStop(&huart1);
    							return NULL; // Force abort if connection cannot open
    						}
    						HAL_UART_DMAStop(&huart1);
    						st=2;
    						break;

    				case 2: // Data length setup
    						lc=strlen((const char *)request);
    						sprintf((char *)aux_buff_WIFI,"AT+CIPSEND=%d\r\n",lc);
    						for (ct=0; ct<2048; ct++) buff_recv[ct]=0;
    						HAL_UART_Receive_DMA(&huart1, buff_recv,2048);
    						HAL_UART_Transmit(&huart1,( const uint8_t *)aux_buff_WIFI,strlen((const char *)aux_buff_WIFI),10000);
    						vTaskDelay(10/portTICK_RATE_MS );
    						if (ESP_TimeOut(2000,buff_recv,">", "SEND",buff_recv)) {
    							HAL_UART_DMAStop(&huart1);
    							st=4; // Jump to secure closure if command fails
    							break;
    						}
    						HAL_UART_DMAStop(&huart1);
    						st=3;
    						break;

    				case 3: // Send HTTP payload
    						for (ct=0; ct<2048; ct++) buff_WIFI_response[ct]=0;
    						HAL_UART_Receive_DMA(&huart1, buff_WIFI_response,2048);
    						HAL_UART_Transmit(&huart1, request,strlen( (const char *)request),10000);

    						// Wait for response markers from server
    						if (ESP_TimeOut_tworesponses(5000,buff_WIFI_response, "reasonPhrase","\"Europe/Madrid\"","SEND2",buff_WIFI_response)) {
    							HAL_UART_DMAStop(&huart1);
    							st=4;
    							break;
    						}
    						vTaskDelay(200/portTICK_RATE_MS );
    						HAL_UART_DMAStop(&huart1);
    						st=4;
    						break;

    				case 4: // Close TCP Socket safely
    						for (ct=0; ct<2048; ct++) buff_recv[ct]=0;
    						HAL_UART_Receive_DMA(&huart1, buff_recv,2048);
    						HAL_UART_Transmit(&huart1, ( unsigned char *) "AT+CIPCLOSE\r\n",strlen("AT+CIPCLOSE\r\n"),10000);

    						// 500ms delay to ensure ESP handles close operations before next round
    						vTaskDelay(500/portTICK_RATE_MS );
    						HAL_UART_DMAStop(&huart1);
    						st=5;
    						break;

    				case 6:
    						bprintf("WIFI error\r\n");
    						st=4;
    						break;
    }
    }
	return buff_WIFI_response;
}

