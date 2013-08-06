#include "configloader.h"
#include "basicplugin.h"
#include <algorithm>
#include <pwd.h>
#include <sys/types.h>
#include <dlfcn.h>


std::string getFileName(std::string path){

	std::string result = path;

	size_t pos;

	pos = result.find_last_of("/"); 
	if (pos != std::string::npos){
		
		//Check if this ends with "/" i.e. a directory
		if(++pos >= result.length())
			return "";

		result = result.substr(pos, std::string::npos);
	}

	pos = result.find_last_of("."); 
	if (pos != std::string::npos){

		//Check if it starts with "." i.e. only an extension or hidden file
		if(pos == 0)
			return "";

		result = result.substr(0, pos);
	}

	return result;
}

std::string getHomeDirectory(){

	char *homeDir = getenv("HOME");
	if(homeDir)
		return std::string(homeDir);
	
	// Do we need getpwuid_r() here ?
	struct passwd* info = getpwuid(getuid());
	if(!info)
		return "";
	
	if(!info->pw_dir)
		return "";

	return std::string(info->pw_dir);

}

std::string trim(std::string str){

	size_t pos;
	pos = str.find_first_not_of(" \f\n\r\t\v");
	if (pos != std::string::npos){
		str = str.substr(pos, std::string::npos);
	}

	pos = str.find_last_not_of(" \f\n\r\t\v");
	if (pos != std::string::npos){
		str = str.substr(0, pos+1);
	}

	return str;
}

bool loadConfig(PluginConfig &config, void *function){

	Dl_info dl_info;
	if(!dladdr(function, &dl_info))
		return false;

	if(!dl_info.dli_fname)
		return false;

	std::string filename = getFileName(std::string(dl_info.dli_fname));
	
	if(filename == "")
		return false;

	std::string homeDir = getHomeDirectory();

	if(homeDir == "")
		return false;

	std::string configPath = homeDir + "/.pipelight/" + filename;
	output << "Trying to load config from " << configPath << std::endl;

	std::ifstream configFile(configPath);

	if(!configFile.is_open())
		return false;

	while (configFile.good()){
		std::string line;

		getline(configFile, line);

		size_t pos;

		//strip comments
		pos = line.find_first_of("#"); 
		if (pos != std::string::npos){
			
			if(pos == 0)
				continue;

			line = line.substr(0, pos);
		}

		line = trim(line);

		//find delimiter
		pos = line.find_first_of("="); 
		if (pos == std::string::npos)
			continue;

		//no key found
		if(pos == 0)
			continue;

		//no value found
		if(pos >= line.length()-1)
			continue;

		std::string key 	= trim(line.substr(0, pos));
		std::string value 	= trim(line.substr(pos+1, std::string::npos));

		//convert key to lower case
		std::transform(key.begin(), key.end(), key.begin(), ::tolower);

		if(key == "winepath"){
			config.winePath = value;
		}else if(key == "wineprefix"){
			config.winePrefix = value;
		}else if(key == "dllpath"){
			config.dllPath = value;			
		}else if(key == "dllname"){
			config.dllName = value;		
		}else if(key == "pluginloaderpath"){
			config.pluginLoaderPath = value;		
		}

	}

	//Check for required arguments
	if (config.dllPath == "" || config.dllName == "" || config.pluginLoaderPath == "")
		return false;

	//Set default values íf the other arguments are missing
	if(config.winePath == "")
		config.winePath = "wine";

	/*
	output << "winePath: " << config.winePath << std::endl;
	output << "winePrefix: " << config.winePrefix << std::endl;
	output << "dllPath: " << config.dllPath << std::endl;
	output << "dllName: " << config.dllName << std::endl;
	output << "pluginLoaderPath: " << config.pluginLoaderPath << std::endl;
	*/
	
	return true;
}
