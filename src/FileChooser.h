#pragma once

#include "jni_md.h"
#include <jni.h>
#include <jvmti.h>

struct FileChooser {
  /**
   * @return selected file, nullptr when action cancelled
   */
  static jobject ChooseFile(JNIEnv *jniEnv);
};