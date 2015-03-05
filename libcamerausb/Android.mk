ifeq ($(USE_CAMERA_STUB),false)
ifeq ($(USE_CAMERA_USB), true)
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

ifeq ($(USE_INTEL_METABUFFER),true)
LOCAL_CFLAGS += -DENABLE_INTEL_METABUFFER
endif

ifeq ($(USE_60HZ_POWER_LINE_FREQUENCY),true)
LOCAL_CFLAGS += -DCONFIG_60HZ
endif

LOCAL_SRC_FILES := \
        DumpImage.cpp \
	ControlThread.cpp \
	PreviewThread.cpp \
	PictureThread.cpp \
	VideoThread.cpp \
	PipeThread.cpp \
	CameraDriver.cpp \
	DebugFrameRate.cpp \
	Callbacks.cpp \
	CallbacksThread.cpp \
	CameraHAL.cpp \
	ColorConverter.cpp \
        VAConvertor.cpp \
        EXIFFields.cpp \
	JpegCompressor.cpp \
	CameraBuffer.cpp \
	CameraBufferAllocator.cpp \
        GraphicBufferAllocator.cpp \
	JpegEncoder.cpp \
        SWJpegEncoder.cpp

LOCAL_C_INCLUDES += \
	$(call include-path-for, frameworks-base) \
	$(call include-path-for, frameworks-base)/binder \
	$(call include-path-for, frameworks-base)/camera \
	$(call include-path-for, system-core)/cutils \
	$(call include-path-for, jpeg) \
	$(call include-path-for, libhardware) \
	$(call include-path-for, skia)/core \
	$(call include-path-for, skia)/images \
        $(call include-path-for, libhardware) \
        $(call include-path-for, stlport) \
	$(TARGET_OUT_HEADERS)/libdrm \
	$(TARGET_OUT_HEADERS)/libmix_videoencoder \
        $(TARGET_OUT_HEADERS)/libva \
        $(TARGET_OUT_HEADERS)/libmix_videovpp \
        $(TARGET_OUT_HEADERS)/libjpegdec \
	bionic \

LOCAL_SHARED_LIBRARIES := \
	libcamera_client \
	libutils \
	libcutils \
	libbinder \
	libskia \
	libandroid \
	libui \
	libdrm \
	libdrm_intel \
	libintelmetadatabuffer \
	libstlport \
	libva-android \
	libva \
        libmix_videovpp \
        libjpegdec \
        libhardware \
        libjpeg

ifeq ($(USE_INTEL_METABUFFER),true)
LOCAL_SHARED_LIBRARIES += \
        libintelmetadatabuffer
endif

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_MODULE := camera.$(TARGET_DEVICE)
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

include $(call all-makefiles-under,$(LOCAL_PATH))
endif #ifeq ($(USE_CAMERA_STUB),false)
endif #ifeq ($(USE_CAMERA_USB), true)
