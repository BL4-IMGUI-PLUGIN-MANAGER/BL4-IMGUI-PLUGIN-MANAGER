#pragma once

#include <Windows.h>
#include <fstream>
#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <sstream>
#include <iomanip>

class GUILogger {
public:
    struct LogEntry {
        std::string timestamp;
        std::string message;
    };

    static GUILogger& Get() {
        static GUILogger instance;
        return instance;
    }

    void Initialize(const std::string& logPath = "Plugin_Manager_GUI.log");
    void Shutdown();
    void Log(const char* format, ...);
    void LogToFileOnly(const char* format, ...);

    const std::vector<LogEntry>& GetLogBuffer() const { return logBuffer; }
    void ClearLogBuffer();

    bool IsDebugWindowVisible() const { return showDebugWindow; }
    void SetDebugWindowVisible(bool visible) { showDebugWindow = visible; }

private:
    GUILogger() = default;
    ~GUILogger() { Shutdown(); }
    GUILogger(const GUILogger&) = delete;
    GUILogger& operator=(const GUILogger&) = delete;

    void WriteToFile(const std::string& message);
    void AddToBuffer(const std::string& message);
    std::string GetTimestamp();

    std::ofstream logFile;
    std::vector<LogEntry> logBuffer;
    std::mutex logMutex;
    bool initialized = false;
    bool showDebugWindow = false;
    static constexpr size_t MAX_BUFFER_SIZE = 1000;
};
