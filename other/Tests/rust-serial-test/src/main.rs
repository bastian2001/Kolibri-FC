//basic serial monitor using com port 10

// use serialport::{Error, Result, SerialPort};
use serialport::new;
use std::io;

fn main() {
    let mut port = new("COM13", 38400)
        .timeout(std::time::Duration::from_millis(0))
        .open()
        .expect("Failed to open port");
    let mut serial_buf: Vec<u8> = vec![0; 100];
    loop {
        let s = port.read(serial_buf.as_mut_slice());
        match s {
            Ok(s) => {
                println!("Read {} bytes from Serial", s);
                let s = String::from_utf8_lossy(&serial_buf);
                println!("Content: {}", s);
            }
            Err(ref e) if e.kind() == io::ErrorKind::TimedOut => (),
            Err(e) => eprintln!("{:?}", e),
        }
        // if s > 0 {
        //     println!("Read {} bytes", s);
        //     let s = String::from_utf8_lossy(&serial_buf);
        //     println!("Content: {}", s);
        // }
        //if there's something to read, read it
        println!("Reading");
        let mut s = String::new();
        let res = io::stdin().read_line(&mut s);
        match res {
            Ok(_) => {
                println!("Read {} bytes from stdin", s.len());
                println!("Content: {}", s);
                let s = s.as_bytes();
                port.write(s).expect("Write failed");
            }
            Err(e) => eprintln!("{:?}", e),
        }
    }
}
