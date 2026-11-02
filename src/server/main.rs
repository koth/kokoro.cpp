use kokoro_server::engine::{library_version, Engine, EngineConfig};
use kokoro_server::listener::bind_exclusive;
use kokoro_server::options::{parse_args, Command, Options, USAGE};
use kokoro_server::routes::AppState;
use std::process::ExitCode;
use std::time::Instant;

const EXIT_FAILURE: u8 = 1;
const EXIT_USAGE: u8 = 2;

fn failure(message: &str) -> ExitCode {
    eprintln!("kokoro-server: {message}");
    ExitCode::from(EXIT_FAILURE)
}

fn usage_failure(message: &str) -> ExitCode {
    eprintln!("kokoro-server: {message}\nTry 'kokoro-server --help'.");
    ExitCode::from(EXIT_USAGE)
}

fn engine_config(opt: &Options) -> EngineConfig {
    EngineConfig {
        model: opt.model.clone(),
        voices: opt.voices.clone(),
        dict: opt.dict.clone(),
        device: opt.device,
        gpu_id: opt.gpu_id,
        number_language: opt.number_language,
        language: opt.language,
    }
}

fn serve(state: AppState, opt: &Options) -> ExitCode {
    let Ok(listener) = bind_exclusive(&opt.host, opt.port) else {
        return failure(&format!("cannot listen on {}:{}", opt.host, opt.port));
    };
    eprintln!("Listening on http://{}:{}", opt.host, opt.port);
    let served = tokio::runtime::Runtime::new()
        .and_then(|runtime| runtime.block_on(kokoro_server::serve(listener, state, std::future::pending())));
    match served {
        Ok(()) => ExitCode::SUCCESS,
        Err(error) => failure(&error.to_string()),
    }
}

fn run(opt: &Options) -> ExitCode {
    let init_start = Instant::now();
    let engine = match Engine::create(&engine_config(opt)) {
        Ok(engine) => engine,
        Err(message) => return failure(&message),
    };
    let device = engine.device_name();
    let state = AppState::new(engine, &opt.voice);
    if !state.has_voice(&opt.voice) {
        return failure(&format!("unknown voice '{}'", opt.voice));
    }
    eprintln!("Initialized engine in {:.3} s ({device})", init_start.elapsed().as_secs_f64());
    serve(state, opt)
}

fn main() -> ExitCode {
    let args: Result<Vec<String>, _> = std::env::args_os().skip(1).map(|arg| arg.into_string()).collect();
    let Ok(args) = args else { return usage_failure("arguments must be valid Unicode") };
    match parse_args(&args) {
        Err(error) => usage_failure(&error.0),
        Ok(Command::Help) => {
            print!("{USAGE}");
            ExitCode::SUCCESS
        }
        Ok(Command::Version) => {
            println!("{}", library_version());
            ExitCode::SUCCESS
        }
        Ok(Command::Run(opt)) => run(&opt),
    }
}
