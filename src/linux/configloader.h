#ifndef ConfigLoader_h_
#define ConfigLoader_h_

#include <string>								// for std::string
#include <map>									// for std::map

struct PluginConfig{
	std::string 	winePath;
	bool			winePathIsDeprecated;

	std::string		wineArch;
	std::string 	winePrefix;
	std::string		wineDLLOverrides;
	std::string 	dllPath; //we may need to extend this to a vector in the future
	std::string 	dllName;
	std::string 	pluginLoaderPath;
	bool 			windowlessMode;
	bool			embed;
	std::string 	fakeVersion;
	std::string 	gccRuntimeDLLs;
	std::string		dependencyInstaller;
	std::string		silverlightVersion;

	bool 			eventAsyncCall;
	bool			operaDetection;

	bool 			experimental_usermodeTimer;

	std::map<std::string, std::string> overwriteArgs;
};

std::string getFileName(const std::string &path);
std::string getHomeDirectory();
std::string trim(std::string str);
bool loadConfig(PluginConfig &config);

#endif // ConfigLoader_h_
