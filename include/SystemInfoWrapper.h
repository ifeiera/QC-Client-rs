#ifndef SYSTEM_INFO_WRAPPER_H
#define SYSTEM_INFO_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

// DLL export macro for Windows compatibility
#ifdef _WIN32
    #define DLL_EXPORT __declspec(dllexport)
#else
    #define DLL_EXPORT
#endif

// Function pointer types for callbacks
typedef void (*SystemInfoCallback)(const char* json_data);  // Callback for system info updates
typedef void (*LogCallback)(const char* level, const char* message);  // Callback for logging events

// Core system information functions
DLL_EXPORT const char* GetSystemInfoJson(void);    // Get current system information as JSON
DLL_EXPORT void FreeSystemInfo(const char* ptr);   // Free memory allocated for system info
DLL_EXPORT void CleanupSystemInfo(void);          // Clean up system info resources
DLL_EXPORT void InitializeCache(void);            // Initialize the system info cache

// Error handling functions
DLL_EXPORT int GetSystemInfoLastError(void);           // Get last error code
DLL_EXPORT const char* GetSystemInfoErrorMessage(void);// Get last error message

// Callback registration functions
DLL_EXPORT void RegisterChangeCallback(SystemInfoCallback callback);    // Register for system info updates
DLL_EXPORT void UnregisterChangeCallback(void);                        // Unregister from updates
DLL_EXPORT void SetLogCallback(LogCallback callback);                  // Set logging callback
DLL_EXPORT void SetDebugMode(bool enabled);                           // Enable/disable debug mode

#ifdef __cplusplus
}
#endif

#endif // SYSTEM_INFO_WRAPPER_H 