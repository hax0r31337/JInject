#include "JarLoader.h"
#include "JvmtiHandle.h"
#include "jni.h"
#include "jvmti.h"
#include <cstring>
#include <iostream>

jobject JarLoader::loadJar(JNIEnv *jniEnv, jobject file) {
  jclass fileCls = jniEnv->FindClass("java/io/File");
  jmethodID toURI = jniEnv->GetMethodID(fileCls, "toURI", "()Ljava/net/URI;");
  jobject uri = jniEnv->CallObjectMethod(file, toURI);
  jclass uriCls = jniEnv->GetObjectClass(uri);
  jmethodID toURL = jniEnv->GetMethodID(uriCls, "toURL", "()Ljava/net/URL;");
  jobject url = jniEnv->CallObjectMethod(uri, toURL);
  jclass urlCls = jniEnv->GetObjectClass(url);
  jobjectArray array = jniEnv->NewObjectArray(1, urlCls, url);
  jniEnv->SetObjectArrayElement(array, 0, url);

  jmethodID getClass = jniEnv->GetMethodID(fileCls, "getClass", "()Ljava/lang/Class;");
  jobject klass = jniEnv->CallObjectMethod(file, getClass);
  jclass klassCls = jniEnv->GetObjectClass(klass);
  jmethodID getClassLoader = jniEnv->GetMethodID(klassCls, "getClassLoader", "()Ljava/lang/ClassLoader;");
  jobject classLoader = jniEnv->CallObjectMethod(klass, getClassLoader);

  jclass urlClassLoaderCls = jniEnv->FindClass("java/net/URLClassLoader");
  jmethodID urlClassLoaderInit = jniEnv->GetMethodID(urlClassLoaderCls, "<init>", "([Ljava/net/URL;Ljava/lang/ClassLoader;)V");
  return jniEnv->NewObject(urlClassLoaderCls, urlClassLoaderInit, array, classLoader);
}

bool JarLoader::tryInvokeMain(JNIEnv *jniEnv, jvmtiEnv *jvmtiEnv, jobject classLoader, jobject file) {
  jclass fileInputStreamCls = jniEnv->FindClass("java/io/FileInputStream");
  jmethodID fileInputStreamInit = jniEnv->GetMethodID(fileInputStreamCls, "<init>", "(Ljava/io/File;)V");
  jclass jarInputStreamCls = jniEnv->FindClass("java/util/jar/JarInputStream");
  jmethodID jarInputStreamInit = jniEnv->GetMethodID(jarInputStreamCls, "<init>", "(Ljava/io/InputStream;)V");
  jobject fileInputStream = jniEnv->NewObject(fileInputStreamCls, fileInputStreamInit, file);
  jobject jarInputStream = jniEnv->NewObject(jarInputStreamCls, jarInputStreamInit, fileInputStream);

  jmethodID getManifest = jniEnv->GetMethodID(jarInputStreamCls, "getManifest", "()Ljava/util/jar/Manifest;");
  jobject manifest = jniEnv->CallObjectMethod(jarInputStream, getManifest);
  jclass manifestCls = jniEnv->GetObjectClass(manifest);
  jmethodID getMainAttributes = jniEnv->GetMethodID(manifestCls, "getMainAttributes", "()Ljava/util/jar/Attributes;");
  jobject mainAttributes = jniEnv->CallObjectMethod(manifest, getMainAttributes);
  jclass attributesCls = jniEnv->GetObjectClass(mainAttributes);

  jmethodID getValue = jniEnv->GetMethodID(attributesCls, "getValue", "(Ljava/lang/String;)Ljava/lang/String;");
  jstring emptyString = jniEnv->NewStringUTF("");
  jclass stringCls = jniEnv->FindClass("java/lang/String");
  jmethodID equals = jniEnv->GetMethodID(stringCls, "equals", "(Ljava/lang/Object;)Z");

  jstring value = (jstring)jniEnv->CallObjectMethod(
      mainAttributes, getValue, jniEnv->NewStringUTF("JIAgent-Class"));
  jboolean equalResult = jniEnv->CallBooleanMethod(value, equals, emptyString);
  if (!equalResult) {
    jclass klass = JarLoader::getClass(jniEnv, classLoader, value);
    if (klass == nullptr) {
      std::cout << "[ERROR] Failed to load defined class ("
                << jniEnv->GetStringUTFChars(value, 0) << ")" << std::endl;
      return false;
    }
    return JarLoader::callJIAgentMain(jniEnv, jvmtiEnv, klass);
  }

  value = (jstring)jniEnv->CallObjectMethod(mainAttributes, getValue, jniEnv->NewStringUTF("Main-Class"));
  equalResult = jniEnv->CallBooleanMethod(value, equals, emptyString);
  if (!equalResult) {
    jclass klass = JarLoader::getClass(jniEnv, classLoader, value);
    if (klass == nullptr) {
      std::cout << "[ERROR] Failed to load defined class ("
                << jniEnv->GetStringUTFChars(value, 0) << ")" << std::endl;
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
  JvmtiHandle::transformerManager = instance;

  jmethodID mainMethod =
      jniEnv->GetStaticMethodID(klass, "main", "(Lme/kotone/jisdk/TransformerManager;)[Ljava/lang/Class;");
  if (!mainMethod || jniEnv->ExceptionCheck()) {
    jniEnv->ExceptionClear();
    std::cout << "[ERROR] No main method found" << std::endl;
    return false; // MethodNotFoundException
  }
  jobjectArray classesNeedRetransform = (jobjectArray)jniEnv->CallStaticObjectMethod(klass, mainMethod, instance);
  
  JvmtiHandle::retransformClasses(jniEnv, jvmtiEnv, classesNeedRetransform);

  return true;
}

bool JarLoader::callMain(JNIEnv *jniEnv, jclass klass) {
  jclass stringCls = jniEnv->FindClass("java/lang/String");

  jmethodID mainMethod =
      jniEnv->GetStaticMethodID(klass, "main", "([Ljava/lang/String;)V");
  if (!mainMethod || jniEnv->ExceptionCheck()) {
    jniEnv->ExceptionClear();
    std::cout << "[ERROR] No main method found" << std::endl;
    return false; // MethodNotFoundException
  }
  jobjectArray array = jniEnv->NewObjectArray(0, stringCls, NULL);
  jniEnv->CallStaticVoidMethod(klass, mainMethod, array);
  return true;
}

jclass JarLoader::getClass(JNIEnv *jniEnv, jobject classLoader, jstring name) {
  jclass classLoaderCls = jniEnv->GetObjectClass(classLoader);
  jmethodID loadClass = jniEnv->GetMethodID(
      classLoaderCls, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");

  jclass klass = (jclass)jniEnv->CallObjectMethod(classLoader, loadClass, name);
  if (!klass || jniEnv->ExceptionCheck()) {
    jniEnv->ExceptionClear();
    return nullptr; // ClassNotFoundException
  }
  return klass;
}