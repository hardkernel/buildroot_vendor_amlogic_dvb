BASE=..

include $(BASE)/rule/def.mk

ifeq ($(TARGET),android)

SUBDIRS= am_cfg_test am_inp_test am_smc_test am_fend_test am_dmx_test am_av_test am_dsc_test am_si_test  am_scan_test am_epg_test am_db_test am_dvr_test am_sub_test am_tt_test

else

SUBDIRS=am_cfg_test am_smc_test am_fend_test am_dmx_test am_av_test am_dsc_test am_si_test am_scan_test am_epg_test am_dvr_test #am_sub_test am_tt_test am_tv_test simple_dtv

endif

include $(BASE)/rule/rule.mk
