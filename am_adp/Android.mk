LOCAL_PATH := $(call my-dir)
AMLOGIC_LIBPLAYER :=y


include $(CLEAR_VARS)

LOCAL_MODULE    := libam_adp
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := am_dmx/am_dmx.c am_dmx/linux_dvb/linux_dvb.c\
		   am_fend/am_fend.c am_fend/am_fend_diseqc_cmd.c am_fend/am_rotor_calc.c am_fend/linux_dvb/linux_dvb.c\
		   am_av/am_av.c am_av/aml/aml.c\
		   am_dvr/am_dvr.c am_dvr/linux_dvb/linux_dvb.c\
		   am_dmx/dvr/dvr.c\
		   am_aout/am_aout.c\
		   am_vout/am_vout.c\
		   am_vout/aml/aml.c\
		   am_misc/am_adplock.c am_misc/am_misc.c am_misc/am_iconv.c\
		   am_time/am_time.c\
		   am_evt/am_evt.c\
		   am_dsc/am_dsc.c am_dsc/aml/aml.c\
		   am_smc/am_smc.c\
		   am_smc/aml/aml.c\
		   am_userdata/am_userdata.c\
		   am_userdata/aml/aml.c\
		   am_userdata/emu/emu.c


LOCAL_CFLAGS+=-DANDROID -DAMLINUX -DCHIP_8226M -DLINUX_DVB_FEND 
ifeq ($(AMLOGIC_LIBPLAYER), y)
LOCAL_CFLAGS+=-DAMLOGIC_LIBPLAYER
endif

LOCAL_ARM_MODE := arm
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../include/am_adp\
		    $(LOCAL_PATH)/../android/ndk/include\
		    packages/amlogic/LibPlayer/amadec/include\
		    packages/amlogic/LibPlayer/amcodec/include\
		    external/icu4c/common\
		    vendor/amlogic/frameworks/av/LibPlayer/amcodec/include\
		    vendor/amlogic/frameworks/av/LibPlayer/dvbplayer/include\
		    vendor/amlogic/frameworks/av/LibPlayer/amadec/include\
		    external/icu/icu4c/source/common\
		    common/include/linux/amlogic

ifeq ($(AMLOGIC_LIBPLAYER), y)
LOCAL_C_INCLUDES+=packages/amlogic/LibPlayer/amffmpeg
LOCAL_C_INCLUDES+=packages/amlogic/LibPlayer/amplayer
endif


ifeq ($(AMLOGIC_LIBPLAYER), y)
LOCAL_SHARED_LIBRARIES+=libamplayer libcutils liblog libdl libc
else
LOCAL_SHARED_LIBRARIES+=libcutils liblog libdl libc libamadec libamcodec
endif

LOCAL_PRELINK_MODULE := false

LOCAL_32_BIT_ONLY := true

include $(BUILD_SHARED_LIBRARY)


include $(CLEAR_VARS)

LOCAL_MODULE    := libam_adp
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := am_dmx/am_dmx.c am_dmx/linux_dvb/linux_dvb.c\
		   am_fend/am_fend.c am_fend/am_fend_diseqc_cmd.c am_fend/am_rotor_calc.c am_fend/linux_dvb/linux_dvb.c\
           am_av/am_av.c am_av/aml/aml.c\
           am_dvr/am_dvr.c am_dvr/linux_dvb/linux_dvb.c\
           am_dmx/dvr/dvr.c\
           am_aout/am_aout.c\
           am_vout/am_vout.c\
           am_vout/aml/aml.c\
           am_misc/am_adplock.c am_misc/am_misc.c am_misc/am_iconv.c\
           am_time/am_time.c\
           am_evt/am_evt.c\
           am_dsc/am_dsc.c am_dsc/aml/aml.c\
           am_smc/am_smc.c\
           am_smc/aml/aml.c\
	   am_userdata/am_userdata.c\
	   am_userdata/aml/aml.c\
	   am_userdata/emu/emu.c



LOCAL_CFLAGS+=-DANDROID -DAMLINUX -DCHIP_8226M -DLINUX_DVB_FEND
ifeq ($(AMLOGIC_LIBPLAYER), y)
LOCAL_CFLAGS+=-DAMLOGIC_LIBPLAYER
endif

LOCAL_ARM_MODE := arm 
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../include/am_adp\
            $(LOCAL_PATH)/../android/ndk/include\
	    packages/amlogic/LibPlayer/amadec/include\
	    packages/amlogic/LibPlayer/amcodec/include\
	    external/icu4c/common\
 vendor/amlogic/frameworks/av/LibPlayer/amcodec/include\
                    vendor/amlogic/frameworks/av/LibPlayer/dvbplayer/include\
                    vendor/amlogic/frameworks/av/LibPlayer/amadec/include\
                    external/icu/icu4c/source/common\
                    common/include/linux/amlogic

ifeq ($(AMLOGIC_LIBPLAYER), y)
LOCAL_C_INCLUDES+=packages/amlogic/LibPlayer/amffmpeg
LOCAL_C_INCLUDES+=packages/amlogic/LibPlayer/amplayer
endif


ifeq ($(AMLOGIC_LIBPLAYER), y)
LOCAL_SHARED_LIBRARIES+=libamplayer libcutils liblog libdl libc
else
LOCAL_SHARED_LIBRARIES+=libcutils liblog libdl libc libamadec libamcodec
endif

LOCAL_PRELINK_MODULE := false

LOCAL_32_BIT_ONLY := true

include $(BUILD_STATIC_LIBRARY)


