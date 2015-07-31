LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= am_dsc_test.c
LOCAL_MODULE:= am_dsc_test
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS+=-DANDROID -DAMLINUX -DCHIP_8226M -DLINUX_DVB_FEND
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../include/am_adp $(LOCAL_PATH)/../../android/ndk/include \
			packages/amlogic/LibPlayer/amadec/include\
		    packages/amlogic/LibPlayer/amcodec/include\
		    packages/amlogic/LibPlayer/amffmpeg\
		    packages/amlogic/LibPlayer/amplayer\
		    vendor/amlogic/frameworks/av/LibPlayer/amcodec/include\
            vendor/amlogic/frameworks/av/LibPlayer/dvbplayer/include\
            vendor/amlogic/frameworks/av/LibPlayer/amadec/include

LOCAL_STATIC_LIBRARIES := libam_adp
LOCAL_SHARED_LIBRARIES := libamplayer libcutils liblog libc
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_SRC_FILES:= am_dsc_test2.c
LOCAL_MODULE:= am_dsc_test2
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS+=-DANDROID -DAMLINUX -DCHIP_8226M -DLINUX_DVB_FEND
LOCAL_C_INCLUDES :=$(LOCAL_PATH)/../../include/am_adp $(LOCAL_PATH)/../../android/ndk/include
LOCAL_STATIC_LIBRARIES :=libam_adp
LOCAL_SHARED_LIBRARIES :=libcutils liblog libc
include $(BUILD_EXECUTABLE)

