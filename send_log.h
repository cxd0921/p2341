#ifndef _SEND_LOG_
#define _SEND_LOG_


#define BEGIN_PACK_LOG 		0x22
#define LOG_PACK_FAILED 	0x24
#define LOG_PACK_OK 		0x26

int start_gpio_task(int bool);
int spi_send_log(void);
void debug_err_ack(int time);
// int check_write_protect();



#endif