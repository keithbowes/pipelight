
#include <algorithm>							// for std::transform
#include <iostream>								// for std::cerr
#include <map>									// for std::map
#include <stdexcept>							// for std::runtime_error
#include <fstream>								// for std::ifstream
#include <string>								// for std::string
#include <map>									// for std::map

#include <pwd.h>								// for getpwuid
#include <sys/types.h>
#include <unistd.h>								// for dladdr

#include "configloader.h"
#include "basicplugin.h"

std::string getFileName(const std::string &path){

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

bool splitConfigValue(std::string line, std::string &key, std::string &value){

	size_t pos;

	line = trim(line);

	//find delimiter
	pos = line.find_first_of("=");
	if (pos == std::string::npos)
		return false;

	//no key found
	if(pos == 0)
		return false;

	//no value found
	if(pos >= line.length()-1)
		return false;

	key 	= trim(line.substr(0, pos));
	value 	= trim(line.substr(pos+1, std::string::npos));

	return true;
}

// If abort != 0 then this reads until the specific character occurs or the string terminates
// If abort == 0 then the function aborts on the first non-variable character
std::string readUntil(const char* &str, char abort = 0){
	const char *start = str;

	while(*str){
		if( *str == abort || (!abort && !( (*str >= 'A' && *str <= 'Z') || (*str >= 'a' && *str <= 'z') || (*str >= '0' && *str <= '9') || *str == '_' ) ) ){
			break;
		}

		str++;
	}

	return std::string(start, str-start);
}

std::string replaceVariables(const std::map<std::string, std::string> &variables, const char* str){

	std::string output 	= "";
	std::string varname = "";
	std::map<std::string, std::string>::const_iterator it;

	while(*str){

		if(*str == '$'){ // Not escaped
			str++;

			if(*str == '$'){ // Escape
				output.append(1, *str);
				str++;
				continue;

			}else if(*str == '{'){ // In brackets
				str++;

				varname = readUntil(str, '}');

				if(*str != '}'){
					throw std::runtime_error("Expected closing tag } at end of line in config file");
				}
				str++; // Skip over it

			}else{
				varname = readUntil(str);
			}

			std::transform(varname.begin(), varname.end(), varname.begin(), ::tolower);
			it = variables.find("$" + varname);
			if( it != variables.end() ){
				output.append( it->second ); // Append value
			}else{
				throw std::runtime_error("Variable not found: " + varname);
			}

		}else{
			output.append(1, *str);
			str++;
		}
	}

	return output;
}

// Tries to open the config and returns true on success
bool openConfig(std::ifstream &configFile){
	std::string configPath;
	std::string homeDir = getHomeDirectory();

	configPath = getEnvironmentString("PIPELIGHT_CONFIG");
	if(configPath != ""){
		std::cerr << "[PIPELIGHT] Trying to load config file from " << configPath << std::endl;
		configFile.open(configPath);
		if(configFile.is_open()) return true;
	}

	if(homeDir != ""){
		configPath = homeDir + "/.config/pipelight";
		std::cerr << "[PIPELIGHT] Trying to load config file from " << configPath << std::endl;
		configFile.open(configPath);
		if(configFile.is_open()) return true;
	}


	configPath = PREFIX "/share/pipelight/pipelight";
	std::cerr << "[PIPELIGHT] Trying to load default config file from " << configPath << std::endl;
	configFile.open(configPath);
	if(configFile.is_open()) return true;

	return false;
}

// Does the actual parsing stuff
bool loadConfig(PluginConfig &config){

	// Variables which can be used inside the config file
	std::map<std::string, std::string> variables;

	// Add homedir to variables
	std::string homeDir = getHomeDirectory();
	if(homeDir != "") variables["$home"] = homeDir;

	// Initialize config variables with default values
	config.wineArch 			= "win32";
	config.winePath 			= "wine";
	config.winePrefix 			= "";
	config.dllPath 				= "";
	config.dllName 				= "";
	config.pluginLoaderPath 	= "";
	config.windowlessMode 		= false; // Default is window mode, as windowless currently is still a bit buggy
	config.embed 				= true;
	config.fakeVersion			= "";
	config.gccRuntimeDLLs		= "";
	config.dependencyInstaller 	= "";
	config.silverlightVersion 	= "";
	config.eventAsyncCall		= false;
	config.experimental_usermodeTimer = false;

	std::ifstream 	configFile;

	if(!openConfig(configFile)){
		std::cerr << "[PIPELIGHT] Couldn't find any configuration file" << std::endl;
		return false;
	}


	while (configFile.good()){
		std::string line;
		getline(configFile, line);

		line = trim(line);
		if(line.length() == 0)
			continue;

		size_t pos;

		//strip comments
		pos = line.find_first_of("#"); 
		if (pos != std::string::npos){
			
			if(pos == 0)
				continue;

			line = line.substr(0, pos);
		}

		std::string key;
		std::string value;

		if(!splitConfigValue(line, key, value))
			continue;

		// convert key to lower case
		std::transform(key.begin(), key.end(), key.begin(), ::tolower);

		// replace $var and ${var} inside of value
		value = replaceVariables(variables, value.c_str());

		// check for variables
		// splitConfiguration takes care that the key has at least one character
		if(key[0] == '$'){
			variables[key] = value;
			continue;
		}

		if(key == "winepath"){
			config.winePath = value;

		}else if(key == "winearch") {
			config.wineArch = value;
		}else if(key == "wineprefix"){
			config.winePrefix = value;

		}else if(key == "dllpath"){
			config.dllPath = value;	

		}else if(key == "dllname"){
			config.dllName = value;

		}else if(key == "gccruntimedlls"){
			config.gccRuntimeDLLs = value;

		}else if(key == "pluginloaderpath"){
			config.pluginLoaderPath = value;

		}else if(key == "windowlessmode"){
			std::transform(value.begin(), value.end(), value.begin(), ::tolower);
			config.windowlessMode = (value == "true" || value == "yes");

		}else if(key == "embed"){
			std::transform(value.begin(), value.end(), value.begin(), ::tolower);
			config.embed = (value == "true" || value == "yes");

		}else if(key == "fakeversion"){
			config.fakeVersion = value;

		}else if(key == "overwritearg"){

			std::string argKey;
			std::string argValue;

			if(!splitConfigValue(value, argKey, argValue))
				continue;

			std::transform(argKey.begin(), argKey.end(), argKey.begin(), ::tolower);
			config.overwriteArgs[argKey] = argValue;

		}else if(key == "dependencyinstaller"){
			config.dependencyInstaller = value;

		}else if(key == "silverlightversion"){
			config.silverlightVersion = value;

		}else if(key == "eventasynccall"){
			std::transform(value.begin(), value.end(), value.begin(), ::tolower);
			config.eventAsyncCall = (value == "true" || value == "yes");

		}else if(key == "experimental-usermodetimer"){
			std::transform(value.begin(), value.end(), value.begin(), ::tolower);
			config.experimental_usermodeTimer = (value == "true" || value == "yes");

		}else{
			std::cerr << "[PIPELIGHT] Unrecognized config key: " << key << std::endl;
		}

	}

	/*
	std::cerr << "[PIPELIGHT] wineArch: " << config.wineArch << std::endl;
	std::cerr << "[PIPELIGHT] winePath: " << config.winePath << std::endl;
	std::cerr << "[PIPELIGHT] winePrefix: " << config.winePrefix << std::endl;
	std::cerr << "[PIPELIGHT] dllPath: " << config.dllPath << std::endl;
	std::cerr << "[PIPELIGHT] dllName: " << config.dllName << std::endl;
	std::cerr << "[PIPELIGHT] pluginLoaderPath: " << config.pluginLoaderPath << std::endl;
	*/
	
	return true;
}
