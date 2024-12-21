#include "SystemInfoCollector.hpp"
#include <iomanip>
#include <sstream>
#include <map>
#include <algorithm>
#include <iphlpapi.h>

// Global variables
extern std::mutex g_logMutex;
extern LogCallback g_logCallback;

namespace {
    // Helper functions for string sanitization and WMI operations
    std::string sanitizeString(const std::string& input) {
        if (input.empty()) return "N/A";
        
        std::string output;
        output.reserve(input.length());
        
        // Only keep printable ASCII and valid UTF-8 characters
        for (unsigned char c : input) {
            if ((c >= 0x20 && c <= 0x7E) ||  // ASCII printable
                (c >= 0xC2 && c <= 0xF4)) {   // Valid UTF-8 lead bytes
                output += c;
            } else {
                output += ' '; // Replace invalid chars with space
            }
        }
        
        // Trim leading and trailing whitespace
        auto start = output.find_first_not_of(" \t\r\n");
        auto end = output.find_last_not_of(" \t\r\n");
        
        if (start == std::string::npos) return "N/A";
        return output.substr(start, end - start + 1);
    }

    // Convert WMI VARIANT to safe string, handling null cases
    std::string safeWMIString(const VARIANT& vtProp) {
        if (vtProp.vt == VT_NULL || vtProp.vt == VT_EMPTY) {
            return "N/A";
        }
        try {
            return sanitizeString(std::string(_bstr_t(vtProp.bstrVal)));
        } catch (...) {
            return "N/A";
        }
    }

    // Thread-safe logging function for system information operations
    void systemLog(const char* level, const std::string& message) {
        std::lock_guard<std::mutex> lock(g_logMutex);
        if (g_logCallback) {
            g_logCallback(level, message.c_str());
        }
    }

    // Format string with different types of values
    std::string formatString(const std::string& base, const std::string& value) {
        return base + value;
    }

    std::string formatString(const std::string& base, int64_t value) {
        return base + std::to_string(value);
    }

    std::string formatString(const std::string& base, size_t value) {
        return base + std::to_string(value);
    }

    // Format value with unit (e.g., "500 MB", "2.5 GB")
    std::string formatWithUnit(const std::string& base, int64_t value, const std::string& unit) {
        return formatString(base, value) + unit;
    }

    std::string formatWithUnit(const std::string& base, size_t value, const std::string& unit) {
        return formatString(base, value) + unit;
    }
}

// Initialize static members for system information cache and thread control
std::mutex SystemInfoCollector::cacheMutex;
json SystemInfoCollector::cachedStaticInfo;
json SystemInfoCollector::cachedDynamicInfo;
std::chrono::steady_clock::time_point SystemInfoCollector::lastStaticUpdate;
std::chrono::steady_clock::time_point SystemInfoCollector::lastDynamicUpdate;
std::atomic<bool> SystemInfoCollector::running{true};
std::thread SystemInfoCollector::updateThread;

// Persistent WMI Connection management class
class WMIConnection {
    static IWbemServices* pSvc;
    static bool initialized;
    static std::mutex initMutex;

public:
    // Get WMI service instance with lazy initialization
    static IWbemServices* getService() {
        if (!initialized) {
            std::lock_guard<std::mutex> lock(initMutex);
            if (!initialized) {
                initialize();
            }
        }
        return pSvc;
    }

private:
    // Initialize WMI service connection
    static void initialize() {
        if (SUCCEEDED(WMIHelper::initialize(nullptr, &pSvc))) {
            initialized = true;
        }
    }
};

// Initialize static members of WMIConnection
IWbemServices* WMIConnection::pSvc = nullptr;
bool WMIConnection::initialized = false;
std::mutex WMIConnection::initMutex;

// Update frequently changing system information
void SystemInfoCollector::updateFastData() {
    cachedDynamicInfo["storage"] = getStorageInfo();
    cachedDynamicInfo["battery"] = getBatteryInfo();
    cachedDynamicInfo["network"] = getNetworkInfo();
}

// Update system information that changes less frequently
void SystemInfoCollector::updateSlowData() {
    static auto lastSlowUpdate = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    
    if (std::chrono::duration_cast<std::chrono::seconds>(now - lastSlowUpdate).count() >= 1) {
        cachedDynamicInfo["cpu"] = getCPUInfo();
        cachedDynamicInfo["memory"] = getMemoryInfo();
        lastSlowUpdate = now;
    }
}

// Background thread for updating dynamic system information
void SystemInfoCollector::updateDynamicDataThread() {
    systemLog("INFO", std::string("Dynamic update thread started"));
    while (running) {
        try {
            {
                std::lock_guard<std::mutex> lock(cacheMutex);
                systemLog("INFO", std::string("=== Updating Dynamic Data ==="));
                
                // Update and time fast-changing data
                auto start = std::chrono::steady_clock::now();
                updateFastData();
                auto fastTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start).count();
                systemLog("INFO", formatWithUnit("Fast data updated in ", fastTime, "ms"));
                
                // Update and time slower-changing data
                start = std::chrono::steady_clock::now();
                updateSlowData();
                auto slowTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start).count();
                systemLog("INFO", formatWithUnit("Slow data updated in ", slowTime, "ms"));

                // Log cache statistics
                systemLog("INFO", std::string("Cache sizes:"));
                systemLog("INFO", formatWithUnit("- Dynamic: ", cachedDynamicInfo.dump().length(), " bytes"));
                systemLog("INFO", formatWithUnit("- Static: ", cachedStaticInfo.dump().length(), " bytes"));
                
                // Verify data completeness
                systemLog("INFO", std::string("\nVerifying cached data:"));
                systemLog("INFO", std::string("- Storage: ") + 
                    (cachedDynamicInfo.contains("storage") ? "Present" : "Missing"));
                systemLog("INFO", std::string("- Battery: ") + 
                    (cachedDynamicInfo.contains("battery") ? "Present" : "Missing"));
                systemLog("INFO", std::string("- Network: ") + 
                    (cachedDynamicInfo.contains("network") ? "Present" : "Missing"));
                systemLog("INFO", std::string("- CPU: ") + 
                    (cachedDynamicInfo.contains("cpu") ? "Present" : "Missing"));
                systemLog("INFO", std::string("- Memory: ") + 
                    (cachedDynamicInfo.contains("memory") ? "Present" : "Missing"));
                systemLog("INFO", std::string("=========================="));
            }
        }
        catch (const std::exception& e) {
            systemLog("ERROR", std::string("Error in update thread: ") + e.what());
        }
        catch (...) {
            systemLog("ERROR", std::string("Unknown error in update thread"));
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    systemLog("INFO", std::string("Dynamic update thread stopped"));
}

// Initialize system information cache and start update thread
void SystemInfoCollector::initializeCache() {
    try {
        std::lock_guard<std::mutex> lock(cacheMutex);
        systemLog("INFO", std::string("\n=== Initializing Cache ==="));

        // Initialize static system information
        systemLog("INFO", std::string("Initializing static data..."));
        cachedStaticInfo["deviceId"] = getDeviceId();
        cachedStaticInfo["deviceName"] = getDeviceName();
        cachedStaticInfo["motherboard"] = getMotherboardInfo();
        cachedStaticInfo["gpu"] = getGPUInfo();
        cachedStaticInfo["audio"] = getAudioInfo();
        lastStaticUpdate = std::chrono::steady_clock::now();

        // Initialize dynamic system information
        systemLog("INFO", std::string("Initializing dynamic data..."));
        updateFastData();
        updateSlowData();
        lastDynamicUpdate = std::chrono::steady_clock::now();

        // Start background update thread
        systemLog("INFO", std::string("Cache initialized successfully"));
        systemLog("INFO", std::string("Starting update thread..."));
        updateThread = std::thread(updateDynamicDataThread);
        systemLog("INFO", std::string("=========================="));
    }
    catch (const std::exception& e) {
        systemLog("ERROR", std::string("Error in initializeCache: ") + e.what());
        throw;
    }
}

// Clean up resources and stop update thread
void SystemInfoCollector::cleanup() {
    try {
        systemLog("INFO", std::string("\n=== Cleaning Up ==="));
        running = false;
        systemLog("INFO", std::string("Stopping update thread..."));
        if (updateThread.joinable()) {
            updateThread.join();
        }
        systemLog("INFO", std::string("Update thread stopped"));
        systemLog("INFO", std::string("Cleanup complete"));
        systemLog("INFO", std::string("================="));
    }
    catch (const std::exception& e) {
        systemLog("ERROR", std::string("Error in cleanup: ") + e.what());
    }
}

// Get complete system information, updating cache if needed
json SystemInfoCollector::getSystemInfo() {
    json info;
    
    try {
        std::lock_guard<std::mutex> lock(cacheMutex);
        
        systemLog("INFO", std::string("\n=== Getting System Info ==="));
        
        // Update static data if cache expired
        auto now = std::chrono::steady_clock::now();
        if (cachedStaticInfo.empty() ||
            std::chrono::duration_cast<std::chrono::seconds>(
                now - lastStaticUpdate)
                    .count() >= STATIC_CACHE_DURATION_SEC)
        {
            systemLog("INFO", std::string("Updating static data..."));
            cachedStaticInfo["deviceId"] = getDeviceId();
            cachedStaticInfo["deviceName"] = getDeviceName();
            cachedStaticInfo["motherboard"] = getMotherboardInfo();
            cachedStaticInfo["gpu"] = getGPUInfo();
            cachedStaticInfo["audio"] = getAudioInfo();
            lastStaticUpdate = now;
        }

        // Combine static and dynamic data
        systemLog("INFO", std::string("Merging data..."));
        info.merge_patch(cachedDynamicInfo);
        info.merge_patch(cachedStaticInfo);
        
        // Log data verification
        systemLog("INFO", std::string("\nVerifying final data:"));
        systemLog("INFO", std::string("- Static fields: ") + std::to_string(info.size()));
        systemLog("INFO", std::string("- Dynamic fields present: ") 
                  + std::to_string(info.contains("cpu") && info.contains("memory") && 
                      info.contains("storage") && info.contains("battery") && 
                      info.contains("network")));
        systemLog("INFO", std::string("Total data size: ") + std::to_string(info.dump().length()) + " bytes");
        systemLog("INFO", std::string("=========================="));
    }
    catch (const std::exception& e) {
        systemLog("ERROR", std::string("Error in getSystemInfo: ") + e.what());
        throw;
    }

    return info;
}

// Generate unique device ID based on hardware information
std::string SystemInfoCollector::getDeviceId() {
    try {
        std::string baseInfo;

        // Query motherboard serial number through WMI
        WMISession wmiSession;
        IWbemServices *pSvc = wmiSession.getServices();
        if (pSvc) {
            IEnumWbemClassObject *pEnumerator = NULL;
            if (SUCCEEDED(pSvc->ExecQuery(
                    bstr_t("WQL"),
                    bstr_t("SELECT * FROM Win32_BaseBoard"),
                    WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                    NULL,
                    &pEnumerator))) {

                IWbemClassObject *pclsObj = NULL;
                ULONG uReturn = 0;
                if (SUCCEEDED(pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn)) && uReturn != 0) {
                    VARIANT vtProp;
                    if (SUCCEEDED(pclsObj->Get(L"SerialNumber", 0, &vtProp, 0, 0))) {
                        baseInfo += _bstr_t(vtProp.bstrVal);
                        VariantClear(&vtProp);
                    }
                    pclsObj->Release();
                }
                pEnumerator->Release();
            }
        }

        // Add computer name to base info
        char computerName[MAX_COMPUTERNAME_LENGTH + 1];
        DWORD size = sizeof(computerName);
        if (GetComputerNameA(computerName, &size)) {
            baseInfo += computerName;
        }

        // Add CPU information to make ID more unique
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        baseInfo += std::to_string(sysInfo.dwProcessorType);
        baseInfo += std::to_string(sysInfo.dwNumberOfProcessors);

        // Generate UUID-like hash from collected information
        std::size_t hash = std::hash<std::string>{}(baseInfo);
        std::stringstream ss;
        ss << std::hex << std::setfill('0')
           << std::setw(8) << (hash & 0xFFFFFFFF) << "-"
           << std::setw(4) << ((hash >> 16) & 0xFFFF) << "-"
           << std::setw(4) << ((hash >> 32) & 0xFFFF) << "-"
           << std::setw(4) << ((hash >> 48) & 0xFFFF) << "-"
           << std::setw(12) << (hash & 0xFFFFFFFFFFFF);

        return ss.str();
    }
    catch (const std::exception &e) {
        logError("getDeviceId", e);
        // Fallback to computer name if UUID generation fails
        char computerName[MAX_COMPUTERNAME_LENGTH + 1];
        DWORD size = sizeof(computerName);
        GetComputerNameA(computerName, &size);
        return std::string(computerName);
    }
}

// Get system's computer name
std::string SystemInfoCollector::getDeviceName() {
    char computerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(computerName);
    if (GetComputerNameA(computerName, &size)) {
        return sanitizeString(std::string(computerName));
    }
    return "Unknown";
}

// Log error with component and exception details
void SystemInfoCollector::logError(const char* function, const std::exception& e) {
    Logger::error("SystemInfo", std::string(function) + ": " + e.what());
}

// Collect motherboard and BIOS information through WMI
json SystemInfoCollector::getMotherboardInfo() {
    // Initialize default values
    json board_info = {
        {"product_name", "N/A"},
        {"manufacturer", "N/A"},
        {"bios_version", "N/A"},
        {"bios_serial", "N/A"},
        {"board_serial", "N/A"}
    };

    try {
        WMISession wmiSession;
        IWbemServices *pSvc = wmiSession.getServices();
        HRESULT hr;
        if (!pSvc) return board_info;

        // Query baseboard (motherboard) information
        IEnumWbemClassObject *pEnumerator = NULL;
        hr = pSvc->ExecQuery(
            bstr_t("WQL"),
            bstr_t("SELECT * FROM Win32_BaseBoard"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            NULL,
            &pEnumerator);

        if (SUCCEEDED(hr)) {
            IWbemClassObject *pclsObj = NULL;
            ULONG uReturn = 0;

            if (SUCCEEDED(pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn)) && uReturn != 0) {
                VARIANT vtProp;

                // Get motherboard product name
                if (SUCCEEDED(pclsObj->Get(L"Product", 0, &vtProp, 0, 0)) && vtProp.vt != VT_NULL) {
                    board_info["product_name"] = safeWMIString(vtProp);
                }
                VariantClear(&vtProp);

                // Get motherboard manufacturer
                if (SUCCEEDED(pclsObj->Get(L"Manufacturer", 0, &vtProp, 0, 0)) && vtProp.vt != VT_NULL) {
                    board_info["manufacturer"] = safeWMIString(vtProp);
                }
                VariantClear(&vtProp);

                // Get motherboard serial number
                if (SUCCEEDED(pclsObj->Get(L"SerialNumber", 0, &vtProp, 0, 0)) && vtProp.vt != VT_NULL) {
                    board_info["board_serial"] = safeWMIString(vtProp);
                }
                VariantClear(&vtProp);

                pclsObj->Release();
            }
            pEnumerator->Release();
        }

        // Query BIOS information
        hr = pSvc->ExecQuery(
            bstr_t("WQL"),
            bstr_t("SELECT * FROM Win32_BIOS"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            NULL,
            &pEnumerator);

        if (SUCCEEDED(hr)) {
            IWbemClassObject *pclsObj = NULL;
            ULONG uReturn = 0;

            if (SUCCEEDED(pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn)) && uReturn != 0) {
                VARIANT vtProp;

                // Get BIOS version
                if (SUCCEEDED(pclsObj->Get(L"SMBIOSBIOSVersion", 0, &vtProp, 0, 0)) && vtProp.vt != VT_NULL) {
                    board_info["bios_version"] = safeWMIString(vtProp);
                }
                VariantClear(&vtProp);

                // Get BIOS serial number
                if (SUCCEEDED(pclsObj->Get(L"SerialNumber", 0, &vtProp, 0, 0)) && vtProp.vt != VT_NULL) {
                    board_info["bios_serial"] = safeWMIString(vtProp);
                }
                VariantClear(&vtProp);

                pclsObj->Release();
            }
            pEnumerator->Release();
        }
    }
    catch (const std::exception &e) {
        logError("getMotherboardInfo", e);
    }

    return board_info;
}

// Collect CPU information including usage statistics
json SystemInfoCollector::getCPUInfo() {
    json cpu_array = json::array();
    try {
        WMISession wmiSession;
        IWbemServices *pSvc = wmiSession.getServices();
        HRESULT hr;
        if (!pSvc)
            return cpu_array;

        // Query processor information through WMI
        IEnumWbemClassObject *pEnumerator = NULL;
        hr = pSvc->ExecQuery(
            bstr_t("WQL"),
            bstr_t("SELECT * FROM Win32_Processor"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            NULL,
            &pEnumerator);

        if (SUCCEEDED(hr)) {
            IWbemClassObject *pclsObj = NULL;
            ULONG uReturn = 0;

            while (SUCCEEDED(pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn)) && uReturn != 0) {
                json cpu_info;
                VARIANT vtProp;

                // Get processor name/model
                if (SUCCEEDED(pclsObj->Get(L"Name", 0, &vtProp, 0, 0)) && vtProp.vt != VT_NULL) {
                    cpu_info["name"] = safeWMIString(vtProp);
                }
                VariantClear(&vtProp);

                // Get number of physical cores
                if (SUCCEEDED(pclsObj->Get(L"NumberOfCores", 0, &vtProp, 0, 0)) && vtProp.vt != VT_NULL) {
                    cpu_info["cores"] = vtProp.uintVal;
                } else {
                    cpu_info["cores"] = 0;
                }
                VariantClear(&vtProp);

                // Get number of logical processors (threads)
                if (SUCCEEDED(pclsObj->Get(L"NumberOfLogicalProcessors", 0, &vtProp, 0, 0)) && vtProp.vt != VT_NULL) {
                    cpu_info["threads"] = vtProp.uintVal;
                } else {
                    cpu_info["threads"] = 0;
                }
                VariantClear(&vtProp);

                // Get maximum clock speed
                if (SUCCEEDED(pclsObj->Get(L"MaxClockSpeed", 0, &vtProp, 0, 0)) && vtProp.vt != VT_NULL) {
                    cpu_info["clock_speed"] = std::to_string(vtProp.uintVal) + " MHz";
                } else {
                    cpu_info["clock_speed"] = "0 MHz";
                }
                VariantClear(&vtProp);

                // Get current CPU usage using Performance Data Helper (PDH)
                PDH_HQUERY cpuQuery;
                PDH_HCOUNTER cpuTotal;
                PdhOpenQuery(NULL, NULL, &cpuQuery);
                PdhAddEnglishCounterA(cpuQuery, "\\Processor(_Total)\\% Processor Time", NULL, &cpuTotal);
                PdhCollectQueryData(cpuQuery);
                Sleep(100);  // Wait for next sample
                PdhCollectQueryData(cpuQuery);
                PDH_FMT_COUNTERVALUE counterVal;
                PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, NULL, &counterVal);
                std::stringstream ss;
                ss << std::fixed << std::setprecision(2) << counterVal.doubleValue;
                cpu_info["usage"] = std::stod(ss.str());  // Store as double with 2 decimal places
                PdhCloseQuery(cpuQuery);

                cpu_array.push_back(cpu_info);
                pclsObj->Release();
            }
            pEnumerator->Release();
        }
    }
    catch (const std::exception &e) {
        logError("getCPUInfo", e);
    }
    return cpu_array;
}

// Collect GPU information including VRAM details
json SystemInfoCollector::getGPUInfo() {
    json gpu_array = json::array();
    try {
        WMISession wmiSession;
        IWbemServices *pSvc = wmiSession.getServices();
        HRESULT hr;
        if (!pSvc)
            return gpu_array;

        // Query video controller information through WMI
        IEnumWbemClassObject *pEnumerator = NULL;
        hr = pSvc->ExecQuery(
            bstr_t("WQL"),
            bstr_t("SELECT * FROM Win32_VideoController"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            NULL,
            &pEnumerator);

        if (SUCCEEDED(hr)) {
            IWbemClassObject *pclsObj = NULL;
            ULONG uReturn = 0;

            while (SUCCEEDED(pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn)) && uReturn != 0) {
                json gpu_info;
                VARIANT vtProp;

                // Get GPU name/model
                std::wstring gpuName;
                if (SUCCEEDED(pclsObj->Get(L"Name", 0, &vtProp, 0, 0)) && vtProp.vt != VT_NULL) {
                    gpuName = vtProp.bstrVal;
                    gpu_info["name"] = safeWMIString(vtProp);
                } else {
                    gpu_info["name"] = "N/A";
                }
                VariantClear(&vtProp);

                // Get GPU driver version
                if (SUCCEEDED(pclsObj->Get(L"DriverVersion", 0, &vtProp, 0, 0)) && vtProp.vt != VT_NULL) {
                    gpu_info["driver_version"] = safeWMIString(vtProp);
                } else {
                    gpu_info["driver_version"] = "N/A";
                }
                VariantClear(&vtProp);

                // Get VRAM information using DXGI
                gpu_info["vram_total"] = "N/A";
                IDXGIFactory *pFactory = nullptr;
                if (SUCCEEDED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void **)&pFactory))) {
                    IDXGIAdapter *pAdapter = nullptr;
                    // Enumerate through all DXGI adapters
                    for (UINT i = 0; pFactory->EnumAdapters(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
                        DXGI_ADAPTER_DESC adapterDesc;
                        if (SUCCEEDED(pAdapter->GetDesc(&adapterDesc))) {
                            std::wstring descStr = adapterDesc.Description;

                            // Case insensitive comparison of GPU names
                            std::wstring lowerDesc = descStr;
                            std::wstring lowerName = gpuName;
                            std::transform(lowerDesc.begin(), lowerDesc.end(), lowerDesc.begin(), ::tolower);
                            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

                            // Match GPU name to get correct VRAM
                            if (lowerDesc.find(lowerName) != std::wstring::npos ||
                                lowerName.find(lowerDesc) != std::wstring::npos) {
                                // Convert VRAM to GB with 2 decimal precision
                                double vramGB = static_cast<double>(adapterDesc.DedicatedVideoMemory) /
                                                (1024.0 * 1024.0 * 1024.0);
                                std::stringstream ss;
                                ss << std::fixed << std::setprecision(2) << vramGB;
                                gpu_info["vram_total"] = ss.str();
                                break;
                            }
                        }
                        pAdapter->Release();
                    }
                    pFactory->Release();
                }

                gpu_array.push_back(gpu_info);
                pclsObj->Release();
            }
            pEnumerator->Release();
        }
    }
    catch (const std::exception &e) {
        logError("getGPUInfo", e);
    }
    return gpu_array;
}

// Collect memory information including RAM usage and details
json SystemInfoCollector::getMemoryInfo() {
    json memory_info;
    try {
        // Get system memory status
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        GlobalMemoryStatusEx(&memInfo);

        // Calculate memory values in GB with 2 decimal precision
        memory_info["total"] = round(static_cast<double>(memInfo.ullTotalPhys) / (1024.0 * 1024.0 * 1024.0) * 100) / 100.0;
        memory_info["available"] = round(static_cast<double>(memInfo.ullAvailPhys) / (1024.0 * 1024.0 * 1024.0) * 100) / 100.0;
        memory_info["used"] = round((static_cast<double>(memInfo.ullTotalPhys - memInfo.ullAvailPhys) / (1024.0 * 1024.0 * 1024.0)) * 100) / 100.0;
        memory_info["percent"] = memInfo.dwMemoryLoad;

        // Get detailed memory slot information
        json memory_slots = json::array();
        double total_capacity = 0;

        WMISession wmiSession;
        IWbemServices *pSvc = wmiSession.getServices();
        HRESULT hr;
        if (!pSvc)
            return memory_info;

        // Query physical memory details through WMI
        IEnumWbemClassObject *pEnumerator = NULL;
        hr = pSvc->ExecQuery(
            bstr_t("WQL"),
            bstr_t("SELECT * FROM Win32_PhysicalMemory"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            NULL,
            &pEnumerator);

        if (SUCCEEDED(hr)) {
            IWbemClassObject *pclsObj = NULL;
            ULONG uReturn = 0;

            while (SUCCEEDED(pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn)) && uReturn != 0) {
                json slot_info;
                VARIANT vtProp;

                // Get RAM module capacity
                if (SUCCEEDED(pclsObj->Get(L"Capacity", 0, &vtProp, 0, 0)) && vtProp.vt != VT_NULL) {
                    UINT64 capacity = _wtoi64(vtProp.bstrVal);
                    double capacityGB = static_cast<double>(capacity) / (1024.0 * 1024.0 * 1024.0);
                    total_capacity += capacityGB;
                    slot_info["capacity"] = std::to_string(static_cast<int>(capacityGB));
                }
                VariantClear(&vtProp);

                // Get RAM module speed
                if (SUCCEEDED(pclsObj->Get(L"Speed", 0, &vtProp, 0, 0)) && vtProp.vt != VT_NULL) {
                    slot_info["speed"] = std::to_string(vtProp.uintVal);
                } else {
                    slot_info["speed"] = "N/A MHz";
                }
                VariantClear(&vtProp);

                // Get RAM slot location
                if (SUCCEEDED(pclsObj->Get(L"DeviceLocator", 0, &vtProp, 0, 0)) && vtProp.vt != VT_NULL) {
                    slot_info["slot"] = safeWMIString(vtProp);
                } else {
                    slot_info["slot"] = "Unknown Slot";
                }
                VariantClear(&vtProp);

                // Get RAM module manufacturer
                if (SUCCEEDED(pclsObj->Get(L"Manufacturer", 0, &vtProp, 0, 0)) && vtProp.vt != VT_NULL) {
                    slot_info["manufacturer"] = safeWMIString(vtProp);
                } else {
                    slot_info["manufacturer"] = "N/A";
                }
                VariantClear(&vtProp);

                memory_slots.push_back(slot_info);
                pclsObj->Release();
            }
            pEnumerator->Release();
        }

        // Add slot information and total capacity to result
        memory_info["slots"] = memory_slots;
        memory_info["total_capacity"] = std::to_string(static_cast<int>(total_capacity)) + " GB";
    }
    catch (const std::exception &e) {
        logError("getMemoryInfo", e);
    }
    return memory_info;
}

// Collect storage information including disk drives and partitions
json SystemInfoCollector::getStorageInfo() {
    json storage_array = json::array();

    try {
        WMISession wmiSession;
        IWbemServices *pSvc = wmiSession.getServices();
        HRESULT hr;
        if (!pSvc)
            return storage_array;

        // Create map to store physical disk information
        std::map<std::wstring, json> physicalDisks;

        // Query physical disk information through WMI
        IEnumWbemClassObject *pDiskEnum = NULL;
        hr = pSvc->ExecQuery(
            bstr_t("WQL"),
            bstr_t("SELECT * FROM Win32_DiskDrive"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            NULL,
            &pDiskEnum);

        if (SUCCEEDED(hr)) {
            IWbemClassObject *pDiskObj = NULL;
            ULONG uReturn = 0;

            while (SUCCEEDED(pDiskEnum->Next(WBEM_INFINITE, 1, &pDiskObj, &uReturn)) && uReturn != 0) {
                VARIANT vtProp;
                json disk_info;

                // Get disk device identifier
                std::wstring deviceID;
                if (SUCCEEDED(pDiskObj->Get(L"DeviceID", 0, &vtProp, 0, 0)) && vtProp.vt != VT_NULL) {
                    deviceID = vtProp.bstrVal;
                }
                VariantClear(&vtProp);

                // Get disk model name
                if (SUCCEEDED(pDiskObj->Get(L"Model", 0, &vtProp, 0, 0)) && vtProp.vt != VT_NULL) {
                    disk_info["model"] = safeWMIString(vtProp);
                }
                VariantClear(&vtProp);

                // Get disk interface type (SATA, NVMe, etc.)
                if (SUCCEEDED(pDiskObj->Get(L"InterfaceType", 0, &vtProp, 0, 0)) && vtProp.vt != VT_NULL) {
                    disk_info["interface"] = safeWMIString(vtProp);
                }
                VariantClear(&vtProp);

                // Query associated partitions for each physical disk
                if (SUCCEEDED(pDiskObj->Get(L"Name", 0, &vtProp, 0, 0)) && vtProp.vt != VT_NULL) {
                    std::wstring diskName = vtProp.bstrVal;

                    // Get partition information using WMI associations
                    IEnumWbemClassObject *pPartEnum = NULL;
                    std::wstring query = L"ASSOCIATORS OF {Win32_DiskDrive.DeviceID='" +
                                       deviceID + L"'} WHERE AssocClass = Win32_DiskDriveToDiskPartition";

                    BSTR bstrQuery = SysAllocString(query.c_str());
                    HRESULT hr = pSvc->ExecQuery(
                        bstr_t("WQL"),
                        bstrQuery,
                        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                        NULL,
                        &pPartEnum);
                    SysFreeString(bstrQuery);

                    if (SUCCEEDED(hr)) {
                        IWbemClassObject *pPartObj = NULL;
                        ULONG uPartReturn = 0;

                        while (SUCCEEDED(pPartEnum->Next(WBEM_INFINITE, 1, &pPartObj, &uPartReturn)) && uPartReturn != 0) {
                            VARIANT vtPartProp;
                            if (SUCCEEDED(pPartObj->Get(L"DeviceID", 0, &vtPartProp, 0, 0))) {
                                // Get logical disks for each partition
                                IEnumWbemClassObject *pLogicalEnum = NULL;
                                std::wstring partQuery = L"ASSOCIATORS OF {Win32_DiskPartition.DeviceID='" +
                                                       std::wstring(vtPartProp.bstrVal) +
                                                       L"'} WHERE AssocClass = Win32_LogicalDiskToPartition";

                                BSTR bstrPartQuery = SysAllocString(partQuery.c_str());
                                hr = pSvc->ExecQuery(
                                    bstr_t("WQL"),
                                    bstrPartQuery,
                                    WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                                    NULL,
                                    &pLogicalEnum);
                                SysFreeString(bstrPartQuery);

                                if (SUCCEEDED(hr)) {
                                    IWbemClassObject *pLogicalObj = NULL;
                                    ULONG uLogicalReturn = 0;

                                    // Process each logical disk
                                    while (SUCCEEDED(pLogicalEnum->Next(WBEM_INFINITE, 1, &pLogicalObj, &uLogicalReturn)) &&
                                           uLogicalReturn != 0) {
                                        VARIANT vtLogicalProp;
                                        // Map physical disk info to logical drive
                                        if (SUCCEEDED(pLogicalObj->Get(L"DeviceID", 0, &vtLogicalProp, 0, 0))) {
                                            std::wstring logicalDrive = vtLogicalProp.bstrVal;
                                            physicalDisks[logicalDrive] = disk_info;
                                        }
                                        VariantClear(&vtLogicalProp);
                                        pLogicalObj->Release();
                                    }
                                    pLogicalEnum->Release();
                                }
                                VariantClear(&vtPartProp);
                            }
                            pPartObj->Release();
                        }
                        pPartEnum->Release();
                    }
                }
                VariantClear(&vtProp);

                pDiskObj->Release();
            }
            pDiskEnum->Release();
        }

        // Query logical disk information
        IEnumWbemClassObject *pEnumerator = NULL;
        hr = pSvc->ExecQuery(
            bstr_t("WQL"),
            bstr_t("SELECT * FROM Win32_LogicalDisk WHERE DriveType = 2 OR DriveType = 3"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            NULL,
            &pEnumerator);

        if (SUCCEEDED(hr)) {
            IWbemClassObject *pclsObj = NULL;
            ULONG uReturn = 0;

            // Process each logical disk
            while (SUCCEEDED(pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn)) && uReturn != 0) {
                json disk_info;
                VARIANT vtProp;

                // Get drive letter
                std::wstring driveId;
                if (SUCCEEDED(pclsObj->Get(L"DeviceID", 0, &vtProp, 0, 0)) && vtProp.vt != VT_NULL) {
                    driveId = vtProp.bstrVal;
                    disk_info["drive"] = safeWMIString(vtProp);
                }
                VariantClear(&vtProp);

                // Get drive type (local or removable)
                if (SUCCEEDED(pclsObj->Get(L"DriveType", 0, &vtProp, 0, 0)) && vtProp.vt != VT_NULL) {
                    disk_info["type"] = (vtProp.intVal == 3) ? "Local Disk" : "Removable Disk";
                }
                VariantClear(&vtProp);

                // Get total size
                if (SUCCEEDED(pclsObj->Get(L"Size", 0, &vtProp, 0, 0)) && vtProp.vt != VT_NULL) {
                    double sizeGB = _wtof(vtProp.bstrVal) / (1024.0 * 1024.0 * 1024.0);
                    disk_info["size"] = round(sizeGB * 100) / 100.0;
                }
                VariantClear(&vtProp);

                // Get free space
                if (SUCCEEDED(pclsObj->Get(L"FreeSpace", 0, &vtProp, 0, 0)) && vtProp.vt != VT_NULL) {
                    double freeGB = _wtof(vtProp.bstrVal) / (1024.0 * 1024.0 * 1024.0);
                    disk_info["free"] = round(freeGB * 100) / 100.0;
                }
                VariantClear(&vtProp);

                // Add physical disk information for local disks
                if (disk_info["type"] == "Local Disk") {
                    auto physicalDiskInfo = physicalDisks.find(driveId);
                    if (physicalDiskInfo != physicalDisks.end()) {
                        disk_info["model"] = physicalDiskInfo->second["model"];
                        disk_info["interface"] = physicalDiskInfo->second["interface"];
                    } else {
                        disk_info["model"] = "Unknown Disk";
                        disk_info["interface"] = "Unknown";
                    }
                } else {
                    // Handle removable disks
                    if (SUCCEEDED(pclsObj->Get(L"VolumeName", 0, &vtProp, 0, 0)) && vtProp.vt != VT_NULL) {
                        disk_info["model"] = safeWMIString(vtProp);
                    } else {
                        disk_info["model"] = "Removable Disk";
                    }
                    VariantClear(&vtProp);
                    disk_info["interface"] = "USB";
                }

                storage_array.push_back(disk_info);
                pclsObj->Release();
            }
            pEnumerator->Release();
        }
    }
    catch (const std::exception &e) {
        logError("getStorageInfo", e);
    }
    return storage_array;
}

// Collect network adapter information including Ethernet and WiFi
json SystemInfoCollector::getNetworkInfo() {
    json result;
    json ethernet = json::array();
    json wlan = json::array();

    // Allocate memory for adapter info
    PIP_ADAPTER_INFO pAdapterInfo;
    PIP_ADAPTER_INFO pAdapter = NULL;
    DWORD dwRetVal = 0;
    ULONG ulOutBufLen = sizeof(IP_ADAPTER_INFO);

    pAdapterInfo = (IP_ADAPTER_INFO *)malloc(sizeof(IP_ADAPTER_INFO));
    if (pAdapterInfo == NULL) {
        return result;
    }

    // Reallocate memory if buffer is too small
    if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW) {
        free(pAdapterInfo);
        pAdapterInfo = (IP_ADAPTER_INFO *)malloc(ulOutBufLen);
        if (pAdapterInfo == NULL) {
            return result;
        }
    }

    // Get network adapter information
    if ((dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen)) == NO_ERROR) {
        pAdapter = pAdapterInfo;
        while (pAdapter) {
            // Skip virtual and system adapters
            std::string description = pAdapter->Description;
            std::transform(description.begin(), description.end(), description.begin(), ::tolower);
            
            if (description.find("virtual") != std::string::npos ||
                description.find("pseudo") != std::string::npos ||
                description.find("loopback") != std::string::npos ||
                description.find("microsoft") != std::string::npos) {
                pAdapter = pAdapter->Next;
                continue;
            }

            // Collect adapter details
            json adapter;
            adapter["name"] = sanitizeString(pAdapter->Description);
            adapter["mac_address"] = sanitizeString(pAdapter->AdapterName);
            
            // Get IP address and connection status
            std::string ipAddress = pAdapter->IpAddressList.IpAddress.String;
            adapter["ip_address"] = (ipAddress != "0.0.0.0") ? ipAddress : "N/A";
            adapter["status"] = (ipAddress != "0.0.0.0") ? "Connected" : "Not Connected";

            // Categorize adapter as Ethernet or WiFi
            if (pAdapter->Type == MIB_IF_TYPE_ETHERNET) {
                ethernet.push_back(adapter);
            } else if (pAdapter->Type == IF_TYPE_IEEE80211) {
                wlan.push_back(adapter);
            }

            pAdapter = pAdapter->Next;
        }
    }

    // Free allocated memory
    if (pAdapterInfo) {
        free(pAdapterInfo);
    }

    // Combine Ethernet and WiFi information
    result["ethernet"] = ethernet;
    result["wlan"] = wlan;
    return result;
}

// Collect audio device information from the system
json SystemInfoCollector::getAudioInfo() {
    json audio_array = json::array();

    try {
        WMISession wmiSession;
        IWbemServices *pSvc = wmiSession.getServices();
        HRESULT hr;
        if (!pSvc)
            return audio_array;

        // Query sound device information through WMI
        IEnumWbemClassObject *pEnumerator = NULL;
        hr = pSvc->ExecQuery(
            bstr_t("WQL"),
            bstr_t("SELECT * FROM Win32_SoundDevice"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            NULL,
            &pEnumerator);

        if (SUCCEEDED(hr)) {
            IWbemClassObject *pclsObj = NULL;
            ULONG uReturn = 0;

            while (SUCCEEDED(pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn)) && uReturn != 0) {
                json audio_info;
                VARIANT vtProp;

                // Get audio device name
                if (SUCCEEDED(pclsObj->Get(L"Name", 0, &vtProp, 0, 0)) && vtProp.vt != VT_NULL) {
                    audio_info["name"] = safeWMIString(vtProp);
                } else {
                    audio_info["name"] = "Unknown Audio Device";
                }
                VariantClear(&vtProp);

                // Get audio device manufacturer
                if (SUCCEEDED(pclsObj->Get(L"Manufacturer", 0, &vtProp, 0, 0)) && vtProp.vt != VT_NULL) {
                    audio_info["manufacturer"] = safeWMIString(vtProp);
                } else {
                    audio_info["manufacturer"] = "N/A";
                }
                VariantClear(&vtProp);

                audio_array.push_back(audio_info);
                pclsObj->Release();
            }
            pEnumerator->Release();
        }
    }
    catch (const std::exception &e) {
        logError("getAudioInfo", e);
    }
    return audio_array;
}

// Collect battery information and power status
json SystemInfoCollector::getBatteryInfo() {
    // Initialize default values for desktop system
    json battery_info = {
        {"percent", 100},
        {"power_plugged", true},
        {"is_desktop", true}
    };

    try {
        // Get system power status
        SYSTEM_POWER_STATUS powerStatus;
        if (GetSystemPowerStatus(&powerStatus)) {
            // Get battery percentage if available
            if (powerStatus.BatteryLifePercent != 255) {
                battery_info["percent"] = static_cast<int>(powerStatus.BatteryLifePercent);
                battery_info["is_desktop"] = false; // Has valid battery, so not a desktop
            }

            // Check if running on AC power
            battery_info["power_plugged"] = (powerStatus.ACLineStatus == 1);

            // Detect if system is a desktop based on battery flags
            if (powerStatus.BatteryFlag == 128 ||
                powerStatus.BatteryFlag == 255 ||
                (powerStatus.BatteryLifePercent == 255 && powerStatus.BatteryFlag == 1)) {
                battery_info["is_desktop"] = true;
                battery_info["percent"] = 100;
                battery_info["power_plugged"] = true;
            }
        }
    }
    catch (const std::exception &e) {
        logError("getBatteryInfo", e);
    }

    return battery_info;
}
  