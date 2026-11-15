#include "android/kk_bridge.h"

#include <jni.h>

#include <vector>

#define J(name) Java_com_karkinolution_app_Native_##name

extern "C" {
JNIEXPORT jint JNICALL J(startServer)(JNIEnv *, jclass) { return kk_start_server(); }
JNIEXPORT void JNICALL J(stopServer)(JNIEnv *, jclass) { kk_stop_server(); }
JNIEXPORT jint JNICALL J(connect)(JNIEnv *, jclass) { return kk_connect(); }
JNIEXPORT void JNICALL J(disconnect)(JNIEnv *, jclass) { kk_disconnect(); }
JNIEXPORT jint JNICALL J(tickAll)(JNIEnv *, jclass) { return kk_tick_all(); }
JNIEXPORT jint JNICALL J(createCreature)(JNIEnv *env, jclass, jstring species, jdouble x, jdouble y) {
        const char *s = env->GetStringUTFChars(species, nullptr);
        const int   r = kk_create_creature(s, x, y);
        env->ReleaseStringUTFChars(species, s);
        return r;
}
JNIEXPORT jstring JNICALL J(snapshot)(JNIEnv *env, jclass) {
        std::vector<char> buf(1 << 16);
        int               n = kk_snapshot(buf.data(), static_cast<int>(buf.size()));
        if (n >= static_cast<int>(buf.size())) {
                buf.resize(static_cast<size_t>(n) + 1);
                kk_snapshot(buf.data(), static_cast<int>(buf.size()));
        }
        return env->NewStringUTF(buf.data());
}
}
