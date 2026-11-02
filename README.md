# Kokoro C++ Inference
[![CI](https://github.com/larroy/kokoro.cpp/actions/workflows/ci.yml/badge.svg)](https://github.com/larroy/kokoro.cpp/actions/workflows/ci.yml)

English | [中文](#kokoro-c-推理) | [Español](#kokoro-c-inferencia) | [Deutsch](#kokoro-c-inferenz)

A high-performance, lightweight C++ inference implementation of the [Kokoro](https://huggingface.co/hexgrad/Kokoro-82M) TTS model, built on ONNX Runtime. The project currently supports mixed **Chinese and English** synthesis, **Spanish** with the Kokoro-82M v1.0 model, and **German** with a German fine-tune.

## Features

- 🚀 **Fast inference**: powered by ONNX Runtime.
- 🌏 **Multilingual**: native support for Chinese and English; Spanish (Castilian) with the v1.0 model; German with a German fine-tune.

## Requirements

- **CMake** (3.15+)
- **Ninja** (recommended; `uv run bootstrap.py build` uses it when it is in `PATH`)
- **C++ compiler** (C++17 support required)
- **Rust** (1.80+, via [rustup](https://rustup.rs)): builds `kokoro-server`; configure with `-DKOKORO_BUILD_SERVER=OFF` to skip it
- **[uv](https://docs.astral.sh/uv/)**: runs `bootstrap.py` and `voice_tool.py`, and installs their Python dependencies (Python 3.10+, click) into `.venv` on first use

## Setup

```bash
uv run bootstrap.py configure
```

This downloads, verifying SHA-256 checksums:

- the prebuilt [ONNX Runtime](https://github.com/microsoft/onnxruntime/releases/tag/v1.23.2) 1.23.2 (CPU; the CUDA build on supported NVIDIA GPUs, see below) for the current platform (Linux x64/aarch64, macOS arm64/x86_64, Windows x64/arm64) into `third_party/onnxruntime/`;
- the model `kokoro-v1.1-zh.onnx` and the voice pack `voices-v1.1-zh.bin` from this repository's [`voices_model_files` release](https://github.com/larroy/kokoro.cpp/releases/tag/voices_model_files) into `models/`;
- the Kokoro-82M v1.0 model `kokoro-v1.0.onnx` from [kokoro-onnx](https://github.com/thewh1teagle/kokoro-onnx/releases/tag/model-files-v1.0), and the Spanish voices `ef_dora`, `em_alex` and `em_santa` from [onnx-community/Kokoro-82M-v1.0-ONNX](https://huggingface.co/onnx-community/Kokoro-82M-v1.0-ONNX), packed into `models/voices-v1.0-es.bin`;
- the German fine-tune `kokoro-de.onnx` from [Godelaune/Kokoro-82M-ONNX-German-Martin](https://huggingface.co/Godelaune/Kokoro-82M-ONNX-German-Martin), and the German voices `df_eva`, `df_victoria`, `dm_bernd` and `dm_martin` from [cstr/kokoro-voices-GGUF](https://huggingface.co/cstr/kokoro-voices-GGUF), packed into `models/voices-de.bin`.

Files that are already present and up to date are skipped; `--force` downloads them again. The ONNX Runtime version and checksums are pinned in `bootstrap.py`. To use another ONNX Runtime installation instead, skip `configure` and pass `-DONNXRUNTIME_ROOT=/path/to/onnxruntime` to CMake.

## Building

```bash
uv run bootstrap.py build
```

`build` runs the CMake configure and build steps with the Ninja generator. If `ninja` is not in `PATH` it prints a warning and falls back to CMake's default generator; passing `-G` after `--` overrides the choice. Options: `--build-dir` (default `build`), `--config` (`Debug`, `Release`, `RelWithDebInfo`, `MinSizeRel`; default `Release`), `-j/--jobs`. Arguments after `--` go to the CMake configure step, e.g. `uv run bootstrap.py build -- -DKOKORO_BUILD_TESTS=OFF`. A build directory can't switch generators: delete it first if it was configured with another one. On Windows, Ninja needs the MSVC environment (run from a Developer Command Prompt). It is equivalent to:

```bash
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

This produces the shared library `kokoro` (`kokoro.dll` / `libkokoro.so` / `libkokoro.dylib`) and the `kokoro` command-line tool. `libkokoro` links against the ONNX Runtime shared library. On Linux/macOS the build tree finds it through the rpath; installed copies need it findable at runtime. On Windows `onnxruntime.dll` is copied next to `kokoro.dll` (and installed with it).

### CUDA (experimental)

CUDA support is experimental. `uv run bootstrap.py configure` uses the prebuilt CUDA build of ONNX Runtime when it detects a supported NVIDIA GPU (Windows x64: sm 7.5, 8.6, 8.9 plus PTX 9.0; Linux x64: sm 6.0, 7.0, 7.5, 8.0 plus PTX 9.0) and the NVIDIA driver supports CUDA 12.8 or newer; pass `--ort cpu` or `--ort gpu` to force a package. For other GPUs, build ONNX Runtime with CUDA from source:

```bash
uv run bootstrap.py build-ort   # needs CMake 3.28+, git, a CUDA 12 toolkit and cuDNN 9 for CUDA 12
```

`build-ort` locates the CUDA toolkit through the `CUDA_HOME` or `CUDA_PATH` environment variable (the CUDA installer sets `CUDA_PATH`, e.g. `C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.9` on Windows), or through `nvcc` in `PATH`, or in the standard install locations (`C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.*`, `/usr/local/cuda`); `--cuda-home` points at an explicit toolkit. cuDNN is located through `CUDNN_HOME` or `CUDNN_PATH` (`--cudnn-home`; use the archive from https://developer.download.nvidia.com/compute/cudnn/redist/cudnn/, not the installer layout). The result replaces the prebuilt package in `third_party/onnxruntime/`.

At runtime the CUDA 12 and cuDNN 9 libraries must be loadable: on `PATH` on Windows (`cudart64_12.dll`, `cublas64_12.dll`, `cublasLt64_12.dll`, `curand64_10.dll`, `cufft64_11.dll`, `cudnn64_9.dll`), on `LD_LIBRARY_PATH` on Linux (`libcudart.so.12`, `libcublas.so.12`, `libcublasLt.so.12`, `libcurand.so.10`, `libcufft.so.11`, `libcudnn.so.9`); if they are missing, kokoro prints a warning and runs on the CPU. Select the inference device with `--device auto|cpu|cuda` and `--gpu-id`; with `auto` a CUDA initialization failure falls back to the CPU, with `cuda` it is an error.

### Tests

```bash
ctest --test-dir build -C Release --output-on-failure
```

Tests live in `tests/` (doctest, vendored in `third_party/doctest`): `c_api` checks the C API without a model, `g2p` pins the Chinese/English G2P output, `server` runs `cargo test` for `kokoro-server` (request parsing, WAV/PCM encoding, the listen socket, and an end-to-end HTTP check against the model), and `synthesis` runs the real model from `models/` (model-dependent tests are skipped if the model or voices file is missing). Configure with `-DKOKORO_BUILD_TESTS=OFF` to skip building them.

The Python tools have their own tests in `tests/python/`: run `uv run pytest`. The `.pt` import tests are skipped unless PyTorch is installed; `uv run --group voice pytest` runs them too.

## Usage / Demo

Run the `kokoro` command-line tool from the project root (the defaults point at `models/` and `dict/`). With Visual Studio or another multi-config generator the executable is in `build/Release/`.

```bash
./build/kokoro [options] <text>
./build/kokoro --list-voices
```

| Option | Meaning |
|---|---|
| `-m, --model <path>` | ONNX model file (default: `models/kokoro-v1.1-zh.onnx`) |
| `--voices <path>` | voices file (default: `models/voices-v1.1-zh.bin`) |
| `-d, --dict <dir>` | dictionary directory (default: `dict`) |
| `-v, --voice <name>` | voice (default: `af_maple`); English voices: `af_maple`, `af_sol`, `bf_vale`; Chinese voices: `zf_*`, `zm_*` |
| `-s, --speed <rate>` | speaking rate, > 0 (default: `1.0`) |
| `--lang <auto\|en\|zh\|es\|de>` | language for reading numbers (default: `auto`) |
| `--language <auto\|zh\|en\|es\|de>` | text language; `auto`: `es` for `ef_*`/`em_*`, `de` for `df_*`/`dm_*`, `en` for `af_`/`am_`/`bf_`/`bm_*` voices, else `zh` (Chinese/English) (default: `auto`) |
| `-o, --output <path>` | output WAV file (default: `output.wav`) |
| `-p, --phonemes` | `<text>` is a phoneme string; skip G2P |
| `--phonemize` | print the phonemes for `<text>` instead of synthesizing |
| `-i, --interactive` | read phrases from stdin and play each one as soon as it is synthesized (`-o` is unused) |
| `--list-voices` | print the available voices |

To add your own voices (blends, imported `.pt` tensors), see [docs/adding-voices.md](docs/adding-voices.md).

The Spanish and German G2Ps are checked against espeak-ng with
`uv run --group eval python eval_bench/compare_g2p.py --language es|de` (needs a built `g2p_dump`).

### Example

```bash
./build/kokoro -o hello.wav "Hello world"
./build/kokoro --voice zf_002 "你好啊，这是一个测试。Hello world"
./build/kokoro --phonemize "中国"
```

Spanish (voices `ef_dora`, `em_alex`, `em_santa`; `ef_*`/`em_*` voices select the Spanish G2P automatically):

```bash
./build/kokoro -m models/kokoro-v1.0.onnx --voices models/voices-v1.0-es.bin -v ef_dora -o hola.wav \
    "Hola, ¿cómo estás? Tengo 25 años."
./build/kokoro -m models/kokoro-v1.0.onnx --voices models/voices-v1.0-es.bin --language es --phonemize "Tengo 25 años."
# tˈɛŋɡo βAntiθˈinko ˈaɲos.
```

German (voices `df_eva`, `df_victoria`, `dm_bernd`, `dm_martin`; `df_*`/`dm_*` voices select the German G2P automatically):

```bash
./build/kokoro -m models/kokoro-de.onnx --voices models/voices-de.bin -v dm_martin -o hallo.wav \
    "Guten Tag, wie geht es dir?"
./build/kokoro -m models/kokoro-de.onnx --voices models/voices-de.bin --language de --phonemize "Ich habe 1.000 Euro."
# ɪç hɑːbə ˈIntWzənt ˈɔørɔ.
```

### Interactive mode

`-i` reads phrases from the terminal and plays each one as soon as it is synthesized. While it runs, `/voice [name]`,
`/speed [rate]` and `/voices` change or show the settings, `/help` lists the commands and `/quit` exits:

```bash
./build/kokoro -v af_maple -i
```

Output is a mono 32-bit float WAV at 24 kHz. Exit status is 0 on success, 1 on a library error, and 2 on a usage error.

### HTTP server

`kokoro-server` loads the model once and serves an OpenAI-compatible text-to-speech API. It is written in Rust (`src/server/`, axum) on top of the C API, and `uv run bootstrap.py build` builds it with cargo next to `libkokoro`. Run it from the project root (it takes the same `-m`, `--voices`, `-d`, `--lang`, `--language`, `--device` and `--gpu-id` options as `kokoro`):

```bash
./build/kokoro-server --host 127.0.0.1 --port 8880 -v af_maple
curl -o hello.wav -H 'Content-Type: application/json' \
    -d '{"model":"kokoro","input":"Hello world","voice":"af_maple"}' http://127.0.0.1:8880/v1/audio/speech
```

- `POST /v1/audio/speech`: `input` (required), `voice` (default: `-v`), `speed` (> 0, default 1.0) and `response_format` (`wav`, the default, or `pcm`). `wav` is a 16-bit PCM mono WAV at 24 kHz; `pcm` is the same samples raw, signed 16-bit little-endian. Other formats, unknown voices and invalid fields get HTTP 400 with an OpenAI-style JSON error. `model` and `instructions` are ignored.
- `GET /v1/audio/voices`: `{"voices": [...]}` with the loaded voice names.
- `GET /health`: `{"status":"ok"}`.

Requests are synthesized one at a time on the shared model. The server has no authentication, so keep it on loopback. Exit status is 1 if the model cannot be loaded or the port is in use, and 2 on a usage error.

## Library

`libkokoro` exposes a C API in [`include/kokoro/kokoro.h`](include/kokoro/kokoro.h), so it can be used from C, C++ or any language with a C FFI (Python `ctypes`, C#, Rust, Go, ...):

```c
#include <kokoro/kokoro.h>

kokoro_ctx* ctx = NULL;
if (kokoro_create("models/kokoro-v1.1-zh.onnx", "models/voices-v1.1-zh.bin", "dict", &ctx) != KOKORO_OK) {
    fprintf(stderr, "%s\n", kokoro_last_error());
    return 1;
}
kokoro_audio audio;
if (kokoro_synthesize(ctx, "你好，世界。Hello world", "zf_002", 1.0f, 0, &audio) == KOKORO_OK) {
    /* audio.samples: mono float PCM, audio.num_samples samples at audio.sample_rate Hz */
    kokoro_audio_free(&audio);
}
kokoro_destroy(ctx);
```

- All strings, including paths, are UTF-8. `dict_dir` is the directory holding `vocab.txt` and the G2P dictionaries (`dict/` in this repo).
- Errors are reported as `kokoro_status` codes; `kokoro_last_error()` returns the message for the calling thread.
- `kokoro_voice_count` / `kokoro_voice_name` enumerate voices, `kokoro_phonemize` returns the phoneme string, and `KOKORO_INPUT_PHONEMES` synthesizes phonemes directly.
- Separate contexts can be used from different threads; a single context must not be used concurrently.

Install it and consume it from CMake:

```bash
cmake --install build --config Release --prefix /path/to/prefix
```

```cmake
find_package(kokoro REQUIRED)   # with CMAKE_PREFIX_PATH=/path/to/prefix
target_link_libraries(app PRIVATE kokoro::kokoro)
```

Alternatively, `add_subdirectory(kokoro.cpp)` and link `kokoro::kokoro`.

### .NET (NuGet)

`Larroy.Kokoro` wraps the C API for .NET 8 and .NET 10 (`dotnet/`); its runtime packages bring `kokoro.dll` for win-x64 and win-arm64 and take ONNX Runtime from the `Microsoft.ML.OnnxRuntime` package, and `Larroy.Kokoro.runtime.win-x64.cuda` adds CUDA through `Microsoft.ML.OnnxRuntime.Gpu.Windows`. See [`packaging/README.md`](packaging/README.md) for usage. To build the packages on Windows (needs VS 2022 with the x64 and ARM64 C++ tools, `nuget.exe` and the .NET SDK):

```powershell
./packaging/build-natives.ps1                   # kokoro.dll per RID -> artifacts/natives/<rid>/
dotnet test dotnet/Kokoro.Net.sln -c Release
./packaging/pack.ps1                            # -> artifacts/nuget/*.nupkg
./packaging/smoke.ps1 -Framework net8.0         # end-to-end check against the local feed
```

Interactive mode also exists as a .NET example (`dotnet/examples/Kokoro.Interactive/`, built by the solution above,
not shipped in the NuGet packages):

```powershell
dotnet run --project dotnet/examples/Kokoro.Interactive -c Release -- --device cpu
```

## English G2P

English words are converted to phonemes in the following order (output uses the Kokoro/misaki English phoneme set):

1. `dict/user_en.dict`: user dictionary in CMU format (`WORD ARPAbet-phonemes`, e.g. `onnx AA1 N IH0 K S`). Highest priority; intended for proper nouns.
2. `dict/cmudict-0.7b/cmudict.dict`: the CMU Pronouncing Dictionary.
3. All-uppercase words of at most 5 letters (e.g. `GPU`) are spelled out by English letter names.
4. Other out-of-vocabulary words are predicted by the `dict/g2p_en.weights` neural network, ported from [g2p_en](https://github.com/Kyubyong/g2p) (Apache-2.0, see `dict/g2p_en.LICENSE.txt`).

`g2p_en.weights` is generated from g2p_en's `checkpoint20.npz` (numpy not required):

```bash
python scripts/export_g2p.py checkpoint20.npz dict/g2p_en.weights
```

## Spanish loanwords

Loanwords whose Spanish pronunciation does not follow their spelling (`parking`, `pizza`, `show`, `jazz`, ...) are
read from a Spanish respelling instead (`párkin`, `pítsa`, `ʃóu`, `yas`). A built-in table covers common words; the
plural `-s` of a listed word is handled too. Add or override entries in the optional `dict/es_loanwords.tsv`, one
`word<TAB>respelling` per line (`#` starts a comment). Write the respelling in Spanish orthography with a written
accent on the stressed vowel; sounds Spanish spelling lacks are written as IPA (`ʃ`).

## Project Structure

- `include/kokoro/kokoro.h`, `src/kokoro_c.cpp`: public C API of `libkokoro`.
- `src/cli/main.cpp`: the `kokoro` command-line tool.
- `src/server/`: the `kokoro-server` OpenAI-compatible HTTP server (Rust crate).
- `src/Kokoro.cpp/h`: main TTS class (internal).
- `src/EnG2P.h`, `src/NeuralG2P.cpp/h`: English G2P (dictionary lookup and neural prediction).
- `src/SpanishG2P.cpp/h`: rule-based Spanish G2P (espeak-ng `es` conventions).
- `src/german/`: German G2P (MFA dictionary, compound splitting, g2p_de neural fallback; espeak-ng `de` conventions).
- `eval_bench/`: G2P evaluation against espeak-ng (`g2p_dump`, `compare_g2p.py`, corpora).
- `tests/`: library tests (run with `ctest`); `tests/python/`: tests for the Python tools (run with `uv run pytest`).
- `dotnet/`, `packaging/`: .NET wrapper (`Larroy.Kokoro`) and the scripts that build its NuGet packages.
- `ARCHITECTURE.md`: how the components fit together.
- `docs/`: guides ([adding voices](docs/adding-voices.md), [thread safety](docs/thread-safety.md)).
- `scripts/`: helper scripts for data processing.
- `dict/`: G2P dictionary files (Jieba, pinyin, CMU, g2p_en weights, German MFA dictionary, g2p_de weights).

## License

MIT

---

# Kokoro C++ 推理

[English](#kokoro-c-inference) | 中文 | [Español](#kokoro-c-inferencia) | [Deutsch](#kokoro-c-inferenz)

基于 ONNX Runtime 的 [Kokoro](https://huggingface.co/hexgrad/Kokoro-82M) TTS 模型的高性能轻量级 C++ 推理实现。本项目目前支持**中英文**混合合成，可使用 Kokoro-82M v1.0 模型合成**西班牙语**，并可使用德语微调模型合成**德语**。

## 特性

- 🚀 **快速推理**：由 ONNX Runtime 驱动。
- 🌏 **多语言**：原生支持中文和英文；使用 v1.0 模型支持西班牙语（卡斯蒂利亚口音）；使用德语微调模型支持德语。

## 依赖环境

- **CMake** (3.15+)
- **Ninja**（推荐；`uv run bootstrap.py build` 在 `PATH` 中找到它时使用）
- **C++ 编译器** (需要支持 C++17)
- **Rust**（1.80+，通过 [rustup](https://rustup.rs) 安装）：用于构建 `kokoro-server`；配置时加 `-DKOKORO_BUILD_SERVER=OFF` 可跳过
- **[uv](https://docs.astral.sh/uv/)**：用于运行 `bootstrap.py` 和 `voice_tool.py`，首次运行时会将其 Python 依赖（Python 3.10+、click）安装到 `.venv`

## 准备

```bash
uv run bootstrap.py configure
```

该命令会下载以下文件并校验 SHA-256：

- 当前平台（Linux x64/aarch64、macOS arm64/x86_64、Windows x64/arm64）的预编译 [ONNX Runtime](https://github.com/microsoft/onnxruntime/releases/tag/v1.23.2) 1.23.2（CPU 版；受支持的 NVIDIA GPU 上为 CUDA 版，见下文），放入 `third_party/onnxruntime/`；
- 模型 `kokoro-v1.1-zh.onnx` 和语音包 `voices-v1.1-zh.bin`，来自本仓库的 [`voices_model_files` release](https://github.com/larroy/kokoro.cpp/releases/tag/voices_model_files)，放入 `models/`；
- Kokoro-82M v1.0 模型 `kokoro-v1.0.onnx`（来自 [kokoro-onnx](https://github.com/thewh1teagle/kokoro-onnx/releases/tag/model-files-v1.0)），以及西班牙语语音 `ef_dora`、`em_alex`、`em_santa`（来自 [onnx-community/Kokoro-82M-v1.0-ONNX](https://huggingface.co/onnx-community/Kokoro-82M-v1.0-ONNX)），打包为 `models/voices-v1.0-es.bin`；
- 德语微调模型 `kokoro-de.onnx`（来自 [Godelaune/Kokoro-82M-ONNX-German-Martin](https://huggingface.co/Godelaune/Kokoro-82M-ONNX-German-Martin)），以及德语语音 `df_eva`、`df_victoria`、`dm_bernd`、`dm_martin`（来自 [cstr/kokoro-voices-GGUF](https://huggingface.co/cstr/kokoro-voices-GGUF)），打包为 `models/voices-de.bin`。

已存在且校验一致的文件会被跳过；`--force` 强制重新下载。ONNX Runtime 版本和校验值固定在 `bootstrap.py` 中。如需使用其他 ONNX Runtime，可跳过 `configure`，并向 CMake 传入 `-DONNXRUNTIME_ROOT=/path/to/onnxruntime`。

## 编译

```bash
uv run bootstrap.py build
```

`build` 使用 Ninja 生成器执行 CMake 的配置和编译。若 `PATH` 中没有 `ninja`，会打印警告并回退到 CMake 默认生成器；在 `--` 之后传入 `-G` 可覆盖该选择。选项：`--build-dir`（默认 `build`）、`--config`（`Debug`、`Release`、`RelWithDebInfo`、`MinSizeRel`，默认 `Release`）、`-j/--jobs`。`--` 之后的参数会传给 CMake 配置步骤，例如 `uv run bootstrap.py build -- -DKOKORO_BUILD_TESTS=OFF`。构建目录不能更换生成器：若之前用其他生成器配置过，需先删除。Windows 下 Ninja 需要 MSVC 环境（在 Developer Command Prompt 中运行）。等价于：

```bash
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

编译产物为共享库 `kokoro`（`kokoro.dll` / `libkokoro.so` / `libkokoro.dylib`）以及 `kokoro` 命令行工具。`libkokoro` 链接 ONNX Runtime 共享库：Linux/macOS 下构建目录中通过 rpath 找到它，安装后运行时需能找到它；Windows 下 `onnxruntime.dll` 会被复制到 `kokoro.dll` 旁边（并随之安装）。

### CUDA（实验性）

CUDA 支持是**实验性**的。检测到受支持的 NVIDIA GPU（Windows x64：sm 7.5、8.6、8.9 加 PTX 9.0；Linux x64：sm 6.0、7.0、7.5、8.0 加 PTX 9.0）且驱动支持 CUDA 12.8 或更新版本时，`uv run bootstrap.py configure` 会使用预编译的 CUDA 版 ONNX Runtime；传入 `--ort cpu` 或 `--ort gpu` 可强制选择。其他 GPU 请从源码构建带 CUDA 的 ONNX Runtime：

```bash
uv run bootstrap.py build-ort   # 需要 CMake 3.28+、git、CUDA 12 工具包和 CUDA 12 版 cuDNN 9
```

`build-ort` 通过 `CUDA_HOME` 或 `CUDA_PATH` 环境变量定位 CUDA 工具包（CUDA 安装器会设置 `CUDA_PATH`，如 Windows 上的 `C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.9`），也使用 `PATH` 中的 `nvcc`，最后回退到标准安装位置（`C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.*`、`/usr/local/cuda`）；可用 `--cuda-home` 显式指定。cuDNN 通过 `CUDNN_HOME` 或 `CUDNN_PATH` 定位（`--cudnn-home`；请使用 https://developer.download.nvidia.com/compute/cudnn/redist/cudnn/ 的压缩包，而非安装器布局）。构建结果会替换 `third_party/onnxruntime/` 中的预编译包。

运行时，CUDA 12 和 cuDNN 9 的库必须能被加载：Windows 上位于 `PATH`（`cudart64_12.dll`、`cublas64_12.dll`、`cublasLt64_12.dll`、`curand64_10.dll`、`cufft64_11.dll`、`cudnn64_9.dll`），Linux 上位于 `LD_LIBRARY_PATH`（`libcudart.so.12`、`libcublas.so.12`、`libcublasLt.so.12`、`libcurand.so.10`、`libcufft.so.11`、`libcudnn.so.9`）；缺失时 kokoro 会打印警告并改用 CPU。用 `--device auto|cpu|cuda` 和 `--gpu-id` 选择推理设备；`auto` 在 CUDA 初始化失败时回退到 CPU，`cuda` 则报错。

### 测试

```bash
ctest --test-dir build -C Release --output-on-failure
```

测试位于 `tests/`（使用 doctest，已内置于 `third_party/doctest`）：`c_api` 在无模型的情况下检查 C API，`g2p` 固定中英文 G2P 的输出，`synthesis` 使用 `models/` 中的真实模型运行（模型或语音文件不存在时跳过）。配置时传入 `-DKOKORO_BUILD_TESTS=OFF` 可不编译测试。

Python 工具的测试位于 `tests/python/`：运行 `uv run pytest`。`.pt` 导入测试在未安装 PyTorch 时跳过；`uv run --group voice pytest` 会一并运行。

## 使用方法

在项目根目录运行 `kokoro` 命令行工具（默认路径指向 `models/` 和 `dict/`）。使用 Visual Studio 等多配置生成器时，可执行文件位于 `build/Release/`。

```bash
./build/kokoro [选项] <文本>
./build/kokoro --list-voices
```

| 选项 | 含义 |
|---|---|
| `-m, --model <路径>` | ONNX 模型文件（默认：`models/kokoro-v1.1-zh.onnx`） |
| `--voices <路径>` | 语音文件（默认：`models/voices-v1.1-zh.bin`） |
| `-d, --dict <目录>` | 词典目录（默认：`dict`） |
| `-v, --voice <名称>` | 语音（默认：`af_maple`）；英文语音：`af_maple`、`af_sol`、`bf_vale`；中文语音：`zf_*`、`zm_*` |
| `-s, --speed <语速>` | 语速，须 > 0（默认：`1.0`） |
| `--lang <auto\|en\|zh\|es\|de>` | 数字朗读语言（默认：`auto`） |
| `--language <auto\|zh\|en\|es\|de>` | 文本语言；`auto`：`ef_*`/`em_*` 语音用西班牙语，`df_*`/`dm_*` 语音用德语，`af_`/`am_`/`bf_`/`bm_*` 语音用英语，否则中文/英文（默认：`auto`） |
| `-o, --output <路径>` | 输出 WAV 文件（默认：`output.wav`） |
| `-p, --phonemes` | `<文本>` 为音素串，跳过 G2P |
| `--phonemize` | 输出 `<文本>` 的音素而不合成 |
| `-i, --interactive` | 从标准输入读取句子，每句合成后立即播放（不使用 `-o`） |
| `--list-voices` | 列出可用语音 |

如需添加自定义语音（混合语音、导入 `.pt` 张量），请参阅 [docs/adding-voices.md](docs/adding-voices.md)（英文）。

### 示例

```bash
./build/kokoro -o hello.wav "Hello world"
./build/kokoro --voice zf_002 "你好啊，这是一个测试。Hello world"
./build/kokoro --phonemize "中国"
```

西班牙语（语音 `ef_dora`、`em_alex`、`em_santa`；`ef_*`/`em_*` 语音会自动使用西班牙语 G2P）：

```bash
./build/kokoro -m models/kokoro-v1.0.onnx --voices models/voices-v1.0-es.bin -v ef_dora -o hola.wav \
    "Hola, ¿cómo estás? Tengo 25 años."
./build/kokoro -m models/kokoro-v1.0.onnx --voices models/voices-v1.0-es.bin --language es --phonemize "Tengo 25 años."
# tˈɛŋɡo βAntiθˈinko ˈaɲos.
```

德语（语音 `df_eva`、`df_victoria`、`dm_bernd`、`dm_martin`；`df_*`/`dm_*` 语音会自动使用德语 G2P）：

```bash
./build/kokoro -m models/kokoro-de.onnx --voices models/voices-de.bin -v dm_martin -o hallo.wav \
    "Guten Tag, wie geht es dir?"
./build/kokoro -m models/kokoro-de.onnx --voices models/voices-de.bin --language de --phonemize "Ich habe 1.000 Euro."
# ɪç hɑːbə ˈIntWzənt ˈɔørɔ.
```

### 交互模式

`-i` 从终端读取句子，每句合成后立即播放。运行期间可用 `/voice [名称]`、`/speed [语速]` 和 `/voices` 查看或修改设置，`/help` 列出命令，`/quit` 退出：

```bash
./build/kokoro -v zf_002 -i
```

输出为 24 kHz 单声道 32 位浮点 WAV。成功时退出码为 0，库错误为 1，用法错误为 2。

## 库

`libkokoro` 在 [`include/kokoro/kokoro.h`](include/kokoro/kokoro.h) 中提供 C API，可在 C、C++ 以及任何支持 C FFI 的语言（Python `ctypes`、C#、Rust、Go 等）中使用：

```c
#include <kokoro/kokoro.h>

kokoro_ctx* ctx = NULL;
if (kokoro_create("models/kokoro-v1.1-zh.onnx", "models/voices-v1.1-zh.bin", "dict", &ctx) != KOKORO_OK) {
    fprintf(stderr, "%s\n", kokoro_last_error());
    return 1;
}
kokoro_audio audio;
if (kokoro_synthesize(ctx, "你好，世界。Hello world", "zf_002", 1.0f, 0, &audio) == KOKORO_OK) {
    /* audio.samples：单声道 float PCM，共 audio.num_samples 个采样，采样率 audio.sample_rate */
    kokoro_audio_free(&audio);
}
kokoro_destroy(ctx);
```

- 所有字符串（包括路径）均为 UTF-8。`dict_dir` 为存放 `vocab.txt` 和 G2P 词典的目录（即本仓库的 `dict/`）。
- 错误通过 `kokoro_status` 返回；`kokoro_last_error()` 返回当前线程的错误信息。
- `kokoro_voice_count` / `kokoro_voice_name` 枚举语音，`kokoro_phonemize` 返回音素串，`KOKORO_INPUT_PHONEMES` 可直接合成音素。
- 不同的 context 可在不同线程中使用；同一个 context 不能被并发调用。

安装并在 CMake 中使用：

```bash
cmake --install build --config Release --prefix /path/to/prefix
```

```cmake
find_package(kokoro REQUIRED)   # 设置 CMAKE_PREFIX_PATH=/path/to/prefix
target_link_libraries(app PRIVATE kokoro::kokoro)
```

也可以 `add_subdirectory(kokoro.cpp)` 后链接 `kokoro::kokoro`。

### .NET (NuGet)

`Larroy.Kokoro` 为 .NET 8 和 .NET 10 封装 C API（`dotnet/`）；其运行时包提供 win-x64 和 win-arm64 的 `kokoro.dll`，ONNX Runtime 来自 `Microsoft.ML.OnnxRuntime` 包，`Larroy.Kokoro.runtime.win-x64.cuda` 通过 `Microsoft.ML.OnnxRuntime.Gpu.Windows` 提供 CUDA 支持。用法见 [`packaging/README.md`](packaging/README.md)。在 Windows 上构建这些包（需要带 x64 和 ARM64 C++ 工具的 VS 2022、`nuget.exe` 和 .NET SDK）：

```powershell
./packaging/build-natives.ps1                   # 按 RID 构建 kokoro.dll -> artifacts/natives/<rid>/
dotnet test dotnet/Kokoro.Net.sln -c Release
./packaging/pack.ps1                            # -> artifacts/nuget/*.nupkg
./packaging/smoke.ps1 -Framework net8.0         # 基于本地源的端到端检查
```

交互模式另有一个 .NET 示例（`dotnet/examples/Kokoro.Interactive/`，由上述解决方案构建，不随 NuGet 包发布）：

```powershell
dotnet run --project dotnet/examples/Kokoro.Interactive -c Release -- --device cpu
```

## 英文 G2P

英文单词按以下顺序转换为音素（输出为 Kokoro/misaki 英文音素集）：

1. `dict/user_en.dict`：用户词典，CMU 格式（`单词 ARPAbet音素`，如 `onnx AA1 N IH0 K S`），优先级最高，用于专有名词。
2. `dict/cmudict-0.7b/cmudict.dict`：CMU 发音词典。
3. 不超过 5 个字母的全大写词（如 `GPU`）按英文字母名拼读。
4. 其他未登录词由 `dict/g2p_en.weights` 神经网络预测，移植自 [g2p_en](https://github.com/Kyubyong/g2p)（Apache-2.0，见 `dict/g2p_en.LICENSE.txt`）。

`g2p_en.weights` 由 g2p_en 的 `checkpoint20.npz` 生成（无需 numpy）：

```bash
python scripts/export_g2p.py checkpoint20.npz dict/g2p_en.weights
```

## 项目结构

- `include/kokoro/kokoro.h`, `src/kokoro_c.cpp`: `libkokoro` 的公共 C API。
- `src/cli/main.cpp`: `kokoro` 命令行工具。
- `src/Kokoro.cpp/h`: 主要的 TTS 类（内部实现）。
- `src/ZHFrontend.cpp/h`: 中文前端（G2P、变调）。
- `src/EnG2P.h`, `src/NeuralG2P.cpp/h`: 英文 G2P（词典查询与神经网络预测）。
- `src/SpanishG2P.cpp/h`: 基于规则的西班牙语 G2P（遵循 espeak-ng `es` 约定）。
- `src/german/`: 德语 G2P（MFA 词典、复合词拆分、g2p_de 神经网络回退；遵循 espeak-ng `de` 约定）。
- `eval_bench/`: 与 espeak-ng 对比的 G2P 评测（`g2p_dump`、`compare_g2p.py`、语料）。
- `tests/`: 库测试（使用 `ctest` 运行）；`tests/python/`：Python 工具的测试（使用 `uv run pytest` 运行）。
- `dotnet/`, `packaging/`: .NET 封装（`Larroy.Kokoro`）及构建其 NuGet 包的脚本。
- `ARCHITECTURE.md`: 各组件如何协同工作（英文）。
- `docs/`: 使用指南（[添加语音](docs/adding-voices.md)、[线程安全](docs/thread-safety.md)（英文））。
- `scripts/`: 数据处理辅助脚本。
- `dict/`: G2P 字典文件（Jieba、拼音、CMU、g2p_en 权重、德语 MFA 词典、g2p_de 权重）。

## 许可证

MIT

---

# Kokoro C++: inferencia

[English](#kokoro-c-inference) | [中文](#kokoro-c-推理) | Español | [Deutsch](#kokoro-c-inferenz)

Implementación ligera y de alto rendimiento en C++ de la inferencia del modelo TTS [Kokoro](https://huggingface.co/hexgrad/Kokoro-82M), basada en ONNX Runtime. El proyecto admite síntesis mixta en **chino e inglés**, en **español** con el modelo Kokoro-82M v1.0 y en **alemán** con un modelo afinado para alemán.

## Características

- 🚀 **Inferencia rápida**: impulsada por ONNX Runtime.
- 🌏 **Multilingüe**: soporte nativo de chino e inglés; español (castellano) con el modelo v1.0; alemán con un modelo afinado para alemán.

## Requisitos

- **CMake** (3.15+)
- **Ninja** (recomendado; `uv run bootstrap.py build` lo usa si está en el `PATH`)
- **Compilador de C++** (con soporte de C++17)
- **Rust** (1.80+, con [rustup](https://rustup.rs)): compila `kokoro-server`; configura con `-DKOKORO_BUILD_SERVER=OFF` para omitirlo
- **[uv](https://docs.astral.sh/uv/)**: ejecuta `bootstrap.py` y `voice_tool.py`, e instala sus dependencias de Python (Python 3.10+, click) en `.venv` la primera vez

## Preparación

```bash
uv run bootstrap.py configure
```

Descarga, verificando las sumas SHA-256:

- el [ONNX Runtime](https://github.com/microsoft/onnxruntime/releases/tag/v1.23.2) 1.23.2 precompilado (CPU; la build CUDA en GPUs NVIDIA compatibles, ver más abajo) para la plataforma actual (Linux x64/aarch64, macOS arm64/x86_64, Windows x64/arm64) en `third_party/onnxruntime/`;
- el modelo `kokoro-v1.1-zh.onnx` y el paquete de voces `voices-v1.1-zh.bin` del [release `voices_model_files`](https://github.com/larroy/kokoro.cpp/releases/tag/voices_model_files) de este repositorio en `models/`;
- el modelo Kokoro-82M v1.0 `kokoro-v1.0.onnx` de [kokoro-onnx](https://github.com/thewh1teagle/kokoro-onnx/releases/tag/model-files-v1.0), y las voces españolas `ef_dora`, `em_alex` y `em_santa` de [onnx-community/Kokoro-82M-v1.0-ONNX](https://huggingface.co/onnx-community/Kokoro-82M-v1.0-ONNX), empaquetadas en `models/voices-v1.0-es.bin`;
- el modelo afinado para alemán `kokoro-de.onnx` de [Godelaune/Kokoro-82M-ONNX-German-Martin](https://huggingface.co/Godelaune/Kokoro-82M-ONNX-German-Martin), y las voces alemanas `df_eva`, `df_victoria`, `dm_bernd` y `dm_martin` de [cstr/kokoro-voices-GGUF](https://huggingface.co/cstr/kokoro-voices-GGUF), empaquetadas en `models/voices-de.bin`.

Los archivos ya presentes y actualizados se omiten; `--force` los descarga de nuevo. La versión de ONNX Runtime y las sumas de verificación están fijadas en `bootstrap.py`. Para usar otra instalación de ONNX Runtime, omite `configure` y pasa `-DONNXRUNTIME_ROOT=/ruta/a/onnxruntime` a CMake.

## Compilación

```bash
uv run bootstrap.py build
```

`build` ejecuta la configuración y la compilación de CMake con el generador Ninja. Si `ninja` no está en el `PATH`, muestra un aviso y usa el generador por defecto de CMake; pasar `-G` después de `--` cambia la elección. Opciones: `--build-dir` (por defecto `build`), `--config` (`Debug`, `Release`, `RelWithDebInfo`, `MinSizeRel`; por defecto `Release`), `-j/--jobs`. Los argumentos después de `--` van al paso de configuración de CMake, p. ej. `uv run bootstrap.py build -- -DKOKORO_BUILD_TESTS=OFF`. Un directorio de compilación no puede cambiar de generador: bórralo antes si se configuró con otro. En Windows, Ninja necesita el entorno de MSVC (ejecútalo desde un Developer Command Prompt). Equivale a:

```bash
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

Genera la biblioteca compartida `kokoro` (`kokoro.dll` / `libkokoro.so` / `libkokoro.dylib`) y la herramienta de línea de comandos `kokoro`. `libkokoro` enlaza con la biblioteca compartida de ONNX Runtime. En Linux/macOS el árbol de compilación la encuentra mediante el rpath; las copias instaladas necesitan poder encontrarla en tiempo de ejecución. En Windows, `onnxruntime.dll` se copia junto a `kokoro.dll` (y se instala con ella).

### CUDA (experimental)

El soporte de CUDA es **experimental**. `uv run bootstrap.py configure` usa la build CUDA precompilada de ONNX Runtime al detectar una GPU NVIDIA compatible (Windows x64: sm 7.5, 8.6 y 8.9 más PTX 9.0; Linux x64: sm 6.0, 7.0, 7.5 y 8.0 más PTX 9.0) y un driver que soporte CUDA 12.8 o posterior; pasa `--ort cpu` o `--ort gpu` para forzar la elección. Para otras GPU, compila ONNX Runtime con CUDA desde el código fuente:

```bash
uv run bootstrap.py build-ort   # requiere CMake 3.28+, git, un toolkit CUDA 12 y cuDNN 9 para CUDA 12
```

`build-ort` localiza el toolkit CUDA con las variables de entorno `CUDA_HOME` o `CUDA_PATH` (el instalador de CUDA define `CUDA_PATH`, p. ej. `C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.9` en Windows), con `nvcc` en el `PATH`, o en las ubicaciones estándar (`C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.*`, `/usr/local/cuda`); `--cuda-home` lo indica explícitamente. cuDNN se localiza con `CUDNN_HOME` o `CUDNN_PATH` (`--cudnn-home`; usa el archivo de https://developer.download.nvidia.com/compute/cudnn/redist/cudnn/, no el del instalador). El resultado sustituye al paquete precompilado en `third_party/onnxruntime/`.

En ejecución, las bibliotecas de CUDA 12 y cuDNN 9 deben poder cargarse: en el `PATH` en Windows (`cudart64_12.dll`, `cublas64_12.dll`, `cublasLt64_12.dll`, `curand64_10.dll`, `cufft64_11.dll`, `cudnn64_9.dll`), en `LD_LIBRARY_PATH` en Linux (`libcudart.so.12`, `libcublas.so.12`, `libcublasLt.so.12`, `libcurand.so.10`, `libcufft.so.11`, `libcudnn.so.9`); si faltan, kokoro imprime un aviso y usa la CPU. Elige el dispositivo con `--device auto|cpu|cuda` y `--gpu-id`; con `auto`, un fallo de inicialización de CUDA hace que se use la CPU; con `cuda`, es un error.

### Pruebas

```bash
ctest --test-dir build -C Release --output-on-failure
```

Las pruebas están en `tests/` (doctest, incluido en `third_party/doctest`): `c_api` comprueba la API de C sin modelo, `g2p` fija la salida del G2P chino/inglés/español, `synthesis` ejecuta el modelo real de `models/` y `synthesis_es` las voces españolas con el modelo v1.0 (se omiten si falta el modelo o el archivo de voces). Configura con `-DKOKORO_BUILD_TESTS=OFF` para no compilarlas.

Las herramientas de Python tienen sus propias pruebas en `tests/python/`: ejecuta `uv run pytest`. Las pruebas de importación `.pt` se omiten si PyTorch no está instalado; `uv run --group voice pytest` también las ejecuta.

## Uso / Demo

Ejecuta la herramienta `kokoro` desde la raíz del proyecto (las rutas por defecto apuntan a `models/` y `dict/`). Con Visual Studio u otro generador multiconfiguración, el ejecutable está en `build/Release/`.

```bash
./build/kokoro [opciones] <texto>
./build/kokoro --list-voices
```

| Opción | Significado |
|---|---|
| `-m, --model <ruta>` | archivo del modelo ONNX (por defecto: `models/kokoro-v1.1-zh.onnx`) |
| `--voices <ruta>` | archivo de voces (por defecto: `models/voices-v1.1-zh.bin`) |
| `-d, --dict <dir>` | directorio de diccionarios (por defecto: `dict`) |
| `-v, --voice <nombre>` | voz (por defecto: `af_maple`); voces inglesas: `af_maple`, `af_sol`, `bf_vale`; chinas: `zf_*`, `zm_*`; españolas (modelo v1.0): `ef_dora`, `em_alex`, `em_santa`; alemanas (modelo `kokoro-de.onnx`): `df_eva`, `df_victoria`, `dm_bernd`, `dm_martin` |
| `-s, --speed <factor>` | velocidad del habla, > 0 (por defecto: `1.0`) |
| `--lang <auto\|en\|zh\|es\|de>` | idioma para leer los números (por defecto: `auto`) |
| `--language <auto\|zh\|en\|es\|de>` | idioma del texto; `auto`: español para las voces `ef_*`/`em_*`, alemán para `df_*`/`dm_*`, inglés para `af_`/`am_`/`bf_`/`bm_*`, si no chino/inglés (por defecto: `auto`) |
| `-o, --output <ruta>` | archivo WAV de salida (por defecto: `output.wav`) |
| `-p, --phonemes` | `<texto>` es una cadena de fonemas; omite el G2P |
| `--phonemize` | imprime los fonemas de `<texto>` en lugar de sintetizar |
| `-i, --interactive` | lee frases de la entrada estándar y reproduce cada una en cuanto se sintetiza (no usa `-o`) |
| `--list-voices` | imprime las voces disponibles |

Para añadir voces propias (mezclas, tensores `.pt` importados), consulta [docs/adding-voices.md](docs/adding-voices.md) (en inglés).

### Ejemplos

Español (castellano, fonemas en la convención `es` de espeak-ng; las voces `ef_*`/`em_*` activan el G2P español automáticamente):

```bash
./build/kokoro -m models/kokoro-v1.0.onnx --voices models/voices-v1.0-es.bin --list-voices
./build/kokoro -m models/kokoro-v1.0.onnx --voices models/voices-v1.0-es.bin -v ef_dora -o hola.wav \
    "Hola, ¿cómo estás? Tengo 25 años."
./build/kokoro -m models/kokoro-v1.0.onnx --voices models/voices-v1.0-es.bin -v em_alex -s 0.9 -o noticia.wav \
    "El agua del río está fría."
./build/kokoro -m models/kokoro-v1.0.onnx --voices models/voices-v1.0-es.bin --language es --phonemize "Tengo 25 años."
# tˈɛŋɡo βAntiθˈinko ˈaɲos.
```

Alemán (las voces `df_*`/`dm_*` activan el G2P alemán automáticamente):

```bash
./build/kokoro -m models/kokoro-de.onnx --voices models/voices-de.bin -v dm_martin -o hallo.wav \
    "Guten Tag, wie geht es dir?"
./build/kokoro -m models/kokoro-de.onnx --voices models/voices-de.bin --language de --phonemize "Ich habe 1.000 Euro."
# ɪç hɑːbə ˈIntWzənt ˈɔørɔ.
```

Chino e inglés:

```bash
./build/kokoro -o hello.wav "Hello world"
./build/kokoro --voice zf_002 "你好啊，这是一个测试。Hello world"
```

### Modo interactivo

`-i` lee frases desde el terminal y reproduce cada una en cuanto se sintetiza. Mientras se ejecuta, `/voice [nombre]`,
`/speed [factor]` y `/voices` muestran o cambian los ajustes, `/help` lista las órdenes y `/quit` sale:

```bash
./build/kokoro -m models/kokoro-v1.0.onnx --voices models/voices-v1.0-es.bin -v ef_dora -i
```

La salida es un WAV mono de 32 bits en coma flotante a 24 kHz. El código de salida es 0 si todo va bien, 1 ante un error de la biblioteca y 2 ante un error de uso.

## Biblioteca

`libkokoro` expone una API de C en [`include/kokoro/kokoro.h`](include/kokoro/kokoro.h), utilizable desde C, C++ o cualquier lenguaje con FFI de C (Python `ctypes`, C#, Rust, Go, ...):

```c
#include <kokoro/kokoro.h>

kokoro_ctx* ctx = NULL;
if (kokoro_create("models/kokoro-v1.0.onnx", "models/voices-v1.0-es.bin", "dict", &ctx) != KOKORO_OK) {
    fprintf(stderr, "%s\n", kokoro_last_error());
    return 1;
}
kokoro_audio audio;
if (kokoro_synthesize(ctx, "Hola, ¿cómo estás?", "ef_dora", 1.0f, 0, &audio) == KOKORO_OK) {
    /* audio.samples: PCM float mono, audio.num_samples muestras a audio.sample_rate Hz */
    kokoro_audio_free(&audio);
}
kokoro_destroy(ctx);
```

- Todas las cadenas, incluidas las rutas, son UTF-8. `dict_dir` es el directorio con `vocab.txt` y los diccionarios del G2P (`dict/` en este repositorio).
- Los errores se devuelven como códigos `kokoro_status`; `kokoro_last_error()` devuelve el mensaje del hilo que llama.
- `kokoro_voice_count` / `kokoro_voice_name` enumeran las voces, `kokoro_phonemize` devuelve la cadena de fonemas y `KOKORO_INPUT_PHONEMES` sintetiza fonemas directamente.
- `kokoro_set_language(ctx, KOKORO_LANGUAGE_SPANISH)` fuerza el G2P español (y `KOKORO_LANGUAGE_GERMAN` el alemán; también para `kokoro_phonemize`, que no recibe voz); `kokoro_set_number_language` elige cómo se leen los números.
- Se pueden usar contextos distintos desde hilos distintos; un mismo contexto no debe usarse de forma concurrente.

Instálala y úsala desde CMake:

```bash
cmake --install build --config Release --prefix /ruta/al/prefijo
```

```cmake
find_package(kokoro REQUIRED)   # con CMAKE_PREFIX_PATH=/ruta/al/prefijo
target_link_libraries(app PRIVATE kokoro::kokoro)
```

Como alternativa, `add_subdirectory(kokoro.cpp)` y enlaza `kokoro::kokoro`.

### .NET (NuGet)

`Larroy.Kokoro` envuelve la API de C para .NET 8 y .NET 10 (`dotnet/`); sus paquetes de runtime aportan `kokoro.dll` para win-x64 y win-arm64 y toman ONNX Runtime del paquete `Microsoft.ML.OnnxRuntime`, y `Larroy.Kokoro.runtime.win-x64.cuda` añade CUDA mediante `Microsoft.ML.OnnxRuntime.Gpu.Windows`. El uso está en [`packaging/README.md`](packaging/README.md). Para generar los paquetes en Windows (requiere VS 2022 con las herramientas de C++ x64 y ARM64, `nuget.exe` y el SDK de .NET):

```powershell
./packaging/build-natives.ps1                   # kokoro.dll por RID -> artifacts/natives/<rid>/
dotnet test dotnet/Kokoro.Net.sln -c Release
./packaging/pack.ps1                            # -> artifacts/nuget/*.nupkg
./packaging/smoke.ps1 -Framework net8.0         # prueba de extremo a extremo con el feed local
```

El modo interactivo también existe como ejemplo de .NET (`dotnet/examples/Kokoro.Interactive/`, compilado por la
solución anterior, no se incluye en los paquetes NuGet):

```powershell
dotnet run --project dotnet/examples/Kokoro.Interactive -c Release -- --device cpu
```

## G2P español

`src/SpanishG2P.cpp` convierte el texto en fonemas con reglas (sin diccionario ni espeak-ng, que es GPL-3.0), siguiendo la convención de espeak-ng `es` con la que se entrenaron las voces: ortografía → fonemas (`θ` para `c`/`z`, `x` para `j`, `ʎ` para `ll`, `ʝ` para `y` ante vocal), diptongos y acento según las reglas ortográficas, y alófonos `β ð ɣ` y asimilación nasal entre palabras. Los números se leen en español (`25` → `veinticinco`).

Las siglas en mayúsculas se leen como palabra si se pueden pronunciar así en español (`OTAN` → `ˈotan`, `ONU`, `NASA`, y también palabras en mayúsculas como `ÁLBUM` o `ROBOT`); si no, se deletrean con los nombres de las letras, con el acento en la última (`CPP` → `θepepˈe`, `GPU` → `xepeˈu`, `DVD` → `deuβeðˈe`). Se considera pronunciable una palabra que empieza por vocal, una consonante o un grupo válido (`pl`, `tr`, `ps`, …) y termina como mucho en una consonante. En inglés, en cambio, las palabras en mayúsculas de hasta 5 letras siempre se deletrean.

Se evalúa contra espeak-ng con un corpus (`eval_bench/corpus/es.txt`):

```bash
uv run --group eval python eval_bench/compare_g2p.py
```

Requiere `g2p_dump` compilado (`uv run bootstrap.py build`). Termina con código 1 si hay diferencias que no figuran en `eval_bench/corpus/es_known_diffs.tsv`.

## G2P inglés

Las palabras inglesas se convierten en fonemas en este orden (la salida usa el conjunto de fonemas ingleses de Kokoro/misaki):

1. `dict/user_en.dict`: diccionario de usuario en formato CMU (`PALABRA fonemas-ARPAbet`, p. ej. `onnx AA1 N IH0 K S`). Máxima prioridad; pensado para nombres propios.
2. `dict/cmudict-0.7b/cmudict.dict`: el CMU Pronouncing Dictionary.
3. Las palabras en mayúsculas de hasta 5 letras (p. ej. `GPU`) se deletrean con los nombres ingleses de las letras.
4. El resto de palabras desconocidas las predice la red neuronal `dict/g2p_en.weights`, portada de [g2p_en](https://github.com/Kyubyong/g2p) (Apache-2.0, ver `dict/g2p_en.LICENSE.txt`).

`g2p_en.weights` se genera a partir de `checkpoint20.npz` de g2p_en (no requiere numpy):

```bash
python scripts/export_g2p.py checkpoint20.npz dict/g2p_en.weights
```

## Estructura del proyecto

- `include/kokoro/kokoro.h`, `src/kokoro_c.cpp`: API pública de C de `libkokoro`.
- `src/cli/main.cpp`: la herramienta de línea de comandos `kokoro`.
- `src/Kokoro.cpp/h`: clase principal de TTS (interna).
- `src/ZHFrontend.cpp/h`: frontend chino (G2P, sandhi tonal).
- `src/EnG2P.h`, `src/NeuralG2P.cpp/h`: G2P inglés (consulta de diccionario y predicción neuronal).
- `src/SpanishG2P.cpp/h`: G2P español basado en reglas (convención `es` de espeak-ng).
- `src/german/`: G2P alemán (diccionario MFA, separación de compuestos, red neuronal g2p_de como respaldo; convención `de` de espeak-ng).
- `eval_bench/`: evaluación del G2P frente a espeak-ng (`g2p_dump`, `compare_g2p.py`, corpus).
- `tests/`: pruebas de la biblioteca (con `ctest`); `tests/python/`: pruebas de las herramientas de Python (con `uv run pytest`).
- `dotnet/`, `packaging/`: wrapper de .NET (`Larroy.Kokoro`) y los scripts que generan sus paquetes NuGet.
- `ARCHITECTURE.md`: cómo encajan los componentes (en inglés).
- `docs/`: guías ([añadir voces](docs/adding-voices.md), [seguridad entre hilos](docs/thread-safety.md) (en inglés)).
- `scripts/`: scripts auxiliares de procesamiento de datos.
- `dict/`: diccionarios del G2P (Jieba, pinyin, CMU, pesos de g2p_en, diccionario MFA alemán, pesos de g2p_de).

## Licencia

MIT

---

# Kokoro C++: Inferenz

[English](#kokoro-c-inference) | [中文](#kokoro-c-推理) | [Español](#kokoro-c-inferencia) | Deutsch

Eine schnelle, schlanke C++-Implementierung der Inferenz des TTS-Modells [Kokoro](https://huggingface.co/hexgrad/Kokoro-82M) auf Basis von ONNX Runtime. Das Projekt unterstützt gemischte Synthese in **Chinesisch und Englisch**, **Spanisch** mit dem Modell Kokoro-82M v1.0 und **Deutsch** mit einem auf Deutsch feinabgestimmten Modell.

## Funktionen

- 🚀 **Schnelle Inferenz**: basiert auf ONNX Runtime.
- 🌏 **Mehrsprachig**: native Unterstützung für Chinesisch und Englisch; Spanisch (Kastilisch) mit dem v1.0-Modell; Deutsch mit einem deutschen Fine-Tune.

## Voraussetzungen

- **CMake** (3.15+)
- **Ninja** (empfohlen; `uv run bootstrap.py build` verwendet es, wenn es im `PATH` liegt)
- **C++-Compiler** (mit C++17-Unterstützung)
- **Rust** (1.80+, über [rustup](https://rustup.rs)): baut `kokoro-server`; mit `-DKOKORO_BUILD_SERVER=OFF` konfigurieren, um es auszulassen
- **[uv](https://docs.astral.sh/uv/)**: führt `bootstrap.py` und `voice_tool.py` aus und installiert beim ersten Aufruf ihre Python-Abhängigkeiten (Python 3.10+, click) in `.venv`

## Einrichtung

```bash
uv run bootstrap.py configure
```

Lädt herunter und prüft dabei die SHA-256-Prüfsummen:

- das vorkompilierte [ONNX Runtime](https://github.com/microsoft/onnxruntime/releases/tag/v1.23.2) 1.23.2 (CPU; auf unterstützten NVIDIA-GPUs die CUDA-Variante, siehe unten) für die aktuelle Plattform (Linux x64/aarch64, macOS arm64/x86_64, Windows x64/arm64) nach `third_party/onnxruntime/`;
- das Modell `kokoro-v1.1-zh.onnx` und das Stimmenpaket `voices-v1.1-zh.bin` aus dem [Release `voices_model_files`](https://github.com/larroy/kokoro.cpp/releases/tag/voices_model_files) dieses Repositorys nach `models/`;
- das Modell Kokoro-82M v1.0 `kokoro-v1.0.onnx` von [kokoro-onnx](https://github.com/thewh1teagle/kokoro-onnx/releases/tag/model-files-v1.0) und die spanischen Stimmen `ef_dora`, `em_alex` und `em_santa` von [onnx-community/Kokoro-82M-v1.0-ONNX](https://huggingface.co/onnx-community/Kokoro-82M-v1.0-ONNX), gepackt in `models/voices-v1.0-es.bin`;
- das deutsche Fine-Tune `kokoro-de.onnx` von [Godelaune/Kokoro-82M-ONNX-German-Martin](https://huggingface.co/Godelaune/Kokoro-82M-ONNX-German-Martin) und die deutschen Stimmen `df_eva`, `df_victoria`, `dm_bernd` und `dm_martin` von [cstr/kokoro-voices-GGUF](https://huggingface.co/cstr/kokoro-voices-GGUF), gepackt in `models/voices-de.bin`.

Bereits vorhandene, aktuelle Dateien werden übersprungen; `--force` lädt sie erneut herunter. Die ONNX-Runtime-Version und die Prüfsummen sind in `bootstrap.py` festgelegt. Um eine andere ONNX-Runtime-Installation zu verwenden, `configure` auslassen und CMake `-DONNXRUNTIME_ROOT=/pfad/zu/onnxruntime` übergeben.

## Kompilieren

```bash
uv run bootstrap.py build
```

`build` führt die CMake-Schritte Konfigurieren und Bauen mit dem Ninja-Generator aus. Liegt `ninja` nicht im `PATH`, gibt es eine Warnung aus und fällt auf den Standardgenerator von CMake zurück; ein `-G` nach `--` legt den Generator selbst fest. Optionen: `--build-dir` (Standard `build`), `--config` (`Debug`, `Release`, `RelWithDebInfo`, `MinSizeRel`; Standard `Release`), `-j/--jobs`. Argumente nach `--` gehen an den CMake-Konfigurationsschritt, z. B. `uv run bootstrap.py build -- -DKOKORO_BUILD_TESTS=OFF`. Ein Build-Verzeichnis kann den Generator nicht wechseln: Wurde es mit einem anderen konfiguriert, zuerst löschen. Unter Windows braucht Ninja die MSVC-Umgebung (aus einer Developer Command Prompt starten). Entspricht:

```bash
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

Erzeugt die gemeinsame Bibliothek `kokoro` (`kokoro.dll` / `libkokoro.so` / `libkokoro.dylib`) und das Kommandozeilenwerkzeug `kokoro`. `libkokoro` linkt gegen die gemeinsame Bibliothek von ONNX Runtime. Unter Linux/macOS findet der Build-Baum sie über den rpath; installierte Kopien müssen sie zur Laufzeit finden können. Unter Windows wird `onnxruntime.dll` neben `kokoro.dll` kopiert (und mit ihr installiert).

### CUDA (experimentell)

Die CUDA-Unterstützung ist experimentell. `uv run bootstrap.py configure` verwendet die vorkompilierte CUDA-Variante von ONNX Runtime, wenn es eine unterstützte NVIDIA-GPU erkennt (Windows x64: sm 7.5, 8.6, 8.9 plus PTX 9.0; Linux x64: sm 6.0, 7.0, 7.5, 8.0 plus PTX 9.0) und der NVIDIA-Treiber CUDA 12.8 oder neuer unterstützt; `--ort cpu` oder `--ort gpu` erzwingt ein Paket. Für andere GPUs ONNX Runtime mit CUDA aus dem Quellcode bauen:

```bash
uv run bootstrap.py build-ort   # braucht CMake 3.28+, git, ein CUDA-12-Toolkit und cuDNN 9 für CUDA 12
```

`build-ort` findet das CUDA-Toolkit über die Umgebungsvariablen `CUDA_HOME` oder `CUDA_PATH` (der CUDA-Installer setzt `CUDA_PATH`, z. B. `C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.9` unter Windows), über `nvcc` im `PATH` oder an den Standardorten (`C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.*`, `/usr/local/cuda`); `--cuda-home` gibt ein Toolkit ausdrücklich an. cuDNN wird über `CUDNN_HOME` oder `CUDNN_PATH` gefunden (`--cudnn-home`; das Archiv von https://developer.download.nvidia.com/compute/cudnn/redist/cudnn/ verwenden, nicht das Installer-Layout). Das Ergebnis ersetzt das vorkompilierte Paket in `third_party/onnxruntime/`.

Zur Laufzeit müssen die Bibliotheken von CUDA 12 und cuDNN 9 ladbar sein: unter Windows im `PATH` (`cudart64_12.dll`, `cublas64_12.dll`, `cublasLt64_12.dll`, `curand64_10.dll`, `cufft64_11.dll`, `cudnn64_9.dll`), unter Linux im `LD_LIBRARY_PATH` (`libcudart.so.12`, `libcublas.so.12`, `libcublasLt.so.12`, `libcurand.so.10`, `libcufft.so.11`, `libcudnn.so.9`); fehlen sie, gibt kokoro eine Warnung aus und rechnet auf der CPU. Das Inferenzgerät wählen `--device auto|cpu|cuda` und `--gpu-id`; mit `auto` fällt ein fehlgeschlagener CUDA-Start auf die CPU zurück, mit `cuda` ist er ein Fehler.

### Tests

```bash
ctest --test-dir build -C Release --output-on-failure
```

Die Tests liegen in `tests/` (doctest, mitgeliefert in `third_party/doctest`): `c_api` prüft die C-API ohne Modell, `g2p` legt die G2P-Ausgabe fest, und `synthesis`, `synthesis_es` und `synthesis_de` führen die echten Modelle aus `models/` aus (übersprungen, wenn Modell oder Stimmendatei fehlen). Mit `-DKOKORO_BUILD_TESTS=OFF` konfigurieren, um sie nicht zu bauen.

Die Python-Werkzeuge haben eigene Tests in `tests/python/`: `uv run pytest` ausführen. Die `.pt`-Importtests werden übersprungen, solange PyTorch nicht installiert ist; `uv run --group voice pytest` führt sie mit aus.

## Verwendung / Demo

Das Kommandozeilenwerkzeug `kokoro` im Projektstamm ausführen (die Standardpfade zeigen auf `models/` und `dict/`). Mit Visual Studio oder einem anderen Multi-Config-Generator liegt die ausführbare Datei in `build/Release/`.

```bash
./build/kokoro [Optionen] <Text>
./build/kokoro --list-voices
```

| Option | Bedeutung |
|---|---|
| `-m, --model <Pfad>` | ONNX-Modelldatei (Standard: `models/kokoro-v1.1-zh.onnx`) |
| `--voices <Pfad>` | Stimmendatei (Standard: `models/voices-v1.1-zh.bin`) |
| `-d, --dict <Verz.>` | Wörterbuchverzeichnis (Standard: `dict`) |
| `-v, --voice <Name>` | Stimme (Standard: `af_maple`); englisch: `af_maple`, `af_sol`, `bf_vale`; chinesisch: `zf_*`, `zm_*`; spanisch (Modell v1.0): `ef_dora`, `em_alex`, `em_santa`; deutsch (Modell `kokoro-de.onnx`): `df_eva`, `df_victoria`, `dm_bernd`, `dm_martin` |
| `-s, --speed <Faktor>` | Sprechgeschwindigkeit, > 0 (Standard: `1.0`) |
| `--lang <auto\|en\|zh\|es\|de>` | Sprache, in der Zahlen gelesen werden (Standard: `auto`) |
| `--language <auto\|zh\|en\|es\|de>` | Sprache des Textes; `auto`: Spanisch für `ef_*`/`em_*`, Deutsch für `df_*`/`dm_*`, Englisch für `af_`/`am_`/`bf_`/`bm_*`, sonst Chinesisch/Englisch (Standard: `auto`) |
| `-o, --output <Pfad>` | WAV-Ausgabedatei (Standard: `output.wav`) |
| `-p, --phonemes` | `<Text>` ist eine Phonemfolge; G2P wird übersprungen |
| `--phonemize` | gibt die Phoneme von `<Text>` aus, statt zu synthetisieren |
| `-i, --interactive` | liest Sätze von der Standardeingabe und spielt jeden ab, sobald er synthetisiert ist (`-o` wird nicht verwendet) |
| `--list-voices` | listet die verfügbaren Stimmen auf |

Eigene Stimmen hinzufügen (Mischungen, importierte `.pt`-Tensoren): siehe [docs/adding-voices.md](docs/adding-voices.md) (auf Englisch).

### Beispiele

Deutsch (Phoneme in der `de`-Konvention von espeak-ng; die Stimmen `df_*`/`dm_*` wählen den deutschen G2P automatisch):

```bash
./build/kokoro -m models/kokoro-de.onnx --voices models/voices-de.bin --list-voices
./build/kokoro -m models/kokoro-de.onnx --voices models/voices-de.bin -v dm_martin -o hallo.wav \
    "Guten Tag, wie geht es dir?"
./build/kokoro -m models/kokoro-de.onnx --voices models/voices-de.bin -v df_eva -s 0.9 -o nachricht.wav \
    "Der Zug hat leider zwanzig Minuten Verspätung."
./build/kokoro -m models/kokoro-de.onnx --voices models/voices-de.bin --language de --phonemize "Ich habe 1.000 Euro."
# ɪç hɑːbə ˈIntWzənt ˈɔørɔ.
```

Spanisch (die Stimmen `ef_*`/`em_*` wählen den spanischen G2P automatisch):

```bash
./build/kokoro -m models/kokoro-v1.0.onnx --voices models/voices-v1.0-es.bin -v ef_dora -o hola.wav \
    "Hola, ¿cómo estás? Tengo 25 años."
```

Chinesisch und Englisch:

```bash
./build/kokoro -o hello.wav "Hello world"
./build/kokoro --voice zf_002 "你好啊，这是一个测试。Hello world"
```

### Interaktiver Modus

`-i` liest Sätze vom Terminal und spielt jeden ab, sobald er synthetisiert ist. Während des Laufs zeigen oder ändern
`/voice [Name]`, `/speed [Faktor]` und `/voices` die Einstellungen, `/help` listet die Befehle auf und `/quit` beendet:

```bash
./build/kokoro -m models/kokoro-de.onnx --voices models/voices-de.bin -v df_victoria -i
```

Die Ausgabe ist eine Mono-WAV-Datei mit 32-Bit-Gleitkommawerten bei 24 kHz. Der Exit-Code ist 0 bei Erfolg, 1 bei einem Bibliotheksfehler und 2 bei einem Bedienungsfehler.

## Bibliothek

`libkokoro` stellt in [`include/kokoro/kokoro.h`](include/kokoro/kokoro.h) eine C-API bereit, nutzbar aus C, C++ oder jeder Sprache mit C-FFI (Python `ctypes`, C#, Rust, Go, ...):

```c
#include <kokoro/kokoro.h>

kokoro_ctx* ctx = NULL;
if (kokoro_create("models/kokoro-de.onnx", "models/voices-de.bin", "dict", &ctx) != KOKORO_OK) {
    fprintf(stderr, "%s\n", kokoro_last_error());
    return 1;
}
kokoro_audio audio;
if (kokoro_synthesize(ctx, "Guten Tag, wie geht es dir?", "dm_martin", 1.0f, 0, &audio) == KOKORO_OK) {
    /* audio.samples: Mono-PCM als float, audio.num_samples Samples bei audio.sample_rate Hz */
    kokoro_audio_free(&audio);
}
kokoro_destroy(ctx);
```

- Alle Zeichenketten, auch Pfade, sind UTF-8. `dict_dir` ist das Verzeichnis mit `vocab.txt` und den G2P-Wörterbüchern (`dict/` in diesem Repository).
- Fehler werden als `kokoro_status`-Codes gemeldet; `kokoro_last_error()` liefert die Meldung des aufrufenden Threads.
- `kokoro_voice_count` / `kokoro_voice_name` zählen die Stimmen auf, `kokoro_phonemize` liefert die Phonemfolge, und `KOKORO_INPUT_PHONEMES` synthetisiert Phoneme direkt.
- `kokoro_set_language(ctx, KOKORO_LANGUAGE_GERMAN)` erzwingt den deutschen G2P (auch für `kokoro_phonemize`, das keine Stimme erhält); `kokoro_set_number_language` legt fest, wie Zahlen gelesen werden.
- Verschiedene Kontexte dürfen aus verschiedenen Threads verwendet werden; ein einzelner Kontext darf nicht gleichzeitig verwendet werden.

Installieren und aus CMake verwenden:

```bash
cmake --install build --config Release --prefix /pfad/zum/prefix
```

```cmake
find_package(kokoro REQUIRED)   # mit CMAKE_PREFIX_PATH=/pfad/zum/prefix
target_link_libraries(app PRIVATE kokoro::kokoro)
```

Alternativ `add_subdirectory(kokoro.cpp)` und gegen `kokoro::kokoro` linken.

### .NET (NuGet)

`Larroy.Kokoro` kapselt die C-API für .NET 8 und .NET 10 (`dotnet/`); seine Runtime-Pakete bringen `kokoro.dll` für win-x64 und win-arm64 mit und beziehen ONNX Runtime aus dem Paket `Microsoft.ML.OnnxRuntime`, und `Larroy.Kokoro.runtime.win-x64.cuda` ergänzt CUDA über `Microsoft.ML.OnnxRuntime.Gpu.Windows`. Die Verwendung beschreibt [`packaging/README.md`](packaging/README.md). Zum Bauen der Pakete unter Windows (braucht VS 2022 mit den C++-Werkzeugen für x64 und ARM64, `nuget.exe` und das .NET SDK):

```powershell
./packaging/build-natives.ps1                   # kokoro.dll pro RID -> artifacts/natives/<rid>/
dotnet test dotnet/Kokoro.Net.sln -c Release
./packaging/pack.ps1                            # -> artifacts/nuget/*.nupkg
./packaging/smoke.ps1 -Framework net8.0         # End-to-End-Prüfung gegen den lokalen Feed
```

Den interaktiven Modus gibt es auch als .NET-Beispiel (`dotnet/examples/Kokoro.Interactive/`, von der obigen Solution
gebaut, nicht in den NuGet-Paketen enthalten):

```powershell
dotnet run --project dotnet/examples/Kokoro.Interactive -c Release -- --device cpu
```

## Deutscher G2P

`src/german/` wandelt deutsche Wörter in dieser Reihenfolge in Phoneme um:

1. `dict/german_mfa.dict`: das German MFA dictionary v3.0.0 (CC BY 4.0, über [g2p_de](https://github.com/gooofy/g2p_de)). Von mehreren Varianten eines Wortes gilt die längste, weil das Wörterbuch umgangssprachliche Verkürzungen zuerst aufführt.
2. Komposita werden in Wörterbuchwörter von mindestens drei Buchstaben zerlegt, auch mit Fugen-s (`Fahrradschlosskette` → `Fahrrad` + `Schloss` + `Kette`).
3. Alle übrigen Wörter sagt das neuronale Netz `dict/g2p_de.weights` voraus, portiert von [g2p_de](https://github.com/gooofy/g2p_de) (Apache-2.0, siehe `dict/g2p_de.LICENSE.txt`).

Die MFA-Phoneme werden in die `de`-Konvention von espeak-ng übertragen, mit der die deutschen Modelle trainiert wurden: Diphthonge `I`, `W`, `ɔø`, vokalisiertes r, Auslautverhärtung und eine Betonung nach Regeln (Stammsilbe; unbetonte Präfixe wie `be-`, `ge-`, `ver-`, `er-`; betonte Suffixe wie `-ieren`, `-ion`, `-ität`, `-ei`). Zahlen werden auf Deutsch gelesen (`21` → `einundzwanzig`), deutsche Schreibweisen wie `1.000` und `3,5` werden erkannt, und `z. B.` und `d. h.` werden ausgeschrieben.

Die Auswertung gegen espeak-ng läuft über einen Korpus (`eval_bench/corpus/de.txt`):

```bash
uv run --group eval python eval_bench/compare_g2p.py --language de
```

Braucht ein gebautes `g2p_dump` (`uv run bootstrap.py build`). Endet mit Code 1, wenn es Abweichungen gibt, die nicht in `eval_bench/corpus/de_known_diffs.tsv` stehen.

`g2p_de.weights` wird aus `checkpoint_de.npz` von g2p_de erzeugt (ohne numpy):

```bash
python scripts/export_g2p.py checkpoint_de.npz dict/g2p_de.weights
```

## Englischer G2P

Englische Wörter werden in dieser Reihenfolge in Phoneme umgewandelt (die Ausgabe verwendet den englischen Phonemsatz von Kokoro/misaki):

1. `dict/user_en.dict`: Benutzerwörterbuch im CMU-Format (`WORT ARPAbet-Phoneme`, z. B. `onnx AA1 N IH0 K S`). Höchste Priorität; gedacht für Eigennamen.
2. `dict/cmudict-0.7b/cmudict.dict`: das CMU Pronouncing Dictionary.
3. Wörter in Großbuchstaben mit höchstens 5 Buchstaben (z. B. `GPU`) werden mit den englischen Buchstabennamen buchstabiert.
4. Alle übrigen unbekannten Wörter sagt das neuronale Netz `dict/g2p_en.weights` voraus, portiert von [g2p_en](https://github.com/Kyubyong/g2p) (Apache-2.0, siehe `dict/g2p_en.LICENSE.txt`).

`g2p_en.weights` wird aus `checkpoint20.npz` von g2p_en erzeugt (ohne numpy):

```bash
python scripts/export_g2p.py checkpoint20.npz dict/g2p_en.weights
```

## Projektstruktur

- `include/kokoro/kokoro.h`, `src/kokoro_c.cpp`: öffentliche C-API von `libkokoro`.
- `src/cli/main.cpp`: das Kommandozeilenwerkzeug `kokoro`.
- `src/Kokoro.cpp/h`: zentrale TTS-Klasse (intern).
- `src/EnG2P.h`, `src/NeuralG2P.cpp/h`: englischer G2P (Wörterbuchsuche und neuronale Vorhersage).
- `src/SpanishG2P.cpp/h`: regelbasierter spanischer G2P (`es`-Konvention von espeak-ng).
- `src/german/`: deutscher G2P (MFA-Wörterbuch, Kompositazerlegung, neuronales g2p_de als Rückfall; `de`-Konvention von espeak-ng).
- `eval_bench/`: G2P-Auswertung gegen espeak-ng (`g2p_dump`, `compare_g2p.py`, Korpora).
- `tests/`: Bibliothekstests (mit `ctest`); `tests/python/`: Tests der Python-Werkzeuge (mit `uv run pytest`).
- `dotnet/`, `packaging/`: .NET-Wrapper (`Larroy.Kokoro`) und die Skripte, die seine NuGet-Pakete bauen.
- `ARCHITECTURE.md`: wie die Komponenten zusammenspielen (auf Englisch).
- `docs/`: Anleitungen ([Stimmen hinzufügen](docs/adding-voices.md), [Thread-Sicherheit](docs/thread-safety.md) (auf Englisch)).
- `scripts/`: Hilfsskripte zur Datenverarbeitung.
- `dict/`: G2P-Wörterbücher (Jieba, Pinyin, CMU, g2p_en-Gewichte, German MFA dictionary, g2p_de-Gewichte).

## Lizenz

MIT
