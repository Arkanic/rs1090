fn main() {
    cc::Build::new()
        .file("c/main.c")
        .flag("-lrtlsdr -lpthread")
        .compile("cr");

    println!("cargo:rustc-link-lib=rtlsdr");
}