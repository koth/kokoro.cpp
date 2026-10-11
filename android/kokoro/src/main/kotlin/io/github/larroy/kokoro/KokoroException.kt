package io.github.larroy.kokoro

/** A failed kokoro C API call; [message] is `kokoro_last_error()`. */
public class KokoroException(public val status: KokoroStatus, message: String) : RuntimeException(message) {
    /** Called from libkokoro_jni.so with the raw status code and the UTF-8 message. */
    internal constructor(code: Int, messageUtf8: ByteArray) :
        this(KokoroStatus.fromCode(code), messageUtf8.decodeToString())
}
