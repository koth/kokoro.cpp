//! Raw declarations mirroring `include/kokoro/kokoro.h`. C enums are `c_int` newtypes so an unexpected value from
//! the library is just another number, not undefined behavior.
#![allow(non_camel_case_types)]

use std::ffi::{c_char, c_float, c_int, c_uint};

macro_rules! c_enum {
    ($name:ident { $($variant:ident = $value:expr),* $(,)? }) => {
        #[repr(transparent)]
        #[derive(Clone, Copy, Debug, PartialEq, Eq)]
        pub struct $name(pub c_int);
        impl $name {
            $(pub const $variant: Self = Self($value);)*
        }
    };
}

c_enum!(kokoro_status {
    OK = 0,
    ERROR_INVALID_ARGUMENT = 1,
    ERROR_LOAD = 2,
    ERROR_VOICE_NOT_FOUND = 3,
    ERROR_INFERENCE = 4,
    ERROR_OUT_OF_MEMORY = 5,
    ERROR_UNKNOWN = 6,
});

c_enum!(kokoro_number_language { AUTO = 0, ENGLISH = 1, CHINESE = 2, SPANISH = 3, GERMAN = 4 });

c_enum!(kokoro_language { AUTO = 0, SPANISH = 1, CHINESE_ENGLISH = 2, ENGLISH = 3, GERMAN = 4 });

c_enum!(kokoro_device { AUTO = 0, CPU = 1, CUDA = 2 });

#[repr(C)]
pub struct kokoro_ctx {
    _private: [u8; 0],
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct kokoro_options {
    pub device: kokoro_device,
    pub gpu_id: c_int,
}

#[repr(C)]
pub struct kokoro_audio {
    pub samples: *mut c_float,
    pub num_samples: usize,
    pub sample_rate: c_int,
}

extern "C" {
    pub fn kokoro_version() -> *const c_char;
    pub fn kokoro_last_error() -> *const c_char;
    pub fn kokoro_create_ex(
        model_path: *const c_char,
        voices_path: *const c_char,
        dict_dir: *const c_char,
        options: *const kokoro_options,
        out_ctx: *mut *mut kokoro_ctx,
    ) -> kokoro_status;
    pub fn kokoro_context_device(ctx: *const kokoro_ctx) -> kokoro_device;
    pub fn kokoro_default_options() -> kokoro_options;
    pub fn kokoro_destroy(ctx: *mut kokoro_ctx);
    pub fn kokoro_voice_count(ctx: *const kokoro_ctx) -> usize;
    pub fn kokoro_voice_name(ctx: *const kokoro_ctx, index: usize) -> *const c_char;
    pub fn kokoro_synthesize(
        ctx: *mut kokoro_ctx,
        text: *const c_char,
        voice: *const c_char,
        speed: c_float,
        flags: c_uint,
        out_audio: *mut kokoro_audio,
    ) -> kokoro_status;
    pub fn kokoro_set_number_language(ctx: *mut kokoro_ctx, language: kokoro_number_language) -> kokoro_status;
    pub fn kokoro_set_language(ctx: *mut kokoro_ctx, language: kokoro_language) -> kokoro_status;
    pub fn kokoro_audio_free(audio: *mut kokoro_audio);
}
