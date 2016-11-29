TARGET=gxb
ARCH=arm
CHIP=gxb

OUTPUT:=/home/gk/work/buildroot/output
BUILD:=$(OUTPUT)/build

CROSS_COMPILE:=$(OUTPUT)/host/usr/bin/aarch64-linux-gnu-
KERNEL_INCDIR?=$(BUILD)/linux-amlogic-3.14-dev/include/uapi $(BUILD)/linux-amlogic-3.14-dev/include $(BUILD)/libplayer-2.1.0/amcodec/include $(BUILD)/linux-amlogic-3.14-dev/include/linux/amlogic
ROOTFS_INCDIR?=$(OUTPUT)/target/usr/include $(BUILD)/libplayer-2.1.0/amffmpeg $(BUILD)/libplayer-2.1.0/amadec/include
ROOTFS_LIBDIR?=$(OUTPUT)/target/usr/lib

DEBUG=y
MEMWATCH=n

LINUX_INPUT=n
TTY_INPUT=n

EMU_DEMUX=n

SDL_OSD=n

LINUX_DVB_FEND=y
EMU_FEND=n

EMU_SMC=n

EMU_DSC=n

EMU_AV=n

EMU_VOUT=n

IMG_BMP=y
IMG_GIF=y
IMG_PNG=y

IMG_JPEG=n

FONT_FREETYPE=n

LIBICONV=y
