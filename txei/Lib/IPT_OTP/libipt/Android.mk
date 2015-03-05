LOCAL_PATH:= $(call my-dir)

#####################
#  libipt library (shared)
#
include $(CLEAR_VARS)

LOCAL_MODULE := libsepipt
LOCAL_MODULE_TAGS := eng

IPT_OTP_DIR  := ..
TXEI_LIB_DIR := $(IPT_OTP_DIR)/..

LOCAL_COPY_HEADERS_TO := libsepipt
LOCAL_EXPORT_C_INCLUDE_DIRS := inc

#Intel IPT library sources
LOCAL_SRC_FILES += \
	$(TXEI_LIB_DIR)/txei_lib.c          \
	$(TXEI_LIB_DIR)/common/src/tee_if.c \
	src/ipt_tee_interface.c             \
	src/ipt.c                           \
	src/mvfw_api.c

#Intel IPT library headers
LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/$(TXEI_LIB_DIR)/inc              \
	$(LOCAL_PATH)/$(TXEI_LIB_DIR)/sec_tool_lib/inc \
	$(LOCAL_PATH)/$(TXEI_LIB_DIR)/common/inc       \
	$(LOCAL_PATH)/inc

#LOCAL_CFLAGS += -D

LOCAL_SHARED_LIBRARIES += libtxei libcutils libc

include $(BUILD_SHARED_LIBRARY)
