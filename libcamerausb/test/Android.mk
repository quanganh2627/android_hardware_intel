# Build the unit tests.
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

# Build the unit tests.
test_src_files := \
    camtest_Features.cpp \

shared_libraries := \
    libcutils \
    libutils \
    libandroid \
    libstlport \

static_libraries := \
    libgtest \
    libgtest_main

c_includes := \
    bionic \
    $(call include-path-for, libstdc++) \
    $(call include-path-for, gtest) \
    $(call include-path-for, stlport) \
    $(call include-path-for, frameworks-base)/camera \
    $(call include-path-for, libhardware)/hardware \
    $(call include-path-for, system-core) \

module_tags := eng tests debug

$(foreach file,$(test_src_files), \
    $(eval include $(CLEAR_VARS)) \
    $(eval LOCAL_SHARED_LIBRARIES := $(shared_libraries)) \
    $(eval LOCAL_STATIC_LIBRARIES := $(static_libraries)) \
    $(eval LOCAL_C_INCLUDES := $(c_includes)) \
    $(eval LOCAL_SRC_FILES := $(file)) \
    $(eval LOCAL_MODULE := $(notdir $(file:%.cpp=%))) \
    $(eval LOCAL_MODULE_TAGS := $(module_tags)) \
    $(eval include $(BUILD_EXECUTABLE)) \
)

include $(call all-makefiles-under, $(LOCAL_PATH))
