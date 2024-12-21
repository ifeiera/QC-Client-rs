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
)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external
)

# Create shared library
add_library(systeminfo ${SOURCES})

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