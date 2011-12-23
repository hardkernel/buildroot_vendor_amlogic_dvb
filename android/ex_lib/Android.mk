LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
	
#$(call add-prebuilt-files, STATIC_LIBRARIES, libfreetype.a) 
#$(call add-prebuilt-files, STATIC_LIBRARIES, libiconv.a)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := libfreetype.a
LOCAL_BUILT_MODULE_STEM := libfreetype.a
LOCAL_MODULE_SUFFIX := a
LOCAL_MODULE := libfreetype
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := libiconv.a
LOCAL_BUILT_MODULE_STEM := libiconv.a
LOCAL_MODULE_SUFFIX := a
LOCAL_MODULE := libiconv
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
include $(BUILD_PREBUILT)

