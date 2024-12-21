#[cfg(windows)]
fn main() {
    // Link to the required system information DLL and library
    println!("cargo:rustc-link-search=native=dll");
    println!("cargo:rustc-link-lib=systeminfo");
    
    // Trigger rebuild if DLL or library files are modified
    println!("cargo:rerun-if-changed=dll/systeminfo.dll");
    println!("cargo:rerun-if-changed=dll/systeminfo.lib");

    // Configure Windows application resources and manifest
    let mut res = winres::WindowsResource::new();
    res.set_manifest_file("manifest.xml");  // Set application manifest for Windows
    res.compile().unwrap();
}