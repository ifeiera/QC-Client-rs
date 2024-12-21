#include "SystemInfoWrapper.h"
#include "SystemInfoCollector.hpp"
#include <cstring>
#include <string>
#include <mutex>
#include <atomic>
#include <iostream>

// Global variables for error handling and logging
std::mutex g_logMutex;
LogCallback g_logCallback = nullptr;
std::string g_lastError;
std::atomic<int> g_errorCode{0};

// Debug mode control flag
static std::atomic<bool> g_debugMode{false};

namespace {
    // Mutex and callback for system information updates
    std::mutex g_callbackMutex;
    SystemInfoCallback g_callback = nullptr;

    // Allocate memory for string and handle errors
    char* allocateString(const std::string& str) {
        try {
            char* result = new char[str.length() + 1];
            #ifdef _WIN32
                strcpy_s(result, str.length() + 1, str.c_str());
            #else
                strcpy(result, str.c_str());
            #endif
            return result;
        } catch (...) {
            g_errorCode = 1;
            g_lastError = "Memory allocation failed";
            return nullptr;
        }
    }

    // Thread-safe logging function
    void log(const char* level, const std::string& message) {
        if (!g_debugMode) {
            return;  // Skip logging if debug mode is off
        }
        std::lock_guard<std::mutex> lock(g_logMutex);
        if (g_logCallback) {
            g_logCallback(level, message.c_str());
        }
    }
}

extern "C" {
    // Get system information as JSON string
    DLL_EXPORT const char* GetSystemInfoJson() {
        try {
            json info = SystemInfoCollector::getSystemInfo();
            return allocateString(info.dump());
        } catch (const std::exception& e) {
            g_errorCode = 2;
            g_lastError = e.what();
            return nullptr;
        } catch (...) {
            g_errorCode = 3;
            g_lastError = "Unknown error in GetSystemInfoJson";
            return nullptr;
        }
    }

    // Free memory allocated for system information string
    DLL_EXPORT void FreeSystemInfo(const char* ptr) {
        try {
            delete[] ptr;
        } catch (...) {
            g_errorCode = 6;
            g_lastError = "Error freeing memory";
        }
    }

    // Get last error code from operations
    DLL_EXPORT int GetSystemInfoLastError() {
        return g_errorCode;
    }

    // Get last error message
    DLL_EXPORT const char* GetSystemInfoErrorMessage() {
        return g_lastError.c_str();
    }

    // Register callback for system information updates
    DLL_EXPORT void RegisterChangeCallback(SystemInfoCallback callback) {
        std::lock_guard<std::mutex> lock(g_callbackMutex);
        g_callback = callback;
        
        // Send immediate update if callback is registered
        if (callback) {
            try {
                json info = SystemInfoCollector::getSystemInfo();
                std::string jsonStr = info.dump();
                callback(jsonStr.c_str());
            } catch (...) {
                // Ignore errors in callback
            }
        }
    }

    // Unregister system information update callback
    DLL_EXPORT void UnregisterChangeCallback() {
        std::lock_guard<std::mutex> lock(g_callbackMutex);
        g_callback = nullptr;
    }

    // Clean up system information resources
    DLL_EXPORT void CleanupSystemInfo() {
        std::cout << "Cleaning up SystemInfo..." << std::endl;
        SystemInfoCollector::cleanup();
        std::cout << "Cleanup complete" << std::endl;
    }

    // Set callback for logging events
    DLL_EXPORT void SetLogCallback(LogCallback callback) {
        std::lock_guard<std::mutex> lock(g_logMutex);
        g_logCallback = callback;
    }

    // Initialize system information cache
    DLL_EXPORT void InitializeCache() {
        try {
            SystemInfoCollector::initializeCache();
        } catch (const std::exception& e) {
            g_errorCode = 7;
            g_lastError = std::string("Failed to initialize cache: ") + e.what();
        }
    }

    // Enable or disable debug mode
    DLL_EXPORT void SetDebugMode(bool enabled) {
        g_debugMode = enabled;
        std::cout << "Debug mode set to: " << (enabled ? "true" : "false") << std::endl;
    }
}
 