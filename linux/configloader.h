#pragma once
#include <string>

struct PluginConfig{
	std::string winePath;
	std::string winePrefix;
	std::string dllPath; //we may need to extend this to a vector in the future
	std::string dllName;
	std::string pluginLoaderPath;
	bool 		windowlessMode;
	bool		embed;
};

std::string getFileName(std::string path);
std::string getHomeDirectory();
std::string trim(std::string str);
bool loadConfig(PluginConfig &config, void *function);
