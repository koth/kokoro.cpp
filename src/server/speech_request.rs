//! Parsing of OpenAI-compatible `/v1/audio/speech` request bodies.
use serde_json::{Map, Value};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ResponseFormat {
    Wav,
    Pcm,
}

impl ResponseFormat {
    pub fn content_type(self) -> &'static str {
        match self {
            Self::Wav => "audio/wav",
            Self::Pcm => "audio/pcm",
        }
    }

    pub fn name(self) -> &'static str {
        match self {
            Self::Wav => "wav",
            Self::Pcm => "pcm",
        }
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct SpeechRequest {
    pub input: String,
    pub voice: String,
    pub speed: f32,
    pub format: ResponseFormat,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct RequestError {
    pub message: String,
    pub param: Option<&'static str>, // None when the error is not about one field
}

type Body = Map<String, Value>;

fn fail<T>(message: impl Into<String>, param: Option<&'static str>) -> Result<T, RequestError> {
    Err(RequestError { message: message.into(), param })
}

// The member, or None when it is missing or JSON null.
fn optional_member<'a>(body: &'a Body, key: &str) -> Option<&'a Value> {
    body.get(key).filter(|value| !value.is_null())
}

// C-locale isspace: space, \t, \n, \v, \f, \r. (char::is_ascii_whitespace leaves out \v.)
fn is_blank(text: &str) -> bool {
    text.bytes().all(|b| b == b' ' || (b'\t'..=b'\r').contains(&b))
}

// C strings end at the first NUL byte, so a NUL would silently truncate the value at the engine boundary.
fn check_no_nul(text: &str, field: &'static str) -> Result<(), RequestError> {
    if text.contains('\0') {
        fail(format!("'{}' must not contain a NUL byte", field), Some(field))
    } else {
        Ok(())
    }
}

fn parse_input(body: &Body) -> Result<String, RequestError> {
    let text = match optional_member(body, "input").and_then(Value::as_str) {
        Some(text) if !is_blank(text) => text,
        _ => return fail("'input' must be a non-empty string", Some("input")),
    };
    check_no_nul(text, "input").map(|()| text.to_owned())
}

fn parse_voice(body: &Body, default_voice: &str) -> Result<String, RequestError> {
    let voice = match optional_member(body, "voice") {
        None => return Ok(default_voice.to_owned()),
        Some(Value::String(voice)) => voice,
        Some(_) => return fail("'voice' must be a string", Some("voice")),
    };
    check_no_nul(voice, "voice").map(|()| voice.clone())
}

fn parse_speed(body: &Body) -> Result<f32, RequestError> {
    let Some(speed) = optional_member(body, "speed") else { return Ok(1.0) };
    // Check the f32 the engine receives: f64 values beyond f32 overflow to infinity or underflow to 0.
    match speed.as_f64().map(|value| value as f32) {
        Some(value) if value.is_finite() && value > 0.0 => Ok(value),
        _ => fail("'speed' must be a number > 0", Some("speed")),
    }
}

fn parse_format(body: &Body) -> Result<ResponseFormat, RequestError> {
    let value = match optional_member(body, "response_format") {
        None => "wav".to_owned(),
        Some(Value::String(text)) => text.clone(),
        Some(other) => other.to_string(),
    };
    match value.as_str() {
        "wav" => Ok(ResponseFormat::Wav),
        "pcm" => Ok(ResponseFormat::Pcm),
        _ => fail(
            format!("Unsupported response_format '{value}'; kokoro-server supports wav and pcm"),
            Some("response_format"),
        ),
    }
}

fn check_stream_format(body: &Body) -> Result<(), RequestError> {
    match optional_member(body, "stream_format") {
        None => Ok(()),
        Some(Value::String(text)) if text == "audio" => Ok(()),
        Some(_) => fail("Only stream_format 'audio' is supported", Some("stream_format")),
    }
}

/// Parses an OpenAI `/v1/audio/speech` JSON body. `model`, `instructions` and unknown keys are accepted and ignored,
/// as OpenAI clients send them.
pub fn parse_speech_request(body: &[u8], default_voice: &str) -> Result<SpeechRequest, RequestError> {
    let Ok(Value::Object(body)) = serde_json::from_slice::<Value>(body) else {
        return fail("Request body must be a JSON object", None);
    };
    let request = SpeechRequest {
        input: parse_input(&body)?,
        voice: parse_voice(&body, default_voice)?,
        speed: parse_speed(&body)?,
        format: parse_format(&body)?,
    };
    check_stream_format(&body)?;
    Ok(request)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn accepted(body: &str) -> SpeechRequest {
        parse_speech_request(body.as_bytes(), "af_maple").expect("request should be accepted")
    }

    fn rejected(body: &str) -> RequestError {
        parse_speech_request(body.as_bytes(), "af_maple").expect_err("request should be rejected")
    }

    #[test]
    fn defaults() {
        let request = accepted(r#"{"model":"tts-1","input":" hello ","instructions":"x","extra":1}"#);
        assert_eq!(request.input, " hello ");
        assert_eq!(request.voice, "af_maple");
        assert_eq!(request.speed, 1.0);
        assert_eq!(request.format, ResponseFormat::Wav);
    }

    #[test]
    fn nul_bytes_are_rejected_instead_of_truncated() {
        let input = rejected(r#"{"input":"Hello\u0000world"}"#);
        assert_eq!(input.message, "'input' must not contain a NUL byte");
        assert_eq!(input.param, Some("input"));
        let voice = rejected(r#"{"input":"x","voice":"af_maple\u0000"}"#);
        assert_eq!(voice.message, "'voice' must not contain a NUL byte");
        assert_eq!(voice.param, Some("voice"));
    }

    #[test]
    fn accepted_values() {
        let request =
            accepted(r#"{"input":"x","voice":"bf_vale","speed":2,"response_format":"pcm","stream_format":"audio"}"#);
        assert_eq!(request.voice, "bf_vale");
        assert_eq!(request.speed, 2.0);
        assert_eq!(request.format, ResponseFormat::Pcm);
        assert_eq!(accepted(r#"{"input":"x","voice":null,"speed":null,"response_format":null}"#).voice, "af_maple");
    }

    #[test]
    fn errors_name_the_offending_field() {
        assert_eq!(rejected("not json").message, "Request body must be a JSON object");
        assert_eq!(rejected("[]").param, None);
        assert_eq!(rejected("{}").param, Some("input"));
        assert_eq!(rejected(r#"{"input":" \t"}"#).message, "'input' must be a non-empty string");
        assert_eq!(rejected(r#"{"input":3}"#).param, Some("input"));
        assert_eq!(rejected(r#"{"input":"x","voice":1}"#).message, "'voice' must be a string");
        assert_eq!(rejected(r#"{"input":"x","speed":0}"#).message, "'speed' must be a number > 0");
        assert_eq!(rejected(r#"{"input":"x","speed":"fast"}"#).param, Some("speed"));
        assert_eq!(rejected(r#"{"input":"x","speed":1e300}"#).param, Some("speed"));
        assert_eq!(rejected(r#"{"input":"x","speed":1e-60}"#).param, Some("speed"));
        let mp3 = rejected(r#"{"input":"x","response_format":"mp3"}"#);
        assert_eq!(mp3.message, "Unsupported response_format 'mp3'; kokoro-server supports wav and pcm");
        assert_eq!(mp3.param, Some("response_format"));
        let sse = rejected(r#"{"input":"x","stream_format":"sse"}"#);
        assert_eq!(sse.message, "Only stream_format 'audio' is supported");
        assert_eq!(sse.param, Some("stream_format"));
    }

    #[test]
    fn vertical_tab_counts_as_blank() {
        assert_eq!(rejected(r#"{"input":"\u000b\f"}"#).param, Some("input"));
    }

    #[test]
    fn non_string_format_is_echoed_as_json() {
        assert_eq!(
            rejected(r#"{"input":"x","response_format":3}"#).message,
            "Unsupported response_format '3'; kokoro-server supports wav and pcm"
        );
    }

    #[test]
    fn fields_are_checked_in_order() {
        assert_eq!(rejected(r#"{"input":"","voice":1,"speed":0}"#).param, Some("input"));
        assert_eq!(rejected(r#"{"input":"x","speed":-1,"response_format":"mp3"}"#).param, Some("speed"));
    }
}
