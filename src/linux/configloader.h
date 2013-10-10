#ifndef ConfigLoader_h_
#define ConfigLoader_h_

#include <strings.h>							// for strcasecmp
#include <string>								// for std::string
#include <vector>								// for std::vector
#include <map>									// for std::map

struct stringInsensitiveCompare { 
	bool operator() (const std::string& a, const std::string& b) const{
		return strcasecmp(a.c_str(), b.c_str()) < 0;
	}
};

struct PluginConfig{
	std::string		configPath;
	std::string		pluginName;
	bool 			diagnosticMode;
	
	std::string 	sandboxPath;

	std::string 	winePath;
	std::string		wineArch;
	std::string 	winePrefix;
	std::string		wineDLLOverrides;

	std::string 	dllPath;
	std::string 	dllName;
	std::string 	regKey;
	std::string 	pluginLoaderPath;
	std::string 	gccRuntimeDLLs;

	bool 			windowlessMode;
	bool			embed;
	std::string 	fakeVersion;
	std::map<std::string, std::string, stringInsensitiveCompare> overwriteArgs;

	std::string		dependencyInstaller;
	std::vector<std::string> dependencies;
	bool 			quietInstallation;

	bool 			eventAsyncCall;
	bool			operaDetection;
	std::string 	executeJavascript;

	std::string		silverlightGraphicDriverCheck;

	bool 			experimental_usermodeTimer;
};

bool loadConfig(PluginConfig &config);

#endif // ConfigLoader_h_
