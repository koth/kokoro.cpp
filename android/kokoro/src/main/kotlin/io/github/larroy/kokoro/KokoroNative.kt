package io.github.larroy.kokoro

/**
 * JNI entry points in libkokoro_jni.so. Strings cross as UTF-8 byte arrays; failures throw [KokoroException].
 * Loading libkokoro_jni.so pulls in libkokoro.so and libonnxruntime.so from the app's native library directory.
 */
internal object KokoroNative {
    init {
        System.loadLibrary("kokoro_jni")
    }

    @JvmStatic external fun nativeVersion(): ByteArray

    @JvmStatic external fun nativeCreate(model: ByteArray, voices: ByteArray, dict: ByteArray): Long

    @JvmStatic external fun nativeDestroy(handle: Long)

    @JvmStatic external fun nativeVoiceNames(handle: Long): Array<ByteArray>

    @JvmStatic external fun nativeSynthesize(
        handle: Long, text: ByteArray, voice: ByteArray, speed: Float, flags: Int,
    ): KokoroAudio

    @JvmStatic external fun nativePhonemize(handle: Long, text: ByteArray): ByteArray

    @JvmStatic external fun nativeSetLanguage(handle: Long, language: Int)

    @JvmStatic external fun nativeSetNumberLanguage(handle: Long, language: Int)
}
