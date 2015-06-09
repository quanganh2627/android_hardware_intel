LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES :=              \
    VideoEncoderBase.cpp        \
    VideoEncoderAVC.cpp         \
    VideoEncoderH263.cpp        \
    VideoEncoderMP4.cpp         \
    VideoEncoderVP8.cpp         \
    VideoEncoderUtils.cpp       \
    VideoEncoderHost.cpp

LOCAL_C_INCLUDES :=             \
    $(LOCAL_PATH)               \
    $(TARGET_OUT_HEADERS)/libva \
    $(call include-path-for, frameworks-native)

LOCAL_SHARED_LIBRARIES :=       \
        libcutils               \
        libutils               \
        libva                   \
        libva-android           \
        libva-tpi		\
        libui \
        libutils \
        libhardware \
	libintelmetadatabuffer

LOCAL_COPY_HEADERS_TO  := libmix_videoencoder

LOCAL_COPY_HEADERS := \
    VideoEncoderHost.h \
    VideoEncoderInterface.h \
    VideoEncoderDef.h

ifeq ($(TARGET_BOARD_PLATFORM),clovertrail)
  LOCAL_CFLAGS += -DCLVT
endif


ifeq ($(ENABLE_IMG_GRAPHICS),true)
# For IMG platform specifics
# ============================
LOCAL_CFLAGS += -DIMG_GFX

LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/pvr

ifeq ($(ENABLE_MRFL_GRAPHICS),true)
LOCAL_CFLAGS += -DIMG_GFX_MRFL
endif

else
# For GEN platform specifics
# ============================

#NO_BUFFER_SHARE := true

LOCAL_CFLAGS += -DBX_RC \
                -DOSCL_IMPORT_REF= -DOSCL_UNUSED_ARG= -DOSCL_EXPORT_REF=

LOCAL_STATIC_LIBRARIES := libstagefright_m4vh263enc

LOCAL_SRC_FILES += PVSoftMPEG4Encoder.cpp

LOCAL_C_INCLUDES +=             \
    frameworks/av/media/libstagefright/codecs/m4v_h263/enc/include \
    frameworks/av/media/libstagefright/codecs/m4v_h263/enc/src \
    frameworks/av/media/libstagefright/codecs/common/include \
    frameworks/native/include/media/openmax \
    frameworks/native/include/media/hardware \
    frameworks/av/media/libstagefright/include

ifeq ($(NO_BUFFER_SHARE),true)
LOCAL_CPPFLAGS += -DNO_BUFFER_SHARE
endif

endif

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libva_videoencoder

include $(BUILD_SHARED_LIBRARY)

# For libintelmetadatabuffer
# =====================================================

include $(CLEAR_VARS)

VIDEO_ENC_LOG_ENABLE := true

LOCAL_SRC_FILES :=              \
    IntelMetadataBuffer.cpp

LOCAL_C_INCLUDES :=             \
    $(LOCAL_PATH)               

LOCAL_COPY_HEADERS_TO  := libmix_videoencoder

LOCAL_COPY_HEADERS := \
    IntelMetadataBuffer.h

ifeq ($(INTEL_VIDEO_XPROC_SHARING),true)
LOCAL_SHARED_LIBRARIES := liblog libutils libbinder libgui \
                          libui libcutils libhardware
endif

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libintelmetadatabuffer

include $(BUILD_SHARED_LIBRARY)
