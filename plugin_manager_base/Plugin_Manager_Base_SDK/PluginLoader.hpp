#pragma once

#include <Windows.h>
#include <vector>
#include <string>
#include <functional>
#include <memory>

// Forward declarations
namespace SDK
{
	class UObject;
	class UFunction;
}

/**
 * Plugin Interface - Mods implement this to hook into MenuOpen
 */
class IPlugin
{
public:
	virtual ~IPlugin() = default;

	/// Called when plugin is loaded
	virtual void OnPluginLoad() = 0;

	/// Called when MenuOpen is invoked on ui_script_menu_base_C
	/// @param Object - The ui_script_menu_base_C instance
	/// @param Function - The MenuOpen function being called
	/// @param Params - Function parameters (void pointer, cast as needed)
	virtual void OnMenuOpen(SDK::UObject* Object, SDK::UFunction* Function, void* Params) = 0;

	/// Called when plugin is unloaded
	virtual void OnPluginUnload() = 0;

	/// Get plugin name/version info
	virtual const char* GetPluginName() const = 0;
};

/**
 * Plugin entry point - plugins must export this function:
 * extern "C" __declspec(dllexport) IPlugin* CreatePlugin()
 */
typedef IPlugin* (*CreatePluginFn)();
typedef void (*DestroyPluginFn)(IPlugin*);

/**
 * Manages plugin loading and MenuOpen hooking
 */
class PluginLoader
{
public:
	static PluginLoader& Get()
	{
		static PluginLoader Instance;
		return Instance;
	}

	/// Initialize the plugin system and hook MenuOpen
	/// @param PluginDirectory - Directory to load plugins from (default: "plugins")
	bool Initialize(const std::string& PluginDirectory = "plugins");

	/// Load a single plugin DLL
	/// @param PluginPath - Full path to the plugin DLL
	bool LoadPlugin(const std::string& PluginPath);

	/// Unload all plugins and cleanup
	void Shutdown();

	/// Hook MenuOpen function for ui_script_menu_base_C
	bool HookMenuOpen();

	/// Get number of loaded plugins
	size_t GetLoadedPluginCount() const { return LoadedPlugins.size(); }

	/// Get plugin by index
	IPlugin* GetPlugin(size_t Index) const
	{
		if (Index < LoadedPlugins.size())
			return LoadedPlugins[Index].get();
		return nullptr;
	}

private:
	PluginLoader() = default;
	~PluginLoader() = default;

	PluginLoader(const PluginLoader&) = delete;
	PluginLoader& operator=(const PluginLoader&) = delete;

	// Structure to hold loaded plugin info
	struct LoadedPlugin
	{
		HMODULE DllHandle = nullptr;
		std::unique_ptr<IPlugin> Instance;
		std::string FilePath;

		~LoadedPlugin()
		{
			if (Instance)
				Instance.reset();
			if (DllHandle)
				FreeLibrary(DllHandle);
		}
	};

	std::vector<LoadedPlugin> LoadedPlugins;
	bool bMenuOpenHooked = false;
	std::string PluginDirectory;
};

/**
 * Helper function to call all loaded plugins' OnMenuOpen callbacks
 */
inline void CallAllPluginsOnMenuOpen(SDK::UObject* Object, SDK::UFunction* Function, void* Params)
{
	PluginLoader& Loader = PluginLoader::Get();
	for (size_t i = 0; i < Loader.GetLoadedPluginCount(); ++i)
	{
		IPlugin* Plugin = Loader.GetPlugin(i);
		if (Plugin)
		{
			try
			{
				Plugin->OnMenuOpen(Object, Function, Params);
			}
			catch (...)
			{
				// Silently catch exceptions from plugins to prevent crash
			}
		}
	}
}
