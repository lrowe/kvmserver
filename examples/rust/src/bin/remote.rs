use kvmserver_examples_rust::get_storage;

fn main() {
    let mut return_value = 0;
    let mut storage = get_storage().unwrap();
    loop {
        return_value = match storage.wait_paused(return_value) {
            Err(num) => num,
            Ok(None) => 0,
            Ok(Some(buf)) => {
                let message = b"Hello, World!";
                if message.len() > buf.len() {
                    -1
                } else {
                    buf[0..message.len()].copy_from_slice(message);
                    message.len().try_into().unwrap()
                }
            }
        };
    }
}
