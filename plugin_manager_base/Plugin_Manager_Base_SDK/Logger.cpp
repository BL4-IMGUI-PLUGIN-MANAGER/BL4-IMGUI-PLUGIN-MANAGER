#include "Logger.hpp"

bool FunctionLogger::Initialize(const std::string& ConfigPath, const std::string& LogDirectory)
{
	if (!ParseConfigFile(ConfigPath))
	{
		// Config file not found, using defaults (no console message needed)
		OutputDebugStringA("FunctionLogger: Config file not found or failed to parse, using defaults.\n");
	}

	if (!LogDirectory.empty())
	{
		std::string FullLogPath = LogDirectory;
		size_t LastSlash = Config.LogFilePath.find_last_of("\\");
		if (LastSlash == std::string::npos)
		{
			LastSlash = Config.LogFilePath.find_last_of("/");
		}
		std::string FileName = (LastSlash != std::string::npos) ? Config.LogFilePath.substr(LastSlash + 1) : Config.LogFilePath;
		FullLogPath += FileName;
		Config.LogFilePath = FullLogPath;
	}

	if (Config.bLogToFile)
	{
		LogFile.open(Config.LogFilePath, std::ios::trunc);
		if (!LogFile.is_open())
		{
			std::string msg = "FunctionLogger: Failed to open log file: " + Config.LogFilePath + "\n";
			OutputDebugStringA(msg.c_str());
			Config.bLogToFile = false;
		}
	}

	bInitialized = true;

	std::string InitMsg = "=== Function Logger Initialized ===\n";
	InitMsg += "Keyword Filtering: " + std::string(Config.bEnableKeywordFiltering ? "ENABLED" : "DISABLED") + "\n";
	InitMsg += "Log to Console: " + std::string(Config.bLogToConsole ? "YES" : "NO") + "\n";
	InitMsg += "Log to File: " + std::string(Config.bLogToFile ? "YES" : "NO") + "\n";
	InitMsg += "Log Function Address: " + std::string(Config.bLogFunctionAddress ? "YES" : "NO") + "\n";
	InitMsg += "Log Full Paths: " + std::string(Config.bLogFullPaths ? "YES" : "NO") + "\n";

	if (Config.bEnableKeywordFiltering && !Config.FilterKeywords.empty())
	{
		std::string KeywordList;
		for (size_t i = 0; i < Config.FilterKeywords.size(); ++i)
		{
			KeywordList += Config.FilterKeywords[i];
			if (i < Config.FilterKeywords.size() - 1)
				KeywordList += ", ";
		}
		InitMsg += "Filter Keywords: " + KeywordList + "\n";
	}
	InitMsg += "====================================\n";

	// Only log to file and debug output, not console
	OutputDebugStringA(InitMsg.c_str());
	if (Config.bLogToFile && LogFile.is_open())
	{
		LogFile << InitMsg;
	}

	return true;
}

void FunctionLogger::LogFunctionCall(
	const std::string& FunctionName,
	const std::string& ObjectPath,
	uintptr_t FunctionAddress,
	const std::string& Parameters,
	const std::string& ReturnValue
)
{
	if (!bInitialized) return;

	if (!ShouldLog(FunctionName))
		return;

	std::lock_guard<std::mutex> Lock(LogMutex);

	std::stringstream SS;
	SS << "[" << GetTimestamp() << "] ";
	SS << "FUNCTION CALL";

	if (Config.bLogFullPaths && !ObjectPath.empty())
	{
		SS << " | Object: " << ObjectPath;
	}

	SS << " | Name: " << FunctionName;

	if (Config.bLogFunctionAddress)
	{
		SS << " | Address: 0x" << std::hex << FunctionAddress << std::dec;
	}

	if (Config.bLogParameters && !Parameters.empty())
	{
		SS << " | Params: " << Parameters;
	}

	if (Config.bLogReturnValues && !ReturnValue.empty())
	{
		SS << " | Return: " << ReturnValue;
	}

	std::string Message = SS.str();

	WriteToConsole(Message);
	WriteToFile(Message);
}

void FunctionLogger::LogSignatureScan(
	const std::string& SignatureName,
	bool bFound,
	uintptr_t Address,
	const std::string& Details
)
{
	if (!Config.bLogScanResults) return;

	std::lock_guard<std::mutex> Lock(LogMutex);

	std::stringstream SS;
	SS << "[" << GetTimestamp() << "] ";
	SS << "SIGNATURE SCAN | " << SignatureName << ": ";
	SS << (bFound ? "FOUND" : "NOT FOUND");

	if (bFound)
	{
		SS << " | Address: 0x" << std::hex << Address << std::dec;
	}

	if (!Details.empty())
	{
		SS << " | Details: " << Details;
	}

	std::string Message = SS.str();

	WriteToConsole(Message);
	WriteToFile(Message);
}

void FunctionLogger::LogDiagnostic(const std::string& Message)
{
	std::lock_guard<std::mutex> Lock(LogMutex);

	std::stringstream SS;
	SS << "[" << GetTimestamp() << "] ";
	SS << "DIAGNOSTIC | " << Message;

	std::string FullMessage = SS.str();

	WriteToConsole(FullMessage);
	WriteToFile(FullMessage);
}

void FunctionLogger::LogError(const std::string& Message)
{
	std::lock_guard<std::mutex> Lock(LogMutex);

	std::stringstream SS;
	SS << "[" << GetTimestamp() << "] ";
	SS << "ERROR | " << Message;

	std::string FullMessage = SS.str();

	WriteToConsole(FullMessage);
	WriteToFile(FullMessage);
}

void FunctionLogger::LogSignaturePattern(
	const std::string& FunctionName,
	uintptr_t Address,
	const std::vector<unsigned char>& PatternBytes,
	const std::string& Pattern
)
{
	if (!Config.bLogSignaturePatterns) return;

	std::lock_guard<std::mutex> Lock(LogMutex);

	std::stringstream SS;
	SS << "[" << GetTimestamp() << "] ";
	SS << "SIGNATURE PATTERN | " << FunctionName << " @ 0x" << std::hex << Address << std::dec << "\n";

	// Log the AOB pattern (with wildcards)
	SS << "Pattern: " << Pattern << "\n";

	// Log raw bytes
	SS << "Bytes: ";
	for (size_t i = 0; i < PatternBytes.size() && i < static_cast<size_t>(Config.iMaxPatternBytes); ++i)
	{
		SS << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(PatternBytes[i]) << " " << std::dec;
	}
	if (PatternBytes.size() > static_cast<size_t>(Config.iMaxPatternBytes))
	{
		SS << "... (truncated)";
	}
	SS << "\n";

	std::string FullMessage = SS.str();

	// Don't log to console - only to file and debug output
	OutputDebugStringA(FullMessage.c_str());

	if (Config.bLogSignaturePatterns)
	{
		std::ofstream SigFile(Config.SignatureLogPath, std::ios::app);
		if (SigFile.is_open())
		{
			SigFile << FullMessage;
			SigFile.close();
		}
	}
}

bool FunctionLogger::ShouldLog(const std::string& FunctionName)
{
	if (!Config.bEnableKeywordFiltering)
		return true;

	if (Config.FilterKeywords.empty())
		return true;

	std::string LowerFunctionName = ToLowercase(FunctionName);

	for (const auto& Keyword : Config.FilterKeywords)
	{
		if (LowerFunctionName.find(ToLowercase(Keyword)) != std::string::npos)
			return true;
	}

	return false;
}

void FunctionLogger::Flush()
{
	std::lock_guard<std::mutex> Lock(LogMutex);
	if (LogFile.is_open())
		LogFile.flush();
}

void FunctionLogger::Shutdown()
{
	std::lock_guard<std::mutex> Lock(LogMutex);

	if (LogFile.is_open())
	{
		LogFile << "Logger shutting down...\n";
		LogFile.close();
	}
}

bool FunctionLogger::ParseConfigFile(const std::string& ConfigPath)
{
	std::ifstream ConfigFile(ConfigPath);
	if (!ConfigFile.is_open())
		return false;

	std::string Line;
	bool bInLoggingSection = false;
	bool bInScanningSection = false;

	while (std::getline(ConfigFile, Line))
	{
		if (Line.empty() || Line[0] == ';')
			continue;

		Line.erase(0, Line.find_first_not_of(" \t\r\n"));
		Line.erase(Line.find_last_not_of(" \t\r\n") + 1);

		if (Line == "[Logging]")
		{
			bInLoggingSection = true;
			bInScanningSection = false;
			continue;
		}
		else if (Line == "[SignatureScanning]")
		{
			bInLoggingSection = false;
			bInScanningSection = true;
			continue;
		}

		size_t EqPos = Line.find('=');
		if (EqPos == std::string::npos)
			continue;

		std::string Key = Line.substr(0, EqPos);
		std::string Value = Line.substr(EqPos + 1);

		Key.erase(Key.find_last_not_of(" \t") + 1);
		Value.erase(0, Value.find_first_not_of(" \t"));
		Value.erase(Value.find_last_not_of(" \t") + 1);

		if (bInLoggingSection)
		{
			if (Key == "EnableKeywordFiltering")
			{
				Config.bEnableKeywordFiltering = (Value == "true" || Value == "1");
			}
			else if (Key == "Keywords")
			{
				Config.FilterKeywords.clear();
				if (!Value.empty())
				{
					size_t Start = 0;
					size_t Pos = 0;
					while ((Pos = Value.find(',', Start)) != std::string::npos)
					{
						std::string Keyword = Value.substr(Start, Pos - Start);
						Keyword.erase(0, Keyword.find_first_not_of(" \t"));
						Keyword.erase(Keyword.find_last_not_of(" \t") + 1);
						if (!Keyword.empty())
							Config.FilterKeywords.push_back(Keyword);
						Start = Pos + 1;
					}
					std::string LastKeyword = Value.substr(Start);
					LastKeyword.erase(0, LastKeyword.find_first_not_of(" \t"));
					LastKeyword.erase(LastKeyword.find_last_not_of(" \t") + 1);
					if (!LastKeyword.empty())
						Config.FilterKeywords.push_back(LastKeyword);
				}
			}
			else if (Key == "LogToFile")
			{
				Config.bLogToFile = (Value == "true" || Value == "1");
			}
			else if (Key == "LogFilePath")
			{
				Config.LogFilePath = Value;
			}
			else if (Key == "LogToConsole")
			{
				Config.bLogToConsole = (Value == "true" || Value == "1");
			}
			else if (Key == "LogParameters")
			{
				Config.bLogParameters = (Value == "true" || Value == "1");
			}
			else if (Key == "LogReturnValues")
			{
				Config.bLogReturnValues = (Value == "true" || Value == "1");
			}
			else if (Key == "LogFunctionAddress")
			{
				Config.bLogFunctionAddress = (Value == "true" || Value == "1");
			}
			else if (Key == "LogFullPaths")
			{
				Config.bLogFullPaths = (Value == "true" || Value == "1");
			}
		}
		else if (bInScanningSection)
		{
			if (Key == "EnableSignatureScanning")
			{
				Config.bEnableSignatureScanning = (Value == "true" || Value == "1");
			}
			else if (Key == "LogScanResults")
			{
				Config.bLogScanResults = (Value == "true" || Value == "1");
			}
			else if (Key == "TargetModule")
			{
				Config.TargetModule = Value;
			}
			else if (Key == "EnableSignatureHooking")
			{
				Config.bEnableSignatureHooking = (Value == "true" || Value == "1");
			}
			else if (Key == "LogSignaturePatterns")
			{
				Config.bLogSignaturePatterns = (Value == "true" || Value == "1");
			}
			else if (Key == "SignatureLogPath")
			{
				Config.SignatureLogPath = Value;
			}
			else if (Key == "MaxPatternBytes")
			{
				try {
					Config.iMaxPatternBytes = std::stoi(Value);
				} catch (...) {
					Config.iMaxPatternBytes = 64;
				}
			}
		}
	}

	ConfigFile.close();
	return true;
}

std::string FunctionLogger::GetTimestamp()
{
	auto Now = std::chrono::system_clock::now();
	auto Time = std::chrono::system_clock::to_time_t(Now);

	struct tm TimeInfo;
	localtime_s(&TimeInfo, &Time);

	std::stringstream SS;
	SS << std::put_time(&TimeInfo, "%Y-%m-%d %H:%M:%S");
	return SS.str();
}

std::string FunctionLogger::ToLowercase(const std::string& Str)
{
	std::string Result = Str;
	std::transform(Result.begin(), Result.end(), Result.begin(),
		[](unsigned char C) { return std::tolower(C); });
	return Result;
}

void FunctionLogger::WriteToFile(const std::string& Message)
{
	if (Config.bLogToFile && LogFile.is_open())
	{
		LogFile << Message << "\n";
	}
}

void FunctionLogger::WriteToConsole(const std::string& Message)
{
	if (Config.bLogToConsole)
	{
		// Console output disabled - only log to debug output
		std::string msg = Message + "\n";
		OutputDebugStringA(msg.c_str());
	}
}
