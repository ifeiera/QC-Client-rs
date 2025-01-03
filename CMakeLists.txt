cmake_minimum_required(VERSION 3.10)
project(SystemInfo)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build shared library
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(BUILD_SHARED_LIBS ON)

# Source files
set(SOURCES
    src/SystemInfoCollector.cpp
    src/SystemInfoWrapper.cpp
    src/WMIHelper.cpp
    src/Logger.cpp
    src/systeminfo.rc
)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external
)

# Create shared library
add_library(systeminfo ${SOURCES})

# Add Windows version definition
target_compile_definitions(systeminfo PRIVATE
    WINVER=0x0601
    _WIN32_WINNT=0x0601
)

# Set DLL properties
set_target_properties(systeminfo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    VERSION 0.1.0
    SOVERSION 1
    DESCRIPTION "System Information Collection Library for Windows"
    COMPANY_NAME "Eterninety"
    PRODUCT_NAME "System Information Collector"
    LEGAL_COPYRIGHT "Copyright (c) 2024 Eterninety. All rights reserved."
    ORIGINAL_FILENAME "systeminfo.dll"
    FILE_DESCRIPTION "Collects detailed system hardware information including CPU, GPU, Memory, Storage, and Peripherals"
)

# Add post-build command to sign the DLL
if(WIN32)
    find_program(SIGNTOOL signtool.exe)
    if(SIGNTOOL)
        # Set certificate password directly (for development only)
        set(CERT_PASSWORD "12317D()/")
        
        # Check if certificate exists
        set(CERT_PATH "${CMAKE_SOURCE_DIR}/certs/EterninetyCert.pfx")
        if(NOT EXISTS ${CERT_PATH})
            message(WARNING "Certificate not found at: ${CERT_PATH}")
            return()
        endif()
        message(STATUS "Found certificate at: ${CERT_PATH}")
        
        # Add signing command with direct password
        add_custom_command(TARGET systeminfo POST_BUILD
            COMMAND ${SIGNTOOL} sign /f "${CERT_PATH}"
                                    /p "${CERT_PASSWORD}"
                                    /fd SHA256
                                    /t http://timestamp.digicert.com
                                    /d "System Information Collector"
                                    /du "https://eterninety.com"
                                    $<TARGET_FILE:systeminfo>
            COMMENT "Signing DLL with Eterninety certificate"
            VERBATIM
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
    endif()
endif()

# Windows specific libraries
if(WIN32)
    target_link_libraries(systeminfo
        pdh
        iphlpapi
        d3d11
        dxgi
        wbemuuid
        ole32
        oleaut32
        Dxva2
        setupapi
    )
endif()

# Installation rules
install(TARGETS systeminfo
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES
    include/SystemInfoCollector.hpp
    include/SystemInfoWrapper.h
    include/WMIHelper.hpp
    include/Logger.hpp
    DESTINATION include
)

# Enable testing
enable_testing()

# Output directory settings
set_target_properties(systeminfo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Debug information
message(STATUS "CMAKE_BINARY_DIR: ${CMAKE_BINARY_DIR}")
message(STATUS "CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}") 