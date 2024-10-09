sysroot_path = /home/rockchip/rk3358m/buildroot/output/rockchip_rk3358_64/host/aarch64-buildroot-linux-gnu/sysroot
CROSS_COMPILE = /home/rockchip/rk3358m/buildroot/output/rockchip_rk3358_64/host/bin/aarch64-linux-
ROOTFS = $(sysroot_path)

LOCAL_PATH = $(shell pwd)

TARGET = Spi_Update.bin 

#SRCFW = $(LOCAL_PATH)/main.c
#SRCFW = $(LOCAL_PATH)/abc/*.c
SRCFW = $(wildcard \
				./*.c \
        )
OBJS = $(SRCFW:.c=.o)
OBJS_D = $(SRCFW:.c=.d)

CC = $(CROSS_COMPILE)gcc

INC = -I$(ROOTFS)/usr/include \

FLAGS = -fPIC -Wall -Wno-overlength-strings -g -fno-strict-aliasing -Wno-maybe-uninitialized --sysroot=$(sysroot_path) -DLINUX
CFLAGS = $(FLAGS) $(INC)
LDFLAGS = -lm -ldl -lpthread 

LIB_DIR = -L$(ROOTFS) 
LIB_DIR += -L$(ROOTFS)/usr/lib64
LIB_DIR += -L$(ROOTFS)/lib64
LIB_DIR += -L$(LOCAL_PATH)
#LIB_DIR += -lchange_slot

LDFLAGS += $(LIB_DIR)

#编译完成清理中间文件
.PHONY: all clean 
all: $(TARGET)
$(TARGET): $(OBJS)  
	$(CC) $(LDFLAGS) $(CFLAGS) $^ $(USERFILE) -o $(TARGET) -lpthread
	@echo $<
	@make clean

%.o : %.c
	$(CC) $(CFLAGS) -MMD -c $< -o $@

clean:
#	rm -f *.o *.d $(OBJS_D) $(TARGET) $(OBJS)
	rm -f *.o *.d $(OBJS_D) $(OBJS)