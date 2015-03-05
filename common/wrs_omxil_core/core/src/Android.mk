LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	wrs_omxcore.cpp

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libwrs_omxil_core_pvwrapped

LOCAL_CFLAGS := $(OMXLOG_CFLAGS) -DLOG_TAG=\"omxil-core\"
LOCAL_CPPFLAGS :=

LOCAL_LDFLAGS :=

LOCAL_SHARED_LIBRARIES := \
	libwrs_omxil_common \
	libdl \
	liblog

LOCAL_C_INCLUDES := \
	$(WRS_OMXIL_CORE_ROOT)/utils/inc \
	$(WRS_OMXIL_CORE_ROOT)/base/inc \
	$(WRS_OMXIL_CORE_ROOT)/core/inc/khronos/openmax/include \
	$(PV_INCLUDES)

include $(BUILD_SHARED_LIBRARY)
