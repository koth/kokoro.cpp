/*
 * kokoro.cpp C API.
 *
 * Strings are NUL-terminated UTF-8. Every function that can fail returns a
 * kokoro_status; on failure kokoro_last_error() describes the cause.
 *
 * Threading: separate contexts are independent and may be used from different
 * threads. A single context must not be used by two threads at the same time.
 */
#ifndef KOKORO_KOKORO_H
#define KOKORO_KOKORO_H

#include <stddef.h>

#if defined(_WIN32)
#  if defined(KOKORO_BUILD)
#    define KOKORO_API __declspec(dllexport)
#  else
#    define KOKORO_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__)
#  define KOKORO_API __attribute__((visibility("default")))
#else
#  define KOKORO_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum kokoro_status {
    KOKORO_OK = 0,
    KOKORO_ERROR_INVALID_ARGUMENT = 1, /* NULL pointer or out-of-range value */
    KOKORO_ERROR_LOAD = 2,             /* model, voices, vocab or dictionary could not be loaded */
    KOKORO_ERROR_VOICE_NOT_FOUND = 3,
    KOKORO_ERROR_INFERENCE = 4,        /* ONNX Runtime failed while synthesizing */
    KOKORO_ERROR_OUT_OF_MEMORY = 5,
    KOKORO_ERROR_UNKNOWN = 6
} kokoro_status;

/* kokoro_synthesize flags. */
#define KOKORO_INPUT_PHONEMES 0x1u /* text is already a phoneme string; skip G2P */

/* How G2P reads digits (see kokoro_set_number_language). */
typedef enum kokoro_number_language {
    KOKORO_NUMBERS_AUTO = 0,    /* Spanish text: Spanish. German text: German. English text: English. Chinese/English
                                   text: per number, the language of the nearest letter or CJK character; Chinese if
                                   none */
    KOKORO_NUMBERS_ENGLISH = 1,
    KOKORO_NUMBERS_CHINESE = 2,
    KOKORO_NUMBERS_SPANISH = 3, /* Spanish words; in Chinese/English and English text they are read by the English
                                   G2P */
    KOKORO_NUMBERS_GERMAN = 4   /* German words; in Chinese/English and English text they are read by the English
                                   G2P */
} kokoro_number_language;

/* Which phonemizer reads text (see kokoro_set_language). */
typedef enum kokoro_language {
    KOKORO_LANGUAGE_AUTO = 0,            /* kokoro_synthesize: by voice name, ef_/em_* Spanish, df_/dm_* German,
                                            af_/am_/bf_/bm_* English, else Chinese and English; kokoro_phonemize:
                                            Chinese and English */
    KOKORO_LANGUAGE_SPANISH = 1,         /* all text is read as Spanish, numbers in Spanish */
    KOKORO_LANGUAGE_CHINESE_ENGLISH = 2, /* Chinese and English by script */
    KOKORO_LANGUAGE_ENGLISH = 3,         /* all text is read as English, numbers in English */
    KOKORO_LANGUAGE_GERMAN = 4           /* all text is read as German, numbers in German */
} kokoro_language;

typedef struct kokoro_ctx kokoro_ctx;

/* Where inference runs. */
typedef enum kokoro_device {
    KOKORO_DEVICE_AUTO = 0, /* CUDA if this build's ONNX Runtime supports it and it initializes, else CPU */
    KOKORO_DEVICE_CPU = 1,
    KOKORO_DEVICE_CUDA = 2  /* fail with KOKORO_ERROR_LOAD instead of falling back to CPU */
} kokoro_device;

typedef struct kokoro_options {
    kokoro_device device;
    int gpu_id; /* CUDA device ordinal, >= 0; ignored on CPU */
} kokoro_options;

/* Mono float PCM owned by the library; release with kokoro_audio_free(). */
typedef struct kokoro_audio {
    float* samples;
    size_t num_samples;
    int sample_rate;
} kokoro_audio;

/* Library version, e.g. "0.1.0". */
KOKORO_API const char* kokoro_version(void);

/*
 * Message for the most recent failed call on the calling thread, or "" if none.
 * Valid until the next kokoro_* call on the same thread.
 */
KOKORO_API const char* kokoro_last_error(void);

/*
 * Loads the ONNX model, the voices file and the dictionaries in dict_dir
 * (vocab.txt, jieba, pinyin, CMU and g2p_en files; see dict/ in the repo).
 * On success stores the new context in *out_ctx.
 * Same as kokoro_create_ex(model_path, voices_path, dict_dir, NULL, out_ctx).
 */
KOKORO_API kokoro_status kokoro_create(const char* model_path, const char* voices_path, const char* dict_dir,
                                       kokoro_ctx** out_ctx);

/*
 * kokoro_create with explicit options; NULL options means kokoro_default_options().
 * With KOKORO_DEVICE_AUTO a CUDA initialization failure is reported on stderr and the
 * context runs on the CPU.
 */
KOKORO_API kokoro_status kokoro_create_ex(const char* model_path, const char* voices_path, const char* dict_dir,
                                          const kokoro_options* options, kokoro_ctx** out_ctx);

/* KOKORO_DEVICE_CPU or KOKORO_DEVICE_CUDA: where ctx runs inference. KOKORO_DEVICE_AUTO for NULL. */
KOKORO_API kokoro_device kokoro_context_device(const kokoro_ctx* ctx);

/* KOKORO_DEVICE_AUTO on GPU 0. */
KOKORO_API kokoro_options kokoro_default_options(void);

/* Frees the context. NULL is ignored. */
KOKORO_API void kokoro_destroy(kokoro_ctx* ctx);

/* Number of voices in the loaded voices file. */
KOKORO_API size_t kokoro_voice_count(const kokoro_ctx* ctx);

/* Name of voice `index` (sorted), or NULL if out of range. Valid for the context's lifetime. */
KOKORO_API const char* kokoro_voice_name(const kokoro_ctx* ctx, size_t index);

/*
 * Synthesizes `text` with `voice` at `speed` (1.0 = normal, must be > 0 and < 2^31).
 * flags: 0 or KOKORO_INPUT_PHONEMES.
 * On success fills *out_audio; the caller must release it with kokoro_audio_free().
 * On failure *out_audio is zeroed.
 */
KOKORO_API kokoro_status kokoro_synthesize(kokoro_ctx* ctx, const char* text, const char* voice, float speed,
                                           unsigned int flags, kokoro_audio* out_audio);

/* Converts text to the phoneme string the model consumes. Release with kokoro_string_free(). */
KOKORO_API kokoro_status kokoro_phonemize(kokoro_ctx* ctx, const char* text, char** out_phonemes);

/* Sets how digits are read by later kokoro_synthesize/kokoro_phonemize calls on ctx. Default: KOKORO_NUMBERS_AUTO. */
KOKORO_API kokoro_status kokoro_set_number_language(kokoro_ctx* ctx, kokoro_number_language language);

/* Sets the G2P language for later kokoro_synthesize/kokoro_phonemize calls on ctx. Default: KOKORO_LANGUAGE_AUTO.
 * A number language other than KOKORO_NUMBERS_AUTO also applies to Spanish and English text. */
KOKORO_API kokoro_status kokoro_set_language(kokoro_ctx* ctx, kokoro_language language);

/* Frees samples and zeroes *audio. NULL is ignored. */
KOKORO_API void kokoro_audio_free(kokoro_audio* audio);

/* Frees a string returned by the library. NULL is ignored. */
KOKORO_API void kokoro_string_free(char* str);

#ifdef __cplusplus
}
#endif

#endif /* KOKORO_KOKORO_H */
