#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// Windows core headers
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

// Standard library headers
#include <iomanip>
#include <sstream>
#include <memory>
#include <chrono>
#include <thread>

// Project headers
#include "Logger.hpp"
#include "SystemInfoCollector.hpp"

int main() {
    try {
        // Basic setup
        SetConsoleOutputCP(CP_UTF8);

        // Message loop
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        return 0;
    } catch (...) {
        return 1;
    }
}