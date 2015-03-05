#
# Copyright 2010 Intrinsyc Software International, Inc.  All rights reserved.
#
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    rillog.cpp \
    extract.cpp \
    util.cpp \
    repository.cpp

LOCAL_IMPORT_C_INCLUDE_DIRS_FROM_SHARED_LIBRARIES += libtcs

LOCAL_SHARED_LIBRARIES := \
    libutils libcutils libmmgrcli libtcs

# Activating this macro enables the optional Video Telephony feature
ifeq ($(strip $(M2_VT_FEATURE_ENABLED)),true)
LOCAL_CFLAGS += -DM2_VT_FEATURE_ENABLED
endif

ifeq ($(strip $(M2_GET_SIM_SMS_STORAGE_ENABLED)),true)
LOCAL_CFLAGS += -DM2_GET_SIM_SMS_STORAGE_ENABLED
endif

LOCAL_C_INCLUDES :=  \
    $(LOCAL_PATH)/../../INC \
    $(LOCAL_PATH)/../../CORE \
    $(LOCAL_PATH)/../../CORE/ND \
    $(LOCAL_PATH)/../../OEM/ND \
    $(TARGET_OUT_HEADERS)/libtcs \
    $(TARGET_OUT_HEADERS)/IFX-modem

#build shared library
LOCAL_PRELINK_MODULE := false
LOCAL_STRIP_MODULE := true
LOCAL_CFLAGS += -DRIL_SHLIB -Os
LOCAL_MODULE:= librapid-ril-util
LOCAL_MODULE_TAGS:= optional
include $(BUILD_SHARED_LIBRARY)

