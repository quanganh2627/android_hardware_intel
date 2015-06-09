LOCAL_PATH:= $(call my-dir)

# Build for target
include $(CLEAR_VARS)
LOCAL_COPY_HEADERS_TO := full_rw
LOCAL_COPY_HEADERS :=  full_rw.h
LOCAL_SRC_FILES := full_rw.c
LOCAL_MODULE := libfull_rw
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS += -Werror -Wall
LOCAL_C_INCLUDES += bionic
include $(BUILD_STATIC_LIBRARY)

# Build for host
include $(CLEAR_VARS)
LOCAL_COPY_HEADERS_TO := full_rw
LOCAL_COPY_HEADERS :=  full_rw.h
LOCAL_SRC_FILES := full_rw.c
LOCAL_MODULE := libfull_rw_host
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS += -Werror -Wall
LOCAL_C_INCLUDES += bionic
include $(BUILD_HOST_STATIC_LIBRARY)
