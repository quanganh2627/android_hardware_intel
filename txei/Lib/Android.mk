LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_COPY_HEADERS_TO := libtxei
LOCAL_COPY_HEADERS := inc/txei.h
include $(BUILD_COPY_HEADERS)

include $(CLEAR_VARS)
LOCAL_MODULE := libtxei
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := txei_lib.c
LOCAL_SHARED_LIBRARIES := libcutils libc
LOCAL_C_INCLUDES := $(LOCAL_PATH)/inc
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libtxei
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := txei_lib.c
LOCAL_STATIC_LIBRARIES := libcutils libc
LOCAL_C_INCLUDES := $(LOCAL_PATH)/inc
include $(BUILD_STATIC_LIBRARY)

#**************************************************
#
#       Build the UMIP_ACCESS library
#
#**************************************************
include $(CLEAR_VARS)
LOCAL_MODULE := CC6_TXEI_UMIP_ACCESS
LOCAL_MODULE_TAGS := optional
LOCAL_COPY_HEADERS_TO := libtxei
LOCAL_COPY_HEADERS := \
    sec_tool_lib/inc/ExtApp_qa_op_code.h \
    sec_tool_lib/inc/umip_access.h \
    sec_tool_lib/inc/acd_reference.h \
    sec_tool_lib/inc/chaabi_error_codes.h \
    sec_tool_lib/inc/acds_module_error.h
LOCAL_SRC_FILES += \
    txei_lib.c \
    sec_tool_lib/src/umip_access.c \
    common/src/tee_if.c
LOCAL_CFLAGS := -DBAYTRAIL -DACD_WIPE_TEST
LOCAL_SHARED_LIBRARIES := libcutils libc libtxei
LOCAL_C_INCLUDES := $(LOCAL_PATH)/inc \
    $(LOCAL_PATH)/sec_tool_lib/inc \
    $(LOCAL_PATH)/common/inc
include $(BUILD_STATIC_LIBRARY)

#####################
#  Utilities library (static)
#
include $(CLEAR_VARS)
LOCAL_MODULE := libtxeimiscutils
LOCAL_MODULE_TAGS := optional
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_SRC_FILES := sec_tool_lib/src/misc_utils.c
LOCAL_STATIC_LIBRARIES := \
    libcutils \
    libc \
    libtxei
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/inc \
    $(LOCAL_PATH)/sec_tool_lib/inc \
    $(LOCAL_PATH)/common/inc
include $(BUILD_STATIC_LIBRARY)

#####################
#  IPT_OTP libraries and Sep service
#

include $(addprefix $(LOCAL_PATH)/,$(addsuffix /Android.mk, IPT_OTP service))
