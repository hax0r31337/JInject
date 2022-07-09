#include "FileChooser.h"
#include "jni.h"

jobject FileChooser::ChooseFile(JNIEnv *jniEnv) {
  jclass fileChooserCls = jniEnv->FindClass("javax/swing/JFileChooser");
  jmethodID fileChooserInit =
      jniEnv->GetMethodID(fileChooserCls, "<init>", "()V");
  jobject fileChooser = jniEnv->NewObject(fileChooserCls, fileChooserInit);

  jmethodID setDialogTitle = jniEnv->GetMethodID(
      fileChooserCls, "setDialogTitle", "(Ljava/lang/String;)V");
  jniEnv->CallVoidMethod(fileChooser, setDialogTitle,
                         jniEnv->NewStringUTF("Select target file"));

  jclass fileCls = jniEnv->FindClass("java/io/File");
  jmethodID initFileWithString =
      jniEnv->GetMethodID(fileCls, "<init>", "(Ljava/lang/String;)V");
  jobject currentDirectory = jniEnv->NewObject(fileCls, initFileWithString, jniEnv->NewStringUTF("./"));
  jmethodID setCurrentDirectory = jniEnv->GetMethodID(
      fileChooserCls, "setCurrentDirectory", "(Ljava/io/File;)V");
  jniEnv->CallVoidMethod(fileChooser, setCurrentDirectory, currentDirectory);
  jmethodID showDialog =
      jniEnv->GetMethodID(fileChooserCls, "showDialog",
                          "(Ljava/awt/Component;Ljava/lang/String;)I");

  jint result = jniEnv->CallIntMethod(fileChooser, showDialog, NULL,
                                      jniEnv->NewStringUTF("Inject"));
  if (result == 0) {
    jmethodID getSelectedFile = jniEnv->GetMethodID(
        fileChooserCls, "getSelectedFile", "()Ljava/io/File;");
    return jniEnv->CallObjectMethod(fileChooser, getSelectedFile);
  }
  return nullptr; // action cancelled
}