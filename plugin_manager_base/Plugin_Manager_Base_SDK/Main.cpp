#include <Windows.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <sstream>
#include <iomanip>
#include <fstream>

// Include necessary SDK headers
#include "SDK/SDK/Basic.hpp"
#include "SDK/SDK/CoreUObject_classes.hpp"
#include "SDK/SDK/Engine_classes.hpp"

// Include MinHook
#include "MinHook.h"

// Include our logger
#include "Logger.hpp"

// Include our hook system
#include "PluginAPI.h"

// --- Forward declarations for proxy functions ---
void load_original_dwmapi();
void setup_proxy_functions();

// --- Global state ---
FunctionLogger* g_Logger = nullptr;
bool bMenuOpenDetected = false;

// Typedef for FunctionLogger's OnProcessEvent export
typedef void (*OnProcessEventFn)(const char*, const char*, void*, bool);

// METHOD 1: GetFullName() - Built-in SDK method
std::string Method1_GetFullName(SDK::UObject* Object)
{
	if (!Object) return "";
	return Object->GetFullName();
}

// METHOD 2: Traverse Outer chain manually
std::string Method2_OuterChain(SDK::UObject* Object)
{
	if (!Object) return "";

	std::string Path;
	SDK::UObject* Current = Object;

	while (Current)
	{
		std::string CurrentName = Current->GetName();
		if (!Path.empty())
		{
			Path = CurrentName + "." + Path;
		}
		else
		{
			Path = CurrentName;
		}
		Current = Current->Outer;
	}

	return Path;
}

// METHOD 3: Class + Name direct access
std::string Method3_ClassAndName(SDK::UObject* Object)
{
	if (!Object) return "";
	std::string ClassName = Object->Class ? Object->Class->GetName() : "Unknown";
	return ClassName + " " + Object->GetName();
}

// METHOD 4: Full outer chain with class info
std::string Method4_OuterChainWithClass(SDK::UObject* Object)
{
	if (!Object) return "";

	std::string Path;
	SDK::UObject* Current = Object;

	while (Current)
	{
		std::string CurrentName = Current->GetName();
		if (!Path.empty())
		{
			Path = CurrentName + "." + Path;
		}
		else
		{
			Path = CurrentName;
		}
		Current = Current->Outer;
	}

	// Add class at the beginning
	std::string ClassName = Object->Class ? Object->Class->GetName() : "Unknown";
	return "/" + ClassName + "." + Path;
}

// METHOD 5: Get outer chain as package path (like Unreal's default path format)
std::string Method5_PackagePath(SDK::UObject* Object)
{
	if (!Object) return "";

	std::vector<std::string> PathParts;
	SDK::UObject* Current = Object;

	while (Current)
	{
		PathParts.push_back(Current->GetName());
		Current = Current->Outer;
	}

	// Reverse to get root to leaf order
	std::string Path;
	for (int i = PathParts.size() - 1; i >= 0; i--)
	{
		if (!Path.empty()) Path += ".";
		Path += PathParts[i];
	}

	return Path;
}

// METHOD 6: Attempt to use FString representation (if available)
std::string Method6_StringCast(SDK::UObject* Object)
{
	if (!Object) return "";

	// Try to get a string representation - this simulates what Python's str() does
	// by combining class name and full path
	std::string ClassName = Object->Class ? Object->Class->GetName() : "Unknown";
	std::string FullPath = Method5_PackagePath(Object);

	return ClassName + " " + FullPath;
}

// METHOD 7: Asset/Pak file path (like /OakGame/Content/Maps/World_P)
std::string Method7_AssetPath(SDK::UObject* Object)
{
	if (!Object) return "";

	// Find the root package
	SDK::UObject* Current = Object;
	SDK::UObject* Package = nullptr;

	while (Current)
	{
		// A package is an object whose outer is nullptr
		if (!Current->Outer)
		{
			Package = Current;
			break;
		}
		Current = Current->Outer;
	}

	if (!Package) return "";

	std::string PackageName = Package->GetName();

	// Get the full path from package to object
	std::vector<std::string> PathParts;
	Current = Object;

	while (Current && Current != Package)
	{
		PathParts.push_back(Current->GetName());
		Current = Current->Outer;
	}

	// Build path: /PackageName/Part1/Part2/ObjectName
	std::string AssetPath = "/" + PackageName;

	// Add all parts in reverse order (root to leaf)
	for (int i = PathParts.size() - 1; i >= 0; i--)
	{
		AssetPath += "/" + PathParts[i];
	}

	return AssetPath;
}

// METHOD 8: Full pak-style path with class info (like /OakGame/Content/Maps/World_P.World_P_C)
std::string Method8_AssetPathWithClass(SDK::UObject* Object)
{
	if (!Object) return "";

	// Get the asset path first
	std::string AssetPath = Method7_AssetPath(Object);
	if (AssetPath.empty()) return "";

	// Add class info at the end if it's a Blueprint or class instance
	std::string ClassName = Object->Class ? Object->Class->GetName() : "";

	if (!ClassName.empty())
	{
		// For Blueprint instances, add _C suffix convention
		AssetPath += "." + ClassName;
	}

	return AssetPath;
}

// Dump GUObjectArray to file with 3 different path methods
void DumpUObjectArray(const std::string& FilePath)
{
	if (g_Logger)
	{
		g_Logger->LogDiagnostic("[DUMP] Starting GUObjectArray dump with 3 path methods to: " + FilePath);
	}

	std::ofstream DumpFile(FilePath);
	if (!DumpFile.is_open())
	{
		if (g_Logger)
		{
			g_Logger->LogError("[DUMP] ERROR: Failed to open file for writing");
		}
		return;
	}

	DumpFile << "=== GUObjectArray Dump - 8 Path Methods ===\n";
	DumpFile << "Timestamp: " << std::time(nullptr) << "\n\n";

	int ObjectCount = 0;
	int SuccessCount = 0;
	std::vector<int> RandomIndices;

	try
	{
		// First pass: collect all valid object indices
		std::vector<int> ValidIndices;
		int MaxElements = 1000000;  // Safety limit to prevent infinite loops

		if (g_Logger)
		{
			g_Logger->LogDiagnostic("[DUMP] First pass: collecting valid object indices...");
		}

		for (int32_t i = 0; i < MaxElements; i++)
		{
			SDK::UObject* Object = SDK::UObject::GObjects->GetByIndex(i);
			if (!Object) continue;
			ValidIndices.push_back(i);
		}

		ObjectCount = ValidIndices.size();
		if (g_Logger)
		{
			std::stringstream ss;
			ss << "[DUMP] Found " << ObjectCount << " valid objects";
			g_Logger->LogDiagnostic(ss.str());
		}

		// Pick 3 random indices
		if (ObjectCount >= 3)
		{
			RandomIndices.push_back(ValidIndices[ObjectCount / 4]);
			RandomIndices.push_back(ValidIndices[ObjectCount / 2]);
			RandomIndices.push_back(ValidIndices[3 * ObjectCount / 4]);
		}
		else if (ObjectCount > 0)
		{
			for (int idx : ValidIndices)
				RandomIndices.push_back(idx);
		}

		// Dump all objects with all 8 methods
		for (int method = 1; method <= 8; method++)
		{
			if (method == 1)
				DumpFile << "=== METHOD 1: GetFullName() - Built-in SDK method ===\n\n";
			else if (method == 2)
				DumpFile << "\n=== METHOD 2: Outer Chain Traversal ===\n\n";
			else if (method == 3)
				DumpFile << "\n=== METHOD 3: Class + Name ===\n\n";
			else if (method == 4)
				DumpFile << "\n=== METHOD 4: Outer Chain with Class Prefix ===\n\n";
			else if (method == 5)
				DumpFile << "\n=== METHOD 5: Package Path Format ===\n\n";
			else if (method == 6)
				DumpFile << "\n=== METHOD 6: Class + Package Path (str() equivalent) ===\n\n";
			else if (method == 7)
				DumpFile << "\n=== METHOD 7: Asset/Pak File Path (like /OakGame/Content/Maps/World_P) ===\n\n";
			else if (method == 8)
				DumpFile << "\n=== METHOD 8: Full Pak-Style with Class (like /OakGame/Content/Maps/World_P.World_P_C) ===\n\n";

			for (int idx : RandomIndices)
			{
				SDK::UObject* Object = SDK::UObject::GObjects->GetByIndex(idx);
				if (!Object) continue;

				try
				{
					std::string ObjectName = Object->GetName();
					std::string Path;

					if (method == 1)
						Path = Method1_GetFullName(Object);
					else if (method == 2)
						Path = Method2_OuterChain(Object);
					else if (method == 3)
						Path = Method3_ClassAndName(Object);
					else if (method == 4)
						Path = Method4_OuterChainWithClass(Object);
					else if (method == 5)
						Path = Method5_PackagePath(Object);
					else if (method == 6)
						Path = Method6_StringCast(Object);
					else if (method == 7)
						Path = Method7_AssetPath(Object);
					else if (method == 8)
						Path = Method8_AssetPathWithClass(Object);

					DumpFile << "Object " << idx << " (Method " << method << "):\n";
					DumpFile << "  Name: " << ObjectName << "\n";
					DumpFile << "  Path: " << Path << "\n";
					DumpFile << "  Address: 0x" << std::hex << (uintptr_t)Object << std::dec << "\n";
					DumpFile << "\n";
					SuccessCount++;
				}
				catch (const std::exception& e)
				{
					DumpFile << "Object " << idx << " (Method " << method << "): ERROR - " << e.what() << "\n";
				}
			}
		}
	}
	catch (const std::exception& e)
	{
		DumpFile << "ERROR during iteration: " << e.what() << "\n";
		if (g_Logger)
		{
			g_Logger->LogError("[DUMP] Exception during dump: " + std::string(e.what()));
		}
	}

	DumpFile << "\n=== Summary ===\n";
	DumpFile << "Total objects found: " << ObjectCount << "\n";
	DumpFile << "Successfully dumped: " << SuccessCount << "\n";
	DumpFile << "Sample indices used: ";
	for (int idx : RandomIndices)
		DumpFile << idx << " ";
	DumpFile << "\n";

	DumpFile.close();

	if (g_Logger)
	{
		std::stringstream ss;
		ss << "[DUMP] Completed! Dumped " << SuccessCount << " samples from " << ObjectCount << " total objects";
		g_Logger->LogDiagnostic(ss.str());
		g_Logger->LogDiagnostic("[DUMP] Check the file to compare the 8 different path methods");
		g_Logger->LogDiagnostic("[DUMP] Methods 7 & 8 show pak-style asset paths like /OakGame/Content/Maps/World_P");
	}
}


// --- Global variables for dwmapi proxy ---
HMODULE g_OriginalDwmapi = nullptr;
extern "C" uintptr_t mProcs[128] = {0}; // Array to hold function pointers


// --- Install hooks ---
bool InstallHooks()
{
	// Initialize the HookSystem
	auto& HookSystem = PluginAPI::GetHookSystem();

	if (g_Logger)
	{
		g_Logger->LogDiagnostic("Initializing HookSystem...");
	}

	if (!HookSystem.InitializeProcessEventHook())
	{
		if (g_Logger)
		{
			g_Logger->LogError("ERROR: Failed to initialize ProcessEvent hook via HookSystem");
		}
		return false;
	}

	if (g_Logger)
	{
		g_Logger->LogDiagnostic("ProcessEvent hook installed successfully via HookSystem!");
	}

	return true;
}

// --- Initialization and Main Loop ---
DWORD WINAPI MainThread(LPVOID lpParam)
{
	try
	{
		HMODULE Module = (HMODULE)lpParam;

		// Get the directory where the DLL is located
		char DllPath[MAX_PATH];
		GetModuleFileNameA(Module, DllPath, MAX_PATH);
		std::string DllDir = DllPath;
		size_t LastSlash = DllDir.find_last_of("\\");
		if (LastSlash != std::string::npos)
		{
			DllDir = DllDir.substr(0, LastSlash + 1);
		}

		// Console output disabled - all logging goes to file
		// AllocConsole();
		// FILE* Dummy;
		// freopen_s(&Dummy, "CONOUT$", "w", stdout);

		// Initialize logger FIRST so we can use it
		FunctionLogger& Logger = FunctionLogger::Get();
		Logger.Initialize("", DllDir);
		g_Logger = &Logger;

		Logger.LogDiagnostic("=== MenuOpen Logger DLL Loaded ===");
		Logger.LogDiagnostic("DLL Directory: " + DllDir);

		// Wait for game to fully initialize before scanning/hooking
		Logger.LogDiagnostic("Waiting 2 seconds for game initialization...");
		std::this_thread::sleep_for(std::chrono::milliseconds(2000));

		// Initialize SDK using offsets from Basic.hpp
		Logger.LogDiagnostic("Initializing Unreal Engine SDK...");

		uintptr_t base = (uintptr_t)GetModuleHandleA(nullptr);

		// Initialize GObjects using offset
		void* GObjects_addr = (void*)(base + SDK::Offsets::GObjects);
		SDK::UObject::GObjects.InitManually(GObjects_addr);

		std::stringstream ss1;
		ss1 << "GObjects initialized from SDK offset: 0x" << std::hex << SDK::Offsets::GObjects << std::dec;
		Logger.LogDiagnostic(ss1.str());

		// Initialize FName using offset
		void* append_string_addr = (void*)(base + SDK::Offsets::AppendString);
		SDK::FName::InitManually(append_string_addr);

		std::stringstream ss2;
		ss2 << "FName initialized from SDK offset: 0x" << std::hex << SDK::Offsets::AppendString << std::dec;
		Logger.LogDiagnostic(ss2.str());

		Logger.LogDiagnostic("SDK Initialization complete.");
		Logger.LogDiagnostic("Logging MenuOpen calls to: " + DllDir + "function_log.txt");

		// Wait 10 seconds before initializing the hook system
		Logger.LogDiagnostic("Waiting 10 seconds before initializing HookSystem...");
		std::this_thread::sleep_for(std::chrono::seconds(10));
		Logger.LogDiagnostic("HookSystem initialization starting...");

		// Install ProcessEvent hook and register MenuOpen callback
		Logger.LogDiagnostic("Installing ProcessEvent hook...");
		if (!InstallHooks())
		{
			Logger.LogError("Hook installation failed");
		}

		// Register MenuOpen callback
		auto& HookSys = PluginAPI::GetHookSystem();

		// Create a global callback wrapper for FunctionLogger
		// This captures all function calls and sends them to the FunctionLogger plugin
		auto GlobalFunctionLoggerCallback = [](void* obj, void* func, void* params) {
			if (!obj || !func) return;

			SDK::UObject* Object = (SDK::UObject*)obj;
			SDK::UFunction* Function = (SDK::UFunction*)func;

			// Try to get the FunctionLogger's OnProcessEvent export
			// We do this dynamically to avoid dependency issues
			static OnProcessEventFn FunctionLoggerCallback = nullptr;
			static int retryCount = 0;

			// Try to load FunctionLogger if we haven't got it yet
			// Retry every 1000 calls (lazy loading for when plugin_loader loads the DLL)
			if (!FunctionLoggerCallback && (retryCount++ % 1000 == 0))
			{
				HMODULE hFunctionLogger = GetModuleHandleA("FunctionLogger.dll");
				if (!hFunctionLogger)
				{
					// Try multiple possible paths
					hFunctionLogger = LoadLibraryA("Plugins\\FunctionLogger.dll");
					if (!hFunctionLogger)
						hFunctionLogger = LoadLibraryA("..\\x64\\Release\\Plugins\\FunctionLogger.dll");
					if (!hFunctionLogger)
						hFunctionLogger = LoadLibraryA("x64\\Release\\Plugins\\FunctionLogger.dll");
				}
				if (hFunctionLogger)
				{
					FunctionLoggerCallback = (OnProcessEventFn)GetProcAddress(hFunctionLogger, "OnProcessEvent");
					if (FunctionLoggerCallback && g_Logger)
					{
						g_Logger->LogDiagnostic("[GlobalCallback] Successfully loaded FunctionLogger.dll and found OnProcessEvent!");
					}
				}
			}

			if (FunctionLoggerCallback && Function)
			{
				std::string ClassName = Object->Class ? Object->Class->GetName() : "Unknown";
				std::string FunctionName = Function->GetName();
				FunctionLoggerCallback(ClassName.c_str(), FunctionName.c_str(), Object, true);  // Pre-call
			}
		};

		// Register the global callback - it will be called for EVERY ProcessEvent
		HookSys.RegisterGlobalPreCallback(GlobalFunctionLoggerCallback);
		std::cout << "Registered FunctionLogger as global callback for all function calls\n";

		// Create a pre-callback for MenuOpen detection
		auto MenuOpenCallback = [&Logger, &DllDir](void* obj, void* func, void* params) {
			if (bMenuOpenDetected) return;

			SDK::UObject* Object = (SDK::UObject*)obj;
			SDK::UFunction* Function = (SDK::UFunction*)func;

			if (Object && Function)
			{
				std::string FunctionName = Function->GetName();
				std::string ClassName = Object->GetName();

				// Filter for MenuOpen on ui_script_menu_base_C
				if (ClassName.find("ui_script_menu_base_C") != std::string::npos && FunctionName == "MenuOpen")
				{
					if (g_Logger)
					{
						g_Logger->LogDiagnostic("[MenuOpen] Detected via HookSystem! Starting UObject array dump...");
					}

					bMenuOpenDetected = true;

					if (g_Logger)
					{
						g_Logger->LogFunctionCall(ClassName, FunctionName, (uintptr_t)Object);
					}

					// Dump GUObjectArray to file
					char DumpPath[MAX_PATH];
					GetTempPathA(MAX_PATH, DumpPath);
					strcat_s(DumpPath, MAX_PATH, "uobject_dump.txt");
					DumpUObjectArray(DumpPath);

					if (g_Logger)
					{
						g_Logger->LogDiagnostic(std::string("[MenuOpen] Dump complete! Saved to: ") + DumpPath);
					}

					// Load Plugin_Manager.dll
					HMODULE hPluginLoader = LoadLibraryA("Plugin_Manager\\Plugin_Manager.dll");
					if (hPluginLoader)
					{
						if (g_Logger)
						{
							std::stringstream ss;
							ss << "[MenuOpen] Plugin_Manager.dll loaded successfully at 0x" << std::hex << (uintptr_t)hPluginLoader << std::dec;
							g_Logger->LogDiagnostic(ss.str());
						}
					}
					else
					{
						if (g_Logger)
						{
							std::stringstream ss;
							ss << "[MenuOpen] ERROR: Failed to load Plugin_Manager.dll (Error: " << GetLastError() << ")";
							g_Logger->LogError(ss.str());
						}
					}
				}
			}
		};

		// Register the callback for MenuOpen function - retry until successful
		// This is essential, so we keep trying silently every 5 seconds until the class is loaded
		bool bMenuOpenHookRegistered = false;
		Logger.LogDiagnostic("Attempting to register MenuOpen hook...");

		// Main loop - keep the logger thread running FOREVER
		// The thread should never exit while the game is running
		while (true)
		{
			// Try to register MenuOpen hook if not already done
			if (!bMenuOpenHookRegistered)
			{
				// Silently attempt to register (suppress error logging)
				if (HookSys.RegisterHook("ui_script_menu_base_C", "MenuOpen", MenuOpenCallback, nullptr, true))
				{
					Logger.LogDiagnostic("[MenuOpen] Hook successfully registered! Waiting for detection...");
					bMenuOpenHookRegistered = true;
				}
				// If it fails, we'll retry in 5 seconds (silently)
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(5000));
			Logger.Flush(); // Periodic flush to ensure logs are written
		}

		// This code should never be reached
		return 0;
	}
	catch (const std::exception& e)
	{
		if (g_Logger)
		{
			g_Logger->LogError(std::string("EXCEPTION: ") + e.what());
		}
		return -1;
	}
	catch (...)
	{
		if (g_Logger)
		{
			g_Logger->LogError("UNKNOWN EXCEPTION");
		}
		return -1;
	}
}

// --- Exported Hook Registration Wrapper for Cross-DLL Access ---
// This function is exported and used by plugins to register hooks with the central HookSystem
// It ensures all plugins use the same singleton instance from plugin_manager_base
extern "C" __declspec(dllexport) bool __cdecl RegisterGlobalHook(
	const char* ClassName,
	const char* FunctionName,
	void* PreCallbackPtr,
	void* PostCallbackPtr)
{
	// Cast void* pointers back to std::function callbacks
	PluginAPI::PreHookCallback* pPre = (PluginAPI::PreHookCallback*)PreCallbackPtr;
	PluginAPI::PostHookCallback* pPost = (PluginAPI::PostHookCallback*)PostCallbackPtr;

	PluginAPI::PreHookCallback PreCallback = pPre ? *pPre : nullptr;
	PluginAPI::PostHookCallback PostCallback = pPost ? *pPost : nullptr;

	return PluginAPI::HookSystem::Get().RegisterHook(ClassName, FunctionName, PreCallback, PostCallback);
}

// --- DLL Entry Point ---
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
		// Load original dwmapi and set up proxy forwarding
		load_original_dwmapi();
		setup_proxy_functions();

		// Create background thread for logger - DllMain returns immediately
		// This prevents blocking the game
		{
			HANDLE hThread = CreateThread(NULL, 0, MainThread, (LPVOID)hModule, 0, NULL);
			if (hThread)
			{
				CloseHandle(hThread); // Close handle, thread continues running
			}
		}
		break;

	case DLL_PROCESS_DETACH:
		// Cleanup - free original dwmapi
		if (g_OriginalDwmapi)
		{
			FreeLibrary(g_OriginalDwmapi);
			g_OriginalDwmapi = nullptr;
		}
		break;

	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		// Not used
		break;
	}

	return TRUE;
}

// --- DWMAPI PROXY LOADER ---
// This DLL acts as a proxy for dwmapi.dll, loading the real dwmapi from System32
// while also running our mod code

void setup_proxy_functions()
{
	if (!g_OriginalDwmapi) return;

	// Load common dwmapi exports (extend as needed)
	// These are the most commonly used dwmapi functions
	const char* exports[] = {
		"DwmEnableBlurBehindWindow",           // 0
		"DwmIsCompositionEnabled",             // 1
		"DwmExtendFrameIntoClientArea",        // 2
		"DwmSetWindowAttribute",               // 3
		"DwmGetWindowAttribute",               // 4
		"DwmFlush",                            // 5
		"DwmSetColorizationColor",             // 6
		"DwmGetCompositionTimingInfo",         // 7
		"DwmGetWindowInteractionFlags",        // 8
		"DwmGetTransformedRect",               // 9
		"DwmSetDxFrameCount",                  // 10
		"DwmTetherContact",                    // 11
		"DwmQueryThumbnailSourceSize",         // 12
		"DwmRegisterThumbnail",                // 13
		"DwmUnregisterThumbnail",              // 14
		"DwmUpdateThumbnailProperties",        // 15
		"DwmDefWindowProc",                    // 16
		"DwmInvalidateIconicBitmaps",          // 17
		"DwmGetDxSharedSurface",               // 18
		"DwmGetLastPresentationTime",          // 19
		"DwmGetGraphicsStreamTransformHint"    // 20
	};

	for (size_t i = 0; i < sizeof(exports) / sizeof(exports[0]); i++)
	{
		mProcs[i] = (uintptr_t)GetProcAddress(g_OriginalDwmapi, exports[i]);
	}
}

void load_original_dwmapi()
{
	wchar_t path[MAX_PATH];
	GetSystemDirectory(path, MAX_PATH);

	std::wstring dll_path = std::wstring(path) + L"\\dwmapi.dll";

	g_OriginalDwmapi = LoadLibraryW(dll_path.c_str());
	if (!g_OriginalDwmapi)
	{
		if (g_Logger)
		{
			g_Logger->LogError("ERROR: Failed to load original dwmapi.dll from System32");
		}
		MessageBoxW(nullptr, L"Failed to load dwmapi.dll from System32", L"Proxy Error", MB_OK | MB_ICONERROR);
		ExitProcess(0);
	}

	if (g_Logger)
	{
		g_Logger->LogDiagnostic("Loaded original dwmapi.dll from System32");
	}
}

// Proxy export functions - these jump to the real dwmapi functions
extern "C"
{
	__declspec(dllexport) void DwmEnableBlurBehindWindow()
	{
		if (mProcs[0]) ((void(*)())mProcs[0])();
	}

	__declspec(dllexport) void DwmIsCompositionEnabled()
	{
		if (mProcs[1]) ((void(*)())mProcs[1])();
	}

	__declspec(dllexport) void DwmExtendFrameIntoClientArea()
	{
		if (mProcs[2]) ((void(*)())mProcs[2])();
	}

	__declspec(dllexport) void DwmSetWindowAttribute()
	{
		if (mProcs[3]) ((void(*)())mProcs[3])();
	}

	__declspec(dllexport) void DwmGetWindowAttribute()
	{
		if (mProcs[4]) ((void(*)())mProcs[4])();
	}

	__declspec(dllexport) void DwmFlush()
	{
		if (mProcs[5]) ((void(*)())mProcs[5])();
	}

	__declspec(dllexport) void DwmSetColorizationColor()
	{
		if (mProcs[6]) ((void(*)())mProcs[6])();
	}

	__declspec(dllexport) void DwmGetCompositionTimingInfo()
	{
		if (mProcs[7]) ((void(*)())mProcs[7])();
	}

	__declspec(dllexport) void DwmGetWindowInteractionFlags()
	{
		if (mProcs[8]) ((void(*)())mProcs[8])();
	}

	__declspec(dllexport) void DwmGetTransformedRect()
	{
		if (mProcs[9]) ((void(*)())mProcs[9])();
	}

	__declspec(dllexport) void DwmSetDxFrameCount()
	{
		if (mProcs[10]) ((void(*)())mProcs[10])();
	}

	__declspec(dllexport) void DwmTetherContact()
	{
		if (mProcs[11]) ((void(*)())mProcs[11])();
	}

	__declspec(dllexport) void DwmQueryThumbnailSourceSize()
	{
		if (mProcs[12]) ((void(*)())mProcs[12])();
	}

	__declspec(dllexport) void DwmRegisterThumbnail()
	{
		if (mProcs[13]) ((void(*)())mProcs[13])();
	}

	__declspec(dllexport) void DwmUnregisterThumbnail()
	{
		if (mProcs[14]) ((void(*)())mProcs[14])();
	}

	__declspec(dllexport) void DwmUpdateThumbnailProperties()
	{
		if (mProcs[15]) ((void(*)())mProcs[15])();
	}

	__declspec(dllexport) void DwmDefWindowProc()
	{
		if (mProcs[16]) ((void(*)())mProcs[16])();
	}

	__declspec(dllexport) void DwmInvalidateIconicBitmaps()
	{
		if (mProcs[17]) ((void(*)())mProcs[17])();
	}

	__declspec(dllexport) void DwmGetDxSharedSurface()
	{
		if (mProcs[18]) ((void(*)())mProcs[18])();
	}

	__declspec(dllexport) void DwmGetLastPresentationTime()
	{
		if (mProcs[19]) ((void(*)())mProcs[19])();
	}

	__declspec(dllexport) void DwmGetGraphicsStreamTransformHint()
	{
		if (mProcs[20]) ((void(*)())mProcs[20])();
	}
}
