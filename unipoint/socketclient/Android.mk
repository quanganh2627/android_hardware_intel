LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
LOCAL_SRC_FILES := \
socketclient.c
 

LOCAL_CFLAGS := \
    -std=gnu99 -pedantic
# LOCAL_SHARED_LIBRARIES := libconfig libsvm libdbus


LOCAL_C_INCLUDES := \
        $(LOCAL_PATH)/include \
	..\include 
#       $(call include-path-for, dbus) \
        $(call include-path-for, dbus)/dbus \
        external/libsvm 
LOCAL_MODULE := socketclient
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)
