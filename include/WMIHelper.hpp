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
};