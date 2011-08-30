LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := libam_adp
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := am_dmx/am_dmx.c am_dmx/linux_dvb/linux_dvb.c\
		   am_fend/am_fend.c am_fend/linux_dvb/linux_dvb.c\
		   am_av/am_av.c am_av/aml/aml.c\
		   am_dvr/am_dvr.c am_dvr/linux_dvb/linux_dvb.c\
		   am_aout/am_aout.c\
		   am_misc/am_adplock.c am_misc/am_misc.c\
		   am_time/am_time.c\
		   am_evt/am_evt.c\
		   am_dsc/am_dsc.c am_dsc/aml/aml.c\
		   am_osd/sw_op.c\
		   am_osd/am_osd.c\
		   am_osd/fb/fb.c\
		   am_osd/cmemlib.c\
		   am_osd/clip.c\
		   am_osd/ge2d_op.c\
		   am_smc/am_smc.c\
		   am_smc/aml/aml.c

LOCAL_CFLAGS+=-DANDROID -DAMLINUX -DCHIP_8226M -DLINUX_DVB_FEND
LOCAL_ARM_MODE := arm
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../include/am_adp\
		    $(LOCAL_PATH)/../android/ndk/include

LOCAL_SHARED_LIBRARIES += libamplayer liblog libc

LOCAL_PRELINK_MODULE := false

include $(BUILD_SHARED_LIBRARY)
