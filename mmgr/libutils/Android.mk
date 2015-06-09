LOCAL_PATH:= $(call my-dir)

#############################################
# General rules:
#############################################
MY_MODULE := libmmgr_utils
MY_MODULE_TAGS := optional

MY_SRC_FILES := $(call all-c-files-under, .)
MY_INCLUDES := \
    $(MMGR_PATH)/inc \
    $(TARGET_OUT_HEADERS)/libtcs \

MY_C_FLAGS := -Wall -Werror -Wvla -DMODULE_NAME=\"MMGR_UTILS\" -std=c99
ifneq ($(TARGET_BUILD_TYPE), user)
MY_C_FLAGS += -DMMGR_DEBUG
endif
MY_SHARED_LIBS := libcutils libc

#############################################
# MODEM MANAGER C utils library
#############################################
include $(CLEAR_VARS)
LOCAL_MODULE := $(MY_MODULE)
LOCAL_MODULE_TAGS := $(MY_MODULE_TAGS)

LOCAL_C_INCLUDES := $(MY_INCLUDES)
LOCAL_SRC_FILES := $(MY_SRC_FILES)
LOCAL_CFLAGS += $(MY_C_FLAGS)
LOCAL_COPY_HEADERS := $(call all-h-files-under, .)
LOCAL_COPY_HEADERS_TO := telephony/libmmgr_utils

LOCAL_SHARED_LIBRARIES := $(MY_SHARED_LIBS)
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)
include $(BUILD_SHARED_LIBRARY)

#############################################
# MODEM MANAGER C utils library - GCOV
#############################################
ifeq ($(mmgr_gcov), true)

include $(CLEAR_VARS)
LOCAL_MODULE := $(addsuffix -gcov, $(MY_MODULE))
LOCAL_MODULE_TAGS := $(MY_MODULE_TAGS)

LOCAL_C_INCLUDES := $(MY_INCLUDES)
LOCAL_SRC_FILES := $(MY_SRC_FILES)
LOCAL_CFLAGS += $(MY_C_FLAGS) -fprofile-arcs -ftest-coverage

LOCAL_LDFLAGS += -fprofile-arcs -ftest-coverage -lgcov
LOCAL_SHARED_LIBRARIES := $(MY_SHARED_LIBS)
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)
include $(BUILD_SHARED_LIBRARY)

endif
