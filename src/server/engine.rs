//! Safe ownership of one kokoro context.
use crate::ffi::{
    self, kokoro_audio, kokoro_ctx, kokoro_device, kokoro_language, kokoro_number_language, kokoro_status,
};
use std::ffi::{c_char, CStr, CString};
use std::ptr;

#[derive(Clone, Debug)]
pub struct EngineConfig {
    pub model: String,
    pub voices: String,
    pub dict: String,
    pub device: kokoro_device,
    pub gpu_id: i32,
    pub number_language: kokoro_number_language,
    pub language: kokoro_language,
}

#[derive(Debug)]
pub struct SynthesisError {
    pub status: kokoro_status,
    pub message: String,
}

/// The library's version string, e.g. "0.1.0".
pub fn library_version() -> String {
    // SAFETY: kokoro_version returns a static NUL-terminated string.
    unsafe { owned(ffi::kokoro_version()) }
}

// SAFETY: text must be NULL or a valid NUL-terminated string.
unsafe fn owned(text: *const c_char) -> String {
    if text.is_null() {
        return String::new();
    }
    CStr::from_ptr(text).to_string_lossy().into_owned()
}

// kokoro_last_error() is thread-local: call this on the thread that made the failing call, before any other.
fn last_error() -> String {
    // SAFETY: kokoro_last_error returns a valid string for the calling thread.
    unsafe { owned(ffi::kokoro_last_error()) }
}

fn c_string(text: &str) -> Result<CString, String> {
    CString::new(text).map_err(|_| format!("'{}' contains a NUL byte", text.escape_debug()))
}

fn check(status: kokoro_status) -> Result<(), String> {
    if status == kokoro_status::OK {
        Ok(())
    } else {
        Err(last_error())
    }
}

/// Owns a `kokoro_ctx`. The C API forbids using one context from two threads at once; callers serialize access.
pub struct Engine {
    ctx: *mut kokoro_ctx,
    voices: Vec<String>, // sorted, as returned by kokoro_voice_name
}

// SAFETY: the context may move between threads; &mut self on synthesize keeps calls exclusive.
unsafe impl Send for Engine {}

impl Engine {
    /// Creates and configures the context; the error is the library's message.
    pub fn create(config: &EngineConfig) -> Result<Self, String> {
        let (model, voices, dict) = (c_string(&config.model)?, c_string(&config.voices)?, c_string(&config.dict)?);
        // SAFETY: kokoro_default_options has no preconditions.
        let mut options = unsafe { ffi::kokoro_default_options() };
        options.device = config.device;
        options.gpu_id = config.gpu_id;
        let mut ctx = ptr::null_mut();
        // SAFETY: all strings are NUL-terminated and live across the call; ctx is a valid out pointer.
        check(unsafe { ffi::kokoro_create_ex(model.as_ptr(), voices.as_ptr(), dict.as_ptr(), &options, &mut ctx) })?;
        let mut engine = Self { ctx, voices: Vec::new() };
        engine.configure(config)?;
        engine.voices = engine.list_voices();
        Ok(engine)
    }

    fn configure(&mut self, config: &EngineConfig) -> Result<(), String> {
        // SAFETY: self.ctx is a live context owned by self.
        check(unsafe { ffi::kokoro_set_number_language(self.ctx, config.number_language) })?;
        // SAFETY: as above.
        check(unsafe { ffi::kokoro_set_language(self.ctx, config.language) })
    }

    fn list_voices(&self) -> Vec<String> {
        // SAFETY: self.ctx is live; names are valid for the context's lifetime and copied here.
        let count = unsafe { ffi::kokoro_voice_count(self.ctx) };
        (0..count).map(|i| unsafe { owned(ffi::kokoro_voice_name(self.ctx, i)) }).collect()
    }

    pub fn voices(&self) -> &[String] {
        &self.voices
    }

    /// Where inference runs: "cpu", "cuda" or "auto".
    pub fn device_name(&self) -> &'static str {
        // SAFETY: self.ctx is live.
        match unsafe { ffi::kokoro_context_device(self.ctx) } {
            kokoro_device::CUDA => "cuda",
            kokoro_device::CPU => "cpu",
            _ => "auto",
        }
    }

    pub fn synthesize(&mut self, text: &str, voice: &str, speed: f32) -> Result<Audio, SynthesisError> {
        // C strings end at the first NUL, as with the C++ server's c_str().
        let c_prefix = |s: &str| CString::new(s.split('\0').next().unwrap_or_default()).expect("prefix has no NUL");
        let (text, voice) = (c_prefix(text), c_prefix(voice));
        let mut audio = Audio(kokoro_audio { samples: ptr::null_mut(), num_samples: 0, sample_rate: 0 });
        // SAFETY: self.ctx is live and exclusively borrowed; strings outlive the call; audio.0 is a valid out value.
        let status = unsafe { ffi::kokoro_synthesize(self.ctx, text.as_ptr(), voice.as_ptr(), speed, 0, &mut audio.0) };
        if status != kokoro_status::OK {
            return Err(SynthesisError { status, message: last_error() });
        }
        Ok(audio)
    }
}

impl Drop for Engine {
    fn drop(&mut self) {
        // SAFETY: self.ctx came from kokoro_create_ex and is destroyed once.
        unsafe { ffi::kokoro_destroy(self.ctx) }
    }
}

/// Mono float samples owned by the library.
pub struct Audio(kokoro_audio);

// SAFETY: the samples buffer is exclusively owned and freed by kokoro_audio_free, which is thread-agnostic.
unsafe impl Send for Audio {}

impl Audio {
    pub fn samples(&self) -> &[f32] {
        if self.0.samples.is_null() {
            return &[];
        }
        // SAFETY: the library returned num_samples floats at samples, valid until kokoro_audio_free.
        unsafe { std::slice::from_raw_parts(self.0.samples, self.0.num_samples) }
    }

    pub fn sample_rate_hz(&self) -> u32 {
        u32::try_from(self.0.sample_rate).unwrap_or(0)
    }

    pub fn duration_s(&self) -> f64 {
        match self.sample_rate_hz() {
            0 => 0.0,
            rate_hz => self.0.num_samples as f64 / f64::from(rate_hz),
        }
    }
}

impl Drop for Audio {
    fn drop(&mut self) {
        // SAFETY: the audio came from kokoro_synthesize (or is zeroed) and is freed once.
        unsafe { ffi::kokoro_audio_free(&mut self.0) }
    }
}
