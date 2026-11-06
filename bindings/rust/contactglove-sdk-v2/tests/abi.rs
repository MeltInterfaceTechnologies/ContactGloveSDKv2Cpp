use contactglove_sdk_v2::{sys, Sdk};

#[test]
fn uses_c_abi_v2() {
    assert_eq!(Sdk::abi_version(), sys::CONTACTGLOVE_C_ABI_VERSION);
}
