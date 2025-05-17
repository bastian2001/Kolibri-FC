use serialport::SerialPort;
use std::collections::HashSet;
use std::io::Read;
use std::io::Write;
use std::net::TcpStream;
use std::net::ToSocketAddrs;
use std::sync::Mutex;
use tauri::command;

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
    let hostnames = ["elrs_rx.local", "elrs_rx.fritz.box"];

    // to_socket_addrs will invoke the same getaddrinfo() your ping uses
    // let addrs = (hostname, 0).to_socket_addrs().map_err(|e| e.to_string())?;
    let mut addrs = Vec::new();
    for h in &hostnames {
        let hostname = h.to_string();
        match (hostname.clone(), 0).to_socket_addrs() {
            Ok(addr) => {
                addrs.extend(addr);
            }
            Err(e) => {
                println!("Error resolving {}: {}", hostname, e);
            }
        }
    }

    // collect unique IP strings
    let mut ips = HashSet::new();
    for addr in addrs {
        ips.insert(addr.ip().to_string());
    }

    Ok(ips.into_iter().collect())
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
    // Attempt to connect to the provided path
    println!("Attempting to connect to TCP path: {}", path);
    // Ensure the path is a valid socket address
    if let Err(e) = path.to_socket_addrs() {
        println!("Invalid socket address: {:?}", e);
        return Err(format!("Invalid socket address: {:?}", e));
    }
    // Attempt to connect to the TCP address
    println!("Connecting to TCP address: {}", path);
    // Use TcpStream to connect to the provided path
    match TcpStream::connect(path) {
        Ok(stream) => {
            // Set the TCP stream to nodelay mode
            if let Err(e) = stream.set_nodelay(true) {
                println!("Error setting nodelay: {:?}", e);
                return Err("Failed to set nodelay".to_string());
            }
            *state.tcp_stream.lock().unwrap() = Some(stream);
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
async fn tcp_close(state: tauri::State<'_, MyState>) -> Result<(), ()> {
    println!("Closing TCP connection");
    // Close the TCP connection, if it exists
    match *state.tcp_stream.lock().unwrap() {
        Some(ref mut stream) => {
            println!("Some");
            if let Err(e) = stream.shutdown(std::net::Shutdown::Both) {
                println!("Error shutting down TCP connection: {:?}", e);
                return Err(());
            }
            println!("No error");
            *state.tcp_stream.lock().unwrap() = None;
            println!("TCP connection closed");
            Ok(())
        }
        None => {
            println!("None");
            println!("No TCP connection to close");
            Err(())
        }
    }
}

#[command]
async fn tcp_read(state: tauri::State<'_, MyState>) -> Result<Vec<u8>, String> {
    // Read data from the TCP connection, if it exists
    let mut stream = state.tcp_stream.lock().unwrap();
    match stream.as_mut() {
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
        None => Err("No TCP connection open".to_string()),
    }
}

#[command]
async fn tcp_write(state: tauri::State<'_, MyState>, data: Vec<u8>) -> Result<(), String> {
    // Write data to the TCP connection, if it exists
    let mut stream = state.tcp_stream.lock().unwrap();
    match stream.as_mut() {
        Some(stream) => match stream.write(data.as_slice()) {
            Ok(_) => Ok(()),
            Err(e) => Err(format!("{:?}", e)),
        },
        None => Err("No TCP connection open".to_string()),
    }
}
