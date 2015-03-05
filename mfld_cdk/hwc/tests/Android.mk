LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	surface.cpp

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libutils \
	libbinder \
    libui \
    libgui

LOCAL_MODULE:= hwc-overlay

LOCAL_MODULE_TAGS := eng

include $(BUILD_EXECUTABLE)
