LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
#
LOCAL_LIB_DIR := $(LOCAL_PATH)
#
LOCAL_COPY_HEADERS_TO := libtxei
LOCAL_COPY_HEADERS += inc/txei.h

LOCAL_SRC_FILES += txei_lib.c
#
LOCAL_SHARED_LIBRARIES := libcutils libc
#
LOCAL_C_INCLUDES := $(LOCAL_PATH)/inc
#
LOCAL_MODULE := libtxei
#
LOCAL_MODULE_TAGS := eng
#
include $(BUILD_SHARED_LIBRARY)
include $(CLEAR_VARS)
#
LOCAL_LIB_DIR := $(LOCAL_PATH)
#
LOCAL_COPY_HEADERS_TO := libtxei
LOCAL_COPY_HEADERS += inc/txei.h

LOCAL_SRC_FILES += txei_lib.c
#
LOCAL_STATIC_LIBRARIES := libcutils libc
#
LOCAL_C_INCLUDES := $(LOCAL_PATH)/inc
#
LOCAL_MODULE := libtxei
#
LOCAL_MODULE_TAGS := eng
#
include $(BUILD_STATIC_LIBRARY)

#**************************************************
#
#       Build the UMIP_ACCESS library
#
#**************************************************
include $(CLEAR_VARS)

LOCAL_LIB_DIR := $(LOCAL_PATH)
LOCAL_SEC_DIR := sec_tool_lib
LOCAL_COMMON_DIR := common

LOCAL_COPY_HEADERS_TO := libtxei
LOCAL_COPY_HEADERS := inc/txei.h	\
sec_tool_lib/inc/ExtApp_qa_op_code.h	\
sec_tool_lib/inc/umip_access.h \
sec_tool_lib/inc/acd_reference.h \
sec_tool_lib/inc/chaabi_error_codes.h \
sec_tool_lib/inc/acds_module_error.h \

LOCAL_SRC_FILES += txei_lib.c   \
$(LOCAL_SEC_DIR)/src/umip_access.c                   \
$(LOCAL_COMMON_DIR)/src/tee_if.c

LOCAL_CFLAGS := -DBAYTRAIL -DACD_WIPE_TEST

LOCAL_STATIC_LIBRARIES := CC6_ALL_BASIC_LIB
LOCAL_SHARED_LIBRARIES := libcutils libc libtxei

LOCAL_C_INCLUDES := $(LOCAL_PATH)/inc/	\
$(LOCAL_PATH)/sec_tool_lib/inc/                           \
$(LOCAL_PATH)/common/inc/

LOCAL_MODULE := CC6_TXEI_UMIP_ACCESS
LOCAL_MODULE_TAGS := optional

include $(BUILD_STATIC_LIBRARY)

#####################
#  Utilities library (static)
#
include $(CLEAR_VARS)
LOCAL_FORCE_STATIC_EXECUTABLE := true

LOCAL_LIB_DIR := $(LOCAL_PATH)
LOCAL_SEC_DIR := sec_tool_lib
LOCAL_COMMON_DIR := common

LOCAL_SRC_FILES := $(LOCAL_SEC_DIR)/src/misc_utils.c

LOCAL_STATIC_LIBRARIES :=			\
	libcutils				\
	libc					\
	libtxei

LOCAL_C_INCLUDES := $(LOCAL_PATH)/inc/	\
$(LOCAL_PATH)/sec_tool_lib/inc/                           \
$(LOCAL_PATH)/common/inc/

LOCAL_MODULE := libtxeimiscutils

LOCAL_MODULE_TAGS := eng

include $(BUILD_STATIC_LIBRARY)

#####################
#  IPT_OTP libraries and Sep service
#
subdirs := $(addprefix $(LOCAL_PATH)/,$(addsuffix /Android.mk,  \
        IPT_OTP                                                 \
        service                                                 \
        ))
 
include $(subdirs)
