# Axioo QC System

![License](https://img.shields.io/badge/license-MIT-blue) ![Version](https://img.shields.io/badge/version-0.1.0-purple) ![Rust](https://img.shields.io/badge/Rust-1.83.0-orange?logo=rust) ![C++](https://img.shields.io/badge/C++-17-brightgreen?logo=c%2B%2B)

The Axioo QC System is designed to rapidly collect and transmit system information to a quality control (QC) website. This ensures that all devices meet the required standards before deployment, enhancing the quality control processes within Axioo's production environment.

## Project Structure

- **C++ Backend**: The core of the system is built in C++ to efficiently gather real-time system information. It uses [WMI (Windows Management Instrumentation)](https://docs.microsoft.com/en-us/windows/win32/wmisdk/wmi-start-page) for general system data and [DirectX](https://docs.microsoft.com/en-us/windows/win32/directx) for GPU details. The data is serialized into JSON format using the [nlohmann/json](https://github.com/nlohmann/json) library.
- **Rust Integration**: The Rust layer captures the JSON data from the C++ backend and facilitates real-time communication with the frontend via WebSockets, using [serde_json](https://crates.io/crates/serde_json) for JSON handling.

## Features

- **Real-time Data Collection**: Continuously gathers and updates system information, including CPU, memory, storage, network, and audio devices, providing up-to-date data for quality control.
- **Thread Safety**: Ensures safe concurrent operations, leveraging C++ and Rust's capabilities to handle multi-threading efficiently.
- **Optimal Resource Management**: Designed to run efficiently with minimal resource usage, thanks to the optimized integration of C++ and Rust, ensuring maximum performance with minimal overhead.
- **Portable and Installer Versions**: Offers both a portable version for quick deployment and an installer for permanent installations.
- **UPX Compression**: Provides a compressed version of the application for reduced storage and faster distribution.
- **Inno Setup Integration**: Generates a Windows installer using Inno Setup for easy installation and configuration.

## Environment Setup

### C++ Environment

The C++ backend of the Axioo QC System is built using CMake, a cross-platform build system generator. CMake is used to manage the build process in a compiler-independent manner. The project is configured to build a shared library (`systeminfo`) that collects system information using various Windows APIs.

#### Key Components in `CMakeLists.txt`:

- **C++ Standard**: The project is set to use C++17.
- **Shared Library**: The `systeminfo` library is built as a shared library, exporting all symbols on Windows.
- **Windows Libraries**: Links against Windows-specific libraries such as `pdh`, `iphlpapi`, `d3d11`, `dxgi`, `wbemuuid`, `ole32`, and `oleaut32` for accessing system information and DirectX for GPU details.
- **Installation Rules**: Specifies where to install the library and header files.
- **Output Directories**: Configures the output directories for binaries and libraries.

### Rust Environment

The Rust component of the Axioo QC System is responsible for capturing JSON data from the C++ backend and facilitating real-time communication with the frontend via WebSockets. The project uses several popular Rust libraries to achieve this functionality.

#### Key Libraries in `Cargo.toml`:

- **tokio**: An asynchronous runtime for the Rust programming language, used for handling asynchronous operations and networking.
- **serde & serde_json**: Libraries for serializing and deserializing data structures in Rust. `serde_json` is specifically used for handling JSON data.
- **anyhow & thiserror**: Libraries for error handling. `anyhow` provides a simple error handling framework, while `thiserror` is used for defining custom error types.
- **tokio-tungstenite**: A WebSocket client library built on top of `tokio`, used for real-time communication with the frontend.
- **libloading**: A library for loading shared libraries at runtime, used to interface with the C++ backend.
- **winapi**: Provides bindings to Windows API functions, used for various system-level operations.
- **crossterm**: A cross-platform terminal manipulation library, used for handling console input and output.

#### Build Dependencies:

- **bindgen**: A tool for generating Rust FFI bindings to C and C++ libraries, used to interface with the C++ backend.
- **winres**: A library for embedding Windows resources in Rust binaries, used for setting application metadata.

## Build and Installation

### Prerequisites

- [Rust](https://www.rust-lang.org/tools/install) - Ensure Rust is installed for building the project.
- [CMake](https://cmake.org/download/) - Required for building the C++ backend.
- [UPX](https://upx.github.io/) - Optional, for compressing the executable.
- [Inno Setup](https://jrsoftware.org/isinfo.php) - Optional, for building the Windows installer.

### Building the Project

1. **Build the C++ Backend**:
   - Create a build directory and navigate into it:
     ```bash
     mkdir build
     cd build
     ```
   - Run CMake to configure the project:
     ```bash
     cmake ..
     ```
   - Build the project in Release mode:
     ```bash
     cmake --build . --config Release
     ```

2. **Copy the DLL and LIB Files**:
   - Copy `systeminfo.dll` from `bin/release` and `systeminfo.lib` from `lib/release`.
   - Place them into the `dll` directory in the Rust project.

3. **Build the Rust Project**:
   - Navigate to the Rust project directory and run the build script:
     ```powershell
     .\build.ps1
     ```

4. The build artifacts will be located in the `release` directory.

## Packaging

- **Portable Version**: A ZIP file containing the executable and necessary DLLs.
- **UPX Compressed Version**: A smaller, compressed version of the portable package.
- **Installer**: A Windows installer created using Inno Setup.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
