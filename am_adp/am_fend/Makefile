BASE=../..

include $(BASE)/rule/def.mk

O_TARGET=am_fend
am_fend_SRCS=am_fend.c

ifeq ($(EMU_FEND), y)
	SUBDIRS+=emu
	am_fend_OBJS+=emu/emu
else
ifeq ($(LINUX_DVB_FEND), y)
	SUBDIRS+=linux_dvb
	am_fend_OBJS+=linux_dvb/linux_dvb
endif
endif

include $(BASE)/rule/rule.mk
