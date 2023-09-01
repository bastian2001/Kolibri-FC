// Prevents additional console window on Windows in release, DO NOT REMOVE!!
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

// Import the serialport crate

// use std::sync::Mutex;
// use std::time::Duration;

// use serialport::{available_ports, SerialPort};

// #[derive(Default)]
// struct PortState(Mutex<Option<dyn SerialPort>>);

use tauri_plugin_serialport;

fn main() {
    tauri::Builder::default()
        // .manage(PortState(Default::default()))
        // .invoke_handler(tauri::generate_handler![
        //     list_serial_devices //,
        //                         // serial_connect,
        //                         // serial_read,
        //                         // serial_write,
        //                         // serial_close
        // ])
        .plugin(tauri_plugin_serialport::init())
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}

// // function that lists serial devices
// #[tauri::command]
// fn list_serial_devices() -> Result<Vec<String>, String> {
//     match available_ports() {
//         Ok(ports) => {
//             let mut port_names: Vec<String> = Vec::new();
//             for p in ports {
//                 port_names.push(p.port_name);
//             }
//             return Ok(port_names);
//         }
//         Err(e) => {
//             return Err(format!("Error listing serial ports: {:?}", e));
//         }
//     }
// }

// // function that opens a serial port
// #[tauri::command]
// fn serial_connect(
//     state: tauri::State<'_, PortState>,
//     port_string: String,
// ) -> Result<String, String> {
//     println!("Connecting to {}", port_string);
//     let port = serialport::new(port_string.as_str(), 115200)
//         .timeout(Duration::from_millis(10))
//         .open();
//     match port {
//         Ok(mut port) => {
//             *state.0.lock().unwrap() = Some(port);
//             return Ok(String::from("Connected"));
//         }
//         Err(e) => {
//             return Err(format!("{}", e));
//         }
//     }
// }

// // // function that reads from a serial port
// // #[tauri::command]
// // fn serial_read(state: tauri::State<'_, PortState>) -> Result<Vec<u8>, String> {
// //     let mut buf = [0; 10000];
// //     println!("Reading");
// //     match state.0.lock().unwrap().as_mut() {
// //         Some(port) => {
// //             println!("Port is open");
// //             match port.read(&mut buf) {
// //                 Ok(n) => {
// //                     // let as_vec = Vec::from(buf[..n].to_vec());
// //                     // println!("Read: {:?}", as_vec);
// //                     println!("Read: {:?}", buf);
// //                     return Ok(Vec::new());
// //                 }
// //                 Err(e) => {
// //                     println!("Error");
// //                     return Err(format!("{}", e));
// //                 } // let read = port.read(&mut buf);
// //                   // if read.is_err() {
// //                   //     println!("Error");
// //                   //     return Err(format!("{}", read.err().unwrap()));
// //                   // }
// //                   // let as_vec = Vec::from(buf[..n]);
// //                   // println!("Read: {:?}", as_vec);
// //                   // return Ok(as_vec);
// //             }
// //         }
// //         None => {
// //             println!("Not connected");
// //             return Err(String::from("Not connected"));
// //         }
// //     }
// // }

// // // function that writes to a serial port
// // #[tauri::command]
// // fn serial_write(state: tauri::State<'_, PortState>, s: String) -> Result<String, String> {
// //     let data = s.as_bytes();
// //     match state.0.lock().unwrap().as_mut() {
// //         Some(port) => match port.write(&data) {
// //             Ok(_) => {
// //                 return Ok(String::from("Sent"));
// //             }
// //             Err(e) => {
// //                 return Err(format!("{}", e));
// //             }
// //         },
// //         None => {
// //             return Err(format!("Not connected"));
// //         }
// //     }
// // }

// // // function that closes a serial port
// // #[tauri::command]
// // fn serial_close(state: tauri::State<'_, PortState>) -> Result<String, String> {
// //     drop(state.0.lock().unwrap());
// //     return Ok(String::from("Closed"));
// // }
