//! kokoro-server: OpenAI-compatible text-to-speech HTTP server on top of libkokoro (public C API only).
pub mod audio_encoding;
pub mod engine;
pub mod ffi;
pub mod listener;
pub mod options;
pub mod routes;
pub mod speech_request;

use std::future::Future;
use std::io;

/// Serves the routes on an already bound, non-blocking listener until shutdown resolves.
pub async fn serve(
    listener: std::net::TcpListener,
    state: routes::AppState,
    shutdown: impl Future<Output = ()> + Send + 'static,
) -> io::Result<()> {
    let listener = tokio::net::TcpListener::from_std(listener)?;
    axum::serve(listener, routes::router(state)).with_graceful_shutdown(shutdown).await
}
