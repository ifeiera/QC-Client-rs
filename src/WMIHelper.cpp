#include "WMIHelper.hpp"

#pragma comment(lib, "wbemuuid.lib")

// Initialize WMI connection and setup security
HRESULT WMIHelper::initialize(IWbemLocator** ppLoc, IWbemServices** ppSvc) {
    // Initialize COM for multi-threaded operations
    HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hr))
        return hr;

    // Setup security context for WMI connection
    hr = CoInitializeSecurity(
        NULL, -1, NULL, NULL,
        RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE, NULL);

    if (FAILED(hr)) {
        CoUninitialize();
        return hr;
    }

    // Create WMI locator instance
    hr = CoCreateInstance(
        CLSID_WbemLocator, 0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID*)ppLoc);

    if (FAILED(hr)) {
        CoUninitialize();
        return hr;
    }

    // Connect to local WMI service
    hr = (*ppLoc)->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"),  // Connect to local root CIMV2 WMI namespace
        NULL, NULL, 0, NULL, 0, 0, ppSvc);

    if (FAILED(hr)) {
        (*ppLoc)->Release();
        CoUninitialize();
        return hr;
    }

    // Set security levels for WMI connection
    hr = CoSetProxyBlanket(
        *ppSvc,
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        NULL,
        RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE);

    return hr;
}

// Clean up WMI resources and COM
void WMIHelper::cleanup(IWbemLocator* pLoc, IWbemServices* pSvc) {
    if (pSvc)
        pSvc->Release();
    if (pLoc)
        pLoc->Release();
    CoUninitialize();
}

// RAII: Initialize WMI session on construction
WMISession::WMISession() : pLoc(nullptr), pSvc(nullptr) {
    WMIHelper::initialize(&pLoc, &pSvc);
}

// RAII: Cleanup WMI session on destruction
WMISession::~WMISession() {
    WMIHelper::cleanup(pLoc, pSvc);
} 