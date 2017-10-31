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
#include "record.h"
#include "spiram_fifo.h"

#define TAG "audiostream:"

static int32_t socket_fd, client_fd;
static struct sockaddr_in server, client;

static int creat_socket_server(in_port_t in_port, in_addr_t in_addr)
{
  int socket_fd, on;
  //struct timeval timeout = {10,0};

  server.sin_family = AF_INET;
  server.sin_port = in_port;
  server.sin_addr.s_addr = in_addr;

  if((socket_fd = socket(AF_INET, SOCK_STREAM, 0))<0) {
    perror("listen socket uninit\n");
    return -1;
  }
  on=1;
  //setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
  setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int) );
  //CALIB_DEBUG("on %x\n", on);
  if((bind(socket_fd, (struct sockaddr *)&server, sizeof(server)))<0) {
    perror("cannot bind srv socket\n");
    return -1;
  }

  if(listen(socket_fd, 1)<0) {
    perror("cannot listen");
    close(socket_fd);
    return -1;
  }

  return socket_fd;
}
static void audio_swrite_timeout_callback( TimerHandle_t xTimer ){
	xEventGroupClearBits(record_event_group, STREAM_EVENT);
  	ESP_LOGI(TAG,"write timeout!!!!!");
  	close(client_fd);
}
//uint8_t sub_buf[350];
void audiostream_task( void *pvParameters ){
	int32_t lBytes;
	esp_err_t err;
	ESP_LOGI(TAG,"audiostream start");
	uint32_t request_cnt=0;
	(void) pvParameters;
	TimerHandle_t audiostream_tm;
	audiostream_tm=xTimerCreate( "audiostream_tm",1000,pdFALSE,(void*)0,audio_swrite_timeout_callback);
    socklen_t client_size=sizeof(client);
	socket_fd = creat_socket_server(htons(3000),htonl(INADDR_ANY));
	EventBits_t event=0;
	if( socket_fd >= 0 ){
		/* Obtain the address of the output buffer.  Note there is no mutual
		exclusion on this buffer as it is assumed only one command console
		interface will be used at any one time. */
		char* buf=NULL;
		buf=malloc(1024);
		esp_err_t nparsed = 0;
		/* Ensure the input string starts clear. */
		for(;;){
			xEventGroupClearBits(record_event_group, STREAM_EVENT);
			client_fd=accept(socket_fd,(struct sockaddr*)&client,&client_size);
			if(client_fd>0L){
				ESP_LOGI(TAG,"start stream");
			    //xEventGroupClearBits(record_event_group, RECORD_EVENT);
				//vTaskDelay(1000);
				xEventGroupSetBits(record_event_group, STREAM_EVENT);
				// strcat(outbuf,pcWelcomeMessage);
				// strcat(outbuf,path);
				// lwip_send( lClientFd, outbuf, strlen(outbuf), 0 );
				do{
					spiRamFifoRead(buf,1024);
					// for(int i=0;i<1024;i++){
					// 	ESP_LOGI(TAG,"buf:%x",buf[i]);
					// }
					// for(int i=0;i<85;i++){
					// 	memcpy(sub_buf+i*4,buf+i*12,4);
					// }
					xTimerStart(audiostream_tm,0);
					lBytes = write( client_fd,buf,1024);
					xTimerStop(audiostream_tm,0);	
					//while(xReturned!=pdFALSE);
					//lwip_send( lClientFd,path,strlen(path), 0 );
				}while(lBytes > 0);	
			}
			ESP_LOGI(TAG,"request_cnt:%d,socket:%d",request_cnt++,client_fd);
			xEventGroupClearBits(record_event_group, STREAM_EVENT);
			close( client_fd );
			vTaskDelay(1000);
			
			
			//xEventGroupSetBits(record_event_group, event);
		}
	}

}