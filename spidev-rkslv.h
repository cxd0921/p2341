/*
 * @Author: weikunlong kunlong.wei@tongdajs.com
 * @Date: 2024-10-12 17:37:33
 * @LastEditors: weikunlong kunlong.wei@tongdajs.com
 * @LastEditTime: 2024-10-15 17:56:34
 * @FilePath: \p2341_spi_driver\spidev-rkslv.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */

#ifndef SPIDEV_RKSLV_H
#define SPIDEV_RKSLV_H

#include <linux/types.h>
#include <linux/ioctl.h>


/* IOCTL commands */

struct slv_ioc_data {
    size_t length; // 数据长度
    char data[512];   // 数据内容
};

 
//切换gpio状态通知IVI
enum SwitchGpioFlag
{
	ResponseRightGpio, 
	ResponseWrongGpio,
};

#define SPI_IOC_MAGIC			'K'

//发送日志数据
#define SPI_IOC_SEND_LOG		_IOW(SPI_IOC_MAGIC, 6, struct slv_ioc_data)

//写GPIO状态
//0 正确应答 1 错误应答 
#define SPI_IOC_WRITE_GPIO      _IOW(SPI_IOC_MAGIC, 7, unsigned char)


#define SPI_IOC_UPDATE_BUFFER  _IOW(SPI_IOC_MAGIC, 8, struct slv_ioc_data)

#endif //SPIDEV_RKSLV_H