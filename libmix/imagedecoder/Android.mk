LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES += \
    JPEGDecoder.cpp \
    JPEGBlitter.cpp \
    JPEGParser.cpp \
    ImageDecoderTrace.cpp
LOCAL_C_INCLUDES += \
    $(TARGET_OUT_HEADERS)/libva
LOCAL_COPY_HEADERS_TO  := libmix_imagedecoder
LOCAL_COPY_HEADERS := \
    JPEGDecoder.h     \
    JPEGCommon.h      \
    ImageDecoderTrace.h
LOCAL_SHARED_LIBRARIES += \
    libcutils         \
    libutils          \
    libva-android     \
    libva             \
    libva-tpi         \
    libhardware
LOCAL_CFLAGS += -Wno-multichar -DLOG_TAG=\"ImageDecoder\"
ifneq ($(filter $(TARGET_BOARD_PLATFORM),baytrail cherrytrail),)
LOCAL_SRC_FILES += JPEGBlitter_gen.cpp
LOCAL_SRC_FILES += JPEGDecoder_gen.cpp
LOCAL_CFLAGS += -Wno-non-virtual-dtor -DGFXGEN
LOCAL_LDFLAGS += -L$(TARGET_OUT_SHARED_LIBRARIES) -l:igfxcmrt32.so
else
LOCAL_SRC_FILES += JPEGBlitter_img.cpp
LOCAL_SRC_FILES += JPEGDecoder_img.cpp
endif
LOCAL_MODULE:= libmix_imagedecoder
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)

ifeq ($(TARGET_BOARD_PLATFORM),baytrail)
include $(CLEAR_VARS)
LOCAL_MODULE := libmix_imagedecoder_genx
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_SUFFIX := .isa
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := libjpeg_cm_genx.vlv.isa
include $(BUILD_PREBUILT)
endif
ifeq ($(TARGET_BOARD_PLATFORM),cherrytrail)
include $(CLEAR_VARS)
LOCAL_MODULE := libmix_imagedecoder_genx
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_SUFFIX := .isa
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := libjpeg_cm_genx.chv.isa
include $(BUILD_PREBUILT)
endif

ifneq ($(filter $(TARGET_BOARD_PLATFORM),baytrail cherrytrail),)
include $(CLEAR_VARS)
LOCAL_SRC_FILES += \
    test/testdecode.cpp
LOCAL_C_INCLUDES += \
    $(TARGET_OUT_HEADERS)/libva
LOCAL_SHARED_LIBRARIES += \
    libcutils \
    libutils \
    libva-android     \
    libva             \
    libva-tpi         \
    libmix_imagedecoder        \
    libhardware
LOCAL_CFLAGS += -Wno-multichar
LOCAL_MODULE:= testjpegdec
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)
endif

include $(CLEAR_VARS)
LOCAL_SRC_FILES += \
    JPEGDecoder_libjpeg_wrapper.cpp
ifeq ($(TARGET_BOARD_PLATFORM),baytrail)
LOCAL_CFLAGS += -DGFXGEN
endif
LOCAL_C_INCLUDES += \
    $(call include-path-for, jpeg) \
    $(TARGET_OUT_HEADERS)/libva \
    $(TARGET_OUT_HEADERS)/libmix_imagedecoder \
LOCAL_COPY_HEADERS_TO  := libjpeg_hw
LOCAL_COPY_HEADERS := \
    JPEGDecoder_libjpeg_wrapper.h
LOCAL_SHARED_LIBRARIES += \
    libcutils \
    libutils \
    liblog  \
    libva \
    libva-android \
    libmix_imagedecoder \
    libhardware
LOCAL_CFLAGS += -Wno-multichar -DLOG_TAG=\"ImageDecoder\"
LOCAL_CFLAGS += -DUSE_INTEL_JPEGDEC
LOCAL_MODULE:= libjpeg_hw
LOCAL_MODULE_TAGS := optional
include $(BUILD_STATIC_LIBRARY)

