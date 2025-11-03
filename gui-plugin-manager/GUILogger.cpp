#include "stdafx.h"
#include "GUILogger.h"
#include <filesystem>

void GUILogger::Initialize(const std::string& logPath) {
    std::lock_guard<std::mutex> lock(logMutex);

    if (initialized) {
        return;
    }

    // Get the directory where the game exe is located
    std::string fullPath = logPath;
    if (logPath.find(':') == std::string::npos && logPath.find("\\\\") != 0) {
        // Relative path - make it relative to the game executable directory
        char exePath[MAX_PATH];
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();
        fullPath = (exeDir / logPath).string();
    }

    logFile.open(fullPath, std::ios::trunc);
    if (!logFile.is_open()) {
        char buffer[512];
        sprintf_s(buffer, "[GUILogger] Failed to open log file: %s\n", fullPath.c_str());
        OutputDebugStringA(buffer);
        return;
    }

    initialized = true;

    std::string startupMsg = "=== Plugin Manager GUI Log Started ===\n";
    logFile << startupMsg;
    logFile.flush();

    OutputDebugStringA(startupMsg.c_str());
}

void GUILogger::Shutdown() {
    std::lock_guard<std::mutex> lock(logMutex);

    if (logFile.is_open()) {
        logFile << "=== Plugin Manager GUI Log Ended ===\n";
        logFile.close();
    }

    initialized = false;
}

void GUILogger::Log(const char* format, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    std::string timestamp = GetTimestamp();
    std::string message = std::string(buffer);
    std::string fullMessage = "[" + timestamp + "] " + message;

    std::lock_guard<std::mutex> lock(logMutex);

    WriteToFile(fullMessage);
    AddToBuffer(message);

    OutputDebugStringA(fullMessage.c_str());
}

void GUILogger::LogToFileOnly(const char* format, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    std::string timestamp = GetTimestamp();
    std::string fullMessage = "[" + timestamp + "] " + std::string(buffer);

    std::lock_guard<std::mutex> lock(logMutex);
    WriteToFile(fullMessage);
}

void GUILogger::ClearLogBuffer() {
    std::lock_guard<std::mutex> lock(logMutex);
    logBuffer.clear();
}

void GUILogger::WriteToFile(const std::string& message) {
    if (initialized && logFile.is_open()) {
        logFile << message;
        if (message.back() != '\n') {
            logFile << '\n';
        }
        logFile.flush();
    }
}

void GUILogger::AddToBuffer(const std::string& message) {
    LogEntry entry;
    entry.timestamp = GetTimestamp();
    entry.message = message;

    logBuffer.push_back(entry);

    if (logBuffer.size() > MAX_BUFFER_SIZE) {
        logBuffer.erase(logBuffer.begin());
    }
}

std::string GUILogger::GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    struct tm timeInfo;
    localtime_s(&timeInfo, &time);

    std::stringstream ss;
    ss << std::put_time(&timeInfo, "%H:%M:%S")
       << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}
