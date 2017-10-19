#ifndef APP_NVS_H
#define APP_NVS_H



#include "esp_err.h"
#include "freertos/FreeRTOS.h"

typedef struct 
{
	uint8_t mode;  //0:sta 1:ap
	char sta_ssid[32];      /**< SSID of target STA*/
    char sta_pw[64];
    char ap_ssid[32];      /**< SSID of target AP*/
    char ap_pw[64];
	uint32_t sd_first_name;//the oldest wav name
	uint32_t sd_now_name;//the newest wav name
}NvsInfoTypeDef;


extern NvsInfoTypeDef system_info;
esp_err_t nvs_write();
esp_err_t nvs_get();





#endif