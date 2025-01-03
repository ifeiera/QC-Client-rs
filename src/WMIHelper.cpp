#include "WMIHelper.hpp"

#pragma comment(lib, "wbemuuid.lib")

// Initialize WMI connection and setup security
HRESULT WMIHelper::initialize(IWbemLocator** ppLoc, IWbemServices** ppSvc) {
    // Initialize COM for multi-threaded operations
    HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) // Ignore if COM is already initialized
        return hr;

    // Setup security context for WMI connection
    hr = CoInitializeSecurity(
        NULL, 
        -1,                          // COM authentication
        NULL,                        // Authentication services
        NULL,                        // Reserved
        RPC_C_AUTHN_LEVEL_DEFAULT,   // Default authentication 
        RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation
        NULL,                        // Authentication info
        EOAC_NONE,                   // Additional capabilities 
        NULL                         // Reserved
    );

    if (FAILED(hr) && hr != RPC_E_TOO_LATE) { // Ignore if security is already initialized
        CoUninitialize();
        return hr;
    }

    // Create WMI locator instance
    hr = CoCreateInstance(
        CLSID_WbemLocator,
        0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator,
        (LPVOID*)ppLoc);

    if (FAILED(hr)) {
        CoUninitialize();
        return hr;
    }

    // Connect to local WMI service (default namespace)
    hr = (*ppLoc)->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"),    // Default namespace
        NULL,                        // User name
        NULL,                        // User password
        0,                          // Locale
        NULL,                       // Security flags
        0,                          // Authority
        0,                          // Context object
        ppSvc                       // IWbemServices proxy
    );

    if (FAILED(hr)) {
        (*ppLoc)->Release();
        CoUninitialize();
        return hr;
    }

    // Set security levels for the default namespace
    hr = CoSetProxyBlanket(
        *ppSvc,
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        NULL,
        RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE
    );

    if (FAILED(hr)) {
        (*ppSvc)->Release();
        (*ppLoc)->Release();
        CoUninitialize();
        return hr;
    }

    return hr;
}

// Clean up WMI resources and COM
void WMIHelper::cleanup(IWbemLocator* pLoc, IWbemServices* pSvc) {
    if (pSvc) {
        pSvc->Release();
    }
    if (pLoc) {
        pLoc->Release();
    }
    CoUninitialize();
}

// RAII: Initialize WMI session on construction
WMISession::WMISession() : pLoc(nullptr), pSvc(nullptr) {
    HRESULT hr = WMIHelper::initialize(&pLoc, &pSvc);
    if (FAILED(hr)) {
        pLoc = nullptr;
        pSvc = nullptr;
    }
}

// RAII: Cleanup WMI session on destruction
WMISession::~WMISession() {
    WMIHelper::cleanup(pLoc, pSvc);
    pLoc = nullptr;
    pSvc = nullptr;
} 