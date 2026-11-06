use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{mpsc, Arc};
use std::thread;
use std::time::Duration;

use contactglove_sdk_v2::{Host, Result, Sdk};

fn main() -> Result<()> {
    let sdk = Sdk::new()?;
    let host = Host::default();
    let (sender, receiver) = mpsc::channel();
    let printed_callback = Arc::new(AtomicBool::new(false));
    let callback_printed = Arc::clone(&printed_callback);
    let subscription = sdk.subscribe_glove_data(move |data| {
        if !callback_printed.swap(true, Ordering::Relaxed) {
            println!(
                "[callback] id={}  battery={}%",
                data.id, data.battery_percentage
            );
        }
        let _ = sender.send(data.id);
    })?;

    sdk.connect(&host)?;
    println!("Listening on port {}...", host.receive_port);
    for _ in 0..5 {
        thread::sleep(Duration::from_secs(1));
        match receiver.try_recv() {
            Ok(id) => match sdk.glove_data(&id) {
                Ok(data) => println!("[poll] id={}  name={}", data.id, data.name),
                Err(error) => println!("[poll] {}: {:?}", id, error),
            },
            Err(_) => println!("[poll] waiting for callback data"),
        }
    }

    drop(subscription);
    sdk.disconnect(&host)
}
