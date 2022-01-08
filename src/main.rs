const MS_DATA_LEN:i32 = 16 * 16384;

const MS_PREAMBLE_US:i32 = 8;
const MS_LONG_MSG_BITS:i32 = 112;
const MS_SHORT_MSG_BITS:i32 = 56;
const MS_FULL_LEN:i32 = MS_PREAMBLE_US + MS_LONG_MSG_BITS;
const MS_LONG_MSG_BYTES:i32 = MS_LONG_MSG_BITS / 8;
const MS_SHORT_MSG_BYTES:i32 = MS_SHORT_MSG_BITS / 8;
const MS_FULL_DATA_LEN:i32 = MS_DATA_LEN + (MS_FULL_LEN - 1) * 4;


extern {
    fn start();

    fn threadready() -> i32;
    fn getmagd() -> [u16; (MS_FULL_DATA_LEN / 2) as usize];

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

        let data:[u16; MS_FULL_DATA_LEN as usize];

        unsafe {
            premsprocess();
            data = getmagd();
            postmsprocess();
        }

        let mut i = 0;
        loop {
            print!("{} ", data[i]);
            i = i + 1;
        }
        println!();
    }
}