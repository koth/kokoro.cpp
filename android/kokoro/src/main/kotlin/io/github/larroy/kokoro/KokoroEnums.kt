package io.github.larroy.kokoro

/** Result of a kokoro C API call (`kokoro_status`). */
public enum class KokoroStatus(public val code: Int) {
    /** The call succeeded. */
    OK(0),

    /** NULL pointer or out-of-range value. */
    INVALID_ARGUMENT(1),

    /** Model, voices, vocab or dictionary could not be loaded. */
    LOAD(2),

    /** The requested voice is not in the voices file. */
    VOICE_NOT_FOUND(3),

    /** ONNX Runtime failed while synthesizing. */
    INFERENCE(4),

    /** The library ran out of memory. */
    OUT_OF_MEMORY(5),

    /** Any other failure. */
    UNKNOWN(6);

    internal companion object {
        fun fromCode(code: Int): KokoroStatus = entries.firstOrNull { it.code == code } ?: UNKNOWN
    }
}

/** Which phonemizer reads text (`kokoro_language`). */
public enum class KokoroLanguage(internal val code: Int) {
    /**
     * Synthesize: by voice name, ef_/em_* Spanish, df_/dm_* German, af_/am_/bf_/bm_* English, else Chinese and
     * English; phonemize: Chinese and English.
     */
    AUTO(0),

    /** All text is read as Spanish, numbers in Spanish. */
    SPANISH(1),

    /** Chinese and English by script. */
    CHINESE_ENGLISH(2),

    /** All text is read as English, numbers in English. */
    ENGLISH(3),

    /** All text is read as German, numbers in German. */
    GERMAN(4),
}

/** How G2P reads digits (`kokoro_number_language`). */
public enum class KokoroNumberLanguage(internal val code: Int) {
    /**
     * Spanish text: Spanish. German text: German. English text: English. Chinese/English text: per number, the
     * language of the nearest letter or CJK character; Chinese if none.
     */
    AUTO(0),

    /** Read digits as English words. */
    ENGLISH(1),

    /** Read digits as Chinese words. */
    CHINESE(2),

    /** Spanish words; in Chinese/English and English text they are read by the English G2P. */
    SPANISH(3),

    /** German words; in Chinese/English and English text they are read by the English G2P. */
    GERMAN(4),
}
