#include "jvmtiHandle.h"
#include <stdlib.h>
#include <string.h>

void JvmtiHandle::retransformClasses(JNIEnv *jnienv, jvmtiEnv *retransformerEnv,
                        jobjectArray classes) {
  jsize numClasses = jnienv->GetArrayLength(classes);
  jclass *classArray =
      (jclass *)alloca(numClasses * sizeof(jclass*));

  jint index;
  for (index = 0; index < numClasses; index++) {
    classArray[index] = (jclass)jnienv->GetObjectArrayElement(classes, index);
  }

  retransformerEnv->RetransformClasses(numClasses, classArray);
}

void JNICALL JvmtiHandle::vmClassFileLoadHook(
    jvmtiEnv *jvmti_env, JNIEnv *jni_env, jclass class_being_redefined,
    jobject loader, const char *name, jobject protection_domain,
    jint class_data_len, const unsigned char *class_data,
    jint *new_class_data_len, unsigned char **new_class_data) {}