CROSS_COMPILE = /home/rockchip/rk3358m/buildroot/output/rockchip_rk3358_64/host/bin/aarch64-buildroot-linux-gnu-
TARGET=libmqtt.so

SRCFW= 	$(wildcard ./src/*.c)
CC=$(CROSS_COMPILE)gcc

OBJS=$(SRCFW:.c=.o)

INC=-I/home/rockchip/cxd_test/SPI/spi_trans_app/src/

CFLAGS= -Wall -fPIC -Werror

$(TARGET):$(OBJS)
	$(CC) -shared $(CFLAGS) $(INC) $(OBJS) -o $@

all: $(TARGET)

.PHONY: all clean

clean:
	rm -rf $(TARGET)

