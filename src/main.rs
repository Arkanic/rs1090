
extern {
    fn msinit();
    fn msinitrtlsdr();
}

fn init() {
    unsafe {
        msinit();
        msinitrtlsdr();
    }
}

fn main() {
    init();

    println!("t");
}