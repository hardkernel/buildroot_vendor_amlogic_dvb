BASE=..

include $(BASE)/rule/def.mk

MODULE_DEPEND=

ifeq ($(TARGET),android)
SUBDIRS=am_misc am_mem am_time am_pes am_ad am_inp am_fend am_smc am_dmx am_osd am_dsc am_av am_aout am_vout am_evt am_dvr 
else
SUBDIRS=am_misc am_mem am_time am_pes am_ad am_inp am_kl am_fend am_smc am_open_lib/libdvbsi am_open_lib/am_freesat am_dmx am_dsc am_av am_aout am_vout am_evt am_dvr
endif

LIB_TARGET=am_adp
ifeq ($(TARGET),android)
am_adp_OBJS=am_misc/am_misc am_mem/am_mem am_time/am_time am_pes/am_pes am_ad/am_ad  am_inp/am_inp am_fend/am_fend am_smc/am_smc am_dmx/am_dmx am_osd/am_osd am_dsc/am_dsc am_av/am_av am_aout/am_aout am_vout/am_vout am_evt/am_evt am_dvr/am_dvr
else
am_adp_OBJS=am_misc/am_misc am_mem/am_mem am_time/am_time am_kl/am_kl am_pes/am_pes am_open_lib/libdvbsi/dvbsi  am_open_lib/am_freesat/am_freesat am_ad/am_ad am_inp/am_inp am_fend/am_fend am_smc/am_smc am_dmx/am_dmx am_dsc/am_dsc am_av/am_av am_aout/am_aout am_vout/am_vout am_evt/am_evt am_dvr/am_dvr
endif


include $(BASE)/rule/rule.mk
