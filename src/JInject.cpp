#include "FileChooser.h"
#include "JarLoader.h"
#include "JvmtiHandle.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <jni.h>
#include <jvmti.h>
#include <thread>
#include "JInject.h"
#if OS_LINUX
#include <jni_md.h>
#else
#include <windows.h>
#endif

void MainThread() {
  JavaVM *jvm = nullptr;
  JNIEnv *jniEnv = nullptr;
  jvmtiEnv *jvmti = nullptr;

  jsize count;
  if (JNI_GetCreatedJavaVMs(&jvm, 1, &count) != JNI_OK || count == 0) {
    std::cout << "[ERROR] No JVM Found" << std::endl;
    return; // no active JVMs
  }

  // obtain JniEnv pointer
  jint result = jvm->GetEnv((void **)&jniEnv, JNI_VERSION_1_6);
  if (result == JNI_EDETACHED)
    result = jvm->AttachCurrentThread((void **)&jniEnv, nullptr);
  if (result != JNI_OK) {
    std::cout << "[ERROR] Failed to obtain JNIEnv!" << std::endl;
    return;
  }
  
  result = jvm->GetEnv((void **)&jvmti, JVMTI_VERSION_1_1);
  if (result != JNI_OK) {
    std::cout << "[ERROR] Failed to obtain JvmtiEnv!" << std::endl;
    return;
  }

  jclass JOptionPane = jniEnv->FindClass("javax/swing/JOptionPane");
  jmethodID showMessageDialog =
      jniEnv->GetStaticMethodID(JOptionPane, "showMessageDialog",
                             "(Ljava/awt/Component;Ljava/lang/Object;)V");

  jobject fileChoosed = FileChooser::ChooseFile(jniEnv);
  if (fileChoosed == nullptr) {
    jniEnv->CallStaticObjectMethod(JOptionPane, showMessageDialog, NULL,
                                jniEnv->NewStringUTF("Action cancelled"));
    return;
  }
  
  JarLoader::loadJar(jniEnv, jvmti, fileChoosed);
  bool invokeResult = JarLoader::tryInvokeMain(jniEnv, jvmti, fileChoosed);
  if (!invokeResult) {
    jniEnv->CallStaticObjectMethod(JOptionPane, showMessageDialog, NULL,
                                jniEnv->NewStringUTF("Unable to invoke main class"));
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
PVOID WINAPI hook(PVOID arg) {
  MainThread();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved) {
  DisableThreadLibraryCalls(hModule);

  switch (dwReason) {
  case DLL_PROCESS_ATTACH:
    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)hook, NULL, 0, NULL);
    break;
  }

  return TRUE;
}
#endif
