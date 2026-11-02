// End-to-end checks against the real model.
// usage: test_synthesis <model.onnx> <voices.bin> <dict_dir> [doctest options]
// Exits 77 (CTest skip) when the model or voices file is missing.
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <kokoro/kokoro.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

kokoro_ctx* g_ctx = nullptr;
const char* g_model_path = nullptr;
const char* g_voices_path = nullptr;
const char* g_dict_dir = nullptr;

const char* const kText = "你好，世界。";
const char* const kVoice = "zf_002";

std::vector<float> synth(kokoro_ctx* ctx, const char* text, const char* voice, float speed, unsigned flags,
                         kokoro_status* status) {
    kokoro_audio audio{};
    *status = kokoro_synthesize(ctx, text, voice, speed, flags, &audio);
    std::vector<float> samples(audio.samples, audio.samples + audio.num_samples);
    kokoro_audio_free(&audio);
    return samples;
}

std::vector<float> synth(const char* text, const char* voice, float speed, unsigned flags, kokoro_status* status) {
    return synth(g_ctx, text, voice, speed, flags, status);
}

std::string ph(const char* text) {
    char* phonemes = nullptr;
    REQUIRE(kokoro_phonemize(g_ctx, text, &phonemes) == KOKORO_OK);
    const std::string result = phonemes;
    kokoro_string_free(phonemes);
    return result;
}

size_t synth_length(const char* text, const char* voice, unsigned flags) {
    kokoro_status status = KOKORO_ERROR_UNKNOWN;
    const size_t n = synth(text, voice, 1.0f, flags, &status).size();
    CAPTURE(std::string(text));
    CAPTURE(std::string(voice));
    CHECK(status == KOKORO_OK);
    return n;
}

size_t code_points(const std::string& s) {
    return static_cast<size_t>(
        std::count_if(s.begin(), s.end(), [](char c) { return (static_cast<unsigned char>(c) & 0xC0) != 0x80; }));
}

std::string join(const std::vector<std::string>& parts, const std::string& separator) {
    std::string out;
    for (size_t i = 0; i < parts.size(); ++i) out += (i == 0 ? "" : separator) + parts[i];
    return out;
}

double synth_seconds(const std::string& text, const char* voice) {
    return static_cast<double>(synth_length(text.c_str(), voice, 0)) / 24000.0;
}

// Minimal reader/writer for the voices file format (docs/adding-voices.md).
using VoiceTable = std::map<std::string, std::vector<float>>;
constexpr size_t kStyleDim = 256;
constexpr size_t kStyleRows = 510;

uint32_t read_u32(std::ifstream& in) {
    uint32_t v = 0;
    in.read(reinterpret_cast<char*>(&v), sizeof v);
    return v;
}

void write_u32(std::ofstream& out, uint32_t v) {
    out.write(reinterpret_cast<const char*>(&v), sizeof v);
}

VoiceTable read_voices(const char* path) {
    std::ifstream in(std::filesystem::u8path(path), std::ios::binary);
    char magic[4] = {};
    in.read(magic, 4);
    if (!in || std::memcmp(magic, "VOIC", 4) != 0 || read_u32(in) != 1) throw std::runtime_error("bad voices file");
    VoiceTable voices;
    for (uint32_t count = read_u32(in); in && count > 0; --count) {
        std::string name(read_u32(in), '\0');
        in.read(&name[0], static_cast<std::streamsize>(name.size()));
        std::vector<float> style(read_u32(in));
        in.read(reinterpret_cast<char*>(style.data()), static_cast<std::streamsize>(style.size() * sizeof(float)));
        voices[name] = std::move(style);
    }
    if (!in) throw std::runtime_error("truncated voices file");
    return voices;
}

void write_voices(const std::filesystem::path& path, const VoiceTable& voices) {
    std::ofstream out(path, std::ios::binary);
    out.write("VOIC", 4);
    write_u32(out, 1);
    write_u32(out, static_cast<uint32_t>(voices.size()));
    for (const auto& [name, style] : voices) {
        write_u32(out, static_cast<uint32_t>(name.size()));
        out.write(name.data(), static_cast<std::streamsize>(name.size()));
        write_u32(out, static_cast<uint32_t>(style.size()));
        out.write(reinterpret_cast<const char*>(style.data()), static_cast<std::streamsize>(style.size() * sizeof(float)));
    }
    if (!out) throw std::runtime_error("cannot write " + path.string());
}

}  // namespace

TEST_CASE("voices_are_listed_sorted") {
    const size_t n = kokoro_voice_count(g_ctx);
    REQUIRE(n > 0);

    bool has_default_voice = false;
    const char* prev = nullptr;
    for (size_t i = 0; i < n; ++i) {
        const char* cur = kokoro_voice_name(g_ctx, i);
        CAPTURE(i);
        REQUIRE(cur != nullptr);
        if (prev) CHECK(std::strcmp(prev, cur) < 0);
        if (std::strcmp(cur, kVoice) == 0) has_default_voice = true;
        prev = cur;
    }
    CHECK(kokoro_voice_name(g_ctx, n) == nullptr);
    CHECK(has_default_voice);
}

TEST_CASE("phonemize_matches_g2p") {
    char* phonemes = nullptr;
    REQUIRE(kokoro_phonemize(g_ctx, "中国", &phonemes) == KOKORO_OK);
    CHECK(std::string(phonemes) == "ʈʂʊ→ŋkwo↗");
    kokoro_string_free(phonemes);
}

TEST_CASE("synthesize_produces_audio") {
    kokoro_audio audio{};
    REQUIRE(kokoro_synthesize(g_ctx, kText, kVoice, 1.0f, 0, &audio) == KOKORO_OK);
    CHECK(audio.sample_rate == 24000);
    REQUIRE(audio.num_samples > 0);

    bool all_finite = true;
    float peak = 0.0f;
    for (size_t i = 0; i < audio.num_samples; ++i) {
        all_finite = all_finite && std::isfinite(audio.samples[i]);
        peak = std::fmax(peak, std::fabs(audio.samples[i]));
    }
    CHECK(all_finite);
    CHECK(peak > 1e-3f);

    kokoro_audio_free(&audio);
    CHECK(audio.samples == nullptr);
    CHECK(audio.num_samples == 0);
}

TEST_CASE("number_language_selects_reading") {
    char* phonemes = nullptr;
    REQUIRE(kokoro_phonemize(g_ctx, "I have 3 apples", &phonemes) == KOKORO_OK);
    const std::string english = phonemes;
    kokoro_string_free(phonemes);
    CHECK(english.find("θɹˈi") != std::string::npos);

    REQUIRE(kokoro_set_number_language(g_ctx, KOKORO_NUMBERS_CHINESE) == KOKORO_OK);
    REQUIRE(kokoro_phonemize(g_ctx, "I have 3 apples", &phonemes) == KOKORO_OK);
    const std::string chinese = phonemes;
    kokoro_string_free(phonemes);
    CHECK(chinese.find("sa→n") != std::string::npos);
    CHECK(chinese.find("θɹˈi") == std::string::npos);

    CHECK(kokoro_set_number_language(g_ctx, static_cast<kokoro_number_language>(7)) == KOKORO_ERROR_INVALID_ARGUMENT);

    REQUIRE(kokoro_set_number_language(g_ctx, KOKORO_NUMBERS_AUTO) == KOKORO_OK);
}

TEST_CASE("language_selection") {
    const std::string p_zh = ph("3");  // AUTO without a voice: Chinese/English
    CHECK(p_zh.find("sa→n") != std::string::npos);
    REQUIRE(kokoro_set_language(g_ctx, KOKORO_LANGUAGE_ENGLISH) == KOKORO_OK);
    const std::string p_en = ph("3");
    CHECK(p_en == "θɹˈi");
    REQUIRE(kokoro_set_language(g_ctx, KOKORO_LANGUAGE_CHINESE_ENGLISH) == KOKORO_OK);
    CHECK(ph("中国") == "ʈʂʊ→ŋkwo↗");
    REQUIRE(kokoro_set_language(g_ctx, KOKORO_LANGUAGE_AUTO) == KOKORO_OK);

    // AUTO routes by voice name; the duration predictor is deterministic, so equal phonemes give equal lengths.
    const size_t n_en = synth_length(p_en.c_str(), "af_maple", KOKORO_INPUT_PHONEMES);
    REQUIRE(n_en != synth_length(p_zh.c_str(), "af_maple", KOKORO_INPUT_PHONEMES));
    CHECK(synth_length("3", "af_maple", 0) == n_en);
    CHECK(synth_length("3", kVoice, 0) == synth_length(p_zh.c_str(), kVoice, KOKORO_INPUT_PHONEMES));
}

TEST_CASE("phoneme_input_matches_text_input") {
    kokoro_status text_status = KOKORO_ERROR_UNKNOWN;
    const std::vector<float> from_text = synth("你好", kVoice, 1.0f, 0, &text_status);

    char* phonemes = nullptr;
    REQUIRE(kokoro_phonemize(g_ctx, "你好", &phonemes) == KOKORO_OK);
    kokoro_status phoneme_status = KOKORO_ERROR_UNKNOWN;
    const std::vector<float> from_phonemes = synth(phonemes, kVoice, 1.0f, KOKORO_INPUT_PHONEMES, &phoneme_status);
    kokoro_string_free(phonemes);

    CHECK(text_status == KOKORO_OK);
    CHECK(phoneme_status == KOKORO_OK);
    // The vocoder injects random noise, so samples differ run to run even for identical input;
    // the duration predictor is deterministic, so the same phonemes yield the same length.
    CHECK(!from_text.empty());
    CHECK(from_text.size() == from_phonemes.size());
}

// A chunk with no in-vocabulary phonemes is skipped rather than run on the padding tokens alone.
TEST_CASE("tokenless_phonemes_produce_no_audio") {
    kokoro_status status = KOKORO_ERROR_UNKNOWN;
    const std::vector<float> samples = synth("###", kVoice, 1.0f, KOKORO_INPUT_PHONEMES, &status);
    CHECK(status == KOKORO_OK);
    CHECK(samples.size() == 0);
}

TEST_CASE("faster_speed_is_shorter") {
    kokoro_status normal_status = KOKORO_ERROR_UNKNOWN;
    kokoro_status fast_status = KOKORO_ERROR_UNKNOWN;
    const size_t normal = synth(kText, kVoice, 1.0f, 0, &normal_status).size();
    const size_t fast = synth(kText, kVoice, 2.0f, 0, &fast_status).size();
    CHECK(normal_status == KOKORO_OK);
    CHECK(fast_status == KOKORO_OK);
    CHECK(fast < normal);
}

TEST_CASE("unknown_voice_is_reported") {
    kokoro_audio audio{};
    CHECK(kokoro_synthesize(g_ctx, kText, "no_such_voice", 1.0f, 0, &audio) == KOKORO_ERROR_VOICE_NOT_FOUND);
    CHECK(audio.samples == nullptr);
    CHECK(audio.num_samples == 0);
    CHECK(audio.sample_rate == 0);
    CHECK(std::string(kokoro_last_error()).find("no_such_voice") != std::string::npos);
}

TEST_CASE("invalid_speed_and_flags_rejected") {
    const float bad_speeds[] = {0.0f, -1.0f, NAN, INFINITY, 2147483648.0f, 1e20f};
    for (float speed : bad_speeds) {
        CAPTURE(speed);
        kokoro_audio audio{};
        CHECK(kokoro_synthesize(g_ctx, kText, kVoice, speed, 0, &audio) == KOKORO_ERROR_INVALID_ARGUMENT);
    }
    kokoro_audio audio{};
    CHECK(kokoro_synthesize(g_ctx, kText, kVoice, 1.0f, 0x2u, &audio) == KOKORO_ERROR_INVALID_ARGUMENT);
}

// A chunk of n phoneme tokens must use style row n - 1, as upstream Kokoro does (`pack[len(ps)-1]`).
// Each probe voice is zf_002 with row 4 taken from af_maple, so on a 5-token input it matches
// af_maple's deterministic duration only if row 4 is the row read. probe_short ends at row 4,
// which covers the last row of a table (row 509 of a full one).
TEST_CASE("style_row_follows_token_count") {
    const char* const input = "nixau";  // 5 phonemes, all in the vocabulary: one token each
    const size_t row = 4;

    VoiceTable voices = read_voices(g_voices_path);
    const std::vector<float>& donor = voices.at("af_maple");
    std::vector<float> probe = voices.at("zf_002");
    REQUIRE(probe.size() == kStyleRows * kStyleDim);
    std::copy_n(donor.begin() + row * kStyleDim, kStyleDim, probe.begin() + row * kStyleDim);
    voices["probe_short"].assign(probe.begin(), probe.begin() + (row + 1) * kStyleDim);
    voices["probe"] = std::move(probe);
    const auto probe_path = std::filesystem::temp_directory_path() / "kokoro_test_probe_voices.bin";
    write_voices(probe_path, voices);

    kokoro_ctx* ctx = nullptr;
    const kokoro_status created = kokoro_create(g_model_path, probe_path.u8string().c_str(), g_dict_dir, &ctx);
    std::filesystem::remove(probe_path);
    REQUIRE_MESSAGE(created == KOKORO_OK, kokoro_last_error());

    auto length = [&](const char* voice) {
        kokoro_status status = KOKORO_ERROR_UNKNOWN;
        const size_t n = synth(ctx, input, voice, 1.0f, KOKORO_INPUT_PHONEMES, &status).size();
        CAPTURE(voice);
        CHECK(status == KOKORO_OK);
        return n;
    };
    const size_t with_donor = length("af_maple");
    // Otherwise a wrong row could not be told apart from the right one.
    REQUIRE(with_donor != length("zf_002"));
    CHECK(length("probe") == with_donor);
    CHECK(length("probe_short") == with_donor);
    kokoro_destroy(ctx);
}

// Unpunctuated text longer than one chunk used to lose everything past byte 510 of the stretch. Commas add pauses, so
// the unpunctuated text is a bit shorter, but not by the truncated share.
TEST_CASE("unpunctuated_long_text_is_not_truncated") {
    struct Variant {
        const char* voice;
        std::vector<std::string> clauses;
        std::string reference_separator;
        std::vector<std::string> separators;
    };
    const std::vector<std::string> english = {
        "the old lighthouse keeper climbed the narrow stairs every single evening",
        "he polished the great brass lamp until it shone across the dark water",
        "the fishing boats followed its steady beam back to the quiet harbor",
        "his daughter wrote down the weather in a small leather notebook",
        "the storms of that winter were the worst anyone could remember",
        "but the light never once failed the sailors who trusted it",
    };
    std::vector<std::string> english_twice = english;
    english_twice.insert(english_twice.end(), english.begin(), english.end());
    const std::vector<Variant> variants = {
        {"af_maple", english_twice, ", and ", {" and ", ": "}},
        {kVoice, std::vector<std::string>(8, "我们明天早上一起去公园散步然后回家吃饭"), "，", {""}},
    };

    for (const Variant& v : variants) {
        const double reference_s = synth_seconds(join(v.clauses, v.reference_separator), v.voice);
        for (const std::string& sep : v.separators) {
            const std::string text = join(v.clauses, sep);
            CAPTURE(text);
            REQUIRE(code_points(ph(text.c_str())) > 510);
            const double plain_s = synth_seconds(text, v.voice);
            MESSAGE(std::string(v.voice), " sep='", sep, "': ", plain_s, " s vs ", reference_s, " s punctuated");
            CHECK(plain_s > 0.85 * reference_s);
            CHECK(plain_s < 1.15 * reference_s);
        }
    }
}

TEST_CASE("cpu_device_is_honored") {
    kokoro_options options = kokoro_default_options();
    options.device = KOKORO_DEVICE_CPU;
    kokoro_ctx* ctx = nullptr;
    REQUIRE_MESSAGE(kokoro_create_ex(g_model_path, g_voices_path, g_dict_dir, &options, &ctx) == KOKORO_OK,
                    kokoro_last_error());
    CHECK(kokoro_context_device(ctx) == KOKORO_DEVICE_CPU);

    kokoro_status status = KOKORO_ERROR_UNKNOWN;
    const std::vector<float> samples = synth(ctx, kText, kVoice, 1.0f, 0, &status);
    CHECK(status == KOKORO_OK);
    CHECK(!samples.empty());
    kokoro_destroy(ctx);
}

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: test_synthesis <model.onnx> <voices.bin> <dict_dir> [doctest options]\n");
        return 2;
    }
    if (!std::filesystem::exists(std::filesystem::u8path(argv[1])) ||
        !std::filesystem::exists(std::filesystem::u8path(argv[2]))) {
        std::printf("SKIP: model or voices file not found (%s, %s)\n", argv[1], argv[2]);
        return 77;
    }
    g_model_path = argv[1];
    g_voices_path = argv[2];
    g_dict_dir = argv[3];
    if (kokoro_create(g_model_path, g_voices_path, g_dict_dir, &g_ctx) != KOKORO_OK) {
        std::fprintf(stderr, "kokoro_create failed: %s\n", kokoro_last_error());
        return 1;
    }

    std::vector<char*> dt_args{argv[0]};
    dt_args.insert(dt_args.end(), argv + 4, argv + argc);
    doctest::Context context;
    context.applyCommandLine(static_cast<int>(dt_args.size()), dt_args.data());
    const int rc = context.run();
    kokoro_destroy(g_ctx);
    return rc;
}
