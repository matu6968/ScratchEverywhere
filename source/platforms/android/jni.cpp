#include <accessory.hpp>
#include <jni.h>

namespace {
std::string toString(JNIEnv *env, jstring string) {
    if (!string) return "";

    const char *chars = env->GetStringUTFChars(string, nullptr);
    if (!chars) return "";

    std::string result(chars);
    env->ReleaseStringUTFChars(string, chars);
    return result;
}
} // namespace

// The Java side of the Android Open Accessory link, see AccessoryBridge.java

extern "C" JNIEXPORT void JNICALL
Java_io_github_scratcheverywhere_AccessoryBridge_nativeAttached(JNIEnv *env, jclass, jint fd, jstring manufacturer, jstring model, jstring description, jstring version, jstring uri, jstring serial) {
    AndroidAccessory::attach(fd, {
                                     toString(env, manufacturer),
                                     toString(env, model),
                                     toString(env, description),
                                     toString(env, version),
                                     toString(env, uri),
                                     toString(env, serial),
                                 });
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_scratcheverywhere_AccessoryBridge_nativeDetached(JNIEnv *, jclass) {
    AndroidAccessory::detach();
}
