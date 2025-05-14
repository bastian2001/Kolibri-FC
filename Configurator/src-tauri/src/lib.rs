use serialport::SerialPort;
use std::collections::HashSet;
use std::net::ToSocketAddrs;
use std::sync::Mutex;
use tauri::command;

#[derive(Default)]
struct MyState {
    port: Mutex<Option<Box<dyn SerialPort>>>,
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
            tcp_search
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
fn tcp_search(_state: tauri::State<'_, MyState>) -> Result<Vec<String>, String> {
    let hostname = "elrs_rx.local";

    // to_socket_addrs will invoke the same getaddrinfo() your ping uses
    let addrs = (hostname, 0).to_socket_addrs().map_err(|e| e.to_string())?;

    // collect unique IP strings
    let mut ips = HashSet::new();
    for addr in addrs {
        ips.insert(addr.ip().to_string());
    }

    Ok(ips.into_iter().collect())
}
