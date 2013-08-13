#pragma once
#include <string>
#include <map>

struct PluginConfig{
	std::string 	winePath;
	std::string 	winePrefix;
	std::string 	dllPath; //we may need to extend this to a vector in the future
	std::string 	dllName;
	std::string 	pluginLoaderPath;
	bool 			windowlessMode;
	bool			embed;
	bool			forceReload;
	bool			killPlugin;
	std::string 	fakeVersion;
	std::string 	gccRuntimeDLLs;
	std::string		silverlightInstaller;
	std::string		wineBrowserInstaller;

	std::map<std::string, std::string> overwriteArgs;
};

std::string getFileName(const std::string &path);
std::string getHomeDirectory();
std::string trim(std::string str);
bool loadConfig(PluginConfig &config, void *function);

#define DEFAULT_GCC_RUNTIME_DLL_SEARCH_PATH "/usr/lib/gcc/i686-w64-mingw32/4.6/"