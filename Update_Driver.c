#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pthread.h> 
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include "Update_Driver.h"
#include <stdint.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include "CRC_16_CCITT.h"
#include <time.h>
#include <stdarg.h>
#include "send_log.h"

#define SPISLAVE_CLR_FIFO	_IOW(SPI_IOC_MAGIC, 5, __u32)
#define SPISLAVE_SET_SPEED	_IOW(SPI_IOC_MAGIC, 6, __u32)
#define SPISLAVE_SET_TMO	_IOW(SPI_IOC_MAGIC, 7, __u32)
/************SPI******************/
// #define SPI_ADDRESS  "/dev/spidev0"      //?
#define SPI_ADDRESS  "/dev/spidev_rkslv_misc"
#define SPI_TMO_MS  10
unsigned char mode = 0;
unsigned char bits_per_word = 32;
unsigned int speed = 3300000;
int spi_fd  = -1;
int i2c_fd  = -1;


/**************GPIO*******************/
#define GPIO_0_ADDRESS "/dev/kl_spi_controller_misc"


unsigned char Right_AckSts;
unsigned char Error_AckSts;
int gpio_fd=-1;
extern int recieve_flag;


#define IIC0_DEVICE    				"/dev/i2c-2"
#define IIC_ADDR_940   				0x3C


DATA_WRITE data_write;
struct timeval start_time;
struct timeval end_time;
TIME_MS time_buf;

uint32_t err_counter = 0;
uint32_t continuous_err_counter = 0;	//记录是否连续错误

int clean_SPI_buffer(void)
{
	// int ret = ioctl(spi_fd,SPISLAVE_CLR_FIFO,&mode);//清空发送和接收的buffer

	// if (ret == -1) {
    //     akst_debug("can't ioctl SPISLAVE_CLR_FIFO");
	// 	return -1;
    // }

 return 0;
}

//初始化SPI设备
int Update_Init_Spi(void)
{
    spi_fd = open(SPI_ADDRESS, O_RDWR);	
    //int fd = spi_fd;
    // int ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);			//设置写模式

    // if (ret == -1) {
    //     akst_debug("can't set spi mode read\n");
    //     goto exit;
    // }

    // ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);				//设置读模式
    // if (ret == -1) {
    //     akst_debug("can't set spi mode write\n");
    //     goto exit;
    // }

	// int timeout_ms = SPI_TMO_MS;
	// ret = ioctl(fd, SPISLAVE_SET_TMO, &timeout_ms);			//2023.12.21 新驱动需要设置超时时间
	// if (ret == -1) {
    //     akst_debug("can't set spi timeout ms\n");
    //     goto exit;
    // }

	akst_debug("spi init ok.\n");

	return 0;
// exit :
//     close(fd);
//     return -1;
}


//初始化IO设备
int Update_Init_Gpio(void)
{
	char init_status = 0;

    gpio_fd = open(GPIO_0_ADDRESS,O_RDWR);
    if(gpio_fd<=0)
    {
        akst_debug("open Gpio Error!");
        return -1;
    }
    Right_AckSts = 0;
    Error_AckSts = 0;
	write(gpio_fd,&init_status,1);

	akst_debug("gpio init ok.\n");
    return 0;
}
//关闭设备
void Update_Close_Gpio(void)
{
    close(gpio_fd);

}

// 数字  GPIO62 GPIO68
// 0    0      0      正应答
// 1    0      1	  负应答
// 2    1      0      正应答
// 3    1      1	  负应答
//正确的应答
int Right_Gpio_Ack(void)
{
    char Ack0=0;        
    char Ack1=2;

	
    if(0 == Right_AckSts){
        write(gpio_fd,&Ack1,1);
    }
    else{
        write(gpio_fd,&Ack0,1);
    }

    Right_AckSts = !Right_AckSts;  
    // akst_debug("Right_AckSts:%d  ",Right_AckSts);
	continuous_err_counter = 0;
    return 0;
}


//错误的应答
int Error_Gpio_Ack(void)
{
	
    char Ack0=1;
    char Ack1=3;

    if(0 == Error_AckSts){
        write(gpio_fd,&Ack0,1);
    }
    else{
        write(gpio_fd,&Ack1,1);
    }
	Error_AckSts = !Error_AckSts; 
	// akst_debug("Error_AckSts:%d\n",Error_AckSts); 
	err_counter++;
	continuous_err_counter++;
    return 0;
}

// 复位GPIO状态
int clear_Gpio(void)
{
	char init_status = 0;
	write(gpio_fd,&init_status,1);

	Right_AckSts = 0;
	return 0;
}

// 初始化I2C驱动
int Update_Init_I2C(void){
	i2c_fd = open(IIC0_DEVICE, O_RDWR);

	if(i2c_fd < 0) {
		akst_debug("open iic failed \n");
		return -1;
	}

	if(ioctl(i2c_fd,I2C_SLAVE_FORCE, IIC_ADDR_940) < 0) {            
		akst_debug("set slave address failed \n");
		return -1;
	}

	akst_debug("i2c init ok.\n");
	return 0;
}


#if 0
int get_gpio_status(void){
	FILE *fp;
    char buffer[256];
	uint8_t bit0,bit1;

    fp = fopen("/sys/devices/platform/soc/11090000.pinctrl/gpio_debug", "r");
    if (fp == NULL) {
        akst_debug("Failed to open file.\n");
        return 1;
    }

    // 逐行读取文件内容，打印以 "062" 和 "068" 开头的行
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        if (strncmp(buffer, "062", 3) == 0) {
			bit0 = buffer[7] - 48;
        }
		if (strncmp(buffer, "068", 3) == 0) {
			bit1 = buffer[7] - 48;
			break;
		}
    }
	akst_debug("gpio:[%02x %02x]\n",bit0,bit1);

    fclose(fp);

    return 0;

}
#endif

// 写入940的0x18寄存器
int write_0x18_1_byte(uint8_t data){
	// int fd;
	unsigned char w_add=0x18;

	/* 要写的数据buf，第0个元素是要操作940的寄存器地址*/
	unsigned char wr_buf[2] = {0};     

	/* 将要操作的寄存器首地址赋给wr_buf[0] */
	wr_buf[0] = w_add;		

	/* 写入的1个字节 */
	wr_buf[1] = data;
	akst_debug("write 940 0x18 0x%02x \n",wr_buf[1]);
	/* 通过write函数完成向940写数据的功能 */
	write(i2c_fd, wr_buf, 2);

	return 0;
}

// 写940寄存器
static int i2c_write_940(uint8_t len, const uint8_t* data){

	if(-1 == write(i2c_fd, data, len)){
		perror("write failed.\n");
		return -1;		
	}

	akst_debug("write 940:");
	for(int i=0; i<len ;i++){
		akst_debug("0x%02x ",data[i]);
	}
	akst_debug("\n");

	return 0;
}

// 初始化940寄存器
int Update_Init_940_REG(void){
	int ret = 0;
	// const uint8_t set_940[7][2] = {{0x6b, 0x50},{0x43, 0x06},{0x6c, 0x16},{0x6d, 0x00},{0x1d, 0x63},{0x1e, 0x03},{0x18,0x00}};
	uint8_t set_940[6][2] = {{0x6b, 0x50},{0x6c, 0x16},{0x6d, 0x00},{0x1d, 0x63},{0x1e, 0x03},{0x18,0x00}};	
	
	// if(!akst_isFileExist("/data/update/m116_update.zip")){
	// 	akst_debug("ota: update pack exist\n");
	// 	set_940[5][1] = 0x4e;	
	// }else{
	// 	akst_debug("ota: no update pack exist\n");	
	// 	set_940[5][1] = 0x00;	
	// }

	for(int i=0; i<6; i++){
		ret |= i2c_write_940(2,set_940[i]);		
	}

	if(ret){
		akst_debug("set 940 reg failed.\n");	
		return -1;
	}

	akst_debug("940 reg init ok.\n\n");
	return 0;

}

//从SPI读取一帧
int spi_read_frame(int fd, uint16_t r_len, uint8_t *data){
	int len, ack_ret;
	uint8_t err_count = 0;
	uint16_t wait_count = 0, delay_count = 0;

	while(recieve_flag){
		clean_SPI_buffer();		
		len = read(fd, data, r_len);
		frame_print("receive_data:", data, len);

		if (len < 0){
			perror("read data fail:");
			return -1;	
		}else{
			if(len != r_len){
				if(!len){
					if(delay_count >= 2 && delay_count <=4 && 0 == wait_count){	// 2023.11.22想要捕捉fwrite()卡住, 和超时退出后的时间间隔, 设置的超时100ms
						time_debug("first timeout, last timestamp:[%s.%03d]\n",time_buf.datetime,time_buf.milliseconds);
					}
					if(delay_count < 0x3F){
						delay_count++;
					}else{
						delay_count = 0;

						if(wait_count < 5){
							wait_count++;
							// akst_debug("wait %d times\n",wait_count);	
							time_debug("wait %d times\n",wait_count);
							// Error_Gpio_Ack();	
						}else{
							akst_debug("wait 6 times, time out, quit.\n");
							return -1;
						}
					}
				}else{
					Error_Gpio_Ack();
					akst_debug("read err, len ≠ read_len\n");
					if(err_count < 20)
						err_count++;
					else{
						akst_debug("err 20 times, quit.");
						return -1;
					}
				}   
				continue;	
			}else{
				ack_ret = crcCheck(&data[0],r_len);
				
				if(ack_ret){
					Error_Gpio_Ack();
					akst_debug("crc check err\n");
					if(err_count < 20)
						err_count++;
					else{
						akst_debug("err 20 times, quit.");
						return -1;
					}
				}else{
					//akst_debug("crc success\n");
					//Right_Gpio_Ack(); //这个放到spi slave读状态配置好后操作
					//start_gpio_task(1);
					break;
				}
			}
		}
	}
	return 0;

}

// 读取第一帧
int spi_read_first_frame(int fd, uint16_t r_len, uint8_t *data){
	int len, ack_ret, first_frame_len = 0;
	// uint8_t delay_count = 0, max_wait_times = 0;
	uint8_t temp[10]={0};

	clean_SPI_buffer();	
	len = read(fd, temp, 3);
	frame_print("first read 3 byte:", temp, 3);
	usleep(100);
	Error_Gpio_Ack();
	if(len < 0){
		perror("read first frame len fail:");
		return -1;	
	}
	usleep(100);
	first_frame_len = temp[2] +3;							//第一帧长度不定, 先牺牲1帧取长度,第2次完整读

	akst_debug("get first frame len :%d\n",first_frame_len);
	if(first_frame_len != 9 && first_frame_len != 10){		
		akst_debug("first frame get len err, not 9 or 10\n");
		return -1;		
	}else{
		clean_SPI_buffer();	
		len = read(fd, data,first_frame_len);
	}
	
	int ret = -1;
	if (len < 0){
		perror("read first frame data fail:");
		frame_print("read",data,first_frame_len);
		ret = -1;	
	}else if(len != 9 && len != 10){
		akst_debug("first frame: len ≠ read_len, read err\n");
		ret = -1;	
	}else{
		ack_ret = crcCheck(&data[0],first_frame_len);
		if(ack_ret || 0x55 != data[0] || 0xAA != data[1]){
			akst_debug("first frame check err.\n");
			frame_print("",data,first_frame_len);
			ret = -1;
		}else{	
			frame_print("\nreceive first frame:",data,first_frame_len);	
			ret = 0;
		}
	}

	// Error_Gpio_Ack();	//第一帧就算对了也不回复应答 后面会再读一次
	return ret;
}

// void show_timestamp(void){
// 	struct timeval tv;
//     gettimeofday(&tv, NULL);
//     time_t seconds = tv.tv_sec;
//     int milliseconds = tv.tv_usec / 1000;
//     struct tm *tm_info;
//     char datetime[30];

// 	tm_info = localtime(&seconds);
// 	// strftime(datetime, sizeof(datetime), "%Y-%m-%d %H:%M:%S", tm_info);
// 	strftime(datetime, sizeof(datetime), "%H:%M:%S", tm_info);

// 	// akst_debug("当前时间戳（毫秒）：%ld\n", seconds * 1000 + milliseconds);
// 	akst_debug("\r[%s.%03d]", datetime, milliseconds);
// }

// 加上时间戳的打印
void timestamp_printf(const char *format, ...){
    char buffer[256];
    va_list args;
	struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t seconds = tv.tv_sec;
    int milliseconds = tv.tv_usec / 1000;
	
    struct tm *tm_info;
    char datetime[30];

    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

	tm_info = localtime(&seconds);
	strftime(datetime, sizeof(datetime), "%H:%M:%S", tm_info);

	akst_debug("[%s.%03d]%s", datetime, milliseconds,buffer);
}

// 计算传包所用的时间
int cal_update_time(uint8_t mode){
	if(!mode){
		gettimeofday(&start_time, NULL); 
	}else if(1 == mode){
		gettimeofday(&end_time, NULL);
		float time_minutes = (float)(end_time.tv_sec - start_time.tv_sec);
		time_minutes /= 60;
		akst_debug("spend %.1fmin.\n",time_minutes); 	
	}
	return 0;
}

// 把时间戳存下来,需要的时候打印,避免一直打太多了
int get_time_stamp(void){
	struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t seconds = tv.tv_sec;
    time_buf.milliseconds = tv.tv_usec / 1000;	//存储ms
    struct tm *tm_info;

	tm_info = localtime(&seconds);
	strftime(time_buf.datetime, sizeof(time_buf.datetime), "%H:%M:%S", tm_info);
	return 0;
}

int check_time_stamp(void){
	struct timeval tv;
    gettimeofday(&tv, NULL);	
	int now_ms = tv.tv_usec / 1000;
	int diff_value = now_ms - time_buf.milliseconds;
	if(diff_value > 20){
		akst_debug("now ms:%d, old ms:%d",now_ms,time_buf.milliseconds);
		akst_debug("read expend %d ms\n",diff_value);
		return -1;
	}

	return 0;
}

// 监控940的0x18寄存器
int i2c_read_940_btye(void){
	unsigned char rd_buf[1] = {0},wr_buf[2];     
	
	/*先写寄存器的首地址*/
	wr_buf[0] = 0x18;
	write(i2c_fd, wr_buf, 1);

	int ret = read(i2c_fd, rd_buf, 1);


	if(ret < 0){
		akst_debug("iic read err.\n");
		return -1;	
	}else{
		// akst_debug("read 940 0x3c: 0x%02x\n",rd_buf[0]);
	}

	//akst_debug("read 940 0x3c: 0x%02x\n",rd_buf[0]);

	if(rd_buf[0] == 0x33){
	// if(rd_buf[0] == 0x26 || rd_buf[0] == 0x33){
		time_debug("\n%sget reset gpio flag.%s\n",LEFT_GREEN,RIGHT);
		recieve_flag = 0;
		clear_Gpio();
		usleep(10*1000);

		write_0x18_1_byte(CLEAR_REG);								//2024.01.17 升级包状态改为收到请求之后再上报
		// if(!akst_isFileExist("/data/update/m116_update.zip")){
		// 	write_0x18_1_byte(PACK_EXIST);
		// }else{
		// 	write_0x18_1_byte(PACK_NOT_EXIST);						
		// }
		recieve_flag = 1;

		usleep(5*1000);		
	}
	
	return 0;
}

// 打印一帧数据
int frame_print(char *head, uint8_t *data, uint32_t len){
	akst_debug("%s",head);
	for(int i=0;i<len;i++){
		akst_debug("%02x ",data[i]);
	}
	akst_debug("\n");

	return 0;
}

int print_data(char *head, uint8_t *data, uint32_t len,int row) {
    akst_debug("%s", head);
    for (uint32_t i = 0; i < len; i++) {
        akst_debug("%02x ", data[i]);
        if ((i + 1) % row == 0) { 
            akst_debug("\n");
        }
    }
    if (len % row != 0) { 
        akst_debug("\n");
    }
    return 0;
}

int frame_printF(char *head, uint8_t *data, uint32_t len){
	printf("%s",head);
	for(int i=0;i<len;i++){
		printf("%02x ",data[i]);
	}
	printf("\n");

	return 0;
}

// 提取错误错误信息
long extractCachedSize() {
    char buffer[256];
	long cachedSize = 0xFF;
	long dirtySize = 0xFF;	
	long writebackSize = 0xFF;
	FILE *fp_meminfo = NULL;

	fp_meminfo = fopen("/proc/meminfo", "r");
    if (fp_meminfo == NULL) {
        fprintf(stderr, "无法打开 /proc/meminfo 文件\n");
        return -1;
    }

	while (fgets(buffer, 256, fp_meminfo)) {
		if (strncmp(buffer, "Cached:", 7) == 0) {
			sscanf(buffer + 7, "%ld", &cachedSize);
		}
		if (strncmp(buffer, "Dirty:", 6) == 0) {
			sscanf(buffer + 6, "%ld", &dirtySize);
		}
		if (strncmp(buffer, "Writeback:", 10) == 0) {
			sscanf(buffer + 10, "%ld", &writebackSize);
			break;
		}
	}
	akst_debug("%scachedSize:%ld kB, dirtySize:%ld kB, writebackSize:%ld kB%s\n",LEFT_RED,cachedSize,dirtySize,writebackSize,RIGHT);

    fclose(fp_meminfo);

    return 0;
}


int akst_isFileExist(const char *fullPathFileName){
    //简单判断输入的路径是否可用
    if(fullPathFileName==NULL||!strlen(fullPathFileName)){
        return -1;
    }
    FILE *fp = fopen(fullPathFileName,"rb");
    if(fp){
        fclose(fp);
        return 0;
    }else{
        return -1;
    }
}

// log传输第一帧比较特殊, 时序严格
int log_read_first_frame(int fd, uint16_t r_len, uint8_t *data){
	int len, ack_ret;
	// uint8_t err_count = 0;
	uint16_t wait_count = 0, delay_count = 0;

	while(recieve_flag){
		//clean_SPI_buffer();		
		len = read(fd, data, r_len);
		delay_count++;
									
		if(delay_count >= 2 && delay_count <=4 && 0 == wait_count){	// 2023.11.22想要捕捉fwrite()卡住, 和超时退出后的时间间隔, 设置的超时100ms
			time_debug("first timeout, last timestamp:[%s.%03d]\n",time_buf.datetime,time_buf.milliseconds);
		}		
		if(delay_count >= 0x3F){
			delay_count = 0;
			wait_count++;
			time_debug("wait %d times\n",wait_count);	
		}
		if(wait_count >= 2){
			akst_debug("wait longtime, timeout.\n");
			return -1;
		}

		if(len < 0){
			perror("read data fail:");
			return -1;
		}else if(len == 0){
			continue;
		}else if(len != r_len){
			Error_Gpio_Ack();
			akst_debug("read err, len ≠ read_len.\n");
			continue;
		}else{
			ack_ret = crcCheck(&data[0],r_len);	
		}

		if(ack_ret){
			Error_Gpio_Ack();
			time_debug("crc check err\n");
			frame_print("err frame:", data, 10);
			continue;
		}else{
			if(data[0] == 0x55 && data[1] == 0xAA && data[3] == 0x0b){
				//Right_Gpio_Ack();
				time_debug("ready first frame delay_count : %d\n", delay_count);
				frame_print("log:read first frame:", data, 10);
				break;	
			}else{
				continue;
			}
		}
	}
	
	return 0;
}

