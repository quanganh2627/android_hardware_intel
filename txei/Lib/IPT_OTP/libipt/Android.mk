LOCAL_PATH := $(call my-dir)

#####################
#  libipt library (shared)
#
include $(CLEAR_VARS)
LOCAL_MODULE := libsepipt
LOCAL_MODULE_TAGS := optional
LOCAL_COPY_HEADERS_TO := libsepipt
LOCAL_EXPORT_C_INCLUDE_DIRS := inc
LOCAL_SRC_FILES := \
    ../../txei_lib.c \
    ../../common/src/tee_if.c \
    src/ipt_tee_interface.c \
    src/ipt.c \
    src/mvfw_api.c
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../../inc \
    $(LOCAL_PATH)/../../sec_tool_lib/inc \
    $(LOCAL_PATH)/../../common/inc \
    $(LOCAL_PATH)/inc
LOCAL_SHARED_LIBRARIES := libtxei libcutils libc
include $(BUILD_SHARED_LIBRARY)
