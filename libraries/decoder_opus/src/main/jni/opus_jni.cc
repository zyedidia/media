/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef __ANDROID__
#include <android/log.h>
#endif
#include <jni.h>

#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "opus.h"              // NOLINT
#include "opus_multistream.h"  // NOLINT

extern "C" void* opus_box_realloc(void*, size_t);
extern "C" void* opus_box_malloc(size_t);
extern "C" void opus_box_free(void*);

#ifdef __ANDROID__
#define LOG_TAG "opus_jni"
#define LOGE(...) \
  ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__))
#define LOGI(...) \
  ((void)__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__))
#else  //  __ANDROID__
#define LOGE(...) \
  do {            \
  } while (0)
#endif  //  __ANDROID__

#define DECODER_FUNC(RETURN_TYPE, NAME, ...)                                  \
  extern "C" {                                                                \
  JNIEXPORT RETURN_TYPE Java_androidx_media3_decoder_opus_OpusDecoder_##NAME( \
      JNIEnv* env, jobject thiz, ##__VA_ARGS__);                              \
  }                                                                           \
  JNIEXPORT RETURN_TYPE Java_androidx_media3_decoder_opus_OpusDecoder_##NAME( \
      JNIEnv* env, jobject thiz, ##__VA_ARGS__)

#define LIBRARY_FUNC(RETURN_TYPE, NAME, ...)                                  \
  extern "C" {                                                                \
  JNIEXPORT RETURN_TYPE Java_androidx_media3_decoder_opus_OpusLibrary_##NAME( \
      JNIEnv* env, jobject thiz, ##__VA_ARGS__);                              \
  }                                                                           \
  JNIEXPORT RETURN_TYPE Java_androidx_media3_decoder_opus_OpusLibrary_##NAME( \
      JNIEnv* env, jobject thiz, ##__VA_ARGS__)

// JNI references for SimpleOutputBuffer class.
static jmethodID outputBufferInit;

jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  JNIEnv* env;
  if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
    return -1;
  }
  return JNI_VERSION_1_6;
}

static const int kBytesPerIntPcmSample = 2;
static const int kBytesPerFloatSample = 4;
static const int kMaxOpusOutputPacketSizeSamples = 960 * 6;
static int channelCount;
static int errorCode;
static bool outputFloat = false;

DECODER_FUNC(jlong, opusInit, jint sampleRate, jint channelCount,
             jint numStreams, jint numCoupled, jint gain,
             jbyteArray jStreamMap) {
  int* opus_status = (int*) opus_box_malloc(sizeof(int));
  *opus_status = OPUS_INVALID_STATE;
  ::channelCount = channelCount;
  errorCode = 0;
  jbyte* streamMapBytes = env->GetByteArrayElements(jStreamMap, 0);
  uint8_t* streamMap = reinterpret_cast<uint8_t*>(streamMapBytes);
  size_t streamMapSize = sizeof(jbyte) * env->GetArrayLength(jStreamMap);
  uint8_t* opus_streamMap = (uint8_t*) opus_box_malloc(streamMapSize);
  memcpy(opus_streamMap, streamMap, streamMapSize);
  OpusMSDecoder* decoder = opus_multistream_decoder_create(
      sampleRate, channelCount, numStreams, numCoupled, opus_streamMap, opus_status);
  opus_box_free(opus_streamMap);
  env->ReleaseByteArrayElements(jStreamMap, streamMapBytes, 0);
  if (!decoder || *opus_status != OPUS_OK) {
    LOGE("Failed to create Opus Decoder; status=%s", opus_strerror(*opus_status));
    return 0;
  }
  int status = opus_multistream_decoder_ctl(decoder, OPUS_SET_GAIN(gain));
  if (status != OPUS_OK) {
    LOGE("Failed to set Opus header gain; status=%s", opus_strerror(status));
    return 0;
  }

  // Populate JNI References.
  const jclass outputBufferClass =
      env->FindClass("androidx/media3/decoder/SimpleDecoderOutputBuffer");
  outputBufferInit =
      env->GetMethodID(outputBufferClass, "init", "(JI)Ljava/nio/ByteBuffer;");

  LOGI("opus-lfi: initialized sandboxed decoder: %p", decoder);

  return reinterpret_cast<intptr_t>(decoder);
}

static _Thread_local void* opus_outputBufferData;
static _Thread_local size_t opus_outputBufferDataSize;
static _Thread_local void* opus_inputBuffer;
static _Thread_local size_t opus_inputBufferSize;

DECODER_FUNC(jint, opusDecode, jlong jDecoder, jlong jTimeUs,
             jobject jInputBuffer, jint inputSize, jobject jOutputBuffer) {
  OpusMSDecoder* decoder = reinterpret_cast<OpusMSDecoder*>(jDecoder);

  const uint8_t* inputBuffer = reinterpret_cast<const uint8_t*>(
      env->GetDirectBufferAddress(jInputBuffer));
  if (inputSize > opus_inputBufferSize) {
    opus_inputBuffer = opus_box_realloc(opus_inputBuffer, inputSize);
    opus_inputBufferSize = inputSize;
    LOGI("opus-lfi: (re)allocated sandboxed inputBuffer %p", opus_inputBuffer);
  }
  memcpy(opus_inputBuffer, inputBuffer, inputSize);

  const int byteSizePerSample =
      outputFloat ? kBytesPerFloatSample : kBytesPerIntPcmSample;
  const jint outputSize =
      kMaxOpusOutputPacketSizeSamples * byteSizePerSample * channelCount;

  env->CallObjectMethod(jOutputBuffer, outputBufferInit, jTimeUs, outputSize);
  if (env->ExceptionCheck()) {
    // Exception is thrown in Java when returning from the native call.
    return -1;
  }
  const jobject jOutputBufferData = env->CallObjectMethod(
      jOutputBuffer, outputBufferInit, jTimeUs, outputSize);
  if (env->ExceptionCheck()) {
    // Exception is thrown in Java when returning from the native call.
    return -1;
  }

  int sampleCount;
  if (outputFloat) {
    float* outputBufferData = reinterpret_cast<float*>(
        env->GetDirectBufferAddress(jOutputBufferData));
    if (outputSize > opus_outputBufferDataSize) {
      opus_outputBufferData = opus_box_realloc(opus_outputBufferData, outputSize);
      opus_outputBufferDataSize = outputSize;
      LOGI("opus-lfi: (re)allocated sandboxed outputBufferData %p", opus_outputBufferData);
    }
    sampleCount = opus_multistream_decode_float(
        decoder, (const uint8_t*) opus_inputBuffer, inputSize, (float*) opus_outputBufferData,
        kMaxOpusOutputPacketSizeSamples, 0);
    memcpy(outputBufferData, opus_outputBufferData, outputSize);
  } else {
    int16_t* outputBufferData = reinterpret_cast<int16_t*>(
        env->GetDirectBufferAddress(jOutputBufferData));
    if (outputSize > opus_outputBufferDataSize) {
      opus_outputBufferData = opus_box_realloc(opus_outputBufferData, outputSize);
      opus_outputBufferDataSize = outputSize;
      LOGI("opus-lfi: (re)allocated sandboxed outputBufferData %p", opus_outputBufferData);
    }
    sampleCount = opus_multistream_decode(decoder, (const uint8_t*) opus_inputBuffer, inputSize,
                                          (int16_t*) opus_outputBufferData,
                                          kMaxOpusOutputPacketSizeSamples, 0);
    memcpy(outputBufferData, opus_outputBufferData, outputSize);
  }

  // record error code
  errorCode = (sampleCount < 0) ? sampleCount : 0;
  return (sampleCount < 0) ? sampleCount
                           : sampleCount * byteSizePerSample * channelCount;
}

DECODER_FUNC(jint, opusSecureDecode, jlong jDecoder, jlong jTimeUs,
             jobject jInputBuffer, jint inputSize, jobject jOutputBuffer,
             jint sampleRate, jobject mediaCrypto, jint inputMode,
             jbyteArray key, jbyteArray javaIv, jint inputNumSubSamples,
             jintArray numBytesOfClearData, jintArray numBytesOfEncryptedData) {
  // Doesn't support
  // Java client should have checked vpxSupportSecureDecode
  // and avoid calling this
  // return -2 (DRM Error)
  return -2;
}

DECODER_FUNC(void, opusClose, jlong jDecoder) {
  OpusMSDecoder* decoder = reinterpret_cast<OpusMSDecoder*>(jDecoder);
  opus_multistream_decoder_destroy(decoder);
}

DECODER_FUNC(void, opusReset, jlong jDecoder) {
  OpusMSDecoder* decoder = reinterpret_cast<OpusMSDecoder*>(jDecoder);
  opus_multistream_decoder_ctl(decoder, OPUS_RESET_STATE);
}

DECODER_FUNC(jstring, opusGetErrorMessage, jlong jContext) {
  return env->NewStringUTF(opus_strerror(errorCode));
}

DECODER_FUNC(jint, opusGetErrorCode, jlong jContext) { return errorCode; }

DECODER_FUNC(void, opusSetFloatOutput) { outputFloat = true; }

LIBRARY_FUNC(jstring, opusIsSecureDecodeSupported) {
  // Doesn't support
  return 0;
}

LIBRARY_FUNC(jstring, opusGetVersion) {
  return env->NewStringUTF(opus_get_version_string());
}
