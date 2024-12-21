#pragma once
#include <json/single_include/nlohmann/json.hpp>
#include <mutex>
#include <chrono>
#include <string>
#include <windows.h>
#include <pdh.h>
#include <d3d11.h>
#include <dxgi.h>
#include <thread>
#include <atomic>
#include "WMIHelper.hpp"
#include "Logger.hpp"
#include "SystemInfoWrapper.h"

using json = nlohmann::json;

// Class responsible for collecting and caching system hardware information
class SystemInfoCollector {
private:
    // Mutex and cache variables for thread-safe operations
    static std::mutex cacheMutex;
    static json cachedStaticInfo;     // Cache for rarely changing data (CPU, GPU, etc.)
    static json cachedDynamicInfo;    // Cache for frequently changing data (memory usage, temps)
    static std::chrono::steady_clock::time_point lastStaticUpdate;
    static std::chrono::steady_clock::time_point lastDynamicUpdate;
    
    // Cache duration constants
    static const int STATIC_CACHE_DURATION_SEC = 60;           // Update static info every 60 seconds
    static constexpr double DYNAMIC_CACHE_DURATION_SEC = 0.1;  // Update dynamic info every 100ms

    // Thread control variables
    static std::atomic<bool> running;
    static std::thread updateThread;
    
    // Background update methods
    static void updateDynamicDataThread();  // Continuous background update thread
    static void updateFastData();           // Update frequently changing data
    static void updateSlowData();           // Update rarely changing data

    // Hardware information collection methods
    static std::string getDeviceId();       // Get unique device identifier
    static std::string getDeviceName();     // Get system name
    static json getMotherboardInfo();       // Get motherboard details
    static json getCPUInfo();               // Get processor information
    static json getGPUInfo();               // Get graphics card details
    static json getMemoryInfo();            // Get RAM information
    static json getStorageInfo();           // Get storage devices info
    static json getNetworkInfo();           // Get network adapters info
    static json getAudioInfo();             // Get audio devices info
    static json getBatteryInfo();           // Get battery status (if applicable)
    static void logError(const char* function, const std::exception& e);  // Error logging helper

public:
    // Main interface methods
    static json getSystemInfo();      // Get complete system information
    static void initializeCache();    // Initialize the cache system
    static void cleanup();            // Clean up resources

    // Friend functions for external C interface
    friend const char* GetStaticSystemInfo();     // Get cached static system info
    friend const char* GetDynamicSystemInfo();    // Get cached dynamic system info
}; 