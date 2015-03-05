ifeq ($(strip $(USE_INTEL_VA)),true)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_COPY_HEADERS_TO := libvavideodecoder
LOCAL_COPY_HEADERS := VAVideoDecoder.h

LOCAL_SRC_FILES := \
    VAVideoDecoder.cpp \


LOCAL_C_INCLUDES := \
    $(call include-path-for, libstagefright) \
    $(call include-path-for, frameworks-native)/media/openmax \
    $(TARGET_OUT_HEADERS)/libmix_videodecoder \
    $(TARGET_OUT_HEADERS)/libmix_asf_extractor \
    $(TARGET_OUT_HEADERS)/libva/


LOCAL_MODULE:= libvavideodecoder
LOCAL_MODULE_TAGS := optional

include $(BUILD_STATIC_LIBRARY)

endif
