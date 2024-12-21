// Required external crates and modules
use anyhow::Result;
use axioo_qc_client::*;
use chrono::Local;
use crossterm::event::{self, Event, KeyCode, KeyModifiers};
use futures_util::SinkExt;
use scopeguard::guard;
use std::io;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Arc;
use std::time::Duration;
use tokio::sync::Notify;
use tokio::{
    net::{TcpListener, TcpStream},
    time::sleep,
};
use tokio_tungstenite::{accept_async, tungstenite::Message, WebSocketStream};

// Track number of active WebSocket connections
static ACTIVE_CONNECTIONS: AtomicUsize = AtomicUsize::new(0);

// Main WebSocket server implementation
async fn run_server(shutdown: Arc<Notify>) -> Result<()> {
    let addr = "127.0.0.1:8765";
    let listener = TcpListener::bind(addr).await?;
    println!("Server running in background on: {}", addr);

    // Get executable directory for shutdown file
    let exe_dir = std::env::current_exe()?
        .parent()
        .ok_or_else(|| anyhow::anyhow!("Failed to get executable directory"))?
        .to_path_buf();
    let shutdown_file = exe_dir.join("shutdown.trigger");

    // Spawn a task to handle shutdown triggers
    let shutdown_signal = shutdown.clone();
    tokio::spawn(async move {
        loop {
            // Check for keyboard shortcut if not in background mode
            if !BACKGROUND_MODE {
                if event::poll(std::time::Duration::from_millis(100)).unwrap_or(false) {
                    if let Ok(Event::Key(key)) = event::read() {
                        if key.code == KeyCode::Char('X')
                            && key.modifiers.contains(KeyModifiers::CONTROL)
                            && key.modifiers.contains(KeyModifiers::SHIFT)
                        {
                            shutdown_signal.notify_one();
                            break;
                        }
                    }
                }
            }

            // Check for shutdown file
            if shutdown_file.exists() {
                if let Err(e) = std::fs::remove_file(&shutdown_file) {
                    eprintln!("Failed to remove shutdown file: {}", e);
                }
                shutdown_signal.notify_one();
                break;
            }

            sleep(Duration::from_millis(100)).await;
        }
    });

    // Main server loop - accepts connections and handles shutdown signal
    loop {
        tokio::select! {
            accept_result = listener.accept() => {
                if let Ok((stream, addr)) = accept_result {
                    tokio::spawn(async move {
                        match accept_async(stream).await {
                            Ok(ws_stream) => {
                                if let Err(e) = handle_connection(ws_stream, addr).await {
                                    eprintln!("Error in connection handler: {}", e);
                                }
                            }
                            Err(e) => eprintln!("Error accepting connection from {}: {}", addr, e),
                        }
                    });
                }
            }
            _ = shutdown.notified() => {
                println!("Shutdown signal received (Ctrl+Shift+X)");
                break;
            }
        }
    }
    Ok(())
}

// Handles individual WebSocket client connections
async fn handle_connection(
    mut ws_stream: WebSocketStream<TcpStream>,
    addr: std::net::SocketAddr,
) -> io::Result<()> {
    ACTIVE_CONNECTIONS.fetch_add(1, Ordering::SeqCst);
    println!(
        "{} New client connected: {} (Total: {})",
        Local::now().format("%H:%M:%S"),
        addr,
        ACTIVE_CONNECTIONS.load(Ordering::SeqCst)
    );

    // Main connection loop - sends system information to client
    loop {
        if DEBUG_MODE {
            for log in get_logs() {
                println!("{}", log);
            }
        }

        match get_system_info() {
            Ok(info) => {
                if let Err(e) = ws_stream.send(Message::Text(info)).await {
                    let error_string = e.to_string();
                    // Handle client disconnection
                    if error_string.contains("10053")
                        || error_string.contains("10054")
                        || error_string.contains("broken pipe")
                    {
                        ACTIVE_CONNECTIONS.fetch_sub(1, Ordering::SeqCst);
                        println!(
                            "{} Client {} disconnected (Total: {})",
                            Local::now().format("%H:%M:%S"),
                            addr,
                            ACTIVE_CONNECTIONS.load(Ordering::SeqCst)
                        );
                        break;
                    } else {
                        eprintln!("{} Error: {}", Local::now().format("%H:%M:%S"), e);
                    }
                }
            }
            Err(_) => {
                eprintln!(
                    "{} Error: {} - {}",
                    Local::now().format("%H:%M:%S"),
                    get_last_error(),
                    get_error_message()
                );
            }
        }

        sleep(Duration::from_millis(1000)).await;
    }
    Ok(())
}

// Windows-specific: Verify required DLL files exist
#[cfg(windows)]
fn ensure_dependencies() -> io::Result<()> {
    let exe_dir = std::env::current_exe()?
        .parent()
        .ok_or_else(|| io::Error::new(io::ErrorKind::NotFound, "Cannot get executable directory"))?
        .to_path_buf();

    let required_files = ["systeminfo.dll"];

    for file in required_files.iter() {
        let file_path = exe_dir.join(file);
        if !file_path.exists() {
            return Err(io::Error::new(
                io::ErrorKind::NotFound,
                format!("Required file not found: {}", file),
            ));
        }
    }

    Ok(())
}

// Application entry point
#[tokio::main]
async fn main() -> Result<()> {
    // Check for required files on Windows
    #[cfg(windows)]
    ensure_dependencies()?;

    println!("Initializing Axioo QC System...");
    init_library()?;
    register_callback()?;

    crossterm::terminal::enable_raw_mode()?;

    let shutdown = Arc::new(Notify::new());

    // Setup cleanup handler for graceful shutdown
    let _cleanup = guard((), |_| {
        println!("Performing cleanup...");
        let _ = crossterm::terminal::disable_raw_mode();
        cleanup_system_info();
        println!("Cleanup completed");
    });

    println!("Starting server in background...");
    println!("To stop the server:");
    println!("1. Use Task Manager to end the process");
    if BACKGROUND_MODE {
        println!("2. Create 'shutdown.trigger' file in application directory");
    } else {
        println!("2. Press Ctrl+Shift+X to exit");
    }

    // Hide console window on Windows based on BACKGROUND_MODE setting
    #[cfg(windows)]
    unsafe {
        use winapi::um::wincon::GetConsoleWindow;
        use winapi::um::winuser::{ShowWindow, SW_HIDE};
        let window = GetConsoleWindow();
        if !window.is_null() && BACKGROUND_MODE {
            ShowWindow(window, SW_HIDE);
        }
    }

    run_server(shutdown).await?;
    Ok(())
}
