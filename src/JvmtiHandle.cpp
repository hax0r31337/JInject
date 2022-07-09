#include "JvmtiHandle.h"
#include "jni.h"
#include "JInject.h"
#if OS_LINUX
#include "jni_md.h"
#endif
#include <cstddef>
#include <iostream>
#include <stdlib.h>
#include <string.h>

static jobject transformerManager = nullptr;

void JvmtiHandle::setTransformerManager(jobject transformerManagerIn) {
  transformerManager = transformerManagerIn;
}

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
    jvmtiEnv *jvmtiEnv, JNIEnv *jniEnv, jclass class_being_redefined,
    jobject loader, const char *name, jobject protection_domain,
    jint class_data_len, const unsigned char *class_data,
    jint *new_class_data_len, unsigned char **new_class_data) {
  if (transformerManager == nullptr) {
    return;
  }
  jclass transformerManagerCls = jniEnv->GetObjectClass(transformerManager);
  jmethodID transformMethod = jniEnv->GetMethodID(transformerManagerCls, "transform", "(Ljava/lang/String;[B)[B");

  jbyteArray classFileBufferObject = jniEnv->NewByteArray(class_data_len);
  jbyte *typedBuffer =
      (jbyte *)class_data; /* nasty cast, dumb JNI interface, const missing */
                           /* The sign cast is safe. The const cast is dumb. */
  jniEnv->SetByteArrayRegion(classFileBufferObject, 0,
                                class_data_len, typedBuffer);
  jbyteArray transformResult = (jbyteArray)jniEnv->CallObjectMethod(transformerManager, transformMethod, jniEnv->NewStringUTF(name), classFileBufferObject);
  if (transformResult == NULL) {
    return;
  }
  jint transformedBufferSize = jniEnv->GetArrayLength(transformResult);
  unsigned char *resultBuffer = NULL;
  jvmtiEnv->Allocate(transformedBufferSize, &resultBuffer);
  jniEnv->GetByteArrayRegion(transformResult, 0, transformedBufferSize, (jbyte *) resultBuffer);

  *new_class_data = resultBuffer;
  *new_class_data_len = transformedBufferSize;
  
  std::cout << name << std::endl;
}

void JvmtiHandle::printStackTrace(JNIEnv *jniEnv) {
  jthrowable exception = jniEnv->ExceptionOccurred();
  jniEnv->ExceptionClear();
  jclass throwableCls = jniEnv->FindClass("java/lang/Throwable");
  jmethodID printStackTrace =
      jniEnv->GetMethodID(throwableCls, "printStackTrace", "()V");
  jniEnv->CallVoidMethod(exception, printStackTrace);
}