BASE=../..

include $(BASE)/rule/def.mk

CFLAGS+=-I$(ROOTDIR)/include/am_mw/libdvbdsi

SUBDIRS=libdvbsi atsc
O_TARGET=am_si
am_si_SRCS=am_si.c
am_si_OBJS=libdvbsi/dvbsi atsc/atsc

include $(BASE)/rule/rule.mk
