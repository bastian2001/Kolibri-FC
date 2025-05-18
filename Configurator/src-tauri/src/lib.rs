use serialport::SerialPort;
use std::collections::HashSet;
use std::io::{Read, Write};
use std::net::{TcpStream, ToSocketAddrs};
use std::process::Command;
use std::sync::{Mutex, MutexGuard};
use std::time::Duration;
use tauri::command;

// Helper function to acquire mutex with timeout
fn acquire_mutex_with_timeout<T>(
    mutex: &Mutex<T>,
    timeout_ms: u64,
) -> Result<MutexGuard<T>, String> {
    // First, try to acquire without waiting
    match mutex.try_lock() {
        Ok(guard) => return Ok(guard),
        Err(_) => {
            // If failed, start trying with small waits
            let start_time = std::time::Instant::now();
            let timeout = Duration::from_millis(timeout_ms);

            while start_time.elapsed() < timeout {
                std::thread::sleep(Duration::from_millis(1));
                if let Ok(guard) = mutex.try_lock() {
                    return Ok(guard);
                }
            }

            Err("Timed out waiting for mutex".to_string())
        }
    }
}

#[derive(Default)]
struct MyState {
    port: Mutex<Option<Box<dyn SerialPort>>>,
    tcp_stream: Mutex<Option<TcpStream>>,
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    let state = MyState::default();
    tauri::Builder::default()
        .manage(state)
        .plugin(tauri_plugin_opener::init())
        .invoke_handler(tauri::generate_handler![
            serial_list,
            serial_open,
            serial_read,
            serial_write,
            serial_close,
            tcp_list,
            tcp_open,
            tcp_read,
            tcp_write,
            tcp_close
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}

#[command]
fn serial_list(_state: tauri::State<'_, MyState>) -> Result<Vec<String>, String> {
    let ports: Result<Vec<serialport::SerialPortInfo>, serialport::Error> =
        serialport::available_ports();
    match ports {
        Ok(ports) => {
            let mut port_names: Vec<String> = Vec::new();
            for port in ports {
                port_names.push(port.port_name);
            }
            Ok(port_names)
        }
        Err(e) => Err(format!("{:?}", e)),
    }
}

#[command]
fn serial_open(state: tauri::State<'_, MyState>, path: String) -> Result<(), String> {
    let port = serialport::new(path.as_str(), 115200)
        .timeout(std::time::Duration::from_millis(0))
        .open();
    match port {
        Ok(port) => {
            *state.port.lock().unwrap() = Some(port);
            println!("Port opened");
            Ok(())
        }
        Err(e) => {
            println!("{:?}", e);
            Err(format!("{:?}", e))
        }
    }
}

#[command]
fn serial_read(state: tauri::State<'_, MyState>) -> Result<Vec<u8>, String> {
    let mut port = state.port.lock().unwrap();
    match port.as_mut() {
        Some(port) => {
            let mut buf: Vec<u8> = vec![0; 10000];
            match port.read(buf.as_mut_slice()) {
                Ok(t) => {
                    buf.resize(t, 0);
                    Ok(buf)
                }
                Err(e) => Err(format!("{:?}", e)),
            }
        }
        None => Err("No port open".to_string()),
    }
}

#[command]
fn serial_write(state: tauri::State<'_, MyState>, data: Vec<u8>) -> Result<(), String> {
    let mut port = state.port.lock().unwrap();
    match port.as_mut() {
        Some(port) => match port.write(data.as_slice()) {
            Ok(_) => Ok(()),
            Err(e) => Err(format!("{:?}", e)),
        },
        None => Err("No port open".to_string()),
    }
}

#[command]
fn serial_close(state: tauri::State<'_, MyState>) {
    *state.port.lock().unwrap() = None;
    println!("Port closed");
}

#[command]
async fn tcp_list(_state: tauri::State<'_, MyState>) -> Result<Vec<String>, String> {
    let hostnames = ["elrs_rx.local", "elrs-rx.fritz.box"];
    let mut output = Vec::new();

    // Track which IPs we've already seen to avoid duplicates
    let mut seen_ips = HashSet::new();

    // Create a vector of futures to execute concurrently
    let mut futures = Vec::new();

    for hostname in &hostnames {
        match (hostname.to_string(), 0).to_socket_addrs() {
            Ok(addr_iter) => {
                for addr in addr_iter {
                    let ip = addr.ip();

                    // Only proceed if we haven't seen this IP before
                    if seen_ips.insert(ip.clone()) {
                        println!("Pinging {}", ip);

                        // ping the IP address so that we can check if it is reachable. Sometimes the OS still knows about nonexistent devices on the network due to DNS caching

                        let hostname = hostname.to_string();
                        let ip_str = ip.to_string();

                        // Create a future that uses the system ping command
                        // On Linux, there are permission issues with ping-rs, so we use the system ping command
                        // On Windows, the ping command has such a stupid output (OS language dependent, and the status code also doesn't tell us anything useful), need to use ping-rs here. Besides that, a ping to an unreachable host is (sometimes?) counted as 0% loss because there's a reply from the local PC's IP telling it it cannot find the IP...
                        let fut = async move {
                            // Different ping arguments based on OS
                            if cfg!(target_os = "windows") {
                                let data = [0u8; 4];
                                let timeout = Duration::from_millis(3000);
                                let options = ping_rs::PingOptions {
                                    ttl: 64,
                                    dont_fragment: true,
                                };
                                match ping_rs::send_ping(&ip, timeout, &data, Some(&options)) {
                                    Ok(reply) => {
                                        println!(
                                            "  Ping to {} successful: {:?}",
                                            ip_str, reply.rtt
                                        );
                                        // Only include hosts that respond within our timeout
                                        if reply.rtt <= 3000 {
                                            return Some((hostname, ip_str));
                                        } else {
                                            return None;
                                        }
                                    }
                                    Err(e) => {
                                        println!("  Ping to {} failed: {:?}", ip_str, e);
                                        return None;
                                    }
                                };
                            };

                            let c = if cfg!(target_os = "macos") {
                                // macOS ping with timeout and limited count
                                Command::new("ping")
                                    .args(["-c", "1", "-t", "3", &ip_str])
                                    .output()
                            } else {
                                // Linux ping with timeout and limited count
                                Command::new("ping")
                                    .args(["-c", "1", "-W", "3", &ip_str])
                                    .output()
                            };

                            match c {
                                Ok(output) => {
                                    let success = output.status.success();
                                    if success {
                                        println!("  Ping to {} successful", ip_str);
                                        Some((hostname, ip_str))
                                    } else {
                                        let stderr = String::from_utf8_lossy(&output.stderr);
                                        println!("  Ping to {} failed: {}", ip_str, stderr);
                                        None
                                    }
                                }
                                Err(e) => {
                                    println!("  Ping command failed for {}: {:?}", ip_str, e);
                                    None
                                }
                            }
                        };

                        futures.push(fut);
                    }
                }
            }
            Err(_e) => {}
        }
    }

    // Execute all the futures concurrently using join_all
    let results = futures_util::future::join_all(futures).await;

    // Process results
    for result in results {
        if let Some((hostname, ip)) = result {
            output.push(format!("{},{}", hostname, ip));
        }
    }

    // If we found no valid hostnames, log it
    if output.is_empty() {
        println!("No hostnames resolved successfully or responded to pings");
    }

    Ok(output)
}

#[command]
async fn tcp_open(state: tauri::State<'_, MyState>, path: String) -> Result<(), String> {
    // Open a TCP connection, store it in the state, and turns on low latency mode
    // path is for example "192.168.1.2:5761", IP:port, or hostname:port
    if path.is_empty() {
        return Err("Path cannot be empty".to_string());
    }
    if !path.contains(':') {
        return Err("Path must include a port, e.g., 'hostname:port'".to_string());
    }

    if let Err(e) = path.to_socket_addrs() {
        println!("Invalid socket address: {:?}", e);
        return Err(format!("Invalid socket address: {:?}", e));
    }

    println!("Connecting to TCP address: {}", path);

    match TcpStream::connect(path) {
        Ok(stream) => {
            // Set TCP_NODELAY to reduce latency
            if let Err(e) = stream.set_nodelay(true) {
                println!("Error setting nodelay: {:?}", e);
                return Err("Failed to set nodelay".to_string());
            }

            // Set a reasonable read timeout
            if let Err(e) = stream.set_read_timeout(Some(Duration::from_millis(100))) {
                println!("Warning: Failed to set read timeout: {:?}", e);
            }

            // Set a reasonable write timeout
            if let Err(e) = stream.set_write_timeout(Some(Duration::from_millis(100))) {
                println!("Warning: Failed to set write timeout: {:?}", e);
            }

            // Get lock with 1s timeout
            let mut stream_guard = acquire_mutex_with_timeout(&state.tcp_stream, 1000)
                .map_err(|e| format!("tcp_open: Failed to acquire lock: {}", e))?;

            *stream_guard = Some(stream);
            println!("TCP connection opened");
            Ok(())
        }
        Err(e) => {
            println!("Error opening TCP connection: {:?}", e);
            Err(format!("Failed to open TCP connection: {:?}", e))
        }
    }
}

#[command]
async fn tcp_close(state: tauri::State<'_, MyState>) -> Result<(), String> {
    println!("Closing TCP connection");

    // Try to acquire the lock with a timeout
    let mut stream_guard = match acquire_mutex_with_timeout(&state.tcp_stream, 1000) {
        Ok(guard) => guard,
        Err(e) => {
            println!("tcp_close: Failed to acquire lock: {}", e);
            return Err(e);
        }
    };

    match stream_guard.take() {
        Some(stream) => {
            if let Err(e) = stream.shutdown(std::net::Shutdown::Both) {
                println!("Error shutting down TCP connection: {:?}", e);
                return Err(format!("Error shutting down: {:?}", e));
            }
            println!("TCP connection closed");
            Ok(())
        }
        None => {
            println!("No TCP connection to close");
            Err("No TCP connection to close".to_string())
        }
    }
}

#[command]
async fn tcp_read(state: tauri::State<'_, MyState>) -> Result<Vec<u8>, String> {
    // For read operations, use try_lock to make it non-blocking
    let lock_result = state.tcp_stream.try_lock();

    if let Ok(mut stream_guard) = lock_result {
        match stream_guard.as_mut() {
            Some(stream) => {
                // Set non-blocking mode for read operations
                if let Err(e) = stream.set_nonblocking(true) {
                    println!("Warning: Failed to set nonblocking mode: {:?}", e);
                }

                let mut buf: Vec<u8> = vec![0; 10000];
                match stream.read(buf.as_mut_slice()) {
                    Ok(t) => {
                        // Restore blocking mode for other operations
                        let _ = stream.set_nonblocking(false);
                        buf.resize(t, 0);
                        Ok(buf)
                    }
                    Err(e) if e.kind() == std::io::ErrorKind::WouldBlock => {
                        // No data available, not an error
                        let _ = stream.set_nonblocking(false);
                        Ok(vec![])
                    }
                    Err(e) => {
                        let _ = stream.set_nonblocking(false);
                        println!("TCP read error: {:?}", e);
                        Ok(vec![]) // Return empty vector to prevent disruption
                    }
                }
            }
            None => Ok(vec![]), // Return empty data when no connection
        }
    } else {
        // If we can't get the lock, just return empty data
        Ok(vec![])
    }
}

#[command]
async fn tcp_write(state: tauri::State<'_, MyState>, data: Vec<u8>) -> Result<(), String> {
    // For write operations, wait with timeout
    let mut stream_guard = match acquire_mutex_with_timeout(&state.tcp_stream, 1000) {
        Ok(guard) => guard,
        Err(e) => {
            println!("Failed to acquire lock for TCP write: {}", e);
            return Err(e);
        }
    };

    match stream_guard.as_mut() {
        Some(stream) => match stream.write(data.as_slice()) {
            Ok(_) => Ok(()),
            Err(e) => {
                println!("Error writing to TCP stream: {:?}", e);
                Err(format!("{:?}", e))
            }
        },
        None => Err("No TCP connection open".to_string()),
    }
}
