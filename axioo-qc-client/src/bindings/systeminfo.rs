// This module provides bindings to interact with system information through a dynamic library.

use crate::DEBUG_MODE;
use anyhow::Result;
use libloading::{Library, Symbol};
use once_cell::sync::Lazy;
use once_cell::sync::OnceCell;
use std::ffi::{c_char, CStr};
use std::sync::Mutex;

// Type definition for a callback function that receives system information.
type SystemInfoCallback = extern "C" fn(*const c_char);

// Type definitions for functions in the dynamic library.
#[allow(non_camel_case_types)]
type GetSystemInfoJsonT = unsafe extern "C" fn() -> *const c_char;
type FreeSystemInfoT = unsafe extern "C" fn(*const c_char);
type GetSystemInfoLastErrorT = unsafe extern "C" fn() -> i32;
type GetSystemInfoErrorMessageT = unsafe extern "C" fn() -> *const c_char;
type RegisterChangeCallbackT = unsafe extern "C" fn(SystemInfoCallback);
type UnregisterChangeCallbackT = unsafe extern "C" fn();
type SetLogCallbackT = unsafe extern "C" fn(extern "C" fn(*const c_char, *const c_char));
type InitializeCacheT = unsafe extern "C" fn();
type SetDebugModeT = unsafe extern "C" fn(bool);

// Static variables for managing the library and buffers.
static LIBRARY: OnceCell<Mutex<Library>> = OnceCell::new();
static LOG_BUFFER: Lazy<Mutex<Vec<String>>> = Lazy::new(|| Mutex::new(Vec::new()));
static CALLBACK_BUFFER: Lazy<Mutex<Vec<String>>> = Lazy::new(|| Mutex::new(Vec::new()));

// Callback function to handle system information received from the library.
extern "C" fn system_info_callback(json_data: *const c_char) {
    unsafe {
        if !json_data.is_null() {
            if let Ok(data) = CStr::from_ptr(json_data).to_str() {
                if DEBUG_MODE {
                    println!("System info callback received: {}", data);
                }

                if let Ok(mut buffer) = CALLBACK_BUFFER.lock() {
                    buffer.push(data.to_string());
                } else {
                    eprintln!("Failed to lock callback buffer");
                }
            }
        }
    }
}

// Callback function to handle logging messages from the library.
extern "C" fn log_callback(level: *const c_char, message: *const c_char) {
    if !DEBUG_MODE {
        return;
    }
    unsafe {
        if !level.is_null() && !message.is_null() {
            let level_str = CStr::from_ptr(level).to_string_lossy();
            let msg_str = CStr::from_ptr(message).to_string_lossy();
            let log_entry = format!("[{}] {}", level_str, msg_str);

            println!("{}", log_entry);

            if let Ok(mut buffer) = LOG_BUFFER.lock() {
                buffer.push(log_entry);
            }
        }
    }
}

// Initializes the dynamic library and sets up the necessary callbacks.
pub fn init_library() -> Result<()> {
    let dll_path = std::env::current_exe()?
        .parent()
        .ok_or_else(|| anyhow::anyhow!("Failed to get executable directory"))?
        .join("systeminfo.dll");

    let lib = unsafe { Library::new(dll_path)? };

    unsafe {
        let set_debug: Symbol<SetDebugModeT> = lib.get(b"SetDebugMode")?;
        println!("Setting debug mode to: {}", DEBUG_MODE);
        set_debug(DEBUG_MODE);
    }

    unsafe {
        let set_log: Symbol<SetLogCallbackT> = lib.get(b"SetLogCallback")?;
        set_log(log_callback);

        let init_cache: Symbol<InitializeCacheT> = lib.get(b"InitializeCache")?;
        init_cache();
    }

    LIBRARY.set(Mutex::new(lib)).unwrap();
    Ok(())
}

// Retrieves system information in JSON format from the library.
pub fn get_system_info() -> Result<String> {
    let lib = LIBRARY.get().unwrap().lock().unwrap();

    unsafe {
        let get_info: Symbol<GetSystemInfoJsonT> = lib.get(b"GetSystemInfoJson")?;
        let free_info: Symbol<FreeSystemInfoT> = lib.get(b"FreeSystemInfo")?;

        let ptr = get_info();
        if ptr.is_null() {
            return Err(anyhow::anyhow!("Failed to get system info"));
        }

        let c_str = CStr::from_ptr(ptr);
        let result = c_str.to_string_lossy().into_owned();

        free_info(ptr);
        Ok(result)
    }
}

// Retrieves the last error code from the library.
pub fn get_last_error() -> i32 {
    let lib = LIBRARY.get().unwrap().lock().unwrap();
    unsafe {
        let get_error: Symbol<GetSystemInfoLastErrorT> =
            lib.get(b"GetSystemInfoLastError").unwrap();
        get_error()
    }
}

// Retrieves the last error message from the library.
pub fn get_error_message() -> String {
    let lib = LIBRARY.get().unwrap().lock().unwrap();
    unsafe {
        let get_message: Symbol<GetSystemInfoErrorMessageT> =
            lib.get(b"GetSystemInfoErrorMessage").unwrap();
        let ptr = get_message();
        CStr::from_ptr(ptr).to_string_lossy().into_owned()
    }
}

// Registers a callback function to receive system information updates.
pub fn register_callback() -> Result<()> {
    let lib = LIBRARY.get().unwrap().lock().unwrap();
    unsafe {
        let register: Symbol<RegisterChangeCallbackT> = lib.get(b"RegisterChangeCallback")?;
        register(system_info_callback);
        Ok(())
    }
}

// Unregisters the previously registered callback function.
pub fn unregister_callback() -> Result<()> {
    let lib = LIBRARY.get().unwrap().lock().unwrap();
    unsafe {
        let unregister: Symbol<UnregisterChangeCallbackT> = lib.get(b"UnregisterChangeCallback")?;
        unregister();
        Ok(())
    }
}

// External function to clean up system information resources.
extern "C" {
    fn CleanupSystemInfo();
}

// Cleans up system information resources.
pub fn cleanup_system_info() {
    unsafe {
        CleanupSystemInfo();
    }
}

// Retrieves and clears the log messages.
pub fn get_logs() -> Vec<String> {
    LOG_BUFFER
        .lock()
        .map(|mut buffer| {
            let logs = buffer.clone();
            buffer.clear();
            logs
        })
        .unwrap_or_default()
}

// Retrieves and clears the callback data.
pub fn get_callback_data() -> Vec<String> {
    CALLBACK_BUFFER
        .lock()
        .map(|mut buffer| {
            let data = buffer.clone();
            buffer.clear();
            data
        })
        .unwrap_or_default()
}
