#Android Make file to build bcu_cpufreqrel Executable
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_SRC_FILES:= bcu_cpufreq_release.c
LOCAL_MODULE:= bcu_cpufreqrel
LOCAL_MODULE_TAGS:= optional
LOCAL_SHARED_LIBRARIES:= liblog libcutils
include $(BUILD_EXECUTABLE)

