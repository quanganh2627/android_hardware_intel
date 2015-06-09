LOCAL_PATH := $(call my-dir)

# Audiocomms headers export
include $(CLEAR_VARS)
LOCAL_COPY_HEADERS_TO := audiocomms-include
LOCAL_COPY_HEADERS := \
    AudioBand.h \
    AudioThrottle.h
include $(BUILD_COPY_HEADERS)
