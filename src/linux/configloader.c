#include <sys/types.h>
#include <sys/stat.h>							// for stat
#include <unistd.h>								// for dladdr
#include <pwd.h>								// for getpwuid
#include <algorithm>							// for std::transform
#include <map>									// for std::map
#include <fstream>								// for std::ifstream
#include <string>								// for std::string
#include <map>									// for std::map
#include <dlfcn.h>

#include "../common/common.h"
#include "configloader.h"

static std::string getHomeDirectory(){
	char *homeDir = getenv("HOME");
	if (homeDir)
		return std::string(homeDir);
	
	// Do we need getpwuid_r() here ?
	struct passwd* info = getpwuid(getuid());
	if (!info || !info->pw_dir)
		return "";
	
	return std::string(info->pw_dir);
}

// see http://source.winehq.org/source/libs/wine/config.c?v=wine-1.7.3#L268
static std::string getWineUser(){
	struct passwd *info = getpwuid(getuid());
	if (info && info->pw_name)
		return std::string(info->pw_name);

	char uid_string[32];
	sprintf(uid_string, "%lu", (unsigned long)getuid());
	return std::string(uid_string);
}

static void getConfigNameFromLibrary(std::string &configName, std::string &configEnv, std::string &pluginName){
	Dl_info 	libinfo;
	size_t 		pos;

	// get full path
	if (!dladdr((void*)getConfigNameFromLibrary, &libinfo) || !libinfo.dli_fname){
		configName = "pipelight";
		configEnv  = "PIPELIGHT_CONFIG";
		pluginName = "";
		return;

	}

	pluginName = std::string(libinfo.dli_fname);

	// strip directory name
	if ((pos = pluginName.find_last_of('/')) != std::string::npos)
		pluginName = pluginName.substr(pos + 1, std::string::npos);

	// strip extension (.so)
	if ((pos = pluginName.find_last_of('.')) != std::string::npos)
		pluginName = pluginName.substr(0, pos);

	// get last component
	pos = pluginName.find_last_of('-');
	pluginName = (pos != std::string::npos) ? pluginName.substr(pos + 1, std::string::npos) : "";

	if (pluginName.length()){
		configEnv = pluginName;

		// convert to lower/upper case
		std::transform(pluginName.begin(), pluginName.end(), pluginName.begin(), ::tolower);
		std::transform(configEnv.begin(), configEnv.end(), configEnv.begin(), ::toupper);

		configName = "pipelight-" + pluginName;
		configEnv  = "PIPELIGHT_" + configEnv + "_CONFIG";
		/* pluginName already set */
		return;
	}


	configName = "pipelight";
	configEnv  = "PIPELIGHT_CONFIG";
	pluginName = "";
	return;
}

static bool splitConfigValue(std::string line, std::string &key, std::string &value){
	size_t pos;
	line = trim(line);

	// find delimiter
	if ((pos = line.find_first_of("=")) == std::string::npos)
		return false;

	key 	= trim(line.substr(0, pos));
	value 	= trim(line.substr(pos + 1, std::string::npos));

	return (key != "");
}

/*
	If abort != 0 then this reads until the specific character occurs or the string terminates
	If abort == 0 then the function aborts on the first non-variable character
*/
static std::string readUntil(const char* &str, char abort = 0){
	const char *start = str;
	char c;

	while (	(c = *str) && 						/* more characters? */	
			(c != abort) &&						/* not the abort character? */
			(abort || isAlphaNumericChar(c)) )	/* if no abort character given, then it should be alphanumeric */
		str++;

	return std::string(start, str-start);
}

static std::string replaceVariables(const std::map<std::string, std::string> &variables, const char* str){
	std::string output 	= "";
	std::string varname = "";
	std::map<std::string, std::string>::const_iterator it;

	while (*str){

		if (*str == '$'){ // Not escaped
			str++;

			if (*str == '$'){ // Escape
				output.append(1, *str);
				str++;
				continue;

			}else if (*str == '{'){ // In brackets
				str++;
				varname = readUntil(str, '}');

				DBG_ASSERT(*str == '}', "expected closing tag } at end of line.");
				str++; // Skip over it

			}else{
				varname = readUntil(str);
			}

			std::transform(varname.begin(), varname.end(), varname.begin(), ::tolower);
			it = variables.find("$" + varname);

			DBG_ASSERT(it != variables.end(), "variable '%s' not found.", varname.c_str());
			output.append( it->second ); // Append value

		}else{
			output.append(1, *str);
			str++;
		}
	}

	return output;
}

/* Tries to open the config and returns true on success */
static  bool openConfig(std::ifstream &configFile, std::string &configPath, std::string &pluginName){
	std::string configName, configEnv, homeDir 	= getHomeDirectory();
	getConfigNameFromLibrary(configName, configEnv, pluginName);

	/* use environment variable */
	if (configEnv != ""){
		DBG_INFO("checking environment variable %s.", configEnv.c_str());

		if ((configPath = getEnvironmentString(configEnv)) != ""){
			DBG_INFO("trying to load config file from '%s'.", configPath.c_str());
			configFile.open(configPath);
			if (configFile.is_open()) return true;
		}

	}

	if (configName != ""){
		DBG_INFO("searching for config file %s.", configName.c_str());

		/* local config */
		if (homeDir != ""){
			configPath = homeDir + "/.config/" + configName;
			DBG_INFO("trying to load config file from '%s'.", configPath.c_str());
			configFile.open(configPath);
			if (configFile.is_open()) return true;
		}

		/* etc config */
		configPath = "/etc/" + configName;
		DBG_INFO("trying to load config file from '%s'.", configPath.c_str());
		configFile.open(configPath);
		if (configFile.is_open()) return true;

		/* default config */
		configPath = PREFIX "/share/pipelight/" + configName;
		DBG_INFO("trying to load config file from '%s'.", configPath.c_str());
		configFile.open(configPath);
		if (configFile.is_open()) return true;

	}

	return false;
}

/* Does the actual parsing stuff */
bool loadConfig(PluginConfig &config){

	/* Variables which can be used inside the config file */
	std::map<std::string, std::string> variables;

	/* Add homedir to variables */
	std::string homeDir = getHomeDirectory();
	if (homeDir != "") variables["$home"] = homeDir;

	variables["$wineuser"] = getWineUser(); /* getWineUser() never returns an empty string */

	/* Initialize config variables with default values */
	config.configPath			= "";
	config.pluginName 			= "";
	config.diagnosticMode 		= false;

	config.sandboxPath			= "";

	config.winePath 			= "wine";
	config.wineArch 			= "win32";
	config.winePrefix 			= "";
	config.wineDLLOverrides		= "mscoree,mshtml,winegstreamer,winemenubuilder.exe="; /* prevent Installation of Geck & Mono by default */

	config.dllPath 				= "";
	config.dllName 				= "";
	config.regKey 				= "";
	config.pluginLoaderPath 	= "";
	config.gccRuntimeDLLs		= "";

	config.windowlessMode 		= false;
	config.embed 				= true;
	config.fakeVersion			= "";
	config.overwriteArgs.clear();
	
	config.dependencyInstaller 	= "";
	config.dependencies.clear();
	config.quietInstallation 	= true;

	config.eventAsyncCall		= false;
	config.operaDetection 		= true;
	config.executeJavascript 	= "";

	config.silverlightGraphicDriverCheck = "";

	config.experimental_usermodeTimer = false;


	std::ifstream 	configFile;

	if (!openConfig(configFile, config.configPath, config.pluginName)){
		DBG_ERROR("couldn't find any configuration file.");
		return false;
	}

	while (configFile.good()){
		std::string line;
		getline(configFile, line);

		line = trim(line);
		if (line.length() == 0)
			continue;

		size_t pos;

		//strip comments
		if ((pos = line.find_first_of("#")) != std::string::npos){
			if (pos == 0)
				continue;

			line = line.substr(0, pos);
		}

		std::string key;
		std::string value;

		if (!splitConfigValue(line, key, value))
			continue;

		// convert key to lower case
		std::transform(key.begin(), key.end(), key.begin(), ::tolower);

		// replace $var and ${var} inside of value
		value = replaceVariables(variables, value.c_str());

		// check for variables
		// splitConfiguration takes care that the key has at least one character
		if (key[0] == '$'){
			variables[key] = value;
			continue;
		}

		if (	key == "diagnosticmode"){
			std::transform(value.begin(), value.end(), value.begin(), ::tolower);
			config.diagnosticMode = (value == "true" || value == "yes");

		}else if (key == "sandboxpath"){
			config.sandboxPath = value;

		}else if (key == "winepath"){
			config.winePath = value;

			if (!checkIsFile(config.winePath))
				config.winePath += "/bin/wine";

		}else if (key == "winearch") {
			config.wineArch = value;

		}else if (key == "wineprefix"){
			config.winePrefix = value;

		}else if (key == "winedlloverrides"){
			config.wineDLLOverrides = value;

		}else if (key == "dllpath"){
			config.dllPath = value;	

		}else if (key == "dllname"){
			config.dllName = value;

		}else if (key == "regkey"){
			config.regKey = value;

		}else if (key == "pluginloaderpath"){
			config.pluginLoaderPath = value;

		}else if (key == "gccruntimedlls"){
			config.gccRuntimeDLLs = value;

		}else if (key == "windowlessmode"){
			std::transform(value.begin(), value.end(), value.begin(), ::tolower);
			config.windowlessMode = (value == "true" || value == "yes");

		}else if (key == "embed"){
			std::transform(value.begin(), value.end(), value.begin(), ::tolower);
			config.embed = (value == "true" || value == "yes");

		}else if (key == "fakeversion"){
			config.fakeVersion = value;

		}else if (key == "overwritearg"){
			std::string argKey;
			std::string argValue;

			if (!splitConfigValue(value, argKey, argValue))
				continue;

			config.overwriteArgs[argKey] = argValue;

		}else if (key == "dependencyinstaller"){
			config.dependencyInstaller = value;

		}else if (key == "silverlightversion"){
			DBG_WARN("the configuration parameter silverlightVersion is deprecated.");
			config.dependencies.insert(config.dependencies.begin(), "wine-" + value + "-installer");

		}else if (key == "dependency"){
			if(value != "") config.dependencies.push_back(value);

		}else if (key == "quietinstallation"){
			std::transform(value.begin(), value.end(), value.begin(), ::tolower);
			config.quietInstallation = (value == "true" || value == "yes");

		}else if (key == "silverlightgraphicdrivercheck" || key == "graphicdrivercheck"){
			if (key == "graphicdrivercheck")
				DBG_WARN("the configuration parameter graphicDriverCheck is deprecated.");
			config.silverlightGraphicDriverCheck = value;

		}else if (key == "eventasynccall"){
			std::transform(value.begin(), value.end(), value.begin(), ::tolower);
			config.eventAsyncCall = (value == "true" || value == "yes");

		}else if (key == "operadetection"){
			std::transform(value.begin(), value.end(), value.begin(), ::tolower);
			config.operaDetection = (value == "true" || value == "yes");

		}else if (key == "executejavascript"){
			config.executeJavascript += value + "\n";

		}else if (key == "experimental-usermodetimer"){
			std::transform(value.begin(), value.end(), value.begin(), ::tolower);
			config.experimental_usermodeTimer = (value == "true" || value == "yes");

		}else{
			DBG_WARN("unrecognized configuration key '%s'.", key.c_str());
		}

	}

	/* set the multiplugin name */
	setMultiPluginName(config.pluginName);

	return true;
}
