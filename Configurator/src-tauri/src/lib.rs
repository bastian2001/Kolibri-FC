use serialport::SerialPort;
use std::collections::HashSet;
use std::io::{Read, Write};
use std::net::{TcpStream, ToSocketAddrs};
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
    let mut result = Vec::new();

    // Track which IPs we've already seen to avoid duplicates
    let mut seen_ips = HashSet::new();

    for hostname in &hostnames {
        match (hostname.to_string(), 0).to_socket_addrs() {
            Ok(addr_iter) => {
                for addr in addr_iter {
                    let ip = addr.ip().to_string();

                    // Only add this IP if we haven't seen it before
                    if seen_ips.insert(ip.clone()) {
                        result.push(format!("{},{}", hostname, ip));
                    }
                }
                println!("Resolved {} successfully", hostname);
            }
            Err(e) => {
                println!("Error resolving {}: {}", hostname, e);
            }
        }
    }

    // If we found no valid hostnames, try to add direct IP entries as fallback
    if result.is_empty() {
        // You could add some common IPs as fallbacks here
        println!("No hostnames resolved successfully");
    } else {
        println!("Found {} resolved addresses", result.len());
    }

    Ok(result)
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

    println!("Attempting to connect to TCP path: {}", path);

    if let Err(e) = path.to_socket_addrs() {
        println!("Invalid socket address: {:?}", e);
        return Err(format!("Invalid socket address: {:?}", e));
    }

    println!("Connecting to TCP address: {}", path);

    match TcpStream::connect(path) {
        Ok(stream) => {
            if let Err(e) = stream.set_nodelay(true) {
                println!("Error setting nodelay: {:?}", e);
                return Err("Failed to set nodelay".to_string());
            }

            // Get lock with 1s timeout
            let mut stream_guard = acquire_mutex_with_timeout(&state.tcp_stream, 1000)
                .map_err(|e| format!("Failed to acquire lock: {}", e))?;

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
    println!("About to acquire lock...");

    // Try to acquire the lock with a timeout
    let mut stream_guard = match acquire_mutex_with_timeout(&state.tcp_stream, 1000) {
        Ok(guard) => {
            println!("Lock acquired");
            guard
        }
        Err(e) => {
            println!("Failed to acquire lock: {}", e);
            return Err(e);
        }
    };

    match stream_guard.take() {
        Some(stream) => {
            println!("Some");
            if let Err(e) = stream.shutdown(std::net::Shutdown::Both) {
                println!("Error shutting down TCP connection: {:?}", e);
                return Err(format!("Error shutting down: {:?}", e));
            }
            println!("No error");
            println!("TCP connection closed");
            Ok(())
        }
        None => {
            println!("None");
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
                let mut buf: Vec<u8> = vec![0; 10000];
                match stream.read(buf.as_mut_slice()) {
                    Ok(t) => {
                        buf.resize(t, 0);
                        Ok(buf)
                    }
                    Err(e) => Err(format!("{:?}", e)),
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
        Err(e) => return Err(e),
    };

    match stream_guard.as_mut() {
        Some(stream) => match stream.write(data.as_slice()) {
            Ok(_) => Ok(()),
            Err(e) => Err(format!("{:?}", e)),
        },
        None => Err("No TCP connection open".to_string()),
    }
}
