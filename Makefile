sysroot_path = /home/rockchip/rk3358m/buildroot/output/rockchip_rk3358_64/host/aarch64-buildroot-linux-gnu/sysroot
CROSS_COMPILE = /home/rockchip/rk3358m/buildroot/output/rockchip_rk3358_64/host/bin/aarch64-buildroot-linux-gnu-
ROOTFS = $(sysroot_path)
	
LOCAL_PATH = $(shell pwd)

TARGET = spi_update.bin

SRCFW = $(wildcard \
				./*.c \
        )
OBJS = $(SRCFW:.c=.o)
OBJS_D = $(SRCFW:.c=.d)

userdata_Dir_a = /home/rockchip/rk3358m_qt_all/device/rockchip/common/images/userdata_p2341/normal/part_a/os_app/update_app
userdata_Dir_b = /home/rockchip/rk3358m_qt_all/device/rockchip/common/images/userdata_p2341/normal/part_b/os_app/update_app
CC = $(CROSS_COMPILE)gcc

LIB_MQTT=/home/rockchip/cxd_test/SPI/spi_trans_app/

INC = -I$(ROOTFS)/usr/include \
	 -I/home/rockchip/cxd_test/SPI/spi_trans_app/src/

FLAGS = -fPIC -Wall -Wno-overlength-strings -g -fno-strict-aliasing -Wno-maybe-uninitialized --sysroot=$(sysroot_path) -DLINUX
CFLAGS = $(FLAGS) $(INC)
LDFLAGS =-lm -ldl -lpthread -lmqtt

# LDFLAGS_k=-Wl,-rpath,/home/rockchip/cxd_test/SPI/spi_trans_app/src

LIB_DIR =  -L$(ROOTFS) 
LIB_DIR += -L$(ROOTFS)/usr/lib64
LIB_DIR += -L$(ROOTFS)/lib64
LIB_DIR += -L$(LOCAL_PATH)
LIB_DIR += -L$(LIB_MQTT)
LDFLAGS += $(LIB_DIR)

all:  $(TARGET) 
$(TARGET): $(OBJS)  
	$(CC) $(LDFLAGS)  $(CFLAGS) $^  -o $(TARGET) 
	@echo $<
	@make cleanp

%.o : %.c
	$(CC) $(CFLAGS) -MMD -c $< -o $@

# 制作库
# SRCFW_K = $(wildcard ./src/*.c)
# OBJS_K =	$(SRCFW_K:.c=.o)
# OBJS_K_D =	$(SRCFW_K:.c=.d)
# CFLAGS_K = -Wall -Werror -fPIC 
# LIBINC=-I/home/rockchip/cxd_test/SPI/spi_trans_app/src/
# LIB_SHARED = libmqtt.so
# $(LIB_SHARED): $(OBJS_K)	
# 		$(CC) -shared $(CFLAGS_K) $(OBJS_K) $(LIBINC) -o $@
# all: $(LIB_SHARED) 

.PHONY: all clean cleanp

clean:
	rm -f *.o *.d $(OBJS_D) $(TARGET) $(OBJS)
	# rm -f *.o *.d $(OBJS_D) $(OBJS) 

cleanp:
	rm -f *.o *.d $(OBJS_D) $(OBJS)

release:
	@make all
	mkdir -p $(userdata_Dir_a)
	mkdir -p $(userdata_Dir_b)
	cp ./$(TARGET) $(userdata_Dir_a)
	cp ./$(TARGET) $(userdata_Dir_b)