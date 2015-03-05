LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

#
# UIM Application
#

LOCAL_C_INCLUDES:= uim.h

LOCAL_SRC_FILES:= \
	uim.c
LOCAL_CFLAGS:= -m32
LOCAL_SHARED_LIBRARIES:= libnetutils liblog
LOCAL_MODULE:=uim
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)
