#pragma once

#include <Windows.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <mutex>

/**
* Headless Function Logger
* A simple, thread-safe logging system for Unreal Engine function hooking
*/

class FunctionLogger
{
public:
	struct LogConfig
	{
		bool bEnableKeywordFiltering = true;
		std::vector<std::string> FilterKeywords;
		bool bLogToFile = true;
		std::string LogFilePath = "function_log.txt";
		bool bLogToConsole = true;
		bool bLogParameters = true;
		bool bLogReturnValues = true;
		bool bLogFunctionAddress = true;
		bool bLogFullPaths = true;
		bool bEnableSignatureScanning = true;
		bool bLogScanResults = true;
		std::string TargetModule = "Borderlands4.exe";
		bool bEnableSignatureHooking = false;
		bool bLogSignaturePatterns = false;
		std::string SignatureLogPath = "signatures.txt";
		int iMaxPatternBytes = 64;
	};

	static FunctionLogger& Get()
	{
		static FunctionLogger Instance;
		return Instance;
	}

	// Initialize logger with config file and optional log directory
	bool Initialize(const std::string& ConfigPath = "config.ini", const std::string& LogDirectory = "");

	// Log a function call with details
	void LogFunctionCall(
		const std::string& FunctionName,
		const std::string& ObjectPath,
		uintptr_t FunctionAddress,
		const std::string& Parameters = "",
		const std::string& ReturnValue = ""
	);

	// Log signature scan result
	void LogSignatureScan(
		const std::string& SignatureName,
		bool bFound,
		uintptr_t Address = 0,
		const std::string& Details = ""
	);

	// Log general diagnostic info
	void LogDiagnostic(const std::string& Message);

	// Log an error
	void LogError(const std::string& Message);

	// Log signature pattern extraction (AOB/Signature)
	void LogSignaturePattern(
		const std::string& FunctionName,
		uintptr_t Address,
		const std::vector<unsigned char>& PatternBytes,
		const std::string& Pattern
	);

	// Check if a keyword matches the filter
	bool ShouldLog(const std::string& FunctionName);

	// Flush output buffers
	void Flush();

	// Get current configuration
	const LogConfig& GetConfig() const { return Config; }

	// Manual shutdown (call this instead of relying on destructor)
	void Shutdown();

private:
	FunctionLogger() = default;
	~FunctionLogger() = default; // Empty destructor to prevent deadlock on shutdown

	FunctionLogger(const FunctionLogger&) = delete;
	FunctionLogger& operator=(const FunctionLogger&) = delete;

	// Parse config file
	bool ParseConfigFile(const std::string& ConfigPath);

	// Get current timestamp as string
	std::string GetTimestamp();

	// Convert string to lowercase
	std::string ToLowercase(const std::string& Str);

	// Write to file
	void WriteToFile(const std::string& Message);

	// Write to console
	void WriteToConsole(const std::string& Message);

	LogConfig Config;
	std::ofstream LogFile;
	std::mutex LogMutex;
	bool bInitialized = false;
};
