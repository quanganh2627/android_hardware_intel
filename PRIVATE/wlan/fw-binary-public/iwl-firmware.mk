TARGET_OUT_WLAN_FW := $(TARGET_OUT)/system/lib/firmware
TARGET_OUT_ETC_WIFI := $(TARGET_OUT)/system/etc/wifi

IWL_UCODE_FILES := $(notdir $(wildcard $(LOCAL_PATH)/*.ucode))
IWL_PAPD_DB_FILES := $(notdir $(shell find $(LOCAL_PATH)/papd_db -type f))

#copy the nvmData file and all the ucode files to $(TARGET_OUT_WLAN_FW)
PRODUCT_COPY_FILES := \
	$(LOCAL_PATH)/nvmData-a620:$(TARGET_OUT_WLAN_FW)/nvmData \
	$(LOCAL_PATH)/nvmData-a620:$(TARGET_OUT_ETC_WIFI)/nvmDataDefault \
	$(LOCAL_PATH)/fw_info.txt:$(TARGET_OUT_WLAN_FW)/fw_info.txt \
	$(LOCAL_PATH)/iwl-dbg-cfg.ini:$(TARGET_OUT_WLAN_FW)/iwl-dbg-cfg.ini \
	$(foreach ucode,$(IWL_UCODE_FILES),\
		$(LOCAL_PATH)/$(ucode):$(TARGET_OUT_WLAN_FW)/$(ucode)) \
	$(foreach file,$(IWL_PAPD_DB_FILES),\
                $(LOCAL_PATH)/papd_db/$(file):$(TARGET_OUT_ETC_WIFI)/papd_db/$(file))
