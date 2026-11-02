#include "kokoro/kokoro.h"
#include "Kokoro.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

struct kokoro_ctx {
    Kokoro tts;
    std::vector<std::string> voice_names;

    kokoro_ctx(const char* model, const char* voices, const char* dict_dir, const InferenceConfig& inference)
        : tts(model, voices, dict_dir, inference), voice_names(tts.voice_names()) {}
};

namespace {

thread_local std::string g_last_error;

// Integer-speed models cast speed to int; 2^31 is the first float that int cannot hold.
constexpr float kSpeedLimit = 2147483648.0f;

kokoro_status fail(kokoro_status status, const char* message) {
    g_last_error = message;
    return status;
}

bool to_inference_config(const kokoro_options& options, InferenceConfig& config) {
    switch (options.device) {
        case KOKORO_DEVICE_AUTO: config.device = InferenceDevice::Auto; break;
        case KOKORO_DEVICE_CPU: config.device = InferenceDevice::Cpu; break;
        case KOKORO_DEVICE_CUDA: config.device = InferenceDevice::Cuda; break;
        default: return false;
    }
    config.gpu_id = options.gpu_id;
    return true;
}

kokoro_device to_c_device(InferenceDevice device) {
    return device == InferenceDevice::Cuda ? KOKORO_DEVICE_CUDA : KOKORO_DEVICE_CPU;
}

// Maps an in-flight exception to a status; `fallback` classifies runtime failures by call site.
kokoro_status fail_current_exception(kokoro_status fallback) {
    try {
        throw;
    } catch (const std::bad_alloc&) {
        return fail(KOKORO_ERROR_OUT_OF_MEMORY, "out of memory");
    } catch (const std::exception& e) {
        return fail(fallback, e.what());
    } catch (...) {
        return fail(KOKORO_ERROR_UNKNOWN, "unknown error");
    }
}

}  // namespace

extern "C" {

const char* kokoro_version(void) {
    return KOKORO_VERSION_STRING;
}

const char* kokoro_last_error(void) {
    return g_last_error.c_str();
}

kokoro_status kokoro_create_ex(const char* model_path, const char* voices_path, const char* dict_dir,
                               const kokoro_options* options, kokoro_ctx** out_ctx) {
    if (!out_ctx) return fail(KOKORO_ERROR_INVALID_ARGUMENT, "out_ctx is NULL");
    *out_ctx = nullptr;
    if (!model_path || !voices_path || !dict_dir) {
        return fail(KOKORO_ERROR_INVALID_ARGUMENT, "model_path, voices_path and dict_dir must not be NULL");
    }
    const kokoro_options resolved = options ? *options : kokoro_default_options();
    InferenceConfig config;
    if (!to_inference_config(resolved, config)) {
        return fail(KOKORO_ERROR_INVALID_ARGUMENT, "unknown device");
    }
    if (resolved.gpu_id < 0) {
        return fail(KOKORO_ERROR_INVALID_ARGUMENT, "gpu_id must be >= 0");
    }
    try {
        *out_ctx = new kokoro_ctx(model_path, voices_path, dict_dir, config);
    } catch (...) {
        return fail_current_exception(KOKORO_ERROR_LOAD);
    }
    return KOKORO_OK;
}

kokoro_status kokoro_create(const char* model_path, const char* voices_path, const char* dict_dir,
                            kokoro_ctx** out_ctx) {
    return kokoro_create_ex(model_path, voices_path, dict_dir, nullptr, out_ctx);
}

kokoro_options kokoro_default_options(void) {
    return kokoro_options{KOKORO_DEVICE_AUTO, 0};
}

kokoro_device kokoro_context_device(const kokoro_ctx* ctx) {
    return ctx ? to_c_device(ctx->tts.device()) : KOKORO_DEVICE_AUTO;
}

void kokoro_destroy(kokoro_ctx* ctx) {
    delete ctx;
}

size_t kokoro_voice_count(const kokoro_ctx* ctx) {
    return ctx ? ctx->voice_names.size() : 0;
}

const char* kokoro_voice_name(const kokoro_ctx* ctx, size_t index) {
    if (!ctx || index >= ctx->voice_names.size()) return nullptr;
    return ctx->voice_names[index].c_str();
}

kokoro_status kokoro_synthesize(kokoro_ctx* ctx, const char* text, const char* voice, float speed,
                                unsigned int flags, kokoro_audio* out_audio) {
    if (!out_audio) return fail(KOKORO_ERROR_INVALID_ARGUMENT, "out_audio is NULL");
    *out_audio = kokoro_audio{nullptr, 0, 0};
    if (!ctx || !text || !voice) return fail(KOKORO_ERROR_INVALID_ARGUMENT, "ctx, text and voice must not be NULL");
    if (!(speed > 0.0f && speed < kSpeedLimit)) {
        return fail(KOKORO_ERROR_INVALID_ARGUMENT, "speed must be > 0 and < 2^31");
    }
    if (flags & ~KOKORO_INPUT_PHONEMES) return fail(KOKORO_ERROR_INVALID_ARGUMENT, "unknown flags");

    const std::vector<float>* style = nullptr;
    try {
        style = &ctx->tts.get_voice_style(voice);
    } catch (const std::out_of_range& e) {
        return fail(KOKORO_ERROR_VOICE_NOT_FOUND, e.what());
    }

    try {
        auto result = ctx->tts.create(text, *style, ctx->tts.language_for(voice), speed,
                                      (flags & KOKORO_INPUT_PHONEMES) != 0);
        const std::vector<float>& samples = result.first;
        if (!samples.empty()) {
            auto* buffer = static_cast<float*>(std::malloc(samples.size() * sizeof(float)));
            if (!buffer) return fail(KOKORO_ERROR_OUT_OF_MEMORY, "out of memory");
            std::memcpy(buffer, samples.data(), samples.size() * sizeof(float));
            out_audio->samples = buffer;
        }
        out_audio->num_samples = samples.size();
        out_audio->sample_rate = result.second;
    } catch (...) {
        return fail_current_exception(KOKORO_ERROR_INFERENCE);
    }
    return KOKORO_OK;
}

kokoro_status kokoro_phonemize(kokoro_ctx* ctx, const char* text, char** out_phonemes) {
    if (!out_phonemes) return fail(KOKORO_ERROR_INVALID_ARGUMENT, "out_phonemes is NULL");
    *out_phonemes = nullptr;
    if (!ctx || !text) return fail(KOKORO_ERROR_INVALID_ARGUMENT, "ctx and text must not be NULL");
    try {
        const std::string phonemes = ctx->tts.phonemize(text, ctx->tts.language_for(""));
        auto* buffer = static_cast<char*>(std::malloc(phonemes.size() + 1));
        if (!buffer) return fail(KOKORO_ERROR_OUT_OF_MEMORY, "out of memory");
        std::memcpy(buffer, phonemes.c_str(), phonemes.size() + 1);
        *out_phonemes = buffer;
    } catch (...) {
        return fail_current_exception(KOKORO_ERROR_UNKNOWN);
    }
    return KOKORO_OK;
}

kokoro_status kokoro_set_number_language(kokoro_ctx* ctx, kokoro_number_language language) {
    if (!ctx) return fail(KOKORO_ERROR_INVALID_ARGUMENT, "ctx is NULL");
    NumberLanguage lang;
    switch (language) {
        case KOKORO_NUMBERS_AUTO:    lang = NumberLanguage::Auto;    break;
        case KOKORO_NUMBERS_ENGLISH: lang = NumberLanguage::English; break;
        case KOKORO_NUMBERS_CHINESE: lang = NumberLanguage::Chinese; break;
        case KOKORO_NUMBERS_SPANISH: lang = NumberLanguage::Spanish; break;
        case KOKORO_NUMBERS_GERMAN: lang = NumberLanguage::German; break;
        default: return fail(KOKORO_ERROR_INVALID_ARGUMENT, "unknown number language");
    }
    ctx->tts.set_number_language(lang);
    return KOKORO_OK;
}

kokoro_status kokoro_set_language(kokoro_ctx* ctx, kokoro_language language) {
    if (!ctx) return fail(KOKORO_ERROR_INVALID_ARGUMENT, "ctx is NULL");
    switch (language) {
        case KOKORO_LANGUAGE_AUTO:    ctx->tts.set_language(std::nullopt);           break;
        case KOKORO_LANGUAGE_SPANISH: ctx->tts.set_language(G2PLanguage::Spanish); break;
        case KOKORO_LANGUAGE_CHINESE_ENGLISH: ctx->tts.set_language(G2PLanguage::ChineseEnglish); break;
        case KOKORO_LANGUAGE_ENGLISH: ctx->tts.set_language(G2PLanguage::English); break;
        case KOKORO_LANGUAGE_GERMAN: ctx->tts.set_language(G2PLanguage::German); break;
        default: return fail(KOKORO_ERROR_INVALID_ARGUMENT, "unknown language");
    }
    return KOKORO_OK;
}

void kokoro_audio_free(kokoro_audio* audio) {
    if (!audio) return;
    std::free(audio->samples);
    *audio = kokoro_audio{nullptr, 0, 0};
}

void kokoro_string_free(char* str) {
    std::free(str);
}

}  // extern "C"
