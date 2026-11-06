use std::env;
use std::path::PathBuf;

fn find_sdk_root() -> Option<PathBuf> {
    if let Some(root) = env::var_os("CONTACTGLOVE_SDK_ROOT") {
        return Some(PathBuf::from(root));
    }

    let mut current = PathBuf::from(env::var_os("CARGO_MANIFEST_DIR")?);
    while current.pop() {
        if current
            .join("include/contactglove/ContactGloveSDKv2C.h")
            .is_file()
        {
            return Some(current);
        }
    }
    None
}

fn main() {
    println!("cargo:rerun-if-env-changed=CONTACTGLOVE_SDK_ROOT");
    let root = find_sdk_root().expect(
        "ContactGlove native SDK was not found. Set CONTACTGLOVE_SDK_ROOT to the SDK root containing include/ and lib/.",
    );
    let header = root.join("include/contactglove/ContactGloveSDKv2C.h");
    assert!(
        header.is_file(),
        "missing C ABI header: {}",
        header.display()
    );
    println!("cargo:rerun-if-changed={}", header.display());

    let library_dir = root.join("lib");
    assert!(
        library_dir.is_dir(),
        "missing native library directory: {}",
        library_dir.display()
    );
    println!("cargo:rustc-link-search=native={}", library_dir.display());
    println!("cargo:rustc-link-lib=dylib=ContactGloveSDKv2");
}
