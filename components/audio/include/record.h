#ifndef _RECORD_H
#define _RECORD_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"


#define RECORD_EVENT BIT0
#define STREAM_EVENT BIT1

extern EventGroupHandle_t record_event_group;



typedef struct 
{
	uint8_t stream;
	uint8_t record;
}AudioStateTypeDef;
extern AudioStateTypeDef audio_state;
void record_task();


#endif