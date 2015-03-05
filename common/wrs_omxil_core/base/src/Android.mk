LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	cmodule.cpp \
	componentbase.cpp \
	portbase.cpp \
	portvideo.cpp \

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libwrs_omxil_base

LOCAL_CPPFLAGS :=

LOCAL_LDFLAGS :=

LOCAL_CFLAGS := $(OMXLOG_CFLAGS) -DLOG_TAG=\"omxil-base\"

ifeq ($(strip $(COMPONENT_SUPPORT_BUFFER_SHARING)), true)
LOCAL_CFLAGS += -DCOMPONENT_SUPPORT_BUFFER_SHARING
endif
ifeq ($(strip $(COMPONENT_SUPPORT_OPENCORE)), true)
LOCAL_CFLAGS += -DCOMPONENT_SUPPORT_OPENCORE
endif


LOCAL_C_INCLUDES := \
	$(WRS_OMXIL_CORE_ROOT)/utils/inc \
	$(WRS_OMXIL_CORE_ROOT)/base/inc \
	$(WRS_OMXIL_CORE_ROOT)/core/inc/khronos/openmax/include

include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libwrs_omxil_common


LOCAL_WHOLE_STATIC_LIBRARIES := \
	libwrs_omxil_utils \
	libwrs_omxil_base

LOCAL_SHARED_LIBRARIES := \
	libdl \
	liblog

include $(BUILD_SHARED_LIBRARY)
