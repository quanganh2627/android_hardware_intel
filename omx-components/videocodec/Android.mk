ifeq ($(strip $(BOARD_USES_WRS_OMXIL_CORE)),true)
LOCAL_PATH := $(call my-dir)

ifeq ($(strip $(USE_VIDEO_EFFECT)),true)
LOCAL_C_FLAGS := -DUSE_VIDEO_EFFECT
else
LOCAL_C_FLAGS :=
endif

include $(CLEAR_VARS)

ifeq ($(TARGET_HAS_VPP),true)
LOCAL_CFLAGS += -DTARGET_HAS_VPP
endif

LOCAL_CPPFLAGS :=
LOCAL_LDFLAGS :=

LOCAL_SHARED_LIBRARIES := \
    libwrs_omxil_common \
    libva_videodecoder \
    liblog \
    libva \
    libva-android

LOCAL_C_INCLUDES := \
    $(TARGET_OUT_HEADERS)/wrs_omxil_core \
    $(TARGET_OUT_HEADERS)/khronos/openmax \
    $(PV_INCLUDES) \
    $(TARGET_OUT_HEADERS)/libmix_videodecoder \
    $(TARGET_OUT_HEADERS)/libva \
    $(call include-path-for, frameworks-native)/media/hardware \
    $(call include-path-for, frameworks-native)/media/openmax

PLATFORM_USE_GEN_HW := \
    baytrail \
    cherrytrail

ifneq ($(filter $(TARGET_BOARD_PLATFORM),$(PLATFORM_USE_GEN_HW)),)
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/ufo
endif

LOCAL_SRC_FILES := \
    OMXComponentCodecBase.cpp\
    OMXVideoDecoderBase.cpp\
    OMXVideoDecoderAVC.cpp

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libOMXVideoDecoderAVC
LOCAL_MULTILIB := 32
ifeq ($(TARGET_BOARD_PLATFORM),clovertrail)
LOCAL_CFLAGS += -DVED_TILING
LOCAL_CFLAGS += -DBUF_HEIGHT_ALIGN32
endif

ifeq ($(TARGET_BOARD_PLATFORM),merrifield)
LOCAL_CFLAGS += -DVED_TILING
endif

ifeq ($(TARGET_BOARD_PLATFORM),moorefield)
LOCAL_CFLAGS += -DVED_TILING
endif

ifeq ($(TARGET_VPP_USE_GEN),true)
LOCAL_CFLAGS += -DDEINTERLACE_EXT
endif

ifeq ($(TARGET_BOARD_PLATFORM),baytrail)
LOCAL_CFLAGS += -DUSE_GEN_HW
endif

include $(BUILD_SHARED_LIBRARY)


PLATFORM_SUPPORT_VP8 := \
    merrifield \
    morganfield \
    moorefield \
    baytrail \
    cherrytrail

ifneq ($(filter $(TARGET_BOARD_PLATFORM),$(PLATFORM_SUPPORT_VP8)),)
include $(CLEAR_VARS)

ifeq ($(TARGET_HAS_VPP),true)
LOCAL_CFLAGS += -DTARGET_HAS_VPP
endif

LOCAL_CPPFLAGS :=
LOCAL_LDFLAGS :=

LOCAL_SHARED_LIBRARIES := \
    libwrs_omxil_common \
    libva_videodecoder \
    liblog \
    libva \
    libva-android

LOCAL_C_INCLUDES := \
    $(TARGET_OUT_HEADERS)/wrs_omxil_core \
    $(TARGET_OUT_HEADERS)/khronos/openmax \
    $(PV_INCLUDES) \
    $(TARGET_OUT_HEADERS)/libmix_videodecoder \
    $(TARGET_OUT_HEADERS)/libva \
    $(call include-path-for, frameworks-native)/media/hardware \
    $(call include-path-for, frameworks-native)/media/openmax

LOCAL_SRC_FILES := \
    OMXComponentCodecBase.cpp\
    OMXVideoDecoderBase.cpp\
    OMXVideoDecoderVP8.cpp

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libOMXVideoDecoderVP8
LOCAL_MULTILIB := 32

ifeq ($(TARGET_BOARD_PLATFORM),merrifield)
LOCAL_CFLAGS += -DVED_TILING
endif

ifeq ($(TARGET_BOARD_PLATFORM),moorefield)
LOCAL_CFLAGS += -DVED_TILING
endif

PLATFORM_USE_GEN_HW := \
    baytrail \
    cherrytrail

ifneq ($(filter $(TARGET_BOARD_PLATFORM),$(PLATFORM_USE_GEN_HW)),)
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/ufo
endif

ifneq ($(filter $(TARGET_BOARD_PLATFORM),$(PLATFORM_USE_GEN_HW)),)
LOCAL_CFLAGS += -DUSE_GEN_HW
endif

ifeq ($(TARGET_BOARD_PLATFORM),baytrail)
LOCAL_CFLAGS += -DUSE_X_TILE
endif

include $(BUILD_SHARED_LIBRARY)
endif

# VP9 with SW decode and HW Render
include $(CLEAR_VARS)

ifeq ($(TARGET_HAS_VPP),true)
LOCAL_CFLAGS += -DTARGET_HAS_VPP
endif

LOCAL_SHARED_LIBRARIES := \
    libui \
    libwrs_omxil_common \
    libva_videodecoder \
    liblog \
    libva \
    libva-android \
    libva-tpi \
    libhardware

LOCAL_STATIC_LIBRARIES := \
    libvpx_internal

LOCAL_C_INCLUDES := \
    $(TARGET_OUT_HEADERS)/wrs_omxil_core \
    $(TARGET_OUT_HEADERS)/khronos/openmax \
    $(TARGET_OUT_HEADERS)/libmix_videodecoder \
    $(TARGET_OUT_HEADERS)/libva \
    $(LOCAL_PATH)/libvpx_internal/libvpx \
    $(LOCAL_PATH)/libvpx_internal/libvpx/vpx_codec \
    $(LOCAL_PATH)/libvpx_internal/libvpx/vpx_ports \
    $(call include-path-for, frameworks-native)/media/hardware \
    $(call include-path-for, frameworks-native)/media/openmax

ifeq ($(TARGET_BOARD_PLATFORM),baytrail)
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/ufo
endif

LOCAL_SRC_FILES := \
    OMXComponentCodecBase.cpp\
    OMXVideoDecoderBase.cpp\
    OMXVideoDecoderVP9HWR.cpp

LOCAL_LDFLAGS := -Wl,--no-warn-shared-textrel
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libOMXVideoDecoderVP9HWR
LOCAL_MULTILIB := 32

ifeq ($(TARGET_BOARD_PLATFORM),merrifield)
LOCAL_CFLAGS += -DVED_TILING
endif

ifeq ($(TARGET_BOARD_PLATFORM),baytrail)
LOCAL_CFLAGS += -DUSE_GEN_HW
endif

include $(BUILD_SHARED_LIBRARY)
# end VP9 SW decode and HW Render


include $(CLEAR_VARS)
ifeq ($(TARGET_HAS_VPP),true)
LOCAL_CFLAGS += -DTARGET_HAS_VPP
endif
LOCAL_CPPFLAGS :=
LOCAL_LDFLAGS :=

LOCAL_SHARED_LIBRARIES := \
    libwrs_omxil_common \
    libva_videodecoder \
    liblog \
    libva \
    libva-android

LOCAL_C_INCLUDES := \
    $(TARGET_OUT_HEADERS)/wrs_omxil_core \
    $(TARGET_OUT_HEADERS)/khronos/openmax \
    $(PV_INCLUDES) \
    $(TARGET_OUT_HEADERS)/libmix_videodecoder \
    $(TARGET_OUT_HEADERS)/libva \
    $(call include-path-for, frameworks-native)/media/hardware \
    $(call include-path-for, frameworks-native)/media/openmax

PLATFORM_USE_GEN_HW := \
    baytrail \
    cherrytrail

ifneq ($(filter $(TARGET_BOARD_PLATFORM),$(PLATFORM_USE_GEN_HW)),)
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/ufo
endif

LOCAL_SRC_FILES := \
    OMXComponentCodecBase.cpp\
    OMXVideoDecoderBase.cpp\
    OMXVideoDecoderMPEG4.cpp

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libOMXVideoDecoderMPEG4
LOCAL_MULTILIB := 32
ifeq ($(TARGET_BOARD_PLATFORM),clovertrail)
LOCAL_CFLAGS += -DVED_TILING
LOCAL_CFLAGS += -DBUF_HEIGHT_ALIGN32
endif

ifeq ($(TARGET_BOARD_PLATFORM),merrifield)
LOCAL_CFLAGS += -DVED_TILING
endif

ifeq ($(TARGET_BOARD_PLATFORM),moorefield)
LOCAL_CFLAGS += -DVED_TILING
endif

ifeq ($(TARGET_BOARD_PLATFORM),baytrail)
LOCAL_CFLAGS += -DUSE_GEN_HW
endif

ifeq ($(USE_SW_MPEG4),true)
LOCAL_CFLAGS += -DUSE_SW_MPEG4
endif

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
ifeq ($(TARGET_HAS_VPP),true)
LOCAL_CFLAGS += -DTARGET_HAS_VPP
endif
LOCAL_CPPFLAGS :=
LOCAL_LDFLAGS :=

LOCAL_SHARED_LIBRARIES := \
    libwrs_omxil_common \
    libva_videodecoder \
    liblog \
    libva \
    libva-android

LOCAL_C_INCLUDES := \
    $(TARGET_OUT_HEADERS)/wrs_omxil_core \
    $(TARGET_OUT_HEADERS)/khronos/openmax \
    $(PV_INCLUDES) \
    $(TARGET_OUT_HEADERS)/libmix_videodecoder \
    $(TARGET_OUT_HEADERS)/libva \
    $(call include-path-for, frameworks-native)/media/hardware \
    $(call include-path-for, frameworks-native)/media/openmax

PLATFORM_USE_GEN_HW := \
    baytrail \
    cherrytrail

ifneq ($(filter $(TARGET_BOARD_PLATFORM),$(PLATFORM_USE_GEN_HW)),)
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/ufo
endif

LOCAL_SRC_FILES := \
    OMXComponentCodecBase.cpp\
    OMXVideoDecoderBase.cpp\
    OMXVideoDecoderH263.cpp

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libOMXVideoDecoderH263
LOCAL_MULTILIB := 32
ifeq ($(TARGET_BOARD_PLATFORM),clovertrail)
LOCAL_CFLAGS += -DVED_TILING
LOCAL_CFLAGS += -DBUF_HEIGHT_ALIGN32
endif

ifeq ($(TARGET_BOARD_PLATFORM),merrifield)
LOCAL_CFLAGS += -DVED_TILING
endif

ifeq ($(TARGET_BOARD_PLATFORM),moorefield)
LOCAL_CFLAGS += -DVED_TILING
endif

ifeq ($(TARGET_BOARD_PLATFORM),baytrail)
LOCAL_CFLAGS += -DUSE_GEN_HW
endif

ifeq ($(USE_SW_MPEG4),true)
LOCAL_CFLAGS += -DUSE_SW_MPEG4
endif

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
ifeq ($(TARGET_HAS_VPP),true)
LOCAL_CFLAGS += -DTARGET_HAS_VPP
endif
LOCAL_CPPFLAGS :=
LOCAL_LDFLAGS :=

LOCAL_SHARED_LIBRARIES := \
    libwrs_omxil_common \
    libva_videodecoder \
    liblog \
    libva \
    libva-android

LOCAL_C_INCLUDES := \
    $(TARGET_OUT_HEADERS)/wrs_omxil_core \
    $(TARGET_OUT_HEADERS)/khronos/openmax \
    $(PV_INCLUDES) \
    $(TARGET_OUT_HEADERS)/libmix_videodecoder \
    $(TARGET_OUT_HEADERS)/libva \
    $(call include-path-for, frameworks-native)/media/hardware \
    $(call include-path-for, frameworks-native)/media/openmax

PLATFORM_USE_GEN_HW := \
    baytrail \
    cherrytrail

ifneq ($(filter $(TARGET_BOARD_PLATFORM),$(PLATFORM_USE_GEN_HW)),)
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/ufo
endif

LOCAL_SRC_FILES := \
    OMXComponentCodecBase.cpp\
    OMXVideoDecoderBase.cpp\
    OMXVideoDecoderWMV.cpp

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libOMXVideoDecoderWMV
LOCAL_MULTILIB := 32
ifeq ($(TARGET_BOARD_PLATFORM),clovertrail)
LOCAL_CFLAGS += -DVED_TILING
LOCAL_CFLAGS += -DBUF_HEIGHT_ALIGN32
endif

ifeq ($(TARGET_BOARD_PLATFORM),merrifield)
LOCAL_CFLAGS += -DVED_TILING
endif

ifeq ($(TARGET_BOARD_PLATFORM),moorefield)
LOCAL_CFLAGS += -DVED_TILING
endif

ifeq ($(TARGET_VPP_USE_GEN),true)
LOCAL_CFLAGS += -DDEINTERLACE_EXT
endif

ifeq ($(TARGET_BOARD_PLATFORM),baytrail)
LOCAL_CFLAGS += -DUSE_GEN_HW
endif

include $(BUILD_SHARED_LIBRARY)

#Build secure AVC video decoder only on supported platforms
ifeq ($(USE_INTEL_SECURE_AVC),true)

include $(CLEAR_VARS)
ifeq ($(TARGET_HAS_VPP),true)
LOCAL_CFLAGS += -DTARGET_HAS_VPP
endif
LOCAL_CPPFLAGS :=
LOCAL_LDFLAGS :=

LOCAL_SHARED_LIBRARIES := \
    libwrs_omxil_common \
    libdrm \
    libva_videodecoder \
    liblog \
    libva \
    libva-android

LOCAL_C_INCLUDES := \
    $(TARGET_OUT_HEADERS)/wrs_omxil_core \
    $(TARGET_OUT_HEADERS)/khronos/openmax \
    $(PV_INCLUDES) \
    $(TARGET_OUT_HEADERS)/libmix_videodecoder \
    $(TARGET_OUT_HEADERS)/libva \
    $(TARGET_OUT_HEADERS)/drm \
    $(TARGET_OUT_HEADERS)/libdrm \
    $(TARGET_OUT_HEADERS)/libttm \
    $(call include-path-for, frameworks-native)/media/hardware \
    $(call include-path-for, frameworks-native)/media/openmax \

LOCAL_SRC_FILES := \
    OMXComponentCodecBase.cpp\
    OMXVideoDecoderBase.cpp

ifeq ($(TARGET_BOARD_PLATFORM),clovertrail)
# Secure AVC decoder for Clovertrail (uses IMR)
LOCAL_SHARED_LIBRARIES += libsepdrm

LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/libsepdrm

LOCAL_SRC_FILES += securevideo/ctp/OMXVideoDecoderAVCSecure.cpp

LOCAL_CFLAGS += -DVED_TILING

LOCAL_CFLAGS += -DBUF_HEIGHT_ALIGN32

else ifeq ($(TARGET_BOARD_PLATFORM),merrifield)
#Secure AVC decoder for Merrifield (uses IED)
LOCAL_SHARED_LIBRARIES += \
    libcutils \
    libsepdrm_cc54 \
    libdx_cc7 \
    libsephdcp2x

LOCAL_C_INCLUDES += $(TOP)/vendor/intel/hardware/cc54/libsepdrm/libdrm/common/inc \
                    $(TOP)/vendor/intel/hardware/cc54/libsepdrm/libdrm/inc \
                    $(TOP)/vendor/intel/hardware/cc54/libsepdrm/libhdcp2x/inc

LOCAL_SRC_FILES += securevideo/merrifield/OMXVideoDecoderAVCSecure.cpp

LOCAL_CFLAGS += -DVED_TILING

else ifeq ($(TARGET_BOARD_PLATFORM),moorefield)
#Secure AVC decoder for Moorefield V0 (uses IED)
LOCAL_SHARED_LIBRARIES += \
    libcutils \
    libsepdrm_cc54 \
    libdx_cc7 \
    libsephdcp2x

LOCAL_C_INCLUDES += $(TOP)/vendor/intel/hardware/cc54/libsepdrm/libdrm/common/inc \
                    $(TOP)/vendor/intel/hardware/cc54/libsepdrm/libdrm/inc \
                    $(TOP)/vendor/intel/hardware/cc54/libsepdrm/libhdcp2x/inc

LOCAL_SRC_FILES += securevideo/moorefield/OMXVideoDecoderAVCSecure.cpp

LOCAL_CFLAGS += -DVED_TILING

else ifeq ($(TARGET_BOARD_PLATFORM),baytrail)
#Secure AVC decoder for Baytrail (uses PAVP)
LOCAL_C_INCLUDES += $(TOP)/vendor/intel/hardware/PRIVATE/ufo/inc/libpavp

LOCAL_SHARED_LIBRARIES += libpavp
LOCAL_LDLIBS += -ldl

LOCAL_SRC_FILES += securevideo/baytrail/OMXVideoDecoderAVCSecure.cpp

else ifeq ($(TARGET_BOARD_PLATFORM),cherrytrail)
#Secure AVC decoder for Cherrytrail (uses PAVP)
LOCAL_C_INCLUDES += $(TOP)/vendor/intel/hardware/PRIVATE/ufo/inc/libpavp

LOCAL_SHARED_LIBRARIES += libpavp

LOCAL_SRC_FILES += securevideo/cherrytrail/OMXVideoDecoderAVCSecure.cpp

endif

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libOMXVideoDecoderAVCSecure
LOCAL_MULTILIB := 32

include $(BUILD_SHARED_LIBRARY)

endif #USE_INTEL_SECURE_AVC

include $(CLEAR_VARS)
ifeq ($(TARGET_HAS_VPP),true)
LOCAL_CFLAGS += -DTARGET_HAS_VPP
endif
LOCAL_CPPFLAGS :=
LOCAL_LDFLAGS :=

LOCAL_SHARED_LIBRARIES := \
        libwrs_omxil_common \
        liblog \
        libva_videoencoder \
        libva \
        libva-android \
        libva-tpi \
        libutils \
        libcutils \
        libhardware \
        libintelmetadatabuffer

LOCAL_C_INCLUDES := \
    $(TARGET_OUT_HEADERS)/wrs_omxil_core \
    $(TARGET_OUT_HEADERS)/khronos/openmax \
    $(TARGET_OUT_HEADERS)/libmix_videoencoder \
    $(TARGET_OUT_HEADERS)/libva \
    $(call include-path-for, frameworks-native)/media/hardware \
    $(call include-path-for, frameworks-native)/media/openmax \

LOCAL_SRC_FILES := \
    OMXComponentCodecBase.cpp \
    OMXVideoEncoderBase.cpp \
    OMXVideoEncoderAVC.cpp

LOCAL_CFLAGS += $(LOCAL_C_FLAGS)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libOMXVideoEncoderAVC
LOCAL_MULTILIB := 32
include $(BUILD_SHARED_LIBRARY)


include $(CLEAR_VARS)
ifeq ($(TARGET_HAS_VPP),true)
LOCAL_CFLAGS += -DTARGET_HAS_VPP
endif
LOCAL_CPPFLAGS :=
LOCAL_LDFLAGS :=

LOCAL_SHARED_LIBRARIES := \
        libwrs_omxil_common \
        liblog \
        libva_videoencoder \
        libva \
        libva-android \
        libva-tpi \
        libutils \
        libcutils \
        libhardware \
        libintelmetadatabuffer

LOCAL_C_INCLUDES := \
    $(TARGET_OUT_HEADERS)/wrs_omxil_core \
    $(TARGET_OUT_HEADERS)/khronos/openmax \
    $(TARGET_OUT_HEADERS)/libmix_videoencoder \
    $(TARGET_OUT_HEADERS)/libva \
    $(call include-path-for, frameworks-native)/media/hardware \
    $(call include-path-for, frameworks-native)/media/openmax \

LOCAL_SRC_FILES := \
    OMXComponentCodecBase.cpp \
    OMXVideoEncoderBase.cpp \
    OMXVideoEncoderH263.cpp

LOCAL_CFLAGS += $(LOCAL_C_FLAGS)

ifeq ($(SW_MPEG4_ENCODER),true)
    LOCAL_CFLAGS += -DSYNC_MODE
endif

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libOMXVideoEncoderH263
LOCAL_MULTILIB := 32
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
ifeq ($(TARGET_HAS_VPP),true)
LOCAL_CFLAGS += -DTARGET_HAS_VPP
endif
LOCAL_CPPFLAGS :=
LOCAL_LDFLAGS :=

LOCAL_SHARED_LIBRARIES := \
        libwrs_omxil_common \
        liblog \
        libva_videoencoder \
        libva \
        libva-android \
        libva-tpi \
        libutils \
        libcutils \
        libhardware \
        libintelmetadatabuffer

LOCAL_C_INCLUDES := \
    $(TARGET_OUT_HEADERS)/wrs_omxil_core \
    $(TARGET_OUT_HEADERS)/khronos/openmax \
    $(TARGET_OUT_HEADERS)/libmix_videoencoder \
    $(TARGET_OUT_HEADERS)/libva \
    $(call include-path-for, frameworks-native)/media/hardware \
    $(call include-path-for, frameworks-native)/media/openmax \

LOCAL_SRC_FILES := \
    OMXComponentCodecBase.cpp \
    OMXVideoEncoderBase.cpp \
    OMXVideoEncoderMPEG4.cpp

LOCAL_CFLAGS += $(LOCAL_C_FLAGS)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libOMXVideoEncoderMPEG4
LOCAL_MULTILIB := 32
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
ifeq ($(TARGET_HAS_VPP),true)
LOCAL_CFLAGS += -DTARGET_HAS_VPP
endif
LOCAL_CPPFLAGS :=
LOCAL_LDFLAGS :=

LOCAL_SHARED_LIBRARIES := \
    libwrs_omxil_common \
    libva_videodecoder \
    liblog \
    libva \
    libva-android

LOCAL_C_INCLUDES := \
    $(TARGET_OUT_HEADERS)/wrs_omxil_core \
    $(TARGET_OUT_HEADERS)/khronos/openmax \
    $(PV_INCLUDES) \
    $(TARGET_OUT_HEADERS)/libmix_videodecoder \
    $(TARGET_OUT_HEADERS)/libva \
    $(call include-path-for, frameworks-native)/media/hardware \
    $(call include-path-for, frameworks-native)/media/openmax

LOCAL_SRC_FILES := \
    OMXComponentCodecBase.cpp\
    OMXVideoDecoderBase.cpp\
    OMXVideoDecoderPAVC.cpp

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libOMXVideoDecoderPAVC
LOCAL_MULTILIB := 32
ifeq ($(TARGET_BOARD_PLATFORM),clovertrail)
LOCAL_CFLAGS += -DVED_TILING
LOCAL_CFLAGS += -DBUF_HEIGHT_ALIGN32
endif

ifeq ($(TARGET_BOARD_PLATFORM),merrifield)
LOCAL_CFLAGS += -DVED_TILING
endif

ifeq ($(TARGET_BOARD_PLATFORM),moorefield)
LOCAL_CFLAGS += -DVED_TILING
endif

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
ifeq ($(TARGET_HAS_VPP),true)
LOCAL_CFLAGS += -DTARGET_HAS_VPP
endif
LOCAL_CPPFLAGS :=
LOCAL_LDFLAGS :=

LOCAL_SHARED_LIBRARIES := \
        libwrs_omxil_common \
        liblog \
        libva_videoencoder \
        libva \
        libva-android \
        libva-tpi \
        libutils \
        libcutils \
        libhardware \
        libintelmetadatabuffer

LOCAL_C_INCLUDES := \
    $(TARGET_OUT_HEADERS)/wrs_omxil_core \
    $(TARGET_OUT_HEADERS)/khronos/openmax \
    $(TARGET_OUT_HEADERS)/libmix_videoencoder \
    $(TARGET_OUT_HEADERS)/libva \
    $(call include-path-for, frameworks-native)/media/hardware \
    $(call include-path-for, frameworks-native)/media/openmax \

LOCAL_SRC_FILES := \
    OMXComponentCodecBase.cpp \
    OMXVideoEncoderBase.cpp \
    OMXVideoEncoderVP8.cpp

LOCAL_CFLAGS += $(LOCAL_C_FLAGS)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libOMXVideoEncoderVP8
LOCAL_MULTILIB := 32
include $(BUILD_SHARED_LIBRARY)

ifeq ($(TARGET_HAS_MULTIPLE_DISPLAY),true)
# VPP
include $(CLEAR_VARS)
ifeq ($(TARGET_HAS_VPP),true)
LOCAL_CFLAGS += -DTARGET_HAS_VPP
endif
ifeq ($(TARGET_HAS_MULTIPLE_DISPLAY),true)
LOCAL_CFLAGS += -DTARGET_HAS_MULTIPLE_DISPLAY
endif

LOCAL_CPPFLAGS :=
LOCAL_LDFLAGS :=

LOCAL_SHARED_LIBRARIES := \
        libwrs_omxil_common \
        liblog \
        libva_videoencoder \
        libva \
        libva-android \
        libva-tpi \
        libutils \
        libcutils \
        libhardware \
        libintelmetadatabuffer
LOCAL_SHARED_LIBRARIES += libbinder libmultidisplay
LOCAL_LDFLAGS += -L$(TARGET_OUT_SHARED_LIBRARIES) -livp

LOCAL_C_INCLUDES := \
    $(TARGET_OUT_HEADERS)/wrs_omxil_core \
    $(TARGET_OUT_HEADERS)/khronos/openmax \
    $(TARGET_OUT_HEADERS)/libva \
	$(TARGET_OUT_HEADERS)/libmedia_utils_vpp \
    $(call include-path-for, frameworks-native)/media/hardware \
    $(call include-path-for, frameworks-native)/media/openmax \
    $(call include-path-for, frameworks-av)/ \

LOCAL_SRC_FILES := \
    OMXComponentCodecBase.cpp \
    OMXVideoPPBase.cpp \
	PPWorker.cpp \
	VPPMds.cpp

LOCAL_CFLAGS += $(LOCAL_C_FLAGS)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libOMXVideoPP
LOCAL_MULTILIB := 32
include $(BUILD_SHARED_LIBRARY)
endif

endif
