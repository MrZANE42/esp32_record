/* Standard includes. */
#include "string.h"
#include "esp_err.h"
/* lwIP core includes */
#include "lwip/opt.h"
#include "lwip/sockets.h"
#include <netinet/in.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
/* Utils includes. */
#include "esp_log.h"
#include "event.h"
#include "cJSON.h"
#include <dirent.h>
#include "hal_i2s.h"
#include "audio.h"
#include "appnvs.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

#define TAG "nvs_info"
#define STORAGE_NAMESPACE "storage"

NvsInfoTypeDef system_info;
esp_err_t nvs_get(){
	nvs_handle my_handle;
    esp_err_t err;
    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;

    // Read run time blob
    size_t required_size = 0;  // value will default to 0, if not set yet in NVS
    // obtain required memory space to store blob being read from NVS
    err = nvs_get_blob(my_handle, "run_time", NULL, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
    ESP_LOGI(TAG,"GET NVS INFO");
    if (required_size == 0) {
        printf("Nothing saved yet!\n");
    } else {
        uint32_t* run_time = malloc(required_size);
        err = nvs_get_blob(my_handle, "run_time", run_time, &required_size);
        if (err != ESP_OK) return err;
       	memcpy(&system_info,run_time,required_size);
       	ESP_LOGI(TAG,"system info:wifi mode:%d,ssid:%s,password:%s,old_name:%d,new_name:%d",\
       		system_info.mode,system_info.ssid,\
       		system_info.password,system_info.sd_first_name,system_info.sd_now_name);
        free(run_time);
    }
    // Close
    nvs_close(my_handle);
    return ESP_OK;
}
void nvs_write_task(){
	nvs_handle my_handle;
    esp_err_t err;
    size_t required_size;
	while(1){
	    // Open
	    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
	    if (err != ESP_OK) return err;

	    // Write value including previously saved blob if available
	    required_size =sizeof(system_info);
	    err = nvs_set_blob(my_handle, "run_time", &system_info, required_size);
	    if (err != ESP_OK) return err;
	    // Commit
	    err = nvs_commit(my_handle);
	    if (err != ESP_OK) return err;
	    // Close
	    nvs_close(my_handle);
	    vTaskDelay(10000 / portTICK_PERIOD_MS);
	    ESP_LOGI(TAG,"write info to nvs");
	}
}