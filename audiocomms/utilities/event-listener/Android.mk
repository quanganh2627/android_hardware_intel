LOCAL_PATH := $(call my-dir)

# Common variables
##################

event_thread_src_files := \
    EventThread.cpp

event_thread_includes_target := \
        $(call include-path-for, stlport) \
        bionic

event_thread_cflags := -DDEBUG

# Build for target
##################

include $(CLEAR_VARS)
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)

LOCAL_SRC_FILES := $(event_thread_src_files)

LOCAL_C_INCLUDES += $(event_thread_includes_target)

LOCAL_CFLAGS := $(event_thread_cflags)

LOCAL_SHARED_LIBRARIES := libstlport libcutils

LOCAL_MODULE := libevent-listener
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

# Build for host test
#####################

include $(CLEAR_VARS)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)

LOCAL_SRC_FILES := $(event_thread_src_files)

LOCAL_MODULE := libevent-listener_static_host
LOCAL_MODULE_TAGS := optional

include $(BUILD_HOST_STATIC_LIBRARY)

# Build static lib
###########################

include $(CLEAR_VARS)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)

LOCAL_SRC_FILES := $(event_thread_src_files)

LOCAL_C_INCLUDES += $(event_thread_includes_target)

LOCAL_MODULE := libevent-listener_static
LOCAL_MODULE_TAGS := optional

include $(BUILD_STATIC_LIBRARY)

