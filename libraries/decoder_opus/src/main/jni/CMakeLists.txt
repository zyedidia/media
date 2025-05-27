#
# Copyright 2024 The Android Open Source Project
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

cmake_minimum_required(VERSION 3.21.0 FATAL_ERROR)

# Enable C++11 features.
set(CMAKE_CXX_STANDARD 11)

# Define project name for your JNI module
project(libopusJNI C CXX)

enable_language(ASM)

set(libopus_jni_root "${CMAKE_CURRENT_SOURCE_DIR}")

# Add the include directory from libopus.
include_directories ("${libopus_jni_root}/libopus/include")

# Build libopusJNI.
add_library(opusV2JNI
            SHARED
            gen/embed/lib/includes.c
            gen/embed/lib/lib.c
            gen/embed/lib/arch/arm64/callback.S
            gen/embed/lib/arch/arm64/trampolines.S
            opus_jni.cc)

target_include_directories(opusV2JNI
    PRIVATE
        gen/embed/lib
)

# Locate NDK log library.
find_library(android_log_lib log)

# Link libopusJNI against used libraries.
target_link_libraries(opusV2JNI
                      PRIVATE android
                      PRIVATE lfi
                      PRIVATE ${android_log_lib})

# Enable 16 KB ELF alignment.
target_link_options(opusV2JNI
                    PRIVATE "-Wl,-z,max-page-size=16384")

