#pragma once

#include "jni_md.h"
#include <jni.h>
#include <jvmti.h>

struct JvmtiHandle {
  static void setTransformerManager(jobject transformerManagerIn);
  static void retransformClasses(JNIEnv *jnienv, jvmtiEnv *retransformerEnv,
                          jobjectArray classes);
  static void JNICALL vmClassFileLoadHook(jvmtiEnv *jvmti_env, JNIEnv *jni_env,
                                   jclass class_being_redefined, jobject loader,
                                   const char *name, jobject protection_domain,
                                   jint class_data_len,
                                   const unsigned char *class_data,
                                   jint *new_class_data_len,
                                   unsigned char **new_class_data);
  static void printStackTrace(JNIEnv *jniEnv);
};