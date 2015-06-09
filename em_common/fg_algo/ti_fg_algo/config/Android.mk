LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE:= fg_config_ti_swfg.bin
LOCAL_MODULE_CLASS:= ETC
LOCAL_MODULE_TAGS:= optional
include $(BUILD_SYSTEM)/base_rules.mk

FG_CONFIG_TI_SWFG_XML := $(LOCAL_PATH)/fg_config_ti_swfg.xml
FG_CONFIG_TI_SWFG_PY := $(LOCAL_PATH)/fg_config_parser.py

$(LOCAL_BUILT_MODULE): $(FG_CONFIG_TI_SWFG_XML) $(FG_CONFIG_TI_SWFG_PY)
	$(hide) mkdir -p $(dir $@)
	$(hide) python $(FG_CONFIG_TI_SWFG_PY) $< $@
