#ifndef MAIN_H
#define MAIN_H

#include "record.h"

typedef struct 
{
	uint8_t battery;
	uint32_t sd_cap;
	AudioStateTypeDef* audio_s;
	uint8_t mode;
}BoardTypeDef;

extern BoardTypeDef board;
extern uint8_t sd;




#endif