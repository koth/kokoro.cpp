//! HTTP routes: POST /v1/audio/speech, GET /v1/audio/voices, GET /health.
use crate::audio_encoding::{encode_pcm16, encode_wav_pcm16, WavTooLong};
use crate::engine::{Audio, Engine, SynthesisError};
use crate::ffi::kokoro_status;
use crate::speech_request::{parse_speech_request, ResponseFormat, SpeechRequest};
use axum::body::Bytes;
use axum::extract::{DefaultBodyLimit, State};
use axum::http::{header, StatusCode};
use axum::response::{IntoResponse, Response};
use axum::routing::{get, post};
use axum::Router;
use serde_json::json;
use std::sync::Arc;
use std::time::Instant;
use tokio::sync::Mutex;

const MAX_BODY_BYTES: usize = 1 << 20;

/// What the handlers share. Synthesis is serialized by the mutex; waiting requests hold no thread.
#[derive(Clone)]
pub struct AppState {
    engine: Arc<Mutex<Engine>>,
    voices: Arc<[String]>, // sorted
    default_voice: Arc<str>,
}

impl AppState {
    pub fn new(engine: Engine, default_voice: &str) -> Self {
        let voices = engine.voices().into();
        Self { engine: Arc::new(Mutex::new(engine)), voices, default_voice: default_voice.into() }
    }

    pub fn has_voice(&self, voice: &str) -> bool {
        self.voices.binary_search_by(|name| name.as_str().cmp(voice)).is_ok()
    }
}

pub fn router(state: AppState) -> Router {
    Router::new()
        .route("/v1/audio/speech", post(speech))
        .route("/v1/audio/voices", get(voices))
        .route("/health", get(health))
        .layer(DefaultBodyLimit::max(MAX_BODY_BYTES))
        .with_state(state)
}

fn json_response(status: StatusCode, body: String) -> Response {
    (status, [(header::CONTENT_TYPE, "application/json")], body).into_response()
}

// OpenAI-style error body.
fn error_response(status: StatusCode, message: &str, param: Option<&str>) -> Response {
    eprintln!("POST /v1/audio/speech {}: {message}", status.as_u16());
    let kind = if status.is_server_error() { "server_error" } else { "invalid_request_error" };
    let body = json!({"error": {"message": message, "type": kind, "param": param, "code": null}});
    json_response(status, body.to_string())
}

fn status_for(status: kokoro_status) -> StatusCode {
    match status {
        kokoro_status::ERROR_VOICE_NOT_FOUND | kokoro_status::ERROR_INVALID_ARGUMENT => StatusCode::BAD_REQUEST,
        _ => StatusCode::INTERNAL_SERVER_ERROR,
    }
}

fn encode(audio: &Audio, format: ResponseFormat) -> Result<Vec<u8>, WavTooLong> {
    match format {
        ResponseFormat::Pcm => Ok(encode_pcm16(audio.samples())),
        ResponseFormat::Wav => encode_wav_pcm16(audio.samples(), audio.sample_rate_hz()),
    }
}

async fn synthesize(state: &AppState, request: &SpeechRequest) -> Result<Audio, SynthesisError> {
    let mut engine = Arc::clone(&state.engine).lock_owned().await;
    let (input, voice, speed) = (request.input.clone(), request.voice.clone(), request.speed);
    tokio::task::spawn_blocking(move || engine.synthesize(&input, &voice, speed))
        .await
        .unwrap_or_else(|join_error| std::panic::resume_unwind(join_error.into_panic()))
}

async fn synthesize_into_response(state: &AppState, request: &SpeechRequest) -> Response {
    let start = Instant::now();
    let audio = match synthesize(state, request).await {
        Ok(audio) => audio,
        Err(error) => return error_response(status_for(error.status), &error.message, None),
    };
    let synth_s = start.elapsed().as_secs_f64();
    match encode(&audio, request.format) {
        Ok(bytes) => {
            eprintln!(
                "POST /v1/audio/speech 200 voice={} format={} ({:.2} s audio, synthesis {synth_s:.3} s)",
                request.voice,
                request.format.name(),
                audio.duration_s(),
            );
            ([(header::CONTENT_TYPE, request.format.content_type())], bytes).into_response()
        }
        Err(error) => error_response(StatusCode::INTERNAL_SERVER_ERROR, &error.to_string(), None),
    }
}

async fn speech(State(state): State<AppState>, body: Bytes) -> Response {
    let request = match parse_speech_request(&body, &state.default_voice) {
        Ok(request) => request,
        Err(error) => return error_response(StatusCode::BAD_REQUEST, &error.message, error.param),
    };
    if !state.has_voice(&request.voice) {
        let message = format!("Unknown voice '{}'; GET /v1/audio/voices lists the available voices", request.voice);
        return error_response(StatusCode::BAD_REQUEST, &message, Some("voice"));
    }
    synthesize_into_response(&state, &request).await
}

async fn voices(State(state): State<AppState>) -> Response {
    json_response(StatusCode::OK, json!({"voices": &*state.voices}).to_string())
}

async fn health() -> Response {
    json_response(StatusCode::OK, r#"{"status":"ok"}"#.to_owned())
}
