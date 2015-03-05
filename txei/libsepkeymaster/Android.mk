#
# Build the key master HAL library
#
ifeq ($(BUILD_WITH_SECURITY_FRAMEWORK),txei)

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libkeymaster
LOCAL_MODULE_TAGS := optional

KM_APP_DIR := $(LOCAL_PATH)

LOCAL_SRC_FILES+= \
    sep_keymaster.c \
    txei_drv.c \
    txei_log.c

LOCAL_WHOLE_STATIC_LIBRARIES += liblog
LOCAL_SHARED_LIBRARIES := libcutils libc

LOCAL_C_INCLUDES := \
    $(KM_APP_DIR)/inc

include $(BUILD_SHARED_LIBRARY)

endif
