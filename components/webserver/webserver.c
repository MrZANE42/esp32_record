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
#include "http.h"
#include "webserver.h"
#include "cJSON.h"
#include <dirent.h>
#include <sys/stat.h>  
#include <unistd.h>
#include "record.h"
#include "main.h"
#include "appnvs.h"
#include "esp_system.h"

#define TAG "webserver:"


#define GPIO_OUTPUT_IO_0    16
#define GPIO_OUTPUT_PIN_SEL  ((1<<GPIO_OUTPUT_IO_0))


static uint32_t http_url_length=0;
static uint32_t http_body_length=0;

static char* http_body=NULL;
static char* http_url=NULL;

static int32_t socket_fd, client_fd;
#define RES_HEAD "HTTP/1.1 200 OK\r\nContent-type: %s\r\nTransfer-Encoding: chunked\r\n\r\n"
static char chunk_len[15];

static int swrite(int32_t fd,char* data,uint32_t len);


const static char not_find_page[] = "<!DOCTYPE html>"
      "<html>\n"
      "<head>\n"
      "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
      "	 <link href=\"https://cdn.bootcss.com/bootstrap/4.0.0-alpha.6/css/bootstrap.min.css\" rel=\"stylesheet\">\n"
      "  <script src=\"https://cdn.bootcss.com/bootstrap/4.0.0-alpha.6/js/bootstrap.min.js\"></script>\n"
      "<title>WhyEngineer-ESP32</title>\n"
      "</head>\n"
      "<body>\n"
      "<h1 class=\"container\">Page not Found!</h1>\n"
      "</body>\n"
      "</html>\n";

static int header_value_callback(http_parser* a, const char *at, size_t length){
	// for(int i=0;i<length;i++)
	// 	putchar(at[i]);
	return 0;
}
static int body_callback(http_parser* a, const char *at, size_t length){
    // static uint32_t cnt=0;
    // printf("cnt:%d\n",cnt++);
    //spiRamFifoWrite(at,length);
    http_body=realloc(http_body,http_body_length+length);
    memcpy(http_body+http_body_length,at,length);
    http_body_length+=length;
  //   for(int i=0;i<length;i++)
		// putchar(at[i]);
    return 0;
}
static int url_callback(http_parser* a, const char *at, size_t length){
    // static uint32_t cnt=0;
    // printf("cnt:%d\n",cnt++);
    //spiRamFifoWrite(at,length);
    http_url=realloc(http_url,http_url_length+length);
    memcpy(http_url+http_url_length,at,length);
    http_url_length+=length;
  //   for(int i=0;i<length;i++)
		// putchar(at[i]);
    return 0;
}
static void chunk_end(int socket){
	swrite(socket, "0\r\n\r\n", 7);
}

typedef struct
{
  char* url;
  void(*handle)(http_parser* a,char*url,char* body);
}HttpHandleTypeDef;

void web_index(http_parser* a,char*url,char* body);
void led_ctrl(http_parser* a,char*url,char* body);
void load_logo(http_parser* a,char*url,char* body);
void load_esp32(http_parser* a,char*url,char* body);
void rest_readdir(http_parser* a,char*url,char* body);
void rest_readwav(http_parser* a,char*url,char* body);
void record_control(http_parser* a,char*url,char* body);
void rm_file(http_parser* a,char*url,char* body);
void read_board(http_parser* a,char*url,char* body);
void set_wifi(http_parser* a,char*url,char* body);


static void not_find();
const HttpHandleTypeDef http_handle[]={
	{"/",web_index},
	{"/api/led/",led_ctrl},
	{"/static/logo.png",load_logo},
	{"/static/esp32.png",load_esp32},
  {"/api/readdir/",rest_readdir},
  {"/api/readwav/",rest_readwav},
  {"/api/setrecord/",record_control},
  {"/api/rmfile/",rm_file,},
  {"/api/readboard/",read_board},
  {"/api/setwifi/",set_wifi},
};
static TimerHandle_t swrite_timeout_timer;
static int swrite(int32_t fd,char* data,uint32_t len){
  xTimerStart(swrite_timeout_timer,0);
  int wl=write(fd,data,len);
  xTimerStop(swrite_timeout_timer,0);
  return wl;
}
static void return_file(char* filename){
	uint32_t r;
	char* read_buf=malloc(1024);
  	FILE* f = fopen(filename, "r");
  	if(f==NULL){
  		ESP_LOGE(TAG,"not find the file");
  		return;
  	}
    int wl;
  	while(1){
    	r=fread(read_buf,1,1024,f);
    	if(r>0){
    		sprintf(chunk_len,"%x\r\n",r);
    		wl=swrite(client_fd, chunk_len, strlen(chunk_len));
        if(wl!=strlen(chunk_len))
          return;
	    	//printf("%s",dst_buf);
	    	wl=swrite(client_fd, read_buf, r);
        if(wl!=r)
          return;
	    	wl=swrite(client_fd, "\r\n", 2);
        if(wl!=2)
          return;
    	}else
    		break;
    }
    fclose(f);
  	chunk_end(client_fd);
}
static void not_find(){
	char *request;
  	asprintf(&request,RES_HEAD,"text/html");//html
  	swrite(client_fd, request, strlen(request));
  	free(request);
  	sprintf(chunk_len,"%x\r\n",strlen(not_find_page));
  	swrite(client_fd, chunk_len, strlen(chunk_len));
  	swrite(client_fd, not_find_page, strlen(not_find_page));
  	swrite(client_fd,"\r\n",2);
  	chunk_end(client_fd);
}
void load_logo(http_parser* a,char*url,char* body){
	char *request;
  	asprintf(&request,RES_HEAD,"image/png");//html
  	swrite(client_fd, request, strlen(request));
  	free(request);
  	return_file("/sdcard/www/static/logo.png");
}
void load_esp32(http_parser* a,char*url,char* body){
	char *request;
  	asprintf(&request,RES_HEAD,"image/png");//html
  	swrite(client_fd, request, strlen(request));
  	free(request);
  	return_file("/sdcard/www/static/esp32.png");
}
void web_index(http_parser* a,char*url,char* body){
	char *request;
  	asprintf(&request,RES_HEAD,"text/html");//html
  	swrite(client_fd, request, strlen(request));
  	free(request);
  	return_file("/sdcard/www/index.html");
}
unsigned long get_file_size(const char *path)  
{  
    unsigned long filesize = -1;  
    FILE *fp;  
    fp = fopen(path, "r");  
    if(fp == NULL)  
        return filesize;  
    fseek(fp, 0L, SEEK_END);  
    filesize = ftell(fp);  
    fclose(fp);  
    return filesize; 
}  
void rest_readdir(http_parser* a,char*url,char* body){
    char *request;
    asprintf(&request,RES_HEAD,"application/json");//json
    swrite(client_fd, request, strlen(request));
    free(request);
    if(sd==1){
      cJSON *root = cJSON_CreateArray();
      cJSON *prev=NULL;
      cJSON *item=NULL;
      int i=0;
      struct dirent *pDirEntry = NULL; 
      DIR          *pDir      = NULL;
      unsigned long size=0;
      pDir=opendir("/sdcard/");
      if(pDir==NULL){
          ESP_LOGE(TAG,"Opendir Failed");
          //xEventGroupWaitBits(eth_event_group,ETH_DISCONNECTED_BIT,pdTRUE,pdTRUE,portMAX_DELAY);
      }else{
          do{
              item=cJSON_CreateObject();
              pDirEntry = readdir(pDir);
              if(pDirEntry==NULL)
                  break;
              //printf("node:%d\ttype:%d\tfilename:%s\n",pDirEntry->d_ino,,pDirEntry->d_name);
              cJSON_AddStringToObject(item,"filename",pDirEntry->d_name);
              cJSON_AddNumberToObject(item,"type",pDirEntry->d_type);
              if(pDirEntry->d_type==1){//filename
                char *path = malloc(strlen("/sdcard/")+strlen(pDirEntry->d_name)+1); 
                strcpy(path, "/sdcard/");
                strcat(path,pDirEntry->d_name);
                ESP_LOGI(TAG,"file path:%s",path);
                size=get_file_size(path);
                free(path);
                cJSON_AddNumberToObject(item,"size",size);
              }else{
                cJSON_AddNumberToObject(item,"size",0);
              }
              //stat();
              if (i==0){
                  root->child = item;
              }else{
                  prev->next = item;
                  item->prev = prev;
              }
              prev =item;
              i++;

          }while(1);
          closedir(pDir);
      }
      char* out = cJSON_PrintUnformatted(root);
      sprintf(chunk_len,"%x\r\n",strlen(out));
      swrite(client_fd, chunk_len, strlen(chunk_len));
      swrite(client_fd, out, strlen(out));
      free(out);
      swrite(client_fd,"\r\n",2);
      chunk_end(client_fd);
      //send(client,out,strlen(out),MSG_WAITALL);
      printf("handle_return: %s\n", out);
      cJSON_Delete(root);
    }else
      chunk_end(client_fd);
}
void read_board(http_parser* a,char*url,char* body){
    char *request;
    asprintf(&request,RES_HEAD,"application/json");//json
    swrite(client_fd, request, strlen(request));
    free(request);
    cJSON *root=NULL;
    root=cJSON_CreateObject();
    if(root==NULL){
      ESP_LOGI(TAG,"cjson root create failed\n");
      return NULL;
    }
    // struct statfs diskInfo;  
    // statfs("/home/carl/", &diskInfo);  
    cJSON_AddNumberToObject(root,"battery",board.battery);
    cJSON_AddNumberToObject(root,"mode",board.mode);
    cJSON_AddNumberToObject(root,"sd_cap",board.sd_cap);
    cJSON_AddNumberToObject(root,"stream",board.audio_s->stream);
    cJSON_AddNumberToObject(root,"record",board.audio_s->record);
    char* out = cJSON_PrintUnformatted(root);
    sprintf(chunk_len,"%x\r\n",strlen(out));
    swrite(client_fd, chunk_len, strlen(chunk_len));
    swrite(client_fd, out, strlen(out));
    free(out);
    swrite(client_fd,"\r\n",2);
    chunk_end(client_fd);
    cJSON_Delete(root);
}
void set_wifi(http_parser* a,char*url,char* body){
    char *request;
    asprintf(&request,RES_HEAD,"application/json");//json
    swrite(client_fd, request, strlen(request));
    free(request);
    cJSON *root=NULL;
    root= cJSON_Parse(http_body);
    //1:sta 0:ap
    uint8_t mode=cJSON_GetObjectItem(root,"mode")->valueint;
    board.mode=mode;
    char* sta_ssid=cJSON_GetObjectItem(root,"sta_ssid")->valuestring;
    char* sta_pw=cJSON_GetObjectItem(root,"sta_pw")->valuestring;
    char* ap_ssid=cJSON_GetObjectItem(root,"ap_ssid")->valuestring;
    char* ap_pw=cJSON_GetObjectItem(root,"ap_pw")->valuestring;
    memcpy(system_info.sta_ssid,sta_ssid,strlen(sta_ssid));
    memcpy(system_info.sta_pw,sta_pw,strlen(sta_pw));
    memcpy(system_info.ap_ssid,ap_ssid,strlen(ap_ssid));
    memcpy(system_info.ap_pw,ap_pw,strlen(ap_pw));
    nvs_write();
    cJSON_Delete(root);
    //gpio_set_level(GPIO_OUTPUT_IO_0,led);
    root=NULL;
    root=cJSON_CreateObject();
    if(root==NULL){
      ESP_LOGI(TAG,"cjson root create failed\n");
      return NULL;
    }
    cJSON_AddNumberToObject(root,"err",0);
    char* out = cJSON_PrintUnformatted(root);
    sprintf(chunk_len,"%x\r\n",strlen(out));
    swrite(client_fd, chunk_len, strlen(chunk_len));
    swrite(client_fd, out, strlen(out));
    free(out);
    swrite(client_fd,"\r\n",2);
    chunk_end(client_fd);
    cJSON_Delete(root);
    if(system_info.mode!=mode){
      system_info.mode=mode;
      nvs_write();
      close(client_fd);
      esp_restart();
    }
}
void rm_file(http_parser* a,char*url,char* body){
    char *request;
    asprintf(&request,RES_HEAD,"application/json");//json
    swrite(client_fd, request, strlen(request));
    free(request);
    cJSON *root=NULL;
    root= cJSON_Parse(http_body);
    char* filename=cJSON_GetObjectItem(root,"filename")->valuestring;
    unlink(filename);
    cJSON_Delete(root);
    //gpio_set_level(GPIO_OUTPUT_IO_0,led);
    root=NULL;
    root=cJSON_CreateObject();
    if(root==NULL){
      ESP_LOGI(TAG,"cjson root create failed\n");
      return NULL;
    }
    cJSON_AddNumberToObject(root,"err",0);
    char* out = cJSON_PrintUnformatted(root);
    sprintf(chunk_len,"%x\r\n",strlen(out));
    swrite(client_fd, chunk_len, strlen(chunk_len));
    swrite(client_fd, out, strlen(out));
    free(out);
    swrite(client_fd,"\r\n",2);
    chunk_end(client_fd);
    cJSON_Delete(root);
}
void rest_readwav(http_parser* a,char*url,char* body){
    char *request;
    asprintf(&request,RES_HEAD,"audio/x-wav");//json
    int wl=swrite(client_fd, request, strlen(request));
    if(wl!=strlen(request))
      return;
    free(request);
    cJSON *root=NULL;
    root= cJSON_Parse(http_body);
    char* name=cJSON_GetObjectItem(root,"filename")->valuestring;
    return_file(name);
    cJSON_Delete(root);
}
void record_control(http_parser* a,char*url,char* body){
    int err=0;
    char *request;
    asprintf(&request,RES_HEAD,"application/json");//json
    swrite(client_fd, request, strlen(request));
    free(request);
    cJSON *root=NULL;
    root= cJSON_Parse(http_body);
    uint8_t record=cJSON_GetObjectItem(root,"record")->valueint;
    cJSON_Delete(root);
    //start record
    if(sd==1){
      if(record)
        xEventGroupSetBits(record_event_group, RECORD_EVENT);
      else
        xEventGroupClearBits(record_event_group, RECORD_EVENT);
    }
    else 
      err=3;
    //gpio_set_level(GPIO_OUTPUT_IO_0,led);
    root=NULL;
    root=cJSON_CreateObject();
    if(root==NULL){
      ESP_LOGI(TAG,"cjson root create failed\n");
      return NULL;
    }
    cJSON_AddNumberToObject(root,"err",err);
    char* out = cJSON_PrintUnformatted(root);
    sprintf(chunk_len,"%x\r\n",strlen(out));
    swrite(client_fd, chunk_len, strlen(chunk_len));
    swrite(client_fd, out, strlen(out));
    free(out);
    swrite(client_fd,"\r\n",2);
    chunk_end(client_fd);
    cJSON_Delete(root);
}
void led_ctrl(http_parser* a,char*url,char* body){
	char *request;
  	asprintf(&request,RES_HEAD,"application/json");//json
  	swrite(client_fd, request, strlen(request));
  	free(request);
  	cJSON *root=NULL;
    root= cJSON_Parse(http_body);
    uint8_t led=cJSON_GetObjectItem(root,"led")->valueint;
    cJSON_Delete(root);
    gpio_set_level(GPIO_OUTPUT_IO_0,led);
  	root=NULL;
	root=cJSON_CreateObject();
	if(root==NULL){
		ESP_LOGI(TAG,"cjson root create failed\n");
		return NULL;
	}
	cJSON_AddNumberToObject(root,"err",0);
	// cJSON_AddStringToObject(root,"cuid","esp32_whyengineer");
	// cJSON_AddStringToObject(root,"token",access_token);
	// cJSON_AddNumberToObject(root, "rate", 8000);
	// cJSON_AddNumberToObject(root, "channel", 1);
	// cJSON_AddNumberToObject(root, "len", len);
	// cJSON_AddStringToObject(root,"speech",speech);
	char* out = cJSON_PrintUnformatted(root);
	sprintf(chunk_len,"%x\r\n",strlen(out));
	swrite(client_fd, chunk_len, strlen(chunk_len));
	swrite(client_fd, out, strlen(out));
  free(out);
  swrite(client_fd,"\r\n",2);
  chunk_end(client_fd);
	//send(client,out,strlen(out),MSG_WAITALL);
	//printf("handle_return: %s\n", out);
	cJSON_Delete(root);
}

static int body_done_callback (http_parser* a){
	http_body=realloc(http_body,http_body_length+1);
    http_body[http_body_length]='\0';
  	ESP_LOGI(TAG,"body:%s",http_body);
  	for(int i=0;i<sizeof(http_handle)/sizeof(http_handle[0]);i++){
  		if(strcmp(http_handle[i].url,http_url)==0){
  			http_handle[i].handle(a,http_url,http_body);
        free(http_url);
        free(http_body);
        http_url=NULL;
        http_body=NULL;
        http_url_length=0;
        http_body_length=0;
  			return 0;
  		}
  	}
  	not_find();
    free(http_url);
    free(http_body);
    http_url=NULL;
    http_body=NULL;
    http_url_length=0;
    http_body_length=0;
  	// char *request;
  	// asprintf(&request,RES_HEAD,HTML);
  	// swrite(client_fd, request, strlen(request));
   //  free(request);
   //  sprintf(chunk_len,"%x\r\n",strlen(not_find_page));
   //  //ESP_LOGI(TAG,"chunk_len:%s,length:%d",chunk_len,strlen(http_index_hml));
   //  swrite(client_fd, chunk_len, strlen(chunk_len));
   //  swrite(client_fd, http_index_hml, strlen(http_index_hml));
   //  swrite(client_fd, "\r\n", 2);
   //  chunk_end(client_fd);
    //close( client_fd );	
    return 0;
}
static int begin_callback (http_parser* a){
  	//ESP_LOGI(TAG,"message begin");
    return 0;
}
static int header_complete_callback(http_parser* a){
	http_url=realloc(http_url,http_url_length+1);
    http_url[http_url_length]='\0';
	ESP_LOGI(TAG,"url:%s",http_url);
    return 0;
}

static http_parser_settings settings =
{   .on_message_begin = begin_callback
    ,.on_header_field = 0
    ,.on_header_value = header_value_callback
    ,.on_url = url_callback
    ,.on_status = 0
    ,.on_body = body_callback
    ,.on_headers_complete = header_complete_callback
    ,.on_message_complete = body_done_callback
    ,.on_chunk_header = 0
    ,.on_chunk_complete = 0
};

/* Dimensions the buffer into which input characters are placed. */

static struct sockaddr_in server, client;


int creat_socket_server(in_port_t in_port, in_addr_t in_addr)
{
	int socket_fd, on;
	//struct timeval timeout = {10,0};

	server.sin_family = AF_INET;
	server.sin_port = in_port;
	server.sin_addr.s_addr = in_addr;

  struct timeval timeout;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;
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
static void swrite_timeout_callback( TimerHandle_t xTimer ){
  ESP_LOGI(TAG,"write timeout!!!!!");
  close(client_fd);
}

static void recv_timeout_callback( TimerHandle_t xTimer ){
  ESP_LOGI(TAG,"read timeout 2!!!!!");
  close(client_fd);
}
void webserver_task( void *pvParameters ){
	int32_t lBytes;
	esp_err_t err;
	ESP_LOGI(TAG,"webserver start");
	uint32_t request_cnt=0;
  TimerHandle_t recv_to=xTimerCreate( "recv_timout",2000,pdFALSE,(void*)0,recv_timeout_callback);
  swrite_timeout_timer=xTimerCreate( "swrite_timout",2000,pdFALSE,(void*)0,swrite_timeout_callback);
	//init gpio
	gpio_config_t io_conf;
  io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
  io_conf.pull_down_en = 0;
  io_conf.pull_up_en = 0;
  gpio_config(&io_conf);
	//init sd card
  if(sd==1){
	  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 10
    };
    sdmmc_card_t* card;
    err = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);
    if (err != ESP_OK) {
        if (err == ESP_FAIL) {
            printf("Failed to mount filesystem. If you want the card to be formatted, set format_if_mount_failed = true.");
        } else {
            printf("Failed to initialize the card (%d). Make sure SD card lines have pull-up resistors in place.", err);
        }
        return;
    }
    sdmmc_card_print_info(stdout, card);
    board.sd_cap=((uint64_t) card->csd.capacity) * card->csd.sector_size / (1024 * 1024);
  }

	(void) pvParameters;
	http_parser parser;
  http_parser_init(&parser, HTTP_REQUEST);
  parser.data = NULL;
  socklen_t client_size=sizeof(client);

	socket_fd = creat_socket_server(htons(80),htonl(INADDR_ANY));
	if( socket_fd >= 0 ){
		/* Obtain the address of the output buffer.  Note there is no mutual
		exclusion on this buffer as it is assumed only one command console
		interface will be used at any one time. */
		char recv_buf[64];
		esp_err_t nparsed = 0;
		/* Ensure the input string starts clear. */
		for(;;){
			client_fd=accept(socket_fd,(struct sockaddr*)&client,&client_size);
			if(client_fd>0L){
				// strcat(outbuf,pcWelcomeMessage);
				// strcat(outbuf,path);
				// lwip_send( lClientFd, outbuf, strlen(outbuf), 0 );
        ESP_LOGI(TAG,"connected");
				do{
          //start a timer callback
          xTimerStart(recv_to,0);
					lBytes = recv( client_fd, recv_buf, sizeof( recv_buf ), 0 );
          xTimerStop(recv_to,0);
          //ESP_LOGI(TAG,"recv len:%d",lBytes);
					if(lBytes==0){
						//close( client_fd );	
            ESP_LOGI(TAG,"socket closed");
						break;
					}
					if(lBytes<0){
            ESP_LOGI(TAG,"timeout 1");
            break;
          }	
					 // for(int i=0;i<lBytes;i++)
						// putchar(recv_buf[i]);
					nparsed = http_parser_execute(&parser, &settings, recv_buf, lBytes);
					
					//while(xReturned!=pdFALSE);
					//lwip_send( lClientFd,path,strlen(path), 0 );
				}while(lBytes==sizeof( recv_buf ) && nparsed >= 0);
						
			}
			ESP_LOGI(TAG,"request_cnt:%d,socket:%d",request_cnt++,client_fd);
			close( client_fd );
		}
	}

}