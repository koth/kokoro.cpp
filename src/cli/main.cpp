// kokoro: command-line front end for libkokoro (public C API only).
#include <kokoro/kokoro.h>

#include "console.h"
#include "engine_options.h"
#include "interactive.h"
#include "speed.h"

#include <chrono>
#include <cstdint>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {

struct Options {
    std::string model = "models/kokoro-v1.1-zh.onnx";
    std::string voices = "models/voices-v1.1-zh.bin";
    std::string dict = "dict";
    std::string voice = "af_maple";
    float speed = 1.0f;
    kokoro_number_language number_language = KOKORO_NUMBERS_AUTO;  // --lang
    kokoro_language language = KOKORO_LANGUAGE_AUTO;  // --language
    std::string output = "output.wav";
    kokoro_device device = KOKORO_DEVICE_AUTO;
    int gpu_id = 0;
    bool input_phonemes = false;  // -p/--phonemes
    bool phonemize = false;       // --phonemize
    bool list_voices = false;     // --list-voices
    bool interactive = false;     // -i/--interactive
    bool help = false;
    bool version = false;
    bool has_text = false;
    std::string text;
};

 enum class Opt { Model, Voices, Dict, Voice, Lang, Language, Speed, Output, Device, GpuId, Phonemes, Phonemize,
                  ListVoices, Interactive, Help, Version };

struct OptionSpec {
    const char* short_name;  // nullptr if the option has no short form
    const char* long_name;
    Opt id;
    bool takes_value;
};

constexpr OptionSpec kOptions[] = {
    {"-m", "--model", Opt::Model, true},
    {nullptr, "--voices", Opt::Voices, true},
    {"-d", "--dict", Opt::Dict, true},
    {"-v", "--voice", Opt::Voice, true},
    {nullptr, "--lang", Opt::Lang, true},
    {nullptr, "--language", Opt::Language, true},
    {"-s", "--speed", Opt::Speed, true},
    {"-o", "--output", Opt::Output, true},
    {nullptr, "--device", Opt::Device, true},
    {nullptr, "--gpu-id", Opt::GpuId, true},
    {"-p", "--phonemes", Opt::Phonemes, false},
    {nullptr, "--phonemize", Opt::Phonemize, false},
    {nullptr, "--list-voices", Opt::ListVoices, false},
    {"-i", "--interactive", Opt::Interactive, false},
    {"-h", "--help", Opt::Help, false},
    {nullptr, "--version", Opt::Version, false},
};

const OptionSpec* find_option(const std::string& name) {
    for (const OptionSpec& spec : kOptions) {
        if (name == spec.long_name || (spec.short_name && name == spec.short_name)) return &spec;
    }
    return nullptr;
}

void print_usage(std::FILE* out) {
    std::fputs(
        "Usage: kokoro [options] <text>\n"
        "       kokoro --list-voices [options]\n"
        "       kokoro --interactive [options]\n"
        "\n"
        "Synthesize UTF-8 <text> to a WAV file with the Kokoro TTS model.\n"
        "\n"
        "Options:\n"
        "  -m, --model <path>    ONNX model file (default: models/kokoro-v1.1-zh.onnx)\n"
        "      --voices <path>   voices file (default: models/voices-v1.1-zh.bin)\n"
        "  -d, --dict <dir>      dictionary directory (default: dict)\n"
        "  -v, --voice <name>    voice to use (default: af_maple)\n"
        "      --lang <auto|en|zh|es|de>  language for reading numbers (default: auto)\n"
        "      --language <auto|zh|en|es|de>  text language; auto: es for ef_*/em_*, de for df_*/dm_*, en for "
        "af_/am_/bf_/bm_* voices, else zh (Chinese/English) (default: auto)\n"
        "  -s, --speed <rate>    speaking rate, > 0 (default: 1.0)\n"
"  -o, --output <path>   output WAV file (default: output.wav; unused with -i)\n"
        "      --device <name>   auto, cpu or cuda (default: auto)\n"
        "      --gpu-id <n>      CUDA device to use (default: 0)\n"
        "  -p, --phonemes        <text> is a phoneme string; skip G2P\n"
        "      --phonemize       print the phonemes for <text> instead of synthesizing\n"
        "      --list-voices     print the available voices and exit\n"
        "  -i, --interactive     read phrases from stdin and play them as they are synthesized\n"
        "  -h, --help            print this help and exit\n"
        "      --version         print the library version and exit\n",
        out);
}

bool usage_error(const std::string& message) {
    std::fprintf(stderr, "kokoro: %s\nTry 'kokoro --help'.\n", message.c_str());
    return false;
}

// Returns false after printing a usage error.
bool parse_args(const std::vector<std::string>& args, Options& opt) {
    bool positional_only = false;
    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& arg = args[i];

        if (!positional_only && arg == "--") {
            positional_only = true;
            continue;
        }
        if (positional_only || arg.size() < 2 || arg[0] != '-') {
            if (opt.has_text) return usage_error("unexpected argument '" + arg + "'");
            opt.text = arg;
            opt.has_text = true;
            continue;
        }

        // Only long options accept an inline "=value".
        std::string name = arg;
        std::string value;
        bool has_value = false;
        if (arg.compare(0, 2, "--") == 0) {
            const size_t eq = arg.find('=');
            if (eq != std::string::npos) {
                name = arg.substr(0, eq);
                value = arg.substr(eq + 1);
                has_value = true;
            }
        }

        const OptionSpec* spec = find_option(name);
        if (!spec) return usage_error("unknown option '" + arg + "'");

        if (!spec->takes_value) {
            if (has_value) return usage_error("option '" + name + "' does not take a value");
        } else if (!has_value) {
            if (i + 1 >= args.size()) return usage_error("option '" + name + "' requires a value");
            value = args[++i];
        }

        switch (spec->id) {
            case Opt::Model: opt.model = value; break;
            case Opt::Voices: opt.voices = value; break;
            case Opt::Dict: opt.dict = value; break;
            case Opt::Voice: opt.voice = value; break;
            case Opt::Lang:
                if (!kokoro_cli::parse_number_language(value, opt.number_language)) {
                    return usage_error("invalid number language '" + value + "' (must be auto, en, zh, es or de)");
                }
                break;
            case Opt::Language:
                if (!kokoro_cli::parse_language(value, opt.language)) {
                    return usage_error("invalid language '" + value + "' (must be auto, zh, en, es or de)");
                }
                break;
            case Opt::Output: opt.output = value; break;
            case Opt::Device:
                if (!kokoro_cli::parse_device(value, opt.device)) {
                    return usage_error("invalid device '" + value + "' (expected auto, cpu or cuda)");
                }
                break;
            case Opt::GpuId:
                if (!kokoro_cli::parse_gpu_id(value, opt.gpu_id)) {
                    return usage_error("invalid GPU id '" + value + "' (must be an integer >= 0)");
                }
                break;
            case Opt::Speed:
                if (!kokoro_cli::parse_speed(value, opt.speed)) {
                    return usage_error("invalid speed '" + value + "' (must be a number > 0)");
                }
                break;
            case Opt::Phonemes: opt.input_phonemes = true; break;
            case Opt::Phonemize: opt.phonemize = true; break;
            case Opt::ListVoices: opt.list_voices = true; break;
            case Opt::Interactive: opt.interactive = true; break;
            case Opt::Help: opt.help = true; return true;
            case Opt::Version: opt.version = true; return true;
        }
    }

    if (opt.list_voices && opt.phonemize) return usage_error("--list-voices cannot be combined with --phonemize");
    if (opt.phonemize && opt.input_phonemes) return usage_error("--phonemize cannot be combined with --phonemes");
    if (opt.list_voices && opt.has_text) return usage_error("--list-voices does not take <text>");
    if (opt.interactive && opt.list_voices) return usage_error("--interactive cannot be combined with --list-voices");
    if (opt.interactive && opt.phonemize) return usage_error("--interactive cannot be combined with --phonemize");
    if (opt.interactive && opt.has_text) return usage_error("--interactive does not take <text>");
    if (!opt.list_voices && !opt.interactive && !opt.has_text) return usage_error("missing <text>");
    return true;
}

void put_u16(std::string& out, uint16_t v) {
    out.push_back(static_cast<char>(v & 0xFFu));
    out.push_back(static_cast<char>((v >> 8) & 0xFFu));
}

void put_u32(std::string& out, uint32_t v) {
    for (int shift = 0; shift < 32; shift += 8) out.push_back(static_cast<char>((v >> shift) & 0xFFu));
}

// Mono 32-bit IEEE float WAV with a 44-byte header.
bool write_wav(const std::string& path, const kokoro_audio& audio, std::string& error) {
    if (audio.num_samples > (0xFFFFFFFFu - 36u) / sizeof(float)) {
        error = "audio too long for a WAV file";
        return false;
    }
    const uint32_t data_size = static_cast<uint32_t>(audio.num_samples * sizeof(float));
    const uint32_t sample_rate = static_cast<uint32_t>(audio.sample_rate);

    std::string header;
    header.reserve(44);
    header += "RIFF";
    put_u32(header, 36u + data_size);
    header += "WAVE";
    header += "fmt ";
    put_u32(header, 16);               // fmt chunk size
    put_u16(header, 3);                // WAVE_FORMAT_IEEE_FLOAT
    put_u16(header, 1);                // channels
    put_u32(header, sample_rate);
    put_u32(header, sample_rate * 4);  // byte rate
    put_u16(header, 4);                // block align
    put_u16(header, 32);               // bits per sample
    header += "data";
    put_u32(header, data_size);

    std::ofstream file(std::filesystem::u8path(path), std::ios::binary);
    if (!file) {
        error = "cannot open '" + path + "' for writing";
        return false;
    }
    file.write(header.data(), static_cast<std::streamsize>(header.size()));
    if (data_size > 0) file.write(reinterpret_cast<const char*>(audio.samples), static_cast<std::streamsize>(data_size));
    file.close();
    if (!file.good()) {
        error = "cannot write '" + path + "'";
        return false;
    }
    return true;
}

int library_error() {
    std::fprintf(stderr, "kokoro: %s\n", kokoro_last_error());
    return 1;
}

using Clock = std::chrono::steady_clock;

double elapsed_s(Clock::time_point start) {
    return std::chrono::duration<double>(Clock::now() - start).count();
}

const char* device_name(kokoro_device device) {
    switch (device) {
        case KOKORO_DEVICE_CUDA: return "cuda";
        case KOKORO_DEVICE_CPU: return "cpu";
        default: return "auto";
    }
}

int run(const Options& opt) {
    const auto init_start = Clock::now();
    kokoro_options options = kokoro_default_options();
    options.device = opt.device;
    options.gpu_id = opt.gpu_id;
    kokoro_ctx* raw_ctx = nullptr;
    if (kokoro_create_ex(opt.model.c_str(), opt.voices.c_str(), opt.dict.c_str(), &options, &raw_ctx) != KOKORO_OK) {
        return library_error();
    }
    const std::unique_ptr<kokoro_ctx, decltype(&kokoro_destroy)> ctx(raw_ctx, &kokoro_destroy);
    if (kokoro_set_number_language(ctx.get(), opt.number_language) != KOKORO_OK) return library_error();
    if (kokoro_set_language(ctx.get(), opt.language) != KOKORO_OK) return library_error();
    const double init_s = elapsed_s(init_start);
    std::fprintf(stderr, "Initialized engine in %.3f s\n", init_s);

    if (opt.list_voices) {
        const size_t count = kokoro_voice_count(ctx.get());
        for (size_t i = 0; i < count; ++i) std::printf("%s\n", kokoro_voice_name(ctx.get(), i));
        return 0;
    }

    if (opt.phonemize) {
        char* phonemes = nullptr;
        if (kokoro_phonemize(ctx.get(), opt.text.c_str(), &phonemes) != KOKORO_OK) return library_error();
        std::printf("%s\n", phonemes);
        kokoro_string_free(phonemes);
        return 0;
    }

    const unsigned flags = opt.input_phonemes ? KOKORO_INPUT_PHONEMES : 0u;
    if (opt.interactive) return kokoro_cli::run_interactive(ctx.get(), {opt.voice, opt.speed, flags});

    kokoro_audio audio{};
    const auto synth_start = Clock::now();
    if (kokoro_synthesize(ctx.get(), opt.text.c_str(), opt.voice.c_str(), opt.speed, flags, &audio) != KOKORO_OK) {
        return library_error();
    }
    const double synth_s = elapsed_s(synth_start);
    std::string error;
    const bool written = write_wav(opt.output, audio, error);
    const double seconds = audio.sample_rate > 0 ? static_cast<double>(audio.num_samples) / audio.sample_rate : 0.0;
    const int sample_rate = audio.sample_rate;
    kokoro_audio_free(&audio);
    if (!written) {
        std::fprintf(stderr, "kokoro: %s\n", error.c_str());
        return 1;
    }
    std::fprintf(stderr, "Wrote %s (%.2f s audio, %d Hz, synthesis %.3f s, %s)\n", opt.output.c_str(), seconds,
                 sample_rate, synth_s, device_name(kokoro_context_device(ctx.get())));
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    kokoro_cli::init_console();
    Options opt;
    if (!parse_args(kokoro_cli::utf8_args(argc, argv), opt)) return 2;
    if (opt.help) {
        print_usage(stdout);
        return 0;
    }
    if (opt.version) {
        std::printf("%s\n", kokoro_version());
        return 0;
    }
    return run(opt);
}
