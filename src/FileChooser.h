#pragma once

#include "JInject.h"
#if OS_LINUX
#include "jni_md.h"
#endif
#include <jni.h>
#include <jvmti.h>

struct FileChooser {
  /**
   * @return selected file, nullptr when action cancelled
   */
  static jobject ChooseFile(JNIEnv *jniEnv);
};