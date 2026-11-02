//! The listen socket. A second server on the same port must fail to bind instead of sharing it.
use socket2::{Domain, Protocol, Socket, Type};
use std::io;
use std::net::{SocketAddr, TcpListener, ToSocketAddrs};

const BACKLOG: i32 = 1024;

#[cfg(windows)]
fn exclusive(socket: &Socket) -> io::Result<()> {
    use std::os::windows::io::AsRawSocket;
    use windows_sys::Win32::Networking::WinSock::{setsockopt, SOCKET, SOL_SOCKET, SO_EXCLUSIVEADDRUSE};
    let enable: i32 = 1;
    // SAFETY: the socket handle is live and the option value points at an i32 of the given length.
    let result = unsafe {
        setsockopt(
            socket.as_raw_socket() as SOCKET,
            SOL_SOCKET,
            SO_EXCLUSIVEADDRUSE,
            (&enable as *const i32).cast(),
            std::mem::size_of::<i32>() as i32,
        )
    };
    if result == 0 {
        Ok(())
    } else {
        Err(io::Error::last_os_error())
    }
}

// Lets a restarted server bind during TIME_WAIT; unlike SO_REUSEPORT it never shares a listening port.
#[cfg(not(windows))]
fn exclusive(socket: &Socket) -> io::Result<()> {
    socket.set_reuse_address(true)
}

fn bind_one(addr: SocketAddr) -> io::Result<TcpListener> {
    let socket = Socket::new(Domain::for_address(addr), Type::STREAM, Some(Protocol::TCP))?;
    exclusive(&socket)?;
    socket.bind(&addr.into())?;
    socket.listen(BACKLOG)?;
    socket.set_nonblocking(true)?;
    Ok(socket.into())
}

/// Binds the first resolved address of host:port that accepts the socket.
pub fn bind_exclusive(host: &str, port: u16) -> io::Result<TcpListener> {
    let mut last_error = io::Error::new(io::ErrorKind::AddrNotAvailable, "host resolved to no address");
    for addr in (host, port).to_socket_addrs()? {
        match bind_one(addr) {
            Ok(listener) => return Ok(listener),
            Err(error) => last_error = error,
        }
    }
    Err(last_error)
}

#[cfg(test)]
mod tests {
    use super::*;

    // A port-sharing socket (SO_REUSEADDR on Windows, SO_REUSEPORT elsewhere) must not bind next to the server.
    #[test]
    fn port_sharing_socket_cannot_bind_beside_the_server() {
        let server = bind_exclusive("127.0.0.1", 0).unwrap();
        let addr = server.local_addr().unwrap();
        let intruder = Socket::new(Domain::IPV4, Type::STREAM, Some(Protocol::TCP)).unwrap();
        intruder.set_reuse_address(true).unwrap();
        #[cfg(unix)]
        intruder.set_reuse_port(true).unwrap();
        assert!(intruder.bind(&addr.into()).is_err());
        assert!(bind_exclusive("127.0.0.1", addr.port()).is_err());
    }
}
