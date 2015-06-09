ifeq ($(strip $(USE_INTEL_MULT_THREAD)),true)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_COPY_HEADERS_TO := libthreadedsource
LOCAL_COPY_HEADERS := ThreadedSource.h

LOCAL_SRC_FILES := \
    ThreadedSource.cpp \


LOCAL_C_INCLUDES := \
    $(call include-path-for, libstagefright) \
    $(call include-path-for, frameworks-native)/media/openmax \


LOCAL_MODULE:= libthreadedsource
LOCAL_MODULE_TAGS := optional

include $(BUILD_STATIC_LIBRARY)

endif
