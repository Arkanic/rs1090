
struct Magptr {
    data:&'static u16,
    length:u32
}

extern {
    fn start();

    fn threadready() -> i32;
    fn getmagd() -> Magptr;

    fn premsprocess();
    fn postmsprocess();
}

fn main() {
    unsafe {
        start();
    }

    loop {
        unsafe {
            if threadready() == 0 {continue};
        }

        let mut data = Magptr {
            data: &0,
            length:0
        };

        unsafe {
            premsprocess();
            data = getmagd();
            postmsprocess();
        }

        println!("d: {}\nl: {}", data.data, data.length);
    }
}