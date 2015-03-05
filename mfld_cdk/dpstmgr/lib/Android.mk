LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_PREBUILT_LIBS := \
    libdm_dpst.so

include $(BUILD_MULTI_PREBUILT)
