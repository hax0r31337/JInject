#pragma once

#include "JInject.h"
#if OS_LINUX
#include <jni_md.h>
#endif
#include <jni.h>
#include <jvmti.h>

struct JarLoader {
  public:
  /**
   * @return UrlClassLoader instance
   */
  static void loadJar(JNIEnv *jniEnv, jvmtiEnv *jvmtiEnv, jobject file);

  /**
   * @return false if failed to locate the main class
   */
  static bool tryInvokeMain(JNIEnv *jniEnv, jvmtiEnv *jvmtiEnv, jobject file);

private:
  static bool callJIAgentMain(JNIEnv *jniEnv, jvmtiEnv *jvmtiEnv, jclass klass);
  static bool callMain(JNIEnv *jniEnv, jclass klass);
  static const char* toClassName(JNIEnv *jniEnv, jstring str);
};