LOCAL_PATH := $(call my-dir)

###########################
# convert static lib target

include $(CLEAR_VARS)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/

LOCAL_MODULE := libaudio_comms_convert

include $(BUILD_STATIC_LIBRARY)

#########################
# convert static lib host

include $(CLEAR_VARS)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/

LOCAL_MODULE := libaudio_comms_convert_host

include $(BUILD_HOST_STATIC_LIBRARY)

