// JNI bindings for io.github.larroy.kokoro.KokoroNative over the kokoro C API.
// Strings cross as byte[] holding UTF-8: JNI's own string functions use modified UTF-8, which mangles characters
// outside the BMP.
#include <jni.h>

#include <kokoro/kokoro.h>

#include <cstdint>
#include <cstring>
#include <string>

namespace {

std::string utf8(JNIEnv* env, jbyteArray array) {
    const jsize length = env->GetArrayLength(array);
    std::string out(static_cast<size_t>(length), '\0');
    env->GetByteArrayRegion(array, 0, length, reinterpret_cast<jbyte*>(out.data()));
    return out;
}

// nullptr if the allocation failed; an OutOfMemoryError is then pending.
jbyteArray bytes(JNIEnv* env, const char* text) {
    const auto length = static_cast<jsize>(std::strlen(text));
    jbyteArray array = env->NewByteArray(length);
    if (array) env->SetByteArrayRegion(array, 0, length, reinterpret_cast<const jbyte*>(text));
    return array;
}

// Throws KokoroException(status, kokoro_last_error()); on any JNI failure that failure stays pending instead.
void throw_kokoro(JNIEnv* env, kokoro_status status) {
    jbyteArray message = bytes(env, kokoro_last_error());
    if (!message) return;
    jclass cls = env->FindClass("io/github/larroy/kokoro/KokoroException");
    if (!cls) return;
    jmethodID ctor = env->GetMethodID(cls, "<init>", "(I[B)V");
    if (!ctor) return;
    auto exception = static_cast<jthrowable>(env->NewObject(cls, ctor, static_cast<jint>(status), message));
    if (exception) env->Throw(exception);
}

kokoro_ctx* ctx(jlong handle) {
    return reinterpret_cast<kokoro_ctx*>(handle);
}

jobject new_audio(JNIEnv* env, jfloatArray samples, int sample_rate) {
    jclass cls = env->FindClass("io/github/larroy/kokoro/KokoroAudio");
    if (!cls) return nullptr;
    jmethodID ctor = env->GetMethodID(cls, "<init>", "([FI)V");
    if (!ctor) return nullptr;
    return env->NewObject(cls, ctor, samples, static_cast<jint>(sample_rate));
}

// Copies and frees `audio`; nullptr with a pending exception on failure.
jobject to_java_audio(JNIEnv* env, kokoro_audio& audio) {
    if (audio.num_samples > static_cast<size_t>(INT32_MAX)) {
        kokoro_audio_free(&audio);
        env->ThrowNew(env->FindClass("java/lang/IllegalStateException"),
                      "synthesized audio exceeds the JVM array limit");
        return nullptr;
    }
    const auto count = static_cast<jsize>(audio.num_samples);
    jfloatArray samples = env->NewFloatArray(count);
    if (samples && count > 0) env->SetFloatArrayRegion(samples, 0, count, audio.samples);
    const int sample_rate = audio.sample_rate;
    kokoro_audio_free(&audio);
    return samples ? new_audio(env, samples, sample_rate) : nullptr;
}

}  // namespace

extern "C" {

JNIEXPORT jbyteArray JNICALL Java_io_github_larroy_kokoro_KokoroNative_nativeVersion(JNIEnv* env, jclass) {
    return bytes(env, kokoro_version());
}

JNIEXPORT jlong JNICALL Java_io_github_larroy_kokoro_KokoroNative_nativeCreate(
    JNIEnv* env, jclass, jbyteArray model, jbyteArray voices, jbyteArray dict) {
    // Android has no CUDA: always CPU.
    const kokoro_options options{KOKORO_DEVICE_CPU, 0};
    kokoro_ctx* out = nullptr;
    const kokoro_status status = kokoro_create_ex(
        utf8(env, model).c_str(), utf8(env, voices).c_str(), utf8(env, dict).c_str(), &options, &out);
    if (status != KOKORO_OK) {
        throw_kokoro(env, status);
        return 0;
    }
    return reinterpret_cast<jlong>(out);
}

JNIEXPORT void JNICALL Java_io_github_larroy_kokoro_KokoroNative_nativeDestroy(JNIEnv*, jclass, jlong handle) {
    kokoro_destroy(ctx(handle));
}

JNIEXPORT jobjectArray JNICALL Java_io_github_larroy_kokoro_KokoroNative_nativeVoiceNames(
    JNIEnv* env, jclass, jlong handle) {
    const size_t count = kokoro_voice_count(ctx(handle));
    jclass byte_array = env->FindClass("[B");
    if (!byte_array) return nullptr;
    jobjectArray names = env->NewObjectArray(static_cast<jsize>(count), byte_array, nullptr);
    for (size_t i = 0; names && i < count; ++i) {
        jbyteArray name = bytes(env, kokoro_voice_name(ctx(handle), i));
        if (!name) return nullptr;
        env->SetObjectArrayElement(names, static_cast<jsize>(i), name);
        env->DeleteLocalRef(name);
    }
    return names;
}

JNIEXPORT jobject JNICALL Java_io_github_larroy_kokoro_KokoroNative_nativeSynthesize(
    JNIEnv* env, jclass, jlong handle, jbyteArray text, jbyteArray voice, jfloat speed, jint flags) {
    kokoro_audio audio{};
    const kokoro_status status = kokoro_synthesize(
        ctx(handle), utf8(env, text).c_str(), utf8(env, voice).c_str(), speed, static_cast<unsigned>(flags), &audio);
    if (status != KOKORO_OK) {
        throw_kokoro(env, status);
        return nullptr;
    }
    return to_java_audio(env, audio);
}

JNIEXPORT jbyteArray JNICALL Java_io_github_larroy_kokoro_KokoroNative_nativePhonemize(
    JNIEnv* env, jclass, jlong handle, jbyteArray text) {
    char* phonemes = nullptr;
    const kokoro_status status = kokoro_phonemize(ctx(handle), utf8(env, text).c_str(), &phonemes);
    if (status != KOKORO_OK) {
        throw_kokoro(env, status);
        return nullptr;
    }
    jbyteArray out = bytes(env, phonemes);
    kokoro_string_free(phonemes);
    return out;
}

JNIEXPORT void JNICALL Java_io_github_larroy_kokoro_KokoroNative_nativeSetLanguage(
    JNIEnv* env, jclass, jlong handle, jint language) {
    const kokoro_status status = kokoro_set_language(ctx(handle), static_cast<kokoro_language>(language));
    if (status != KOKORO_OK) throw_kokoro(env, status);
}

JNIEXPORT void JNICALL Java_io_github_larroy_kokoro_KokoroNative_nativeSetNumberLanguage(
    JNIEnv* env, jclass, jlong handle, jint language) {
    const kokoro_status status =
        kokoro_set_number_language(ctx(handle), static_cast<kokoro_number_language>(language));
    if (status != KOKORO_OK) throw_kokoro(env, status);
}

}  // extern "C"
