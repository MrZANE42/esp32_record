#ifndef APP_NVS_H
#define APP_NVS_H



#include "esp_err.h"
#include "freertos/FreeRTOS.h"

typedef struct 
{
	uint8_t mode;  //0:sta 1:ap
	char ssid[50];    //sta or ap ssid
	char password[50];//sta or ap password
	uint32_t sd_first_name;//the oldest wav name
	uint32_t sd_now_name;//the newest wav name
}NvsInfoTypeDef;


extern NvsInfoTypeDef system_info;
void nvs_write_task();
esp_err_t nvs_get();





#endif