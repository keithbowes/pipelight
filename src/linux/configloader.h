#ifndef ConfigLoader_h_
#define ConfigLoader_h_

#include <string>								// for std::string
#include <map>									// for std::map

struct PluginConfig{
	std::string 	winePath;
	std::string 	winePrefix;
	std::string 	dllPath; //we may need to extend this to a vector in the future
	std::string 	dllName;
	std::string 	pluginLoaderPath;
	bool 			windowlessMode;
	bool			embed;
	std::string 	fakeVersion;
	std::string 	gccRuntimeDLLs;
	std::string		silverlightInstaller;
	std::string		wineBrowserInstaller;

	std::map<std::string, std::string> overwriteArgs;
};

std::string getFileName(const std::string &path);
std::string getHomeDirectory();
std::string trim(std::string str);
bool loadConfig(PluginConfig &config);

#define DEFAULT_GCC_RUNTIME_DLL_SEARCH_PATH "/usr/lib/gcc/i686-w64-mingw32/4.6/"

#endif // ConfigLoader_h_
