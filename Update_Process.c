#include <unistd.h>
#include "Update_Process.h"
#include "Update_Driver.h"
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <sys/time.h>
#include <signal.h>
#include "CRC_16_CCITT.h"
#include <pthread.h>
#include "send_log.h"

const char *my_version = "2024.01.25-v3";

const char *recieve_file = "/data/update/m116_update.bin";
const char *recieve_file_zip = "/data/update/m116_update.zip";
const char *update_file_path = "/data/update/";
const char *md5_file_path = "/data/update/save_md5";
static void Update_Process_Task(void);
static int Update_Init_Task(void);
static int begin_update(void);
static int spi_main(int fd);
static int initRecieveFile(void);
static int get_read_len(uint8_t *index,uint16_t *r_len);
static int get_valid_len(uint8_t *data, uint16_t*len);
static int get_recieve_md5(uint8_t *data, char *md5sum);
static int get_pack_num(uint8_t *data, uint32_t *pack_num);
static int get_total_bytes(uint8_t *data, uint64_t *total_len);
static int check_recieve_over(uint32_t pack_num, uint64_t data_len);
static int check_timeout(int len, uint16_t *delay_count, uint16_t *max_wait_times);

static int p_write_task(void *arg);
static int p_spi_monitor(void *arg);
static int p_reg_940_monitor(void *arg);

static int Add_Node(uint8_t *new_data);
static int Read_Node(void);
static int set_step_B(int step);
static int set_step_A(int step);

static int save_md5_file(char *md5sum);
static int read_md5_file(char *md5sum);
static int debug_record_ack(void);
static int check_pack_exist(void);

typedef struct monitor {
	uint8_t step;
	uint8_t counter;
}MONITOR;
MONITOR m_AddNode;
MONITOR m_ReadNode;

typedef struct linknode {
    uint8_t data[512];
    struct linknode* next;
}LinkNode;
LinkNode* Header = NULL;

int recieve_flag;
uint8_t delay_200ms_counter = 0;
FILE *fp_receive = NULL;
pthread_mutex_t link_mutex;
extern uint32_t err_counter;
extern uint32_t continuous_err_counter;
uint8_t recieve_present_last = 0xFF;
char r_md5sum[50] = {0}, cal_md5sum[50] = {0};
uint8_t ack_record[5] = {0};

void signal_handler(int sig) {
    akst_debug("Received signal %d, exiting...\n", sig);
    exit(0);
}

int main(void)
{
	// 注册信号处理函数
    signal(SIGINT, signal_handler);  // 使Ctrl+C可以退出程序
    
	akst_debug("=========== :spi progress Enter ===========\n");
    akst_debug("version:%s%s%s\n",LEFT_BLUE,my_version,RIGHT); 
	
	Update_Process_Task();

	return 0;
}

static void Update_Process_Task(void)
{
	int ret = 0;
	recieve_flag = 0;	

	ret |= Update_Init_Spi();
	ret |= Update_Init_Gpio();
	ret |= Update_Init_I2C();
	ret |= Update_Init_940_REG();
	if(ret){
		akst_debug("%speripheral init error! spi process quit.%s\n",LEFT_RED,RIGHT);
		return;
	}
	Update_Init_Task();

	while(1){
    	spi_main(spi_fd);
	}
}



static int spi_main(int fd)
{
	uint8_t read_buf_2[1024], FN_last = 0xFF, index = 0;
	uint16_t r_len,delay_count, max_wait_times;
	uint32_t pack_num = 0,pack_num_last = 1;
	uint16_t data_len = 0;
	int ack_ret,len = 0, recieve_pack_result = 0;
	

	if(recieve_flag){
		// clear_Gpio();
		// Right_AckSts = 0;
	}else{
		for(int leisure = 0; leisure<5 && !recieve_flag;leisure++){
			usleep(2*1000);
		}
	}

		
	//收取第一条指令
	int max_read_times = 30; 
	while(recieve_flag && --max_read_times){
		if(0 == spi_read_first_frame(fd,10,read_buf_2)){
			if(0x0B == read_buf_2[3]){
				spi_send_log();
				recieve_flag = 0;
				return 0;
			}else if(0x0A == read_buf_2[3] && 0x01 == read_buf_2[4]){
				begin_update();
				recieve_flag = 0;
				return 0;	
			}else if(0x0A == read_buf_2[3] && 0x00 == read_buf_2[4]){
				check_pack_exist();
				continue;
			}else if(0x09 == read_buf_2[3] && 0x00 == read_buf_2[4]){
				break;	
			}
		}	
	}
	if(!max_read_times){
		akst_debug("first frame timeout, quit\n");
		recieve_flag = 0;						//2024.01.17 还是发现会丢包 并且一直再读第一帧 这里跳出		
	}
	if(!recieve_flag || max_read_times == 0){
		return 0;
	}

	time_debug("get update request.\n");
	cal_update_time(0);
	if(initRecieveFile())
		return -1;

	max_wait_times = 0;

	while(recieve_flag){
		for(;index < 18;){
			if(continuous_err_counter > 2){	//检测连续错误
				time_debug("continuous err, continuous_err_counter:%d, err_counter:%d\n",continuous_err_counter, err_counter);
			}

			get_read_len(&index,&r_len);
			clean_SPI_buffer();								//每次收发必须清空一下buffer
			debug_record_ack();
			get_time_stamp();								//保存时间戳,如果发现卡了,可以对比卡了多久 
			len = read(fd, read_buf_2, r_len); 

			// if(check_time_stamp()){
			// 	time_debug("suspected timeout, last timestamp:[%s.%03d]\n",time_buf.datetime,time_buf.milliseconds);
			// 	akst_debug("right ack:%d, err ack:%d\n",Right_AckSts,Error_AckSts);
			// 	akst_debug("recieve:%lu write:%lu\n",data_write.revieve_bytes, data_write.write_bytes);
			// 	debug_err_ack(10*1000);
			// }
			if (len < 0){
            	perror("read data fail:");
				// Error_Gpio_Ack();
				usleep(820);			//延迟一会再接收下一帧, 避免连续错误
				continue;	
			}else if(len != r_len){
				if(check_timeout(len,&delay_count,&max_wait_times)){
					recieve_pack_result = 1;
				}
				
				if(recieve_flag)
					continue;
				else
					break;	
			}else{
				delay_count = 0;
				max_wait_times = 0;	
				if(read_buf_2[0] == 0x55 && read_buf_2[1] == 0xAA){	//有时候驱动出问题校验对, 但是头不对, 所以先判断头	
					ack_ret = crcCheck(&read_buf_2[0],r_len);
				}else{
					ack_ret = 1;
				}
			}

			if(!recieve_flag)break;

			// akst_debug("%02X %02X %02X\n", read_buf_2[6],read_buf_2[7],read_buf_2[8]);
			if(!ack_ret){
				if((index<7 && FN_last != read_buf_2[5]) || index>=7)
					index++;
				if(read_buf_2[3] == 9 && read_buf_2[4] == 0 && read_buf_2[6] == 4)
					get_total_bytes(read_buf_2,&data_write.total_bytes);
				if(read_buf_2[3] == 9 && read_buf_2[4] == 1)
					get_recieve_md5(read_buf_2,r_md5sum);

				if(index < 8){
					FN_last = read_buf_2[5];
					akst_debug("[%d]",index);
					frame_print("",read_buf_2,read_buf_2[2]+3);
				}else{
					get_pack_num(read_buf_2,&pack_num);	
				}
				// akst_debug("#1 pack num :%d last:%d\n", pack_num,pack_num_last);	
				if(index == 11 && pack_num != pack_num_last){	
					// akst_debug("#2 pack num :%d last:%d\n", pack_num,pack_num_last);	
					get_valid_len(read_buf_2,&data_len);			//498字节 升级数据
					data_write.revieve_bytes += data_len;
					Add_Node(read_buf_2);
					recieve_pack_result = check_recieve_over(pack_num, data_len);
					if(recieve_pack_result)	
						break;		
					pack_num_last = pack_num;
				}
				Right_Gpio_Ack();
			}else{
				// akst_debug("len ok, but crc err.\n");
				Error_Gpio_Ack();
				usleep(820);	//延迟一会再读下一帧, 避免连续错误
			}

		}
		if(recieve_pack_result)
			break;
	}
	
	cal_update_time(1);

	int check_pack_ret = 0xff;
	if(2 == recieve_pack_result){
		check_pack_ret = 0;	
	}else{
		check_pack_ret = 1;	
	}
	
	if(0 == check_pack_ret){
		// time_debug("%scheck md5sum ok.%s\n\n",LEFT_GREEN,RIGHT);
		time_debug("recieve pack ok.\n\n");	
		// write_0x18_1_byte(PACK_EXIST);
	}else{
		time_debug("\nrecieve pack failed.\n\n");
		system("rm -r /data/update/m116_update.zip");
		system("sync");
		// write_0x18_1_byte(PACK_NOT_EXIST);		//2024.01.17 升级包状态改为收到请求之后再上报
	}

	// clear_Gpio();
	// recieve_flag = 0;		//传输完毕, 退出流程, 等待发起升级请求			
	return 0;

}

static int Update_Init_Task(void){
	int ret;
	pthread_mutex_init(&link_mutex, NULL);
	set_step_A(0);
	set_step_B(0);

	pthread_t reg_940_monitor;
    ret = pthread_create(&reg_940_monitor,
                        NULL,
                        (void *)p_reg_940_monitor,
                        NULL
                        );
    akst_debug("940 reg 0x18 monitor thread create ret: %d \n",ret);	

	pthread_t write_task;
    ret = pthread_create(&write_task,
                        NULL,
                        (void *)p_write_task,
                        NULL
                        );
    akst_debug("write thread create ret: %d \n",ret);	

#if 1
	pthread_t spi_monitor;
    ret = pthread_create(&spi_monitor,
                        NULL,
                        (void *)p_spi_monitor,
                        NULL
                        );
    akst_debug("spi monitor thread create ret: %d \n",ret);	

#endif

	return 0;
}

//初始化要接受的文件
static int initRecieveFile(void)
{
	system("mkdir -p /data/update");
	// system("rm -r /data/update/m116_update.bin");
	system("rm -r /data/update/m116_update.zip");	
	if ((fp_receive = fopen(recieve_file, "wb")) == NULL) {
		perror("Can not create file: /data/update/m116_update.bin\n");
		return -1;
	}
	system("sync");

	err_counter = 0;
	data_write.node_counter = 0;
	data_write.write_over = 0;
	data_write.total_bytes = 0;
	data_write.revieve_bytes = 0;
	data_write.write_bytes = 0;
	data_write.persent_last = 0;

	memset(r_md5sum,0,50);
	memset(cal_md5sum,0,50);
	return 0;
}

//SPI驱动只能读取固定长度，定好每次读取的长度
static int get_read_len(uint8_t *index,uint16_t *r_len){
	const uint16_t set_len[16] = {10,10,13,14,19,13,40,512,512,512,512,512,512,512,512,512};
	if(*index < 7){
		*r_len = set_len[*index];
	}else{
		*index = 10;
		if(data_write.total_bytes != 0 && data_write.total_bytes-data_write.revieve_bytes < 498){
			*r_len = data_write.total_bytes - data_write.revieve_bytes + 14;
			// akst_debug("\nlast frame len:%u\n",r_len);
		}else{
			*r_len = 512;
		}
	}
	return 0;
}

// 计算总长度
static int get_total_bytes(uint8_t *data, uint64_t *total_len){
	*total_len = 0;
	for(int i = 0;i < 10;i++){
		*total_len = *total_len*10 + (uint64_t)(data[7+i] - '0');
	}
	akst_debug("total_len: %lu Byte, %.2f MB\n",*total_len,(float)*total_len/1024/1024);
	return 0;	
}

static int get_recieve_md5(uint8_t *data, char *md5sum){

	for(int i=0; i<32; i++){
		md5sum[i] = (char)data[i+6];
	}
	md5sum[32] = '\0';
	akst_debug("md5sum: %s len:%ld\n",md5sum,strlen(md5sum));
	if(save_md5_file(md5sum)){
		return -1;
	}	

	return 0;	
}

// 获取包序号
static int get_pack_num(uint8_t *data, uint32_t *pack_num){
	*pack_num = data[6];
	*pack_num <<= 8; 
	*pack_num += data[7];
	*pack_num <<= 8;  
	*pack_num += data[8];
	// akst_debug("pack_num: %u\n",*pack_num); 
	return 0;	
}

// 获取1包中的有效长度
static int get_valid_len(uint8_t *data, uint16_t *len){
	*len = data[10]; 
	*len <<= 8; 
	*len += data[11];

	if(*len != 498 && data_write.total_bytes - data_write.revieve_bytes > 498){
		time_debug("valid len: %ld\n",*len);
		frame_printF("data:",data,512);		//2023.11.22 发现异常长度的时候打印一帧
		time_debug("\n");
	}
	return 0;	
}

// 接收超时计数器
static int check_timeout(int len, uint16_t *delay_count, uint16_t *max_wait_times){

	if(check_time_stamp()){
		time_debug("suspected timeout, last timestamp:[%s.%03d]\n",time_buf.datetime,time_buf.milliseconds);
		akst_debug("right ack:%d, err ack:%d\n",Right_AckSts,Error_AckSts);
		akst_debug("recieve:%lu write:%lu\n",data_write.revieve_bytes, data_write.write_bytes);
		// debug_err_ack(10*1000);
	}

	if(len){					//传输中，len不对，spi驱动错误timeout
		*delay_count = 0;
		*max_wait_times = 0;	
		Error_Gpio_Ack();
		usleep(820);			//延迟一会再接收下一帧, 避免连续错误
		return 0;	
	}else{						//传输中，len=0,表示没收到，等待一会后超时
		if(*delay_count < 5){
			(*delay_count)++;
		}else{
			*delay_count = 0;
			(*max_wait_times)++;
		}

		if(*max_wait_times && !(*delay_count)){
			akst_debug("\n");
			time_debug("wait frame, %d times. err counter:%d\n",*max_wait_times,err_counter);	
			// Error_Gpio_Ack();	
		}
		if(*max_wait_times > MAX_TIMEOUT_WAIT){
			akst_debug("%s\ntimeout over 5 times, recieve err, quit.%s\n",LEFT_RED,RIGHT);
			recieve_flag = 0;
			return -1;
		}

		if(*delay_count >= 2 && 0 == *max_wait_times){	// 2023.11.12想要捕捉卡住的时刻和退出后的的时间
			time_debug("delay count:%d, last timestamp:[%s.%03d]\n",*delay_count,time_buf.datetime,time_buf.milliseconds);
			// debug_err_ack(6*1000);	
		}

	}
	return 0;
}

// 检查是否接收完毕
static int check_recieve_over(uint32_t pack_num, uint64_t data_len){
	uint16_t wait_over_counter = 1000;
	int ret = 0;
	int r_counter = 0;
	if((pack_num+1)*498 - data_write.revieve_bytes != 0 && data_len == 498){
		Error_Gpio_Ack();
		akst_debug("%s\n\nlose 1 pack, revieve err, quit.%s\n\n",LEFT_RED,RIGHT);
		akst_debug("pack_num:%d\n revieve_bytes:%ld",pack_num,data_write.revieve_bytes);
		for(r_counter = 0;r_counter < 5;r_counter++){
			akst_debug("ack_record[%d]:%02x",r_counter,ack_record[r_counter]);
		}
		ret = 1;
	}

	if(data_len != 498 || data_write.revieve_bytes == data_write.total_bytes){
		akst_debug("\n");
		time_debug("recieve over!\n");
		akst_debug("last_len:%ld, revieve_bytes:%ld, total_bytes:%ld\n",data_len,data_write.revieve_bytes,data_write.total_bytes);//2023.11.20测试发现异常结束
		Right_Gpio_Ack();
		while(!data_write.write_over && wait_over_counter--){
			usleep(1000);
		}
		if(data_write.write_over){
			time_debug("get write over flag.\n");
			ret = 2;
		}else{
			time_debug("wait write over flag timeout, quit.\n");
			ret = 1;		
		}
	}

	if(1 == ret){
		fclose(fp_receive);	
		system("sync");
		system("rm -r /data/update/m116_update.bin");
		system("sync");
	}
	if(2 == ret){
		fclose(fp_receive);
		system("sync");
		system("mv /data/update/m116_update.bin /data/update/m116_update.zip");
		system("sync");
	}

	return ret;
}

// 开始升级
static int begin_update(void){
	char save_md5[50] = {0};
	Right_Gpio_Ack();
	time_debug("recieve begin update flag.\n");
	if(read_md5_file(save_md5)){
		write_0x18_1_byte(UPDATE_FAILED);	
		return -1;
	}
	write_0x18_1_byte(BEGIN_UPDATE);
	getMD5sum(recieve_file_zip,cal_md5sum);
	akst_debug("calmd5:%s len:%ld\n",cal_md5sum,strlen(cal_md5sum));
	akst_debug("savemd5:%s len:%ld\n",save_md5,strlen(save_md5));

	if(0 == strcmp(cal_md5sum,save_md5)){
		akst_debug("md5sum check ok.\n");
	}else{
		akst_debug("md5sum check failed.\n");
		write_0x18_1_byte(UPDATE_FAILED);		
		return -1;
	}

	char cmd_temp[100];
	akst_debug("begin update.\n");
	system("echo emmc_disable_all_wp > /proc/mmc_debug");
	usleep(20*1000);
	system("mount -o remount,rw /");
	usleep(20*1000);							//先解除写保护 等待一段时间

	sprintf(cmd_temp,"unzip  -o -q %s -d  %s",recieve_file_zip,update_file_path);
	akst_debug("%s\n",cmd_temp);
	system(cmd_temp);
	system("rm -r /data/update/m116_update.zip");		

	system("mv -f /data/update/upgrade.bin /usr/bin/upgrade.bin");
	system("chmod +x /usr/bin/upgrade.bin");
	system("sync");
	system("/usr/bin/upgrade.bin");    

	return 0;
}



//监控940寄存器 100ms周期
static int p_reg_940_monitor(void *arg){
	uint8_t err_count = 0;

	while(1){	
		usleep(100*1000);
		int ret = i2c_read_940_btye();
		if(ret){
			if(err_count < 10){
				err_count++;
				akst_debug("940 monitor err %d times\n",err_count);
			}else{
				return -1;
			}
		}
	}

	return 0;
}

static int set_step_A(int step){
	m_AddNode.counter = 0;
	m_AddNode.step = step;
	return 0;
}

static int set_step_B(int step){
	m_ReadNode.counter = 0;
	m_ReadNode.step = step;
	return 0;
}

// 写入文件的线程
static int p_write_task(void *arg){
	while(1){
		if(data_write.node_counter){
			Read_Node();	
		}else{
			while(delay_200ms_counter++ < 200){
				usleep(1*1000);
			}
			delay_200ms_counter = 0;
		}	
	}
	return 0;
}

// spi监控线程
static int p_spi_monitor(void *arg){
	while(1){	
		if(m_AddNode.step){
			if(++m_AddNode.counter>2){
				akst_debug("%s\nadd task block, monitor_counter:%d, Astep:%d, node_counter:%d%s\n",LEFT_RED,m_AddNode.counter,m_AddNode.step,data_write.node_counter,RIGHT);
				extractCachedSize();	
			}
			if(m_AddNode.counter > 200){
				akst_debug("%serr too many times, stop.%s\n",LEFT_RED,RIGHT);
				while(m_AddNode.counter > 200);
			}	
		}

		if(m_ReadNode.step){
			if(++m_ReadNode.counter>1){
				akst_debug("%s\nwrite task block, monitor_counter:%d, Bstep:%d, node_counter:%d,%s\n",LEFT_RED,m_ReadNode.counter,m_ReadNode.step,data_write.node_counter,RIGHT);
				extractCachedSize();	
			}
			if(m_ReadNode.counter > 200){
				akst_debug("%serr too many times, stop.%s\n",LEFT_RED,RIGHT);
				while(m_ReadNode.counter > 200);
			}
		}

		usleep(6*1000);
	}

	return 0;
}

// 添加节点线程
static int Add_Node(uint8_t *new_data) {
	uint8_t counter = 20;
	data_write.node_counter++;
	if(data_write.node_counter > 1){
		akst_debug("\n%swrite block, pack_counter %d%s\n",LEFT_RED,data_write.node_counter,RIGHT);
	}

	set_step_A(1);
	LinkNode *newNode = (LinkNode*)malloc(sizeof(LinkNode));
	memcpy(newNode->data,new_data,512);		//malloc有可能会阻塞，不要加到锁里面
    newNode->next = NULL;
	set_step_A(2);
	
	pthread_mutex_lock(&link_mutex);		//链表要加锁，同一时间只有一处能访问
    if (Header == NULL) {
        Header = newNode;
    } else {
		LinkNode* current = Header;	
        while (current->next != NULL) {
            current = current->next;
			set_step_A(counter++);	
        }
        current->next = newNode;
		akst_debug("add tail node.\n");	
    }
	pthread_mutex_unlock(&link_mutex);	
	
	set_step_A(0);
	delay_200ms_counter = 201;	
    return 0;
}

// 读取节点线程
static int Read_Node(void) {
	uint16_t data_len = 0;
	uint32_t pack_num = 0;
	uint16_t persent;
	LinkNode *current = Header;
    if (current != NULL) {				//每次进来只消1个节点
		set_step_B(1);
		data_len = 0;
		get_valid_len(current->data,&data_len);
		get_pack_num(current->data,&pack_num);	
		set_step_B(2);
		fwrite(&current->data[12], data_len, 1, fp_receive);
		set_step_B(3);	
		data_write.write_bytes += data_len;
		persent = (uint16_t)(data_write.write_bytes*100/data_write.total_bytes);
		// akst_debug("\rpacknum:%d, %ld/%ld, [%d%%], err_counter:%ld",pack_num,data_write.write_bytes,data_write.total_bytes,persent,err_counter);
		if(recieve_present_last != persent)
			akst_debug("recieve[%d%%], err_counter:%d\n",persent,err_counter);
		recieve_present_last = persent;

		pthread_mutex_lock(&link_mutex);
		LinkNode *tmp = current;
        current = current->next;
		free(tmp);
		tmp = NULL;
		Header = current;
		pthread_mutex_unlock(&link_mutex);	

		data_write.node_counter--;
		if(data_write.write_bytes == data_write.total_bytes){
			akst_debug("\n");
			time_debug("write over\n");
			data_write.write_over = 1;
			set_step_B(0);	
			return 0;
		}
	}
	set_step_B(0);	
    return 0;
}


static int save_md5_file(char *md5sum){
	FILE *fp_md5 = NULL;
	if ((fp_md5 = fopen(md5_file_path, "wb")) == NULL) {
		akst_debug("\n*Can not create file:%s\n",md5_file_path);
		return -1;
	}
	fwrite(md5sum, 33, 1, fp_md5);
	akst_debug("md5 save ok.\n");
	fclose(fp_md5);
	system("sync");
	return 0;
}

static int read_md5_file(char *md5sum){
	FILE* fp_md5 = fopen(md5_file_path, "rb");
	if(fp_md5 == NULL) {
		akst_debug("cannot open file '%s'。\n", md5_file_path);
		return -1;
    }
	
	size_t read_num = fread(md5sum, sizeof(char), 32, fp_md5);
	if(read_num != 32){
		akst_debug("read save md5 file err\n");
		return -1;
	}
	md5sum[32] = '\0';
	akst_debug("read save md5:%s len:%ld\n",md5sum,strlen(md5sum));
	fclose(fp_md5);
	return 0;
}

// 记录前面几次的ACK 捕捉丢包的时序
static int debug_record_ack(void){
	for(int i=4;i>0;i--){
		ack_record[i] =  ack_record[i-1];
	}	
	ack_record[0] = (Right_AckSts<<4) |  Error_AckSts;
	return 0;
}

static int check_pack_exist(void){

	time_debug("recieve check update flag.\n");

	if(!akst_isFileExist("/data/update/m116_update.zip")){
		write_0x18_1_byte(PACK_EXIST);
	}else{
		write_0x18_1_byte(PACK_NOT_EXIST);	
	}

	Right_Gpio_Ack();

	return 0;
}