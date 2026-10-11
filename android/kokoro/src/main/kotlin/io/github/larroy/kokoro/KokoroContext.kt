package io.github.larroy.kokoro

/**
 * A loaded Kokoro model, voices file and G2P dictionaries (`kokoro_ctx`). Inference runs on the CPU.
 *
 * Threading: separate contexts are independent and may be used from different threads. Calls on one context are
 * serialized, and [close] waits for an in-flight call. Always [close] a context (e.g. with `use {}`); there is no
 * finalizer.
 *
 * @param modelPath path to the Kokoro ONNX model.
 * @param voicesPath path to the voices file.
 * @param dictDir directory holding the G2P dictionaries, normally `KokoroDict.install(context).path`.
 * @throws KokoroException the native library could not create the context.
 */
public class KokoroContext(modelPath: String, voicesPath: String, dictDir: String) : AutoCloseable {
    private val lock = Any()
    private var handle: Long = KokoroNative.nativeCreate(
        modelPath.encodeToByteArray(), voicesPath.encodeToByteArray(), dictDir.encodeToByteArray())

    /** Voice names in the loaded voices file, sorted. */
    public val voices: List<String>

    init {
        voices = try {
            KokoroNative.nativeVoiceNames(handle).map { it.decodeToString() }
        } catch (t: Throwable) {
            KokoroNative.nativeDestroy(handle)
            handle = 0L
            throw t
        }
    }

    /**
     * Synthesizes [text] with [voice].
     *
     * @param text text to speak, or a phoneme string when [inputIsPhonemes] is true.
     * @param voice voice name, e.g. "af_maple".
     * @param speed speaking rate; 1.0 is normal and it must be greater than 0.
     * @param inputIsPhonemes [text] is already a phoneme string; skip G2P.
     * @throws KokoroException synthesis failed.
     * @throws IllegalStateException the context is closed.
     */
    @JvmOverloads
    public fun synthesize(
        text: String, voice: String, speed: Float = 1.0f, inputIsPhonemes: Boolean = false,
    ): KokoroAudio = withHandle {
        KokoroNative.nativeSynthesize(
            it, text.encodeToByteArray(), voice.encodeToByteArray(), speed, if (inputIsPhonemes) INPUT_PHONEMES else 0)
    }

    /**
     * Converts [text] to the phoneme string the model consumes.
     *
     * @throws KokoroException phonemization failed.
     * @throws IllegalStateException the context is closed.
     */
    public fun phonemize(text: String): String = withHandle {
        KokoroNative.nativePhonemize(it, text.encodeToByteArray()).decodeToString()
    }

    /**
     * Sets the G2P language for later synthesize/phonemize calls. Default: [KokoroLanguage.AUTO]. A number language
     * other than [KokoroNumberLanguage.AUTO] also applies to Spanish, German and English text.
     */
    public fun setLanguage(language: KokoroLanguage): Unit = withHandle {
        KokoroNative.nativeSetLanguage(it, language.code)
    }

    /** Sets how digits are read by later synthesize/phonemize calls. Default: [KokoroNumberLanguage.AUTO]. */
    public fun setNumberLanguage(language: KokoroNumberLanguage): Unit = withHandle {
        KokoroNative.nativeSetNumberLanguage(it, language.code)
    }

    /** Frees the native context once no call is using it. Calling it again does nothing. */
    override fun close() {
        synchronized(lock) {
            if (handle != 0L) {
                KokoroNative.nativeDestroy(handle)
                handle = 0L
            }
        }
    }

    private inline fun <T> withHandle(block: (Long) -> T): T = synchronized(lock) {
        check(handle != 0L) { "KokoroContext is closed" }
        block(handle)
    }

    public companion object {
        /** `KOKORO_INPUT_PHONEMES`. */
        private const val INPUT_PHONEMES = 1

        /** Native library version, e.g. "0.1.0". */
        @JvmStatic
        public fun version(): String = KokoroNative.nativeVersion().decodeToString()
    }
}
