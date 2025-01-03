#pragma once
#include <windows.h>
#include <Wbemidl.h>
#include <comdef.h>

// Helper class for Windows Management Instrumentation (WMI) operations
class WMIHelper
{
public:
    // Initialize WMI connection and get required interfaces
    // ppLoc: Pointer to WMI locator interface
    // ppSvc: Pointer to WMI service interface
    static HRESULT initialize(IWbemLocator **ppLoc, IWbemServices **ppSvc);

    // Clean up WMI resources and interfaces
    // pLoc: WMI locator to be released
    // pSvc: WMI service to be released
    static void cleanup(IWbemLocator *pLoc, IWbemServices *pSvc);
};

// RAII wrapper for automatic WMI resource management
class WMISession
{
    IWbemLocator *pLoc;  // WMI locator interface
    IWbemServices *pSvc; // WMI service interface

public:
    WMISession();  // Initialize WMI session
    ~WMISession(); // Automatically cleanup resources

    // Get WMI service interface for queries
    IWbemServices *getServices() { return pSvc; }

    // Get WMI locator interface
    IWbemLocator *getLocator() { return pLoc; }

    // Connect to a specific WMI namespace
    HRESULT connectToNamespace(const wchar_t* namespaceName, IWbemServices** ppSvc) {
        if (!pLoc) return E_FAIL;

        // Connect to the specified namespace
        HRESULT hr = pLoc->ConnectServer(
            _bstr_t(namespaceName),
            NULL,
            NULL,
            0,
            NULL,
            0,
            0,
            ppSvc);

        if (SUCCEEDED(hr)) {
            // Set security levels on the proxy
            hr = CoSetProxyBlanket(
                *ppSvc,
                RPC_C_AUTHN_WINNT,
                RPC_C_AUTHZ_NONE,
                NULL,
                RPC_C_AUTHN_LEVEL_CALL,
                RPC_C_IMP_LEVEL_IMPERSONATE,
                NULL,
                EOAC_NONE);
        }

        return hr;
    }
};