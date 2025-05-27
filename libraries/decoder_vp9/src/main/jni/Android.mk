#
# Copyright (C) 2016 The Android Open Source Project
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
#

WORKING_DIR := $(call my-dir)
include $(CLEAR_VARS)
LIBVPX_ROOT := $(WORKING_DIR)/libvpx

# build libvpx.so
# LOCAL_PATH := $(WORKING_DIR)
# include libvpx.mk

# build libvpxV2JNI.so
include $(CLEAR_VARS)
LOCAL_PATH := $(WORKING_DIR)
LOCAL_MODULE := libvpxV2JNI
LOCAL_ARM_MODE := arm
LOCAL_CPP_EXTENSION := .cc
LOCAL_C_INCLUDES := gen/embed/lib $(LOCAL_PATH)/libvpx $(LOCAL_PATH)/libvpx_android_configs/$(TARGET_ARCH_ABI)
LOCAL_SRC_FILES := vpx_jni.cc gen/embed/lib/includes.c gen/embed/lib/lib.c gen/embed/lib/arch/arm64/callback.S gen/embed/lib/arch/arm64/trampolines.S
LOCAL_LDLIBS := -llog -lz -lm -landroid /home/zyedidia/programming/media/libraries/decoder_vp9/src/main/obj/local/arm64-v8a/liblfi.a
LOCAL_STATIC_LIBRARIES := cpufeatures
# Enable 16 KB ELF alignment.
LOCAL_LDFLAGS += "-Wl,-z,max-page-size=16384"
include $(BUILD_SHARED_LIBRARY)

$(call import-module,android/cpufeatures)
