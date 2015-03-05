LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)


LOCAL_MODULE_TAGS:= debug eng tests optional
LOCAL_SRC_FILES := meimm.c
LOCAL_MODULE    := libmeimm

LOCAL_COPY_HEADERS_TO := libmei
LOCAL_COPY_HEADERS := meimm.h
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include
LOCAL_SHARED_LIBRARIES := libcutils

include $(BUILD_SHARED_LIBRARY)


ALL_DEFAULT_INSTALLED_MODULES += $(SYMLINKS)

# We need this so that the installed files could be picked up based on the
# local module name
ALL_MODULES.$(LOCAL_MODULE).INSTALLED := \
    $(ALL_MODULES.$(LOCAL_MODULE).INSTALLED) $(SYMLINKS)

