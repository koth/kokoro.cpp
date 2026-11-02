//! End-to-end HTTP checks against the real model. KOKORO_TEST_MODEL, KOKORO_TEST_VOICES and KOKORO_TEST_DICT_DIR
//! name the files; the test is skipped when the model or voices file is missing.
use kokoro_server::engine::{Engine, EngineConfig};
use kokoro_server::ffi::{kokoro_device, kokoro_language, kokoro_number_language};
use kokoro_server::listener::bind_exclusive;
use kokoro_server::routes::AppState;
use serde_json::Value;
use std::path::Path;

fn test_config() -> Option<EngineConfig> {
    let path = |key: &str| std::env::var(key).ok();
    let (model, voices, dict) =
        (path("KOKORO_TEST_MODEL")?, path("KOKORO_TEST_VOICES")?, path("KOKORO_TEST_DICT_DIR")?);
    if !Path::new(&model).exists() || !Path::new(&voices).exists() {
        return None;
    }
    Some(EngineConfig {
        model,
        voices,
        dict,
        device: kokoro_device::AUTO,
        gpu_id: 0,
        number_language: kokoro_number_language::AUTO,
        language: kokoro_language::AUTO,
    })
}

// Starts a server on an ephemeral loopback port and returns its base URL.
fn start_server(config: &EngineConfig) -> String {
    let engine = Engine::create(config).expect("engine should load");
    let listener = bind_exclusive("127.0.0.1", 0).unwrap();
    let url = format!("http://{}", listener.local_addr().unwrap());
    let state = AppState::new(engine, "af_maple");
    std::thread::spawn(move || {
        let runtime = tokio::runtime::Runtime::new().unwrap();
        runtime.block_on(kokoro_server::serve(listener, state, std::future::pending())).unwrap();
    });
    url
}

struct Reply {
    status: u16,
    content_type: String,
    body: Vec<u8>,
}

fn reply(mut response: ureq::http::Response<ureq::Body>) -> Reply {
    let content_type = response.headers().get("content-type").map_or("", |v| v.to_str().unwrap()).to_owned();
    Reply { status: response.status().as_u16(), content_type, body: response.body_mut().read_to_vec().unwrap() }
}

fn post(agent: &ureq::Agent, url: &str, body: &str) -> Reply {
    reply(agent.post(format!("{url}/v1/audio/speech")).header("Content-Type", "application/json").send(body).unwrap())
}

fn json(reply: &Reply) -> Value {
    serde_json::from_slice(&reply.body).unwrap()
}

#[test]
fn server_synthesizes_audio() {
    let Some(config) = test_config() else {
        eprintln!("skipped: model or voices file not found");
        return;
    };
    let url = start_server(&config);
    let agent: ureq::Agent = ureq::Agent::config_builder().http_status_as_error(false).build().into();

    let wav = post(&agent, &url, r#"{"input":"Hello world"}"#);
    assert_eq!((wav.status, wav.content_type.as_str()), (200, "audio/wav"));
    assert!(wav.body.len() > 44 && wav.body.starts_with(b"RIFF"));

    let pcm = post(&agent, &url, r#"{"input":"Hello world","response_format":"pcm"}"#);
    assert_eq!((pcm.status, pcm.content_type.as_str()), (200, "audio/pcm"));
    assert!(!pcm.body.is_empty() && pcm.body.len() % 2 == 0);

    let voice = post(&agent, &url, r#"{"input":"Hello world","voice":"nope"}"#);
    assert_eq!(voice.status, 400);
    assert_eq!(json(&voice)["error"]["param"], "voice");

    assert_eq!(post(&agent, &url, r#"{"input":"Hello world","response_format":"mp3"}"#).status, 400);

    let voices = reply(agent.get(format!("{url}/v1/audio/voices")).call().unwrap());
    assert_eq!(voices.status, 200);
    assert!(json(&voices)["voices"].as_array().unwrap().iter().any(|v| v == "af_maple"));
}
