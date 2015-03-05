# Copyright (C) 2012 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH := $(call my-dir)

# Build only when BOARD_USES_ALSA_AUDIO is set
ifeq ($(BOARD_USES_ALSA_AUDIO), true)

include $(CLEAR_VARS)

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_CFLAGS := -D_POSIX_C_SOURCE=200809

ifeq ($(BOARD_USES_TINY_ALSA_AUDIO), true)

LOCAL_SRC_FILES := \
        tinyaudio_hw.c

LOCAL_C_INCLUDES += \
	$(call include-path-for, tinyalsa)

LOCAL_SHARED_LIBRARIES := liblog libcutils libtinyalsa

else

LOCAL_SRC_FILES := \
        audio_hw.c

LOCAL_C_INCLUDES += \
        $(call include-path-for, alsa-lib)

LOCAL_SHARED_LIBRARIES := liblog libcutils libasound

endif #BOARD_USES_TINY_ALSA_AUDIO

LOCAL_MODULE := audio.hdmi.$(TARGET_DEVICE)
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
endif
