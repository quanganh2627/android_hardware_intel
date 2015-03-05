# Author: tianyang.zhu@intel.com

LOCAL_PATH:= $(call my-dir)

ifeq ($(TARGET_HAS_MULTIPLE_DISPLAY),true)

include $(CLEAR_VARS)
LOCAL_COPY_HEADERS_TO := display
LOCAL_COPY_HEADERS := \
    native/include/IExtendDisplayListener.h \
    native/include/IMultiDisplayComposer.h \
    native/include/MultiDisplayClient.h \
    native/include/MultiDisplayComposer.h \
    native/include/MultiDisplayType.h \
    native/include/MultiDisplayService.h

include $(BUILD_COPY_HEADERS)

include $(CLEAR_VARS)
LOCAL_SRC_FILES:= \
    native/MultiDisplayService.cpp \
    native/MultiDisplayClient.cpp \
    native/IMultiDisplayComposer.cpp \
    native/MultiDisplayComposer.cpp \
    native/IExtendDisplayListener.cpp

LOCAL_MODULE:= libmultidisplay
LOCAL_MODULE_TAGS := optional

LOCAL_SHARED_LIBRARIES := \
    libui libcutils libutils libbinder
LOCAL_CFLAGS := -DLOG_TAG=\"MultiDisplay\"

ifeq ($(ENABLE_IMG_GRAPHICS),true)
    LOCAL_SRC_FILES += \
        native/drm_hdmi.cpp

    LOCAL_C_INCLUDES = \
        $(TARGET_OUT_HEADERS)/libdrm \
        $(TARGET_OUT_HEADERS)/pvr/pvr2d \
        $(TARGET_OUT_HEADERS)/libttm

    LOCAL_SHARED_LIBRARIES += \
         libdrm

    LOCAL_CFLAGS += -DENABLE_DRM
    LOCAL_CFLAGS += -DDVI_SUPPORTED
    LOCAL_SHARED_LIBRARIES += libdl
endif

ifeq ($(ENABLE_HDCP),true)
    LOCAL_SHARED_LIBRARIES += libsepdrm
    LOCAL_SRC_FILES += native/drm_hdcp.cpp
    LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/libspedrm
    LOCAL_CFLAGS += -DENABLE_HDCP
endif

include $(BUILD_SHARED_LIBRARY)

# Build JNI library
include $(CLEAR_VARS)

LOCAL_SRC_FILES := jni/com_intel_multidisplay_DisplaySetting.cpp

LOCAL_MODULE:= libmultidisplayjni
LOCAL_MODULE_TAGS:=optional

LOCAL_SHARED_LIBRARIES := \
     libcutils \
     libutils \
     libbinder \
     libandroid_runtime \
     libmultidisplay \
     libnativehelper

LOCAL_C_INCLUDES := \
     $(JNI_H_INCLUDE) \
     $(call include-path-for, frameworks-base)

include $(BUILD_SHARED_LIBRARY)

# ============================================================
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
     java/com/intel/multidisplay/DisplaySetting.java \
     java/com/intel/multidisplay/DisplayObserver.java

LOCAL_MODULE:= com.intel.multidisplay
LOCAL_MODULE_TAGS:=optional

LOCAL_JNI_SHARED_LIBRARIES := libmultidisplayjni

LOCAL_NO_EMMA_INSTRUMENT := true
LOCAL_NO_EMMA_COMPILE := true

include $(BUILD_JAVA_LIBRARY)

else
# ============================================================
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
     dummy/DisplayObserver.java

LOCAL_MODULE := com.intel.multidisplay
LOCAL_MODULE_TAGS := optional

LOCAL_NO_EMMA_INSTRUMENT := true
LOCAL_NO_EMMA_COMPILE := true

include $(BUILD_JAVA_LIBRARY)

endif

# ===========================================================
# Declare the library to the framework by copying it to /system/etc/permissions directory.
include $(CLEAR_VARS)

LOCAL_MODULE := com.intel.multidisplay.xml

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE_CLASS := ETC

# This will install the file in /system/etc/permissions
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)/permissions

LOCAL_SRC_FILES := $(LOCAL_MODULE)

include $(BUILD_PREBUILT)

include $(BUILD_DROIDDOC)
