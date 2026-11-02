# Third-party notices

kokoro.cpp is MIT licensed (see `LICENSE`). It builds on, or redistributes, the components below.
Each component remains under its own license; the full license texts are in the paths listed.

## Compiled into kokoro.dll

### cppjieba

- Source: https://github.com/yanyiwu/cppjieba
- License: MIT
- License text: `third_party/cppjieba/LICENSE`
- The Chinese segmentation dictionaries in `dict/` (`jieba.dict.utf8`, `hmm_model.utf8`, `idf.utf8`,
  `stop_words.utf8`, `user.dict.utf8`, `pos_dict/`) come from cppjieba and are under the same license.

### limonp

- Source: https://github.com/yanyiwu/limonp
- License: MIT
- License text: `third_party/cppjieba/deps/limonp/LICENSE`

## Compiled into kokoro-server

kokoro-server statically links Rust crates from crates.io; the exact versions are pinned in `src/server/Cargo.lock`
and each crate's license text ships in its source package (`cargo tree -e normal -f "{p} {l}"` lists them).

- MIT: axum, axum-core, bytes, http-body, http-body-util, hyper, hyper-util, mio, slab, tokio, tower, tower-layer,
  tower-service, zmij
- MIT OR Apache-2.0: atomic-waker, futures-core, futures-task, futures-util, http, httparse, httpdate, itoa, mime,
  percent-encoding, pin-project-lite, serde_core, serde_json, smallvec, socket2, windows-link, windows-sys (Windows)
- MIT AND BSD-3-Clause: matchit
- Unlicense OR MIT: memchr
- Apache-2.0: sync_wrapper
- Build-time only (proc macros): proc-macro2, quote, syn (MIT OR Apache-2.0), tokio-macros (MIT), unicode-ident
  ((MIT OR Apache-2.0) AND Unicode-3.0)

## Redistributed data (`dict/`, shipped as `kokoro-dict/` in the Larroy.Kokoro NuGet package)

### CMU Pronouncing Dictionary

- Source: http://www.speech.cs.cmu.edu/cgi-bin/cmudict
- License: BSD-style (Carnegie Mellon University)
- License text: `dict/cmudict-0.7b/LICENSE`

### g2p_en model weights

- Source: https://github.com/Kyubyong/g2p
- License: Apache License 2.0
- License text: `dict/g2p_en.LICENSE.txt`
- File: `dict/g2p_en.weights`

### g2p_de model weights

- Source: https://github.com/gooofy/g2p_de
- License: Apache License 2.0
- License text: `dict/g2p_de.LICENSE.txt`
- File: `dict/g2p_de.weights`

### German MFA dictionary v3.0.0

- Source: https://mfa-models.readthedocs.io/en/latest/dictionary/German/German%20MFA%20dictionary%20v3_0_0.html
  (McAuliffe & Sonderegger 2024), redistributed via https://github.com/gooofy/g2p_de
- License: Creative Commons Attribution 4.0 (CC BY 4.0)
- File: `dict/german_mfa.dict`

### pinyin-data

- Source: https://github.com/mozillazg/pinyin-data
- License: MIT
- Files: `dict/pinyin.txt`, `dict/pinyin_phrase.txt`

## Dependencies that are not redistributed

### ONNX Runtime

- Source: https://github.com/microsoft/onnxruntime
- License: MIT
- The NuGet packages depend on `Microsoft.ML.OnnxRuntime` (or `Microsoft.ML.OnnxRuntime.Gpu.Windows`)
  and do not ship their own copy of `onnxruntime.dll`.

### Kokoro-82M model weights and voices

- Source: https://huggingface.co/hexgrad/Kokoro-82M and https://huggingface.co/hexgrad/Kokoro-82M-v1.1-zh
- License: Apache License 2.0
- German model: https://huggingface.co/Godelaune/kikiri-tts (ONNX export
  https://huggingface.co/Godelaune/Kokoro-82M-ONNX-German-Martin), Apache License 2.0
- German voices: https://huggingface.co/cstr/kokoro-voices-GGUF, Apache License 2.0
- The model (`*.onnx`) and voice (`voices-*.bin`) files are not packaged; users download them separately.
