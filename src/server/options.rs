//! Command-line options of kokoro-server.
use crate::ffi::{kokoro_device, kokoro_language, kokoro_number_language};

pub const USAGE: &str = "\
Usage: kokoro-server [options]

Serve OpenAI-compatible text-to-speech over HTTP: POST /v1/audio/speech (wav, pcm), GET /v1/audio/voices, GET /health.

Options:
  -m, --model <path>    ONNX model file (default: models/kokoro-v1.1-zh.onnx)
      --voices <path>   voices file (default: models/voices-v1.1-zh.bin)
  -d, --dict <dir>      dictionary directory (default: dict)
  -v, --voice <name>    voice used when a request omits \"voice\" (default: af_maple)
      --lang <auto|en|zh|es|de>  language for reading numbers (default: auto)
      --language <auto|zh|en|es|de>  text language; auto: es for ef_*/em_*, de for df_*/dm_*, en for \
af_/am_/bf_/bm_* voices, else zh (Chinese/English) (default: auto)
      --device <name>   auto, cpu or cuda (default: auto)
      --gpu-id <n>      CUDA device to use (default: 0)
      --host <addr>     address to listen on (default: 127.0.0.1)
      --port <n>        TCP port to listen on (default: 8880)
  -h, --help            print this help and exit
      --version         print the library version and exit
";

#[derive(Clone, Debug, PartialEq)]
pub struct Options {
    pub model: String,
    pub voices: String,
    pub dict: String,
    pub voice: String,
    pub number_language: kokoro_number_language, // --lang
    pub language: kokoro_language,               // --language
    pub device: kokoro_device,
    pub gpu_id: i32,
    pub host: String,
    pub port: u16,
}

impl Default for Options {
    fn default() -> Self {
        Self {
            model: "models/kokoro-v1.1-zh.onnx".to_owned(),
            voices: "models/voices-v1.1-zh.bin".to_owned(),
            dict: "dict".to_owned(),
            voice: "af_maple".to_owned(),
            number_language: kokoro_number_language::AUTO,
            language: kokoro_language::AUTO,
            device: kokoro_device::AUTO,
            gpu_id: 0,
            host: "127.0.0.1".to_owned(),
            port: 8880,
        }
    }
}

#[derive(Debug, PartialEq)]
pub enum Command {
    Run(Options),
    Help,
    Version,
}

/// A usage error message, printed as `kokoro-server: <message>`.
#[derive(Debug, PartialEq, Eq)]
pub struct UsageError(pub String);

#[derive(Clone, Copy)]
enum Opt {
    Model,
    Voices,
    Dict,
    Voice,
    Lang,
    Language,
    Device,
    GpuId,
    Host,
    Port,
    Help,
    Version,
}

struct OptionSpec {
    short_name: Option<&'static str>,
    long_name: &'static str,
    id: Opt,
    takes_value: bool,
}

const fn spec(short_name: Option<&'static str>, long_name: &'static str, id: Opt, takes_value: bool) -> OptionSpec {
    OptionSpec { short_name, long_name, id, takes_value }
}

const OPTIONS: [OptionSpec; 12] = [
    spec(Some("-m"), "--model", Opt::Model, true),
    spec(None, "--voices", Opt::Voices, true),
    spec(Some("-d"), "--dict", Opt::Dict, true),
    spec(Some("-v"), "--voice", Opt::Voice, true),
    spec(None, "--lang", Opt::Lang, true),
    spec(None, "--language", Opt::Language, true),
    spec(None, "--device", Opt::Device, true),
    spec(None, "--gpu-id", Opt::GpuId, true),
    spec(None, "--host", Opt::Host, true),
    spec(None, "--port", Opt::Port, true),
    spec(Some("-h"), "--help", Opt::Help, false),
    spec(None, "--version", Opt::Version, false),
];

fn find_option(name: &str) -> Option<&'static OptionSpec> {
    OPTIONS.iter().find(|spec| spec.long_name == name || spec.short_name == Some(name))
}

fn usage_error<T>(message: String) -> Result<T, UsageError> {
    Err(UsageError(message))
}

// strtol-style: leading C whitespace and a sign are accepted, trailing characters are not.
fn parse_integer(value: &str) -> Option<i64> {
    value.trim_start_matches(|c: char| c == ' ' || ('\t'..='\r').contains(&c)).parse().ok()
}

pub fn parse_number_language(value: &str) -> Option<kokoro_number_language> {
    match value {
        "auto" => Some(kokoro_number_language::AUTO),
        "en" => Some(kokoro_number_language::ENGLISH),
        "zh" => Some(kokoro_number_language::CHINESE),
        "es" => Some(kokoro_number_language::SPANISH),
        "de" => Some(kokoro_number_language::GERMAN),
        _ => None,
    }
}

pub fn parse_language(value: &str) -> Option<kokoro_language> {
    match value {
        "auto" => Some(kokoro_language::AUTO),
        "zh" => Some(kokoro_language::CHINESE_ENGLISH),
        "en" => Some(kokoro_language::ENGLISH),
        "es" => Some(kokoro_language::SPANISH),
        "de" => Some(kokoro_language::GERMAN),
        _ => None,
    }
}

pub fn parse_device(value: &str) -> Option<kokoro_device> {
    match value {
        "auto" => Some(kokoro_device::AUTO),
        "cpu" => Some(kokoro_device::CPU),
        "cuda" => Some(kokoro_device::CUDA),
        _ => None,
    }
}

pub fn parse_gpu_id(value: &str) -> Option<i32> {
    parse_integer(value).and_then(|id| i32::try_from(id).ok()).filter(|&id| id >= 0)
}

pub fn parse_port(value: &str) -> Option<u16> {
    parse_integer(value).and_then(|port| u16::try_from(port).ok()).filter(|&port| port >= 1)
}

fn checked<T>(parsed: Option<T>, message: impl FnOnce() -> String) -> Result<T, UsageError> {
    parsed.map_or_else(|| usage_error(message()), Ok)
}

// Applies one option value to opt.
fn apply_value(id: Opt, value: String, opt: &mut Options) -> Result<(), UsageError> {
    match id {
        Opt::Model => opt.model = value,
        Opt::Voices => opt.voices = value,
        Opt::Dict => opt.dict = value,
        Opt::Voice => opt.voice = value,
        Opt::Host => opt.host = value,
        Opt::Lang => {
            opt.number_language = checked(parse_number_language(&value), || {
                format!("invalid number language '{value}' (must be auto, en, zh, es or de)")
            })?
        }
        Opt::Language => {
            opt.language = checked(parse_language(&value), || {
                format!("invalid language '{value}' (must be auto, zh, en, es or de)")
            })?
        }
        Opt::Device => {
            opt.device =
                checked(parse_device(&value), || format!("invalid device '{value}' (expected auto, cpu or cuda)"))?
        }
        Opt::GpuId => {
            opt.gpu_id =
                checked(parse_gpu_id(&value), || format!("invalid GPU id '{value}' (must be an integer >= 0)"))?
        }
        Opt::Port => opt.port = checked(parse_port(&value), || format!("invalid port '{value}' (must be 1-65535)"))?,
        Opt::Help | Opt::Version => {}
    }
    Ok(())
}

// Splits "--name=value"; only long options accept an inline value.
fn split_inline(arg: &str) -> (&str, Option<&str>) {
    match arg.split_once('=') {
        Some((name, value)) if arg.starts_with("--") => (name, Some(value)),
        _ => (arg, None),
    }
}

/// Parses the arguments after the program name. Parsing stops at the first `--help` or `--version`.
pub fn parse_args(args: &[String]) -> Result<Command, UsageError> {
    let mut opt = Options::default();
    let mut rest = args.iter();
    while let Some(arg) = rest.next() {
        if arg.len() < 2 || !arg.starts_with('-') {
            return usage_error(format!("unexpected argument '{arg}'"));
        }
        let (name, inline) = split_inline(arg);
        let Some(spec) = find_option(name) else { return usage_error(format!("unknown option '{arg}'")) };
        match (spec.id, spec.takes_value, inline) {
            (_, false, Some(_)) => return usage_error(format!("option '{name}' does not take a value")),
            (Opt::Help, ..) => return Ok(Command::Help),
            (Opt::Version, ..) => return Ok(Command::Version),
            (id, true, Some(value)) => apply_value(id, value.to_owned(), &mut opt)?,
            (id, true, None) => {
                let Some(value) = rest.next() else {
                    return usage_error(format!("option '{name}' requires a value"));
                };
                apply_value(id, value.clone(), &mut opt)?
            }
            (_, false, None) => {}
        }
    }
    Ok(Command::Run(opt))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn parse(args: &[&str]) -> Result<Command, UsageError> {
        parse_args(&args.iter().map(|arg| arg.to_string()).collect::<Vec<_>>())
    }

    fn run(args: &[&str]) -> Options {
        match parse(args) {
            Ok(Command::Run(opt)) => opt,
            other => panic!("expected Run, got {other:?}"),
        }
    }

    fn error(args: &[&str]) -> String {
        parse(args).expect_err("arguments should be rejected").0
    }

    #[test]
    fn values_short_and_long_inline() {
        let opt = run(&["-m", "a.onnx", "--voices=v.bin", "-v", "bf_vale", "--port=9000", "--lang", "es"]);
        assert_eq!((opt.model.as_str(), opt.voices.as_str(), opt.voice.as_str()), ("a.onnx", "v.bin", "bf_vale"));
        assert_eq!(opt.port, 9000);
        assert_eq!(opt.number_language, kokoro_number_language::SPANISH);
        assert_eq!(run(&[]), Options::default());
    }

    #[test]
    fn usage_errors() {
        assert_eq!(error(&["x"]), "unexpected argument 'x'");
        assert_eq!(error(&["-v=x"]), "unknown option '-v=x'");
        assert_eq!(error(&["--help=1"]), "option '--help' does not take a value");
        assert_eq!(error(&["--port"]), "option '--port' requires a value");
        assert_eq!(error(&["--device", "tpu"]), "invalid device 'tpu' (expected auto, cpu or cuda)");
        assert_eq!(error(&["--gpu-id", "-1"]), "invalid GPU id '-1' (must be an integer >= 0)");
        for port in ["0", "65536", "8a", ""] {
            assert_eq!(error(&["--port", port]), format!("invalid port '{port}' (must be 1-65535)"));
        }
    }

    #[test]
    fn help_and_version_stop_parsing() {
        assert_eq!(parse(&["--help", "--bogus"]), Ok(Command::Help));
        assert_eq!(parse(&["--version", "x"]), Ok(Command::Version));
        assert_eq!(error(&["--bogus", "--help"]), "unknown option '--bogus'");
    }
}
