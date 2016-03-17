LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= am_epg_test.c

LOCAL_MODULE:= am_epg_test

LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS+=-DANDROID -DAMLINUX -DCHIP_8226M -DLINUX_DVB_FEND
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../include/am_adp $(LOCAL_PATH)/../../android/ndk/include\
		 $(LOCAL_PATH)/../../../../packages/amlogic/LibPlayer/amadec/include\
                    $(LOCAL_PATH)/../../../../packages/amlogic/LibPlayer/amcodec/include\
                    $(LOCAL_PATH)/../../../../packages/amlogic/LibPlayer/amffmpeg\
                    $(LOCAL_PATH)/../../../../packages/amlogic/LibPlayer/amplayer\
		    $(LOCAL_PATH)/../../include/am_mw\
		    external/libzvbi/src\
                    external/sqlite/dist\
                    external/icu4c/common

LOCAL_STATIC_LIBRARIES += libam_mw libam_adp
LOCAL_SHARED_LIBRARIES += libdl libicuuc libzvbi libsqlite libamplayer libcutils liblog libc

include $(BUILD_EXECUTABLE)

