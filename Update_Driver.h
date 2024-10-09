
#ifndef _UPDATE_DRIVER_H
#define _UPDATE_DRIVER_H

#include <stdio.h>
#include <stdint.h>

#define akst_debug(fmt, ...) \
    do {\
        fprintf(stderr, fmt, ##__VA_ARGS__);\
    } while(0)


#define CLEAR_REG					0X00
#define PACK_EXIST					0X4E
#define PACK_NOT_EXIST				0X4F
#define BEGIN_UPDATE				0XA1
#define UPDATE_FAILED				0X99

#define BUF_MAX_SIZE	(512/4)

// 调试期间的颜色打印
#define COLOR_PRINTF 0
#if COLOR_PRINTF
	#define LEFT_RED 					"\033[1;31m"
	#define LEFT_GREEN 					"\033[1;32m"
	#define LEFT_YELLOW 				"\033[1;33m"
	#define LEFT_BLUE 					"\033[1;34m"
	#define LEFT_PURPLE 				"\033[1;35m"
	#define RIGHT 						"\033[0m"
#else
	#define LEFT_RED 					"#"
	#define LEFT_GREEN 					"#"
	#define LEFT_YELLOW 				"#"
	#define LEFT_BLUE 					"#"
	#define LEFT_PURPLE 				"#"
	#define RIGHT 						"#"

#endif


#define MAX_TIMEOUT_WAIT			4

extern int g_fota_dbglevel ;

typedef struct{
	int node_counter;
	int write_over;
	uint64_t total_bytes;	
	uint64_t revieve_bytes;
	uint64_t write_bytes;
	uint16_t persent_last;
}DATA_WRITE;
extern DATA_WRITE data_write;


typedef struct{
	char datetime[30];
	int milliseconds;
}TIME_MS;
extern TIME_MS time_buf;

/**********function***********/
int Update_Init_Spi(void);

void getFotaData(unsigned char *Read_buf,int size);

extern int spi_fd;

int Update_Init_Gpio(void);
void Update_Close_Gpio(void);
int Right_Gpio_Ack(void);
int Error_Gpio_Ack(void);
int clean_SPI_buffer(void);
int clear_Gpio(void);
int Update_Init_940_REG(void);
int Update_Init_I2C(void);
int write_0x18_1_byte(uint8_t data);
int spi_read_frame(int fd, uint16_t r_len, uint8_t *data);
int spi_read_first_frame(int fd, uint16_t r_len, uint8_t *data);
int i2c_read_940_btye(void);
int frame_print(char *head, uint8_t *data, uint32_t len);
void timestamp_printf(const char *format, ...);
int frame_printF(char *head, uint8_t *data, uint32_t len);
long extractCachedSize();

extern unsigned char Right_AckSts;
extern unsigned char Error_AckSts;
int cal_update_time(uint8_t mode);
int get_time_stamp(void);
int check_time_stamp(void);

int akst_isFileExist(const char *fullPathFileName);

int log_read_first_frame(int fd, uint16_t r_len, uint8_t *data);
#define time_debug timestamp_printf

#endif