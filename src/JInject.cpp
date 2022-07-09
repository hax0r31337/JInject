#include "FileChooser.h"
#include "JarLoader.h"
#include "jni_md.h"
#include "JvmtiHandle.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <jni.h>
#include <jvmti.h>
#include <thread>

#define OS_LINUX true

void MainThread() {
  JavaVM *jvm = nullptr;
  JNIEnv *env = nullptr;
  jvmtiEnv *jvmti = nullptr;

  jsize count;
  if (JNI_GetCreatedJavaVMs(&jvm, 1, &count) != JNI_OK || count == 0)
    return; // no active JVMs

  // obtain JniEnv pointer
  jint result = jvm->GetEnv((void **)&env, JNI_VERSION_1_6);
  if (result == JNI_EDETACHED)
    result = jvm->AttachCurrentThread((void **)&env, nullptr);
  if (result != JNI_OK) {
    std::cout << "[ERROR] Failed to obtain JNIEnv!" << std::endl;
    return;
  }
  
  result = jvm->GetEnv((void **)&jvmti, JVMTI_VERSION_1_1);
  if (result != JNI_OK) {
    std::cout << "[ERROR] Failed to obtain JvmtiEnv!" << std::endl;
    return;
  }
  jvmtiCapabilities desiredCapabilities;
  result = jvmti->GetCapabilities(&desiredCapabilities);
  if (result != JVMTI_ERROR_NONE) {
    std::cout << "[ERROR] Failed to get capabilities code=" << result
              << std::endl;
    return;
  }
  desiredCapabilities.can_redefine_classes = 1;
  desiredCapabilities.can_retransform_classes = 1;
  desiredCapabilities.can_redefine_any_class = 1;
  desiredCapabilities.can_retransform_any_class = 1;
  result = jvmti->AddCapabilities(&desiredCapabilities);
  if (result != JVMTI_ERROR_NONE) {
    std::cout << "[ERROR] Failed to add capabilities code=" << result << std::endl;
    return;
  }

  jvmtiEventCallbacks callbacks;
  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.ClassFileLoadHook = JvmtiHandle::vmClassFileLoadHook;
  jvmti->SetEventCallbacks(&callbacks, sizeof(callbacks));
  jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, NULL);

  jclass JOptionPane = env->FindClass("javax/swing/JOptionPane");
  jmethodID showMessageDialog =
      env->GetStaticMethodID(JOptionPane, "showMessageDialog",
                             "(Ljava/awt/Component;Ljava/lang/Object;)V");

  jobject fileChoosed = FileChooser::ChooseFile(env);
  if (fileChoosed == nullptr) {
    env->CallStaticObjectMethod(JOptionPane, showMessageDialog, NULL,
                                env->NewStringUTF("Action cancelled"));
    return;
  }
  
  jobject urlClassLoader = JarLoader::loadJar(env, fileChoosed);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    env->CallStaticObjectMethod(JOptionPane, showMessageDialog, NULL,
                                env->NewStringUTF("Exception occur"));
  }
  bool invokeResult = JarLoader::tryInvokeMain(env, jvmti, urlClassLoader, fileChoosed);
  if (!invokeResult) {
    env->CallStaticObjectMethod(JOptionPane, showMessageDialog, NULL,
                                env->NewStringUTF("Unable to invoke main class"));
  }

  // jvm->DetachCurrentThread();
}

#if OS_LINUX

/* Entrypoint to the Library. Called when loading */
int __attribute__((constructor)) Startup() {
  std::thread mainThread(MainThread);
  // The root of all suffering is attachment
  // Therefore our little buddy must detach from this realm.
  // Farewell my thread, may we join again some day..
  mainThread.detach();

  return 0;
}

#else
// TODO: winapi DLLMain
#endif
