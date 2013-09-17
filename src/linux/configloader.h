#ifndef ConfigLoader_h_
#define ConfigLoader_h_

#include <string>								// for std::string
#include <map>									// for std::map
#include <strings.h>							// for strcasecmp

struct stringInsensitiveCompare { 
	bool operator() (const std::string& a, const std::string& b) const{
		return strcasecmp(a.c_str(), b.c_str()) < 0;
	}
};

struct PluginConfig{
	std::string		configPath;
	bool 			diagnosticMode;
	
	std::string 	sandboxPath;

	std::string 	winePath;
	std::string		wineArch;
	std::string 	winePrefix;
	std::string		wineDLLOverrides;

	std::string 	dllPath; //we may need to extend this to a vector in the future
	std::string 	dllName;
	std::string 	pluginLoaderPath;
	std::string 	gccRuntimeDLLs;

	bool 			windowlessMode;
	bool			embed;
	std::string 	fakeVersion;
	std::map<std::string, std::string, stringInsensitiveCompare> overwriteArgs;

	std::string		dependencyInstaller;
	std::vector<std::string> dependencies;
	bool 			quietInstallation;

	std::string		graphicDriverCheck;

	bool 			eventAsyncCall;
	bool			operaDetection;
	std::string 	executeJavascript;

	bool 			experimental_usermodeTimer;
};

std::string getFileName(const std::string &path);
std::string getHomeDirectory();
std::string trim(std::string str);
bool loadConfig(PluginConfig &config);

#endif // ConfigLoader_h_
