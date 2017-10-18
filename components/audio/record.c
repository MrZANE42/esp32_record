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
#include "wm8978.h"
#include "spiram_fifo.h"
#include <stdio.h>
#include "appnvs.h"
#include "record.h"

EventGroupHandle_t record_event_group;

#define TAG "record_task"

static int start_wav_file(FILE* f){
	if(f==NULL)
		return -1;
	WAV_HEADER wav_header;
	memcpy(wav_header.rld,"RIFF",4);
    memcpy(wav_header.wld,"WAVE",4);
    memcpy(wav_header.fld,"fmt ",4);
    wav_header.fLen=0x00000010;
    wav_header.wFormatTag=0x0001;
    wav_header.wChannels=0x0002;
    wav_header.nSamplesPersec=48000;
    wav_header.nAvgBitsPerSample=48000*2*2;
    wav_header.wBlockAlign=2*16/8;
    wav_header.wBitsPerSample=16;
    memcpy(wav_header.dld,"data",4);
    int w=fwrite(&wav_header,1,sizeof(wav_header),f);
    return w;
}
static int end_wav_file(FILE*f,uint32_t length){
	if(f==NULL)
		return -1;
	if(length==0)
		return 0;
	uint32_t n=ftell(f);
	ESP_LOGI(TAG,"wav file lenght:%d",n);
	fseek(f, 0, SEEK_SET);
	WAV_HEADER wav_header;
	memcpy(wav_header.rld,"RIFF",4);
    memcpy(wav_header.wld,"WAVE",4);
    memcpy(wav_header.fld,"fmt ",4);
    wav_header.fLen=0x00000010;
    wav_header.wFormatTag=0x0001;
    wav_header.wChannels=0x0002;
    wav_header.nSamplesPersec=48000;
    wav_header.nAvgBitsPerSample=48000*2*2;
    wav_header.wBlockAlign=2*16/8;
    wav_header.wBitsPerSample=16;
    memcpy(wav_header.dld,"data",4);
	wav_header.wSampleLength=n-sizeof(wav_header);
    wav_header.rLen=n-8;
    int w = fwrite(&wav_header, 1, sizeof(wav_header), f);
    fclose(f);
    return w;
}
AudioStateTypeDef audio_state;
void record_task(){
	
	esp_err_t err;
	audio_state.stream=0;
	audio_state.record=0;

	uint8_t i2s_on_off=0;
	uint32_t sd_wl=0;
	char* file_name;
	char* data=NULL;
	char name_index[10];
	FILE *f=NULL;
	EventBits_t event;
	//init queue
	record_event_group=xEventGroupCreate();
	//init codec
	hal_i2c_init(0,5,17);
    WM8978_Init();
    WM8978_ADDA_Cfg(1,1); 
    WM8978_Input_Cfg(1,0,0);     
    WM8978_Output_Cfg(1,0); 
    WM8978_MIC_Gain(35);
    WM8978_AUX_Gain(0);
    WM8978_LINEIN_Gain(0);
    WM8978_SPKvol_Set(0);
    WM8978_HPvol_Set(20,20);
    WM8978_EQ_3D_Dir(1);
    WM8978_EQ1_Set(0,24);
    WM8978_EQ2_Set(0,24);
    WM8978_EQ3_Set(0,24);
    WM8978_EQ4_Set(0,24);
    WM8978_EQ5_Set(0,0);
    spiRamFifoInit();
    //hal_i2s_init(0,48000,16,2);
    while(1){
    	//not record
    	event=xEventGroupWaitBits(record_event_group,RECORD_EVENT|STREAM_EVENT,pdFALSE,pdFALSE,2000);
    	if( ( event & ( RECORD_EVENT | STREAM_EVENT ) ) == ( RECORD_EVENT | STREAM_EVENT ) )
		{
		    /* xEventGroupWaitBits() returned because both bits were set. */
		    ESP_LOGE(TAG,"both mode is not working");
		 //    if(record==0){
			// 	record=1;
			// 	if(i2s_on_off==0){
		 //    		i2s_start(0);
		 //    		i2s_on_off=1;
		 //    	}
			// 	ESP_LOGI(TAG,"start record");
			// }
			// if(stream==0){
			// 	ESP_LOGI(TAG,"start stream");
			// 	if(i2s_on_off==0){
		 //    		i2s_start(0);
		 //    		i2s_on_off=1;
		 //    	}
			// 	stream=1;
			// }
		    // if(record==0&&stream==0){
		    // 	if(i2s_on_off==0){
		    // 		i2s_start(0);
		    // 		i2s_on_off=1;
		    // 	}
		    // 	ESP_LOGI(TAG,"start record and stream!");
		    // 	spiRamFifoReset();
		    // 	ESP_LOGI(TAG,"init fifo,freespace:%d",spiRamFifoFree());
		    // 	record=1;
		    // 	stream=1;
		    // }
		    //hal_i2s_read(0,i2s_buf,1024,portMAX_DELAY);
		}else if( ( event & RECORD_EVENT ) != 0 ){
			i2s_on_off=1;
			if(audio_state.stream==1){
				audio_state.stream=0;
				spiRamFifoReset();
				i2s_stop(0);
				i2s_driver_uninstall(0);
				ESP_LOGI(TAG,"rest fifo,stop stream");
			}
			if(audio_state.record==0){
				audio_state.record=1;
				wm8978_48k();
		    	hal_i2s_init(0,48000,16,2);	
				ESP_LOGI(TAG,"start record");
			}
		}else if( ( event & STREAM_EVENT ) != 0){
			i2s_on_off=1;
			if(audio_state.record==1){
				audio_state.record=0;
				i2s_stop(0);
				i2s_driver_uninstall(0);
				ESP_LOGI(TAG,"stop record,compelete the file");
				err=end_wav_file(f,sd_wl);
				f=NULL;
				sd_wl=0;
				if(err<0)
					ESP_LOGE(TAG,"stop record error:%d",err);
			}
			if(audio_state.stream==0){
				audio_state.stream=1;
				ESP_LOGI(TAG,"start stream");
				wm8978_8k();
		    	hal_i2s_init(0,8000,16,2);				
			}
		}else{
			ESP_LOGI(TAG,"both record and stream are stop");
			if(audio_state.record==1){
				audio_state.record=0;
				ESP_LOGI(TAG,"stop record,compelete the file");
				err=end_wav_file(f,sd_wl);
				f=NULL;
				sd_wl=0;
				if(err<0)
					ESP_LOGE(TAG,"stop record error:%d",err);
			}
			if(audio_state.stream==1){
				audio_state.stream=0;
				ESP_LOGI(TAG,"stop stream");
				spiRamFifoReset();
			}
			if(i2s_on_off==1){
				i2s_on_off=0;
				i2s_stop(0);
				i2s_driver_uninstall(0);
			}
		}

		//work flow
		if(audio_state.record==1||audio_state.stream==1){
			if(data==NULL)
				data=malloc(1024);
			hal_i2s_read(0,data,1024,portMAX_DELAY);
		}
		if(audio_state.record==1){
			if(f==NULL&&sd_wl==0){
				sprintf(name_index,"%d",system_info.sd_now_name);
				file_name=malloc(strlen("/sdcard/WF")+strlen(name_index)+1+4);
				strcpy(file_name,"/sdcard/WF");
				strcat(file_name,name_index);
				strcat(file_name,".wav");
				f = fopen(file_name, "wb");
				ESP_LOGI(TAG,"start write wav file:%s",file_name);
				free(file_name);
				system_info.sd_now_name++;
				start_wav_file(f);
			}
			sd_wl+=fwrite(data,1,1024,f);
			//3M
			if(sd_wl>=3*1024*1024*4){
				err=end_wav_file(f,sd_wl);
				if(err<0)
					ESP_LOGE(TAG,"end loop failed:%d",err);
				ESP_LOGI(TAG,"end loop successful")
				f=NULL;
				sd_wl=0;
			}
		}
		if(audio_state.stream==1){
			//if(spiRamFifoFree()>1024)
			spiRamFifoWrite(data,1024);
		}

    }
    vTaskSuspend(NULL);
}