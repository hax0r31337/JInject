#include "JarLoader.h"
#include "JvmtiHandle.h"
#include "jni.h"
#include "jvmti.h"
#include <cstddef>
#include <cstring>
#include <iostream>
#include <ostream>

void JarLoader::loadJar(JNIEnv *jniEnv, jvmtiEnv *jvmtiEnv, jobject file) {
  jclass fileCls = jniEnv->FindClass("java/io/File");
  jmethodID getName = jniEnv->GetMethodID(fileCls, "getAbsolutePath", "()Ljava/lang/String;");
  jstring name = (jstring)jniEnv->CallObjectMethod(file, getName);
  auto nameUTF = jniEnv->GetStringUTFChars(name, 0);
  jvmtiEnv->AddToSystemClassLoaderSearch(nameUTF);
  // jvmtiEnv->AddToBootstrapClassLoaderSearch(nameUTF);
  jniEnv->ReleaseStringUTFChars(name, nameUTF);
}

bool JarLoader::tryInvokeMain(JNIEnv *jniEnv, jvmtiEnv *jvmtiEnv, jobject file) {
  jclass fileInputStreamCls = jniEnv->FindClass("java/io/FileInputStream");
  jmethodID fileInputStreamInit = jniEnv->GetMethodID(fileInputStreamCls, "<init>", "(Ljava/io/File;)V");
  jclass jarInputStreamCls = jniEnv->FindClass("java/util/jar/JarInputStream");
  jmethodID jarInputStreamInit = jniEnv->GetMethodID(jarInputStreamCls, "<init>", "(Ljava/io/InputStream;)V");
  jobject fileInputStream = jniEnv->NewObject(fileInputStreamCls, fileInputStreamInit, file);
  jobject jarInputStream = jniEnv->NewObject(jarInputStreamCls, jarInputStreamInit, fileInputStream);

  jmethodID getManifest = jniEnv->GetMethodID(jarInputStreamCls, "getManifest", "()Ljava/util/jar/Manifest;");
  jobject manifest = jniEnv->CallObjectMethod(jarInputStream, getManifest);
  if (manifest == NULL || manifest == nullptr) {
    std::cout << "[ERROR] No Manifest found" << std::endl;
    return false;
  }
  jclass manifestCls = jniEnv->GetObjectClass(manifest);
  jmethodID getMainAttributes = jniEnv->GetMethodID(manifestCls, "getMainAttributes", "()Ljava/util/jar/Attributes;");
  jobject mainAttributes = jniEnv->CallObjectMethod(manifest, getMainAttributes);
  jclass attributesCls = jniEnv->GetObjectClass(mainAttributes);

  jmethodID getValue = jniEnv->GetMethodID(attributesCls, "getValue", "(Ljava/lang/String;)Ljava/lang/String;");

  jstring value = (jstring)jniEnv->CallObjectMethod(
      mainAttributes, getValue, jniEnv->NewStringUTF("JIAgent-Class"));
  if (value != NULL && jniEnv->GetStringLength(value) != 0) {
    jclass klass = jniEnv->FindClass(JarLoader::toClassName(jniEnv, value));
    if (klass == NULL || klass == nullptr) {
      std::cout << "[ERROR] Failed to load defined class (Main)" << std::endl;
      return false;
    }
    return JarLoader::callJIAgentMain(jniEnv, jvmtiEnv, klass);
  }

  value = (jstring)jniEnv->CallObjectMethod(mainAttributes, getValue, jniEnv->NewStringUTF("Main-Class"));
  if (value != NULL && jniEnv->GetStringLength(value) != 0) {
    jclass klass = jniEnv->FindClass(JarLoader::toClassName(jniEnv, value));
    if (klass == NULL || klass == nullptr) {
      std::cout << "[ERROR] Failed to load defined class (Main)" << std::endl;
      return false;
    }
    return JarLoader::callMain(jniEnv, klass);
  }

  std::cout << "[ERROR] No invokeable class found" << std::endl;
  return false;
}

bool JarLoader::callJIAgentMain(JNIEnv *jniEnv, jvmtiEnv *jvmtiEnv, jclass klass) {
  jvmtiCapabilities desiredCapabilities;
  jint result = jvmtiEnv->GetCapabilities(&desiredCapabilities);
  if (result != JVMTI_ERROR_NONE) {
    std::cout << "[ERROR] Failed to get capabilities code=" << result
              << std::endl;
    return false;
  }
  desiredCapabilities.can_redefine_classes = 1;
  desiredCapabilities.can_retransform_classes = 1;
  desiredCapabilities.can_redefine_any_class = 1;
  desiredCapabilities.can_retransform_any_class = 1;
  result = jvmtiEnv->AddCapabilities(&desiredCapabilities);
  if (result != JVMTI_ERROR_NONE) {
    std::cout << "[ERROR] Failed to add capabilities code=" << result
              << std::endl;
    return false;
  }

  jvmtiEventCallbacks callbacks;
  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.ClassFileLoadHook = JvmtiHandle::vmClassFileLoadHook;
  jvmtiEnv->SetEventCallbacks(&callbacks, sizeof(callbacks));
  jvmtiEnv->SetEventNotificationMode(JVMTI_ENABLE,
                                  JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, NULL);
  
  jclass transformerManagerCls = jniEnv->FindClass("me/kotone/jisdk/TransformerManager");
  if (!transformerManagerCls) {
    std::cout << "[ERROR] No JISDK class found" << std::endl;
    return false;
  }
  jmethodID transformerManagerInit = jniEnv->GetMethodID(transformerManagerCls, "<init>", "()V");
  jobject instance = jniEnv->NewObject(transformerManagerCls, transformerManagerInit);
  JvmtiHandle::setTransformerManager(instance);

  jmethodID mainMethod =
      jniEnv->GetStaticMethodID(klass, "main", "(Lme/kotone/jisdk/TransformerManager;)[Ljava/lang/Class;");
  if (!mainMethod || jniEnv->ExceptionCheck()) {
    JvmtiHandle::printStackTrace(jniEnv);
    std::cout << "[ERROR] No main method found" << std::endl;
    return false; // MethodNotFoundException
  }
  jobjectArray classesNeedRetransform = (jobjectArray)jniEnv->CallStaticObjectMethod(klass, mainMethod, instance);
  if (jniEnv->ExceptionCheck()) {
    JvmtiHandle::printStackTrace(jniEnv);
    std::cout << "[ERROR] Exception occur in main method" << std::endl;
    return false;
  }

  JvmtiHandle::retransformClasses(jniEnv, jvmtiEnv, classesNeedRetransform);

  return true;
  }

bool JarLoader::callMain(JNIEnv *jniEnv, jclass klass) {
  jclass stringCls = jniEnv->FindClass("java/lang/String");

  jmethodID mainMethod =
      jniEnv->GetStaticMethodID(klass, "main", "([Ljava/lang/String;)V");
  if (!mainMethod || jniEnv->ExceptionCheck()) {
    JvmtiHandle::printStackTrace(jniEnv);
    std::cout << "[ERROR] No main method found" << std::endl;
    return false; // MethodNotFoundException
  }
  jobjectArray array = jniEnv->NewObjectArray(0, stringCls, NULL);
  jniEnv->CallStaticVoidMethod(klass, mainMethod, array);
  if (jniEnv->ExceptionCheck()) {
    JvmtiHandle::printStackTrace(jniEnv);
    std::cout << "[ERROR] Exception occur in main method" << std::endl;
    return false;
  }
  return true;
}

const char* JarLoader::toClassName(JNIEnv *jniEnv, jstring str) {
  jclass stringCls = jniEnv->FindClass("java/lang/String");
  jmethodID replaceAll = jniEnv->GetMethodID(stringCls, "replaceAll", "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");
  jstring replaced = (jstring)jniEnv->CallObjectMethod(str, replaceAll, jniEnv->NewStringUTF("\\."), jniEnv->NewStringUTF("/"));
  const char* result = jniEnv->GetStringUTFChars(replaced, 0);
  std::cout << result << std::endl;
  return result;
}