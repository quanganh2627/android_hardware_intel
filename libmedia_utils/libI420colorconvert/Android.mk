LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    ColorConvert.cpp

LOCAL_C_INCLUDES:= \
        $(TARGET_OUT_HEADERS)/khronos/openmax \
        $(call include-path-for, frameworks-native)/media/openmax \
        $(call include-path-for, frameworks-native)/media/editor

LOCAL_SHARED_LIBRARIES :=       \

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := libI420colorconvert

ifeq ($(USE_VIDEOEDITOR_INTEL_NV12_VERSION),true)
LOCAL_CFLAGS += -DVIDEOEDITOR_INTEL_NV12_VERSION
endif

include $(BUILD_SHARED_LIBRARY)


