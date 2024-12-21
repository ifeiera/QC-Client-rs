#pragma once
#include <string>
#include <mutex>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <filesystem>
#include "ConsoleColors.hpp"
#include <windows.h>

// Logger class provides methods for logging messages with different severity levels
class Logger {
public:
    // Log a message with custom severity level
    static void log(const std::string& level, const std::string& component, const std::string& message);
    
    // Log an error message with red highlighting
    static void error(const std::string& component, const std::string& message);
    
    // Log an informational message with default formatting
    static void info(const std::string& component, const std::string& message);

private:
    // Set console text color for log messages
    static void setColor(int color);
    
    // Position cursor for log message display
    static void setCursorPosition(int x, int y);
};

// External function to log messages (used by other components)
void logMessage(const std::string& message); 