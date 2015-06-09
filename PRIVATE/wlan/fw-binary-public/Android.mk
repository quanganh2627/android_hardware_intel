LOCAL_PATH := $(my-dir)
#android default path
TARGET_OUT_WLAN_FW ?= $(TARGET_OUT_ETC)/firmware

#making sure that we dont brake the byt build.
include $(CLEAR_VARS)
LOCAL_MODULE := wifi_intel_wkp
LOCAL_MODULE_TAGS := optional
LOCAL_REQUIRED_MODULES := \
     wifi_intel_device
include $(BUILD_PHONY_PACKAGE)

# need to put the iwlwifi FW into the FS
include $(CLEAR_VARS)
LOCAL_MODULE := wifi_intel_device
LOCAL_MODULE_TAGS := optional
LOCAL_REQUIRED_MODULES := \
     iwlwifi-7260-9.ucode \
     iwlwifi-3160-9.ucode \
     iwlwifi-a620-9.ucode \
     usniffer-iwlwifi-7260-9.ucode \
     usniffer-iwlwifi-3160-9.ucode \
     usniffer-iwlwifi-a620-9.ucode \
     iwl-dbg-cfg.ini \
     nvmData \
     fw_info.txt

# WARNING: To be kept as the last required module.
LOCAL_REQUIRED_MODULES +=  \
    wifi_common
include $(BUILD_PHONY_PACKAGE)

include $(CLEAR_VARS)
LOCAL_MODULE := usniffer-iwlwifi-7260-9.ucode
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_WLAN_FW)
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := usniffer-iwlwifi-3160-9.ucode
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_WLAN_FW)
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := usniffer-iwlwifi-a620-9.ucode
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_WLAN_FW)
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := iwlwifi-7260-9.ucode
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_WLAN_FW)
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := iwlwifi-3160-9.ucode
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_WLAN_FW)
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := iwl-dbg-cfg.ini
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_WLAN_FW)
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := iwlwifi-a620-9.ucode
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_WLAN_FW)
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := nvmData
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_WLAN_FW)
LOCAL_SRC_FILES := nvmData-a620
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := fw_info.txt
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_WLAN_FW)
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)
