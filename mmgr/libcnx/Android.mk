LOCAL_PATH:= $(call my-dir)

#############################################
# General rules:
#############################################
MY_MODULE := libmmgr_cnx
MY_MODULE_TAGS := optional

MY_SRC_FILES := $(call all-c-files-under, .)

MY_INCLUDES := \
    $(LOCAL_PATH)/../inc \

MY_C_FLAGS := -Wall -Werror -Wvla -DMODULE_NAME=\"MMGR_CNX\"
MY_SHARED_LIBS := libcutils libc
MY_LOCAL_IMPORT := libmmgr_utils

#############################################
# MODEM MANAGER C connection library
#############################################
include $(CLEAR_VARS)
LOCAL_MODULE := $(MY_MODULE)
LOCAL_MODULE_TAGS := $(MY_MODULE_TAGS)

LOCAL_C_INCLUDES := $(MY_INCLUDES)
LOCAL_SRC_FILES := $(MY_SRC_FILES)
LOCAL_CFLAGS += $(MY_C_FLAGS)

LOCAL_IMPORT_C_INCLUDE_DIRS_FROM_SHARED_LIBRARIES := $(MY_LOCAL_IMPORT)
LOCAL_SYSTEM_SHARED_LIBRARIES := $(MY_SHARED_LIBS)
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)
include $(BUILD_SHARED_LIBRARY)

#############################################
# MODEM MANAGER C client connection - GCOV
#############################################
ifeq ($(mmgr_gcov), true)

include $(CLEAR_VARS)
LOCAL_MODULE := $(addsuffix -gcov, $(MY_MODULE))
LOCAL_MODULE_TAGS := $(MY_MODULE_TAGS)

LOCAL_C_INCLUDES := $(MY_INCLUDES)
LOCAL_SRC_FILES := $(MY_SRC_FILES)
LOCAL_CFLAGS += $(MY_C_FLAGS) -fprofile-arcs -ftest-coverage
LOCAL_LDFLAGS += -fprofile-arcs -ftest-coverage -lgcov

LOCAL_IMPORT_C_INCLUDE_DIRS_FROM_SHARED_LIBRARIES := $(addsuffix -gcov, $(MY_LOCAL_IMPORT))
LOCAL_SYSTEM_SHARED_LIBRARIES := $(MY_SHARED_LIBS)
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)
include $(BUILD_SHARED_LIBRARY)

endif
