LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= am_upd_test.c

LOCAL_MODULE:= am_upd_test

LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS+=-DANDROID -DAMLINUX -DCHIP_8226M -DLINUX_DVB_FEND
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../include/am_adp \
					$(LOCAL_PATH)/../../android/ndk/include \
					$(LOCAL_PATH)/../../include/am_mw

LOCAL_STATIC_LIBRARIES := libam_mw libam_adp
LOCAL_SHARED_LIBRARIES := liblog libc

LOCAL_LDFLAGS += -ldl

include $(BUILD_EXECUTABLE)
