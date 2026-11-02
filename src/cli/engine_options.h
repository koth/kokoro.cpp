// Value parsers for the engine options of the kokoro front end (kokoro-server ports them in src/server/options.rs).
#ifndef KOKORO_CLI_ENGINE_OPTIONS_H
#define KOKORO_CLI_ENGINE_OPTIONS_H

#include <kokoro/kokoro.h>

#include <climits>
#include <cstdlib>
#include <string>

namespace kokoro_cli {

// Maps the --lang value; returns false for anything but "auto", "en", "zh", "es" or "de".
inline bool parse_number_language(const std::string& value, kokoro_number_language& language) {
    if (value == "auto") { language = KOKORO_NUMBERS_AUTO; return true; }
    if (value == "en") { language = KOKORO_NUMBERS_ENGLISH; return true; }
    if (value == "zh") { language = KOKORO_NUMBERS_CHINESE; return true; }
    if (value == "es") { language = KOKORO_NUMBERS_SPANISH; return true; }
    if (value == "de") { language = KOKORO_NUMBERS_GERMAN; return true; }
    return false;
}

// Maps the --language value; returns false for anything but "auto", "zh", "en", "es" or "de".
inline bool parse_language(const std::string& value, kokoro_language& language) {
    if (value == "auto") { language = KOKORO_LANGUAGE_AUTO; return true; }
    if (value == "zh") { language = KOKORO_LANGUAGE_CHINESE_ENGLISH; return true; }
    if (value == "en") { language = KOKORO_LANGUAGE_ENGLISH; return true; }
    if (value == "es") { language = KOKORO_LANGUAGE_SPANISH; return true; }
    if (value == "de") { language = KOKORO_LANGUAGE_GERMAN; return true; }
    return false;
}

inline bool parse_device(const std::string& value, kokoro_device& device) {
    if (value == "auto") { device = KOKORO_DEVICE_AUTO; return true; }
    if (value == "cpu") { device = KOKORO_DEVICE_CPU; return true; }
    if (value == "cuda") { device = KOKORO_DEVICE_CUDA; return true; }
    return false;
}

inline bool parse_gpu_id(const std::string& value, int& gpu_id) {
    if (value.empty()) return false;
    char* end = nullptr;
    const long parsed = std::strtol(value.c_str(), &end, 10);
    if (end != value.c_str() + value.size() || parsed < 0 || parsed > INT_MAX) return false;
    gpu_id = static_cast<int>(parsed);
    return true;
}

}  // namespace kokoro_cli

#endif  // KOKORO_CLI_ENGINE_OPTIONS_H
