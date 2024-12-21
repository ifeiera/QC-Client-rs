#include "Logger.hpp"
#include <fstream>
#include <ctime>
#include <filesystem>

namespace {
    // Get current timestamp in YYYY-MM-DD HH:MM:SS format
    std::string getTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto timeT = std::chrono::system_clock::to_time_t(now);
        struct tm timeinfo;
        localtime_s(&timeinfo, &timeT);
        
        char buffer[80];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
        return std::string(buffer);
    }
    
    // Write log entry to file with thread safety
    void writeToLogFile(const std::string& level, const std::string& component, 
                       const std::string& message) {
        static std::mutex fileMutex;
        std::lock_guard<std::mutex> lock(fileMutex);
        
        try {
            std::ofstream logFile("qc_server.log", std::ios::app);
            if (logFile.is_open()) {
                logFile << getTimestamp() << " [" << level << "] " 
                       << component << ": " << message << std::endl;
            }
        } catch (...) {
            // Ignore file writing errors
        }
    }
}

// Main logging function - writes to both file and console
void Logger::log(const std::string& level, const std::string& component, const std::string& message) {
    try {
        static std::mutex logMutex;
        std::lock_guard<std::mutex> lock(logMutex);

        // Create logs directory if it doesn't exist
        std::filesystem::path logPath("logs");
        if (!std::filesystem::exists(logPath)) {
            std::filesystem::create_directory(logPath);
        }

        // Write log entry to file
        {
            std::ofstream logFile("logs/qc_server.log", std::ios::app);
            if (logFile.is_open()) {
                auto now = std::chrono::system_clock::now();
                auto timeT = std::chrono::system_clock::to_time_t(now);
                struct tm timeinfo;
                localtime_s(&timeinfo, &timeT);

                char timestamp[26];
                strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);
                
                logFile << timestamp << " [" << level << "] " 
                       << component << ": " << message << std::endl;
            }
        }

        // Update console display with log entry
        auto now = std::chrono::system_clock::now();
        auto timeT = std::chrono::system_clock::to_time_t(now);
        struct tm timeinfo;
        localtime_s(&timeinfo, &timeT);

        char timestamp[26];
        strftime(timestamp, sizeof(timestamp), "%H:%M:%S", &timeinfo);

        // Clear and update log display area
        setCursorPosition(0, 17);
        for (int i = 0; i < 3; i++) {
            std::cout << std::string(71, ' ') << std::endl;
        }
        setCursorPosition(0, 17);

        // Display log with appropriate colors
        setColor(level == "ERROR" ? axioo::console::LIGHTRED : axioo::console::LIGHTCYAN);
        std::cout << "     " << timestamp;
        std::cout << " " << std::left << level;
        setColor(axioo::console::WHITE);
        std::cout << " " << component << ": " << message << std::endl;
        std::cout.flush();

    } catch (const std::exception& e) {
        // Fallback error handling
        std::cerr << "Logger error: " << e.what() << std::endl;
        
        // Attempt to write error to backup file
        try {
            std::ofstream errorLog("error.log", std::ios::app);
            errorLog << "Logger error: " << e.what() << "\n";
            errorLog << "Original message: [" << level << "] " << component << ": " << message << "\n";
        } catch (...) {
            // Nothing more we can do
        }
    }
}

// Log error message with ERROR level
void Logger::error(const std::string& component, const std::string& message) {
    log("ERROR", component, message);
}

// Log informational message with INFO level
void Logger::info(const std::string& component, const std::string& message) {
    log("INFO", component, message);
}

// Set console text color using Windows API
void Logger::setColor(int color) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

// Set console cursor position using Windows API
void Logger::setCursorPosition(int x, int y) {
    COORD coord;
    coord.X = x;
    coord.Y = y;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
}

// External logging function for server messages
void logMessage(const std::string& message) {
    Logger::info("Server", message);
} 