#include "JarLoader.h"
#include "jni.h"
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

  jstring value = (jstring)jniEnv->CallObjectMethod(mainAttributes, getValue, jniEnv->NewStringUTF("Main-Class"));
  jboolean equalResult = jniEnv->CallBooleanMethod(value, equals, emptyString);
  if (!equalResult) {
    return JarLoader::callMain(jniEnv, classLoader, value);
  }

  std::cout << "[ERROR] No Class Found" << std::endl;
  return false;
}

bool JarLoader::callMain(JNIEnv *jniEnv, jobject classLoader, jstring name) {
  jclass classLoaderCls = jniEnv->GetObjectClass(classLoader);
  jmethodID loadClass = jniEnv->GetMethodID(
      classLoaderCls, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
  jclass stringCls = jniEnv->FindClass("java/lang/String");

  jclass klass =
      (jclass)jniEnv->CallObjectMethod(classLoader, loadClass, name);
  if (!klass || jniEnv->ExceptionCheck()) {
    jniEnv->ExceptionClear();
    std::cout << "[ERROR] Defined Main Class Not Exists (" << jniEnv->GetStringUTFChars(name, 0) << ")" << std::endl;
    return false; // ClassNotFoundException
  }
  jmethodID mainMethod =
      jniEnv->GetStaticMethodID(klass, "main", "([Ljava/lang/String;)V");
  if (!mainMethod || jniEnv->ExceptionCheck()) {
    jniEnv->ExceptionClear();
    std::cout << "[ERROR] No Main Method Found" << std::endl;
    return false; // MethodNotFoundException
  }
  jobjectArray array = jniEnv->NewObjectArray(0, stringCls, NULL);
  jniEnv->CallStaticVoidMethod(klass, mainMethod, array);
  return true;
}