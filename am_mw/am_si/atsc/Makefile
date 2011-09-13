BASE=../../..

include $(BASE)/rule/def.mk

CFLAGS+=-I$(ROOTDIR)/include/am_mw/atsc

O_TARGET=atsc
atsc_SRCS=atsc_cvct.c atsc_tvct.c atsc_mgt.c atsc_rrt.c atsc_stt.c atsc_descriptor.c huffman_decode.c

include $(BASE)/rule/rule.mk
