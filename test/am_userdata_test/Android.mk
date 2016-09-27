LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= am_userdata_test.c

LOCAL_MODULE:= am_userdata_test

LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS+=-DANDROID -DAMLINUX -DCHIP_8226M -DLINUX_DVB_FEND
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../include/am_adp $(LOCAL_PATH)/../../android/ndk/include \
			$(LOCAL_PATH)/../../../../packages/amlogic/LibPlayer/amadec/include\
		    $(LOCAL_PATH)/../../../../packages/amlogic/LibPlayer/amcodec/include\
		    $(LOCAL_PATH)/../../../../packages/amlogic/LibPlayer/amffmpeg\
		    $(LOCAL_PATH)/../../../../packages/amlogic/LibPlayer/amplayer


LOCAL_STATIC_LIBRARIES := libam_adp
LOCAL_SHARED_LIBRARIES := libamplayer libcutils liblog libc

include $(BUILD_EXECUTABLE)
