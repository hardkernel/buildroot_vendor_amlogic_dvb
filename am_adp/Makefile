BASE=..

include $(BASE)/rule/def.mk

MODULE_DEPEND=

ifeq ($(TARGET),android)
SUBDIRS=am_misc am_mem am_time am_inp am_fend am_smc am_dmx am_osd am_dsc am_av am_aout am_vout am_evt am_dvr 
else
SUBDIRS=am_misc am_mem am_time am_inp am_fend am_smc am_dmx am_osd am_dsc am_av am_aout am_vout am_evt am_net am_dvr
endif

LIB_TARGET=am_adp
ifeq ($(TARGET),android)
am_adp_OBJS=am_misc/am_misc am_mem/am_mem am_time/am_time am_inp/am_inp am_fend/am_fend am_smc/am_smc am_dmx/am_dmx am_osd/am_osd am_dsc/am_dsc am_av/am_av am_aout/am_aout am_vout/am_vout am_evt/am_evt am_dvr/am_dvr
else
am_adp_OBJS=am_misc/am_misc am_mem/am_mem am_time/am_time am_inp/am_inp am_fend/am_fend am_smc/am_smc am_dmx/am_dmx am_osd/am_osd am_dsc/am_dsc am_av/am_av am_aout/am_aout am_vout/am_vout am_evt/am_evt am_net/am_net am_dvr/am_dvr
endif


include $(BASE)/rule/rule.mk
