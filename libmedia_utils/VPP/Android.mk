ifeq ($(TARGET_HAS_VPP),true)

LOCAL_PATH := $(call my-dir)

#### first ####

include $(CLEAR_VARS)
LOCAL_SRC_FILES:= \
        VPPSetting.cpp

LOCAL_COPY_HEADERS_TO := libmedia_utils_vpp

LOCAL_COPY_HEADERS := \
    VPPSetting.h \

LOCAL_CFLAGS += -DTARGET_HAS_VPP -Wno-non-virtual-dtor

LOCAL_MODULE:=  libvpp_setting
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)


#### second ####

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
        VPPProcessor.cpp \
        VPPProcThread.cpp \
        VPPWorker.cpp \
        NuPlayerVPPProcessor.cpp

LOCAL_C_INCLUDES:= \
        $(call include-path-for, frameworks-av) \
        $(call include-path-for, frameworks-native) \
        $(call include-path-for, frameworks-native)/media/openmax \
        $(TARGET_OUT_HEADERS)/libva

LOCAL_SHARED_LIBRARIES := libva libvpp_setting

ifeq ($(TARGET_HAS_MULTIPLE_DISPLAY),true)
LOCAL_CFLAGS += -DTARGET_HAS_MULTIPLE_DISPLAY
ifeq ($(USE_MDS_LEGACY),true)
LOCAL_CFLAGS += -DUSE_MDS_LEGACY
endif
LOCAL_SHARED_LIBRARIES += libmultidisplay
endif

LOCAL_COPY_HEADERS_TO := libmedia_utils_vpp

LOCAL_COPY_HEADERS := \
    VPPProcessor.h \
    VPPBuffer.h \
    VPPProcThread.h \
    VPPWorker.h \
    NuPlayerVPPProcessor.h

LOCAL_CFLAGS += -DTARGET_HAS_VPP -Wno-non-virtual-dtor
ifeq ($(TARGET_VPP_USE_GEN),true)
	LOCAL_CFLAGS += -DTARGET_VPP_USE_GEN
endif

LOCAL_MODULE:=  libvpp
LOCAL_MODULE_TAGS := optional

include $(BUILD_STATIC_LIBRARY)

endif

