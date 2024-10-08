#include"send_log.h"
#include <stdio.h>
#include <stdint.h>
#include "Update_Driver.h"
#include <unistd.h>
#include <sys/stat.h>
#include "CRC_16_CCITT.h"
#include <string.h>
#include <stdlib.h>
#include "change_slot.h"
#include<pthread.h>

static const char *log_file_path = "/userdata/alog.7z";
static const char *log_file_name = "alog.7z";
// static const char *log_file_path = "/userdata/log.tar.bz2";
// static const char *log_file_name = "log.tar.bz2";


#define first_frame_len 	27
#define log_name_len 		7


static int init_log_files(uint32_t *log_size);
static void fill_first_frame(uint32_t log_size, uint8_t *log_first_frame);
static void fill_pack_frame(uint8_t SN_num, uint32_t pack_num, int bytes_Read,uint8_t *data, uint8_t *rd_log);
static void fill_empty_frame(uint8_t SN_num, uint32_t pack_num,uint8_t *data);



extern int recieve_flag;
extern int spi_fd;

//wkl add
/**
 * @description:  延迟10ms 操作gpio 等待spi控制器读数据配置完
 * @param {void*} arg
 * @return {*}
 */
void* gpio_right_task() {
    
    // 延时操作
    usleep(30*1000); //延时30ms
	//akst_debug("do right ack\n");
	Right_Gpio_Ack();

    return NULL;
}
void* gpio_error_task() {
    
    // 延时操作
    usleep(30*1000); //延时30ms
	//akst_debug("do right ack\n");
	Error_Gpio_Ack();

    return NULL;
}
/**
 * @description:  起一个操作gpio的ack的任务
 * @param {void*} arg
 * @return {*} -1 failed 0 success
 */
pthread_t thread;
int start_gpio_task(int bool)
{
	
	// 创建线程
	if(bool){
			if (pthread_create(&thread, NULL, gpio_right_task, NULL) != 0) {
				akst_debug("Failed to create gpio task \n");
				return -1;
			}else {
				akst_debug("Right_Gpio_Ack\n");
			}
	}else {
		    if (pthread_create(&thread, NULL, gpio_error_task, NULL) != 0) {
        	akst_debug("Failed to create gpio task \n");
        	return -1;
    		}else {
				//akst_debug("Error_Gpio_Ack\n");
			}
	}

	return 0;
}

int spi_send_log(void){
	uint8_t read_buf_2[10], read_buf[256], rd_log_buff[498], send_buffer[512], SN_num ,persent, persent_last = 0;;
	int  ret, bytes_Read;
	uint32_t log_size, pack_num,err_num=0, send_size = 0; 
	uint8_t log_first_frame[first_frame_len], last_empty_frame = 0;

	time_debug("get send log request.\n");
	start_gpio_task(1); 
	write_0x18_1_byte(BEGIN_PACK_LOG);	

	ret = init_log_files(&log_size);
	if(ret){
		write_0x18_1_byte(LOG_PACK_FAILED);
		system("rm -r /userdata/alog.7z");
		return -1;
	}

	FILE* file = fopen(log_file_path, "rb"); 
    if(file == NULL) {
		akst_debug("cannot open file '%s'。\n", log_file_path);
		write_0x18_1_byte(LOG_PACK_FAILED);
		fclose(file);
		system("rm -r /userdata/alog.7z");
		return -1;
    }

	//发送第一帧log文件信息
	fill_first_frame(log_size,log_first_frame);
	frame_print("send first frame log msg:",log_first_frame,first_frame_len);
	write_0x18_1_byte(LOG_PACK_OK);	
	for(int wait_ok_counter = 0; wait_ok_counter < 3; wait_ok_counter++){
		time_debug("log:first write [%d] \n", wait_ok_counter);
		get_time_stamp();
		//start_gpio_task(1);
		write(spi_fd, log_first_frame, first_frame_len);
		usleep(10*1000);
		ret = log_read_first_frame(spi_fd,10,read_buf_2);
		if(ret){
			system("rm -r /userdata/alog.7z");
			return -1;
		}
		if(read_buf_2[4] == 0){
			time_debug("ivi not recieve first log frame, resend\n");
			write_0x18_1_byte(BEGIN_PACK_LOG);	
			usleep(350*1000);									//中控200ms检测1次, 400ms之后会重新读, write和中控的read对齐
			write_0x18_1_byte(LOG_PACK_OK);	
			continue;
		}else{
			break;	
		}
	}

	akst_debug("begin send log data.\n");
	SN_num = 2;
	pack_num = 1;
	akst_debug("log_size:%d\n",log_size);
	while (((bytes_Read = fread(rd_log_buff, sizeof(unsigned char), sizeof(rd_log_buff), file)) > 0 || last_empty_frame)  && recieve_flag) {
		fill_pack_frame(SN_num, pack_num, bytes_Read, send_buffer, rd_log_buff);
		akst_debug("\nSN:%d\t",SN_num);
		akst_debug("bytes_Read:%d\n",bytes_Read);
		if(last_empty_frame){
			akst_debug("send empty frame.\n");
			fill_empty_frame(SN_num, pack_num,send_buffer);	
		}
		while(recieve_flag){
			//clean_SPI_buffer();
			//get_time_stamp();

			start_gpio_task(1);
			if(bytes_Read!=498){
				akst_debug("last frame1\n");
			}
			ret=write(spi_fd, send_buffer, 512);

			if(!ret){
				frame_print("send_buf:",send_buffer,bytes_Read+14);
			}
			if(bytes_Read!=498){
				akst_debug("last frame2\n");
			}

			//usleep(50*1000);
			if(spi_read_frame(spi_fd,10,read_buf)){
				recieve_flag = 0;
				akst_debug("spi_read_frame err\n");
				break;
			}
			if(bytes_Read!=498){
				akst_debug("last frame3\n");
			}
			if(read_buf[6] == SN_num&&read_buf[5]==read_buf[6]){
				if(read_buf[7] == 0){
					if(1 == last_empty_frame){
						last_empty_frame = 0;
					}
					break;
				}else{
					err_num++;
					continue;	
				}
			}else{
				continue;
			}
			
		}

		send_size += bytes_Read;
		persent =  (uint8_t)((uint64_t)send_size*100/log_size);
		if(persent != persent_last){
			akst_debug("send_percent[%d%%],err_num[%d]\n",persent,err_num);
			
		}
		akst_debug("send_size:%d\n",send_size);
		if(send_size== log_size&& bytes_Read == 498){
			last_empty_frame = 1;
			akst_debug("last_empty_frame:%d",last_empty_frame);
			akst_debug("last frame 498, need send 1 empty frame. %s┌( `_ゝ` )┐%s\n",LEFT_RED,RIGHT);
		}
		
		persent_last = persent;
		SN_num++;
		pack_num++;
    }

	if(recieve_flag)
		akst_debug("%slog send ok, quit.%s\n",LEFT_GREEN,RIGHT);
	else
		akst_debug("%slog send failed, quit.%s\n",LEFT_RED,RIGHT);

	fclose(file);
	system("rm -r /userdata/alog.7z");
	system("sync");
	recieve_flag = 0;
	clear_Gpio();
	
	usleep(100*1000);
	
	return 0;
}

// 初始化log文件
static int init_log_files(uint32_t *log_size){
	int ret = 0;
	struct stat st;
	system("rm -r /userdata/alog.7z");
	system("rm -r /userdata/alog.7z");
	ret |= system("7zr a /userdata/alog.7z /userdata/logs/history/");
	//ret |= system("gzip /userdata/alog.7z");

	if(ret){
		akst_debug("%spack failed.%s\n",LEFT_RED,RIGHT);
		return ret;
	}
	akst_debug("\n%spack success.%s\n",LEFT_GREEN,RIGHT);

	if (stat(log_file_path, &st) == 0) {
		*log_size = st.st_size;
		akst_debug("log size:0x%06x (%.2f MB) \n\n", *log_size,(float)*log_size/1024/1024);
    }else {
        akst_debug("无法获取文件大小\n");
		return -1;
    }

	return 0;
}

// 填充第一帧
static void fill_first_frame(uint32_t log_size, uint8_t *log_first_frame){
	const uint8_t data[9] = {0x55,0xAA,0x18,0x8b,0x01,0x01,0x00,0x00,0x00};
	memcpy(log_first_frame,data,9);
	log_first_frame[9] = (uint8_t)((log_size>>24)&0xFF);
	log_first_frame[10] = (uint8_t)((log_size>>16)&0xFF);
	log_first_frame[11] = (uint8_t)((log_size>>8)&0xFF);
	log_first_frame[12] = (uint8_t)(log_size&0xFF);
	log_first_frame[13] = log_name_len;
	memcpy(log_first_frame+14,log_file_name,log_name_len);
	uint16_t crc_value = CRC16_CCITT(&log_first_frame[2],first_frame_len-4);
	log_first_frame[first_frame_len-2] = (uint8_t)((crc_value>>8)&0xFF);
	log_first_frame[first_frame_len-1] = (uint8_t)(crc_value&0xFF);
}

// 填充数据帧
static void fill_pack_frame(uint8_t SN_num, uint32_t pack_num, int bytes_Read,uint8_t *data, uint8_t *rd_log){
	const uint8_t set[5] = {0x55,0xAA,0x00,0x8B,0x01};
	memcpy(data,set,5);
	data[5] = SN_num;
	data[6] = (uint8_t)(pack_num>>16);	
	data[7] = (uint8_t)(pack_num>>8);	
	data[8] = (uint8_t)(pack_num&0xFF);		
	data[9] = (uint8_t)(bytes_Read>>16);	
	data[10] = (uint8_t)(bytes_Read>>8);	
	data[11] = (uint8_t)(bytes_Read);
	memcpy(data+12,rd_log,bytes_Read);
	uint16_t crc_value = CRC16_CCITT(&data[2],508);
	data[510] = (uint8_t)((crc_value>>8)&0xFF);
	data[511] = (uint8_t)(crc_value&0xFF);
}

// 填充空帧
static void fill_empty_frame(uint8_t SN_num, uint32_t pack_num,uint8_t *data){
	const uint8_t set[5] = {0x55,0xAA,0x00,0x8B,0x01};
	memcpy(data,set,5);
	data[5] = SN_num;
	data[6] = (uint8_t)(pack_num>>16);	
	data[7] = (uint8_t)(pack_num>>8);	
	data[8] = (uint8_t)(pack_num&0xFF);		
	memset(data+12-3,0,498+3);
	uint16_t crc_value = CRC16_CCITT(&data[2],508);
	data[510] = (uint8_t)((crc_value>>8)&0xFF);
	data[511] = (uint8_t)(crc_value&0xFF);
}

#if 0
#define BUFFER_SIZE 100
int check_write_protect(){
    const char *command = "echo emmc_check_wp > /proc/mmc_debug";
	char *slot_str;
    char buffer[BUFFER_SIZE];
    char* target;
    int found = 0;

    FILE* output = popen(command, "r");
    if (output == NULL) {
        akst_debug("%scheck write protect error, popen error.%s\n",LEFT_RED,RIGHT);
        return -1;
    }

	int cur_slot = api_getCurslot();
	if(0 == cur_slot){
		slot_str = "lk_a";
		akst_debug("search %s.\n",slot_str);
	}else if(1 == cur_slot){
		slot_str = "lk_b";
		akst_debug("search %s.\n",slot_str);
	}else{
		akst_debug("%scheck write protect error, check slot error.%s\n",LEFT_RED,RIGHT);
		return -1;
	}

    while (fgets(buffer, BUFFER_SIZE, output) != NULL) {
		akst_debug("%s",buffer);
        if ((target = strstr(buffer, slot_str)) != NULL) {
            strncpy(buffer, target + 2, 10);
            buffer[10] = '\0';
            akst_debug("结果%s\n", buffer);
            found = 1;
            break;
        }
    }

    pclose(output);

    if (!found) {
        akst_debug("111未找到目标字符串\n");
        // return -1;
    }
	while(1);

    return 0;
}
#endif

// 调试看时序
void debug_err_ack(int time){
	Right_Gpio_Ack();
	usleep(time);
	Right_Gpio_Ack();
}