// The emulator's plugins find their callbacks in this program by name.
fn main() {
    println!("cargo:rustc-link-arg-bins=-rdynamic");
}
