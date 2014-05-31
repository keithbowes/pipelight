/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is fds-team.de code.
 *
 * The Initial Developer of the Original Code is
 * Michael Müller <michael@fds-team.de>
 * Portions created by the Initial Developer are Copyright (C) 2013
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michael Müller <michael@fds-team.de>
 *   Sebastian Lackner <sebastian@fds-team.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

/* defined in basicplugin.h, but we don't want to pull in the whole header */
extern bool checkSilverlightGraphicDriver();

/* getHomeDirectory */
static std::string getHomeDirectory(){
	char *homeDir = getenv("HOME");
	if (homeDir)
		return std::string(homeDir);

	/* Do we need getpwuid_r() here ? */
	struct passwd* info = getpwuid(getuid());
	if (!info || !info->pw_dir)
		return "";

	return std::string(info->pw_dir);
}

/* getWineUser, see http://source.winehq.org/source/libs/wine/config.c?v=wine-1.7.3#L268 */
static std::string getWineUser(){
	struct passwd *info = getpwuid(getuid());
	if (info && info->pw_name)
		return std::string(info->pw_name);

	char uid_string[32];
	sprintf(uid_string, "%lu", (unsigned long)getuid());
	return std::string(uid_string);
}

/* getConfigNameFromLibrary */
static void getConfigNameFromLibrary(std::string &configName, std::string &configEnv, std::string &pluginName){
	Dl_info		libinfo;
	size_t		pos;

	/* get full path for this library, if not fallback to old behaviour (shouldn't occur) */
	if (!dladdr((void*)getConfigNameFromLibrary, &libinfo) || !libinfo.dli_fname){
		configName = "pipelight";
		configEnv  = "PIPELIGHT_CONFIG";
		pluginName = "";
		return;
	}

	pluginName = std::string(libinfo.dli_fname);

	/* strip directory name */
	if ((pos = pluginName.find_last_of('/')) != std::string::npos)
		pluginName = pluginName.substr(pos + 1, std::string::npos);

	/* strip extension (.so) */
	if ((pos = pluginName.find_last_of('.')) != std::string::npos)
		pluginName = pluginName.substr(0, pos);

	/* get last component */
	pos = pluginName.find_first_of('-');
	pluginName = (pos != std::string::npos) ? pluginName.substr(pos + 1, std::string::npos) : "";

	/* extracting path successful */
	if (pluginName.length()){
		configEnv = pluginName;

		/* convert to lower/upper case and clean up */
		std::transform(pluginName.begin(), pluginName.end(), pluginName.begin(), c_tolower);
		std::transform(configEnv.begin(), configEnv.end(), configEnv.begin(), c_toupper);
		std::replace(configEnv.begin(), configEnv.end(), '.', '_');

		configName = "pipelight-" + pluginName;
		configEnv  = "PIPELIGHT_" + configEnv + "_CONFIG";
		return;
	}

	/* failed (shouldn't occur) */
	configName = "pipelight";
	configEnv  = "PIPELIGHT_CONFIG";
	pluginName = "";
	return;
}

static bool splitConfigValue(std::string line, std::string &key, std::string &value, std::string splitChar = "="){
	size_t pos;
	line = trim(line);

	/* find delimiter */
	if ((pos = line.find_first_of(splitChar)) == std::string::npos)
		return false;

	key		= trim(line.substr(0, pos));
	value	= trim(line.substr(pos + 1, std::string::npos));
	return (key != "");
}

/*
	If abort != 0 then this reads until the specific character occurs or the string terminates
	If abort == 0 then the function aborts on the first non-variable character
*/
static std::string readUntil(const char* &str, char abort = 0){
	const char *start = str;
	char c;

	while (	(c = *str) &&						/* more characters? */
			(c != abort) &&						/* not the abort character? */
			(abort || c_alphanumchar(c)) )		/* if no abort character given, then it should be alphanumeric */
		str++;

	return std::string(start, str-start);
}

/* replaceVariables */
static std::string replaceVariables(const std::map<std::string, std::string> &variables, const char* str){
	std::string output	= "";
	std::string varname = "";
	std::map<std::string, std::string>::const_iterator it;

	while (*str){

		/* variables */
		if (*str == '$'){
			str++;

			if (*str == '$'){ /* escape */
				output.append(1, *str);
				str++;
				continue;

			}else if (*str == '{'){ /* in brackets */
				str++;
				varname = readUntil(str, '}');

				DBG_ASSERT(*str == '}', "expected closing tag } at end of line.");
				str++; /* skip over it */

			}else
				varname = readUntil(str);

			std::transform(varname.begin(), varname.end(), varname.begin(), c_tolower);
			it = variables.find("$" + varname);

			DBG_ASSERT(it != variables.end(), "variable '%s' not found.", varname.c_str());
			output.append( it->second ); /* append value */

		/* regular character */
		}else{
			output.append(1, *str);
			str++;
		}
	}

	return output;
}

/* openConfig, tries to open the config and returns true on success */
static  bool openConfig(std::ifstream &configFile, std::string &configPath, std::string &pluginName){
	std::string configName, configEnv, homeDir	= getHomeDirectory();
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

		/* environment path */
		if ((configPath = getEnvironmentString("PIPELIGHT_CONFIG_PATH")) != ""){
			configPath = configPath + "/" + configName;
			DBG_INFO("trying to load config file from '%s'.", configPath.c_str());
			configFile.open(configPath);
			if (configFile.is_open()) return true;
		}

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
		configPath = PIPELIGHT_CONFIG_PATH "/" + configName;
		DBG_INFO("trying to load config file from '%s'.", configPath.c_str());
		configFile.open(configPath);
		if (configFile.is_open()) return true;

	}

	return false;
}

/* loadConfig, parses the config file */
bool loadConfig(PluginConfig &config){
	std::map<std::string, std::string> variables;
	int environmentVariable;

	/* Add $home to variables */
	std::string homeDir = getHomeDirectory();
	if (homeDir != "") variables["$home"] = homeDir;

	/* Add $wineuser variable */
	variables["$wineuser"] = getWineUser();

	/* initialize config variables with default values */
	config.configPath			= "";
	config.pluginName			= "";
	config.diagnosticMode		= false;

	config.sandboxPath			= "";

	config.winePath				= "wine";
	config.wineArch				= "win32";
	config.winePrefix			= "";
	config.wineDLLOverrides		= "mscoree,mshtml,winegstreamer,winemenubuilder.exe="; /* prevent installation of Geck & Mono by default */

	config.dllPath				= "";
	config.dllName				= "";
	config.regKey				= "";
	config.pluginLoaderPath		= "";
	config.gccRuntimeDLLs		= "";

	config.embed				= true;
	config.windowlessMode		= false;
	config.linuxWindowlessMode	= false;

	config.fakeVersion			= "";
	config.fakeMIMEtypes.clear();
	config.overwriteArgs.clear();
	config.windowlessOverwriteArgs.clear();

	config.dependencyInstaller	= "";
	config.dependencies.clear();
	config.optionalDependencies.clear();
	config.quietInstallation	= true;

	config.eventAsyncCall		= false;
	config.operaDetection		= true;
	config.executeJavascript	= "";
	config.replaceJavascript.clear();

	config.silverlightGraphicDriverCheck		= false;

	config.experimental_unityHacks				= false;
	config.experimental_forceSetWindow			= false;
	config.experimental_windowClassHook			= false;
	config.experimental_strictDrawOrdering		= false;

	/* open configuration file */
	std::ifstream configFile;
	if (!openConfig(configFile, config.configPath, config.pluginName)){
		DBG_ERROR("couldn't find any configuration file.");
		return false;
	}

	while (configFile.good()){
		std::string line;
		getline(configFile, line);

		line = trim(line);

		/* skip empty lines */
		if (line.length() == 0)
			continue;

		/* strip comments */
		size_t pos;
		if ((pos = line.find_first_of("#")) != std::string::npos){
			if (pos == 0)
				continue;
			line = line.substr(0, pos);
		}

		/* read key/value pair */
		std::string key;
		std::string value;
		if (!splitConfigValue(line, key, value))
			continue;

		/* convert key to lower case */
		std::transform(key.begin(), key.end(), key.begin(), c_tolower);
		/* replace $var and ${var} inside of value */
		value = replaceVariables(variables, value.c_str());

		if (key[0] == '$'){
			variables[key] = value;

		}else if (key == "diagnosticmode"){
			std::transform(value.begin(), value.end(), value.begin(), c_tolower);
			config.diagnosticMode = (value == "true" || value == "yes");

		}else if (key == "sandboxpath"){
			if (checkIfExists(value))
				config.sandboxPath = value;
			else
				DBG_WARN("sandbox not found or not installed!");

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

		}else if (key == "embed"){
			std::transform(value.begin(), value.end(), value.begin(), c_tolower);
			config.embed = (value == "true" || value == "yes");

		}else if (key == "windowlessmode"){
			std::transform(value.begin(), value.end(), value.begin(), c_tolower);
			config.windowlessMode = (value == "true" || value == "yes");

		}else if (key == "linuxwindowlessmode"){
			std::transform(value.begin(), value.end(), value.begin(), c_tolower);
			config.linuxWindowlessMode = (value == "true" || value == "yes");

		}else if (key == "fakeversion"){
			config.fakeVersion = value;

		}else if (key == "fakemimetype"){
			MimeInfo info;
			std::string fakeType, remaining;
			if (!splitConfigValue(value, fakeType, info.originalMime))
				continue;
			if (!splitConfigValue(fakeType, info.mimeType, remaining, ":"))
				continue;
			if (!splitConfigValue(remaining, info.extension, info.description, ":"))
				continue;
			config.fakeMIMEtypes.push_back(info);

		}else if (key == "overwritearg"){
			std::string argKey, argValue;
			if (!splitConfigValue(value, argKey, argValue))
				continue;
			config.overwriteArgs[argKey] = argValue;

		}else if (key == "windowlessoverwritearg"){
			std::string argKey, argValue;
			if (!splitConfigValue(value, argKey, argValue))
				continue;
			config.windowlessOverwriteArgs[argKey] = argValue;

		}else if (key == "dependencyinstaller"){
			config.dependencyInstaller = value;

		}else if (key == "silverlightversion"){
			DBG_WARN("the configuration parameter silverlightVersion is deprecated.");
			config.dependencies.insert(config.dependencies.begin(), "wine-" + value + "-installer");

		}else if (key == "dependency"){
			if (value != "") config.dependencies.push_back(value);

		}else if (key == "optional-dependency"){
			if (value != "") config.optionalDependencies.push_back(value);

		}else if (key == "quietinstallation"){
			std::transform(value.begin(), value.end(), value.begin(), c_tolower);
			config.quietInstallation = (value == "true" || value == "yes");

		}else if (key == "eventasynccall"){
			std::transform(value.begin(), value.end(), value.begin(), c_tolower);
			config.eventAsyncCall = (value == "true" || value == "yes");

		}else if (key == "operadetection"){
			std::transform(value.begin(), value.end(), value.begin(), c_tolower);
			config.operaDetection = (value == "true" || value == "yes");

		}else if (key == "executejavascript"){
			config.executeJavascript += value + "\n";

		}else if (key == "replacejavascript"){
			std::string argKey, argValue;
			if (!splitConfigValue(value, argKey, argValue))
				continue;
			config.replaceJavascript[argKey] = argValue;

		}else if (key == "silverlightgraphicdrivercheck" || key == "graphicdrivercheck"){
			if (key == "graphicdrivercheck")
				DBG_WARN("the configuration parameter graphicDriverCheck is deprecated.");
			config.silverlightGraphicDriverCheck = (value == "true" || value == "yes" ||
				(value.find_first_of("true") != std::string::npos) ||
				(value.find_first_of("hw-accel-default") != std::string::npos) );

		}else if (key == "experimental-unityhacks"){
			std::transform(value.begin(), value.end(), value.begin(), c_tolower);
			config.experimental_unityHacks = (value == "true" || value == "yes");

		}else if (key == "experimental-forcesetwindow"){
			std::transform(value.begin(), value.end(), value.begin(), c_tolower);
			config.experimental_forceSetWindow = (value == "true" || value == "yes");

		}else if (key == "experimental-windowclasshook"){
			std::transform(value.begin(), value.end(), value.begin(), c_tolower);
			config.experimental_windowClassHook = (value == "true" || value == "yes");

		}else if (key == "experimental-strictdrawordering"){
			std::transform(value.begin(), value.end(), value.begin(), c_tolower);
			config.experimental_strictDrawOrdering = (value == "true" || value == "yes");

		}else{
			DBG_WARN("unrecognized configuration key '%s'.", key.c_str());
		}

	}

	/* config.linuxWindowlessMode implies windowlessMode */
	if (config.linuxWindowlessMode)
		config.windowlessMode = true;

	/* set the multiplugin name */
	setMultiPluginName(config.pluginName);

	/* ensure that all necessary keys are provided */
	if	(	config.winePath == "" || ((config.dllPath == "" || config.dllName == "") &&
			config.regKey == "") || config.pluginLoaderPath == "" || config.winePrefix == "" ){
		DBG_ERROR("Your configuration file doesn't contain all necessary keys - aborting.");
		DBG_ERROR("please take a look at the original configuration file for more details.");
		return false;
	}

	/* environment variable to overwrite embedding */
	environmentVariable = getEnvironmentInteger("PIPELIGHT_EMBED", -1);
	if (environmentVariable >= 0){
		config.embed = (environmentVariable >= 1);
		DBG_INFO("embed set via commandline to %s", config.embed ? "true" : "false");
	}

#ifdef __APPLE__
	if (config.embed){
		DBG_WARN("embedding is not yet supported for MacOS, it will be disabled.");
		config.embed = false;
	}
#endif

	/* environment variable to overwrite windowlessmode */
	environmentVariable = getEnvironmentInteger("PIPELIGHT_WINDOWLESSMODE", -1);
	if (environmentVariable >= 0){
		config.windowlessMode		= (environmentVariable >= 1);
		config.linuxWindowlessMode	= (environmentVariable >= 2);
		DBG_INFO("windowlessMode set via commandline to %s",		config.windowlessMode ? "true" : "false");
		DBG_INFO("linuxWindowlessMode set via commandline to %s",	config.linuxWindowlessMode ? "true" : "false");
	}

#ifdef __APPLE__
	if (config.linuxWindowlessMode){
		DBG_WARN("linuxWindowlessMode is not yet supported for MacOS, it will be disabled.");
		config.linuxWindowlessMode = false;
	}
#endif

	/* check if hw acceleration should be used (only for Silverlight) */
	if (config.silverlightGraphicDriverCheck){
		environmentVariable = getEnvironmentInteger("PIPELIGHT_GPUACCELERATION", -1);
		if (environmentVariable >= 0){
			config.overwriteArgs["enableGPUAcceleration"]	= (environmentVariable >= 1) ? "true" : "false";
			config.experimental_strictDrawOrdering			= (environmentVariable >= 2);
			config.silverlightGraphicDriverCheck			= false;
			DBG_INFO("enableGPUAcceleration set via commandline to %s", (environmentVariable >= 1) ? "true" : "false");

		}else if (config.overwriteArgs.find("enableGPUAcceleration") != config.overwriteArgs.end()){
			config.silverlightGraphicDriverCheck			= false;
			DBG_INFO("enableGPUAcceleration set manually - skipping compatibility check.");
		}
	}

	/* check for optional dependencies */
	if (!config.optionalDependencies.empty()){
		std::vector<std::string>::iterator it;
		for (it = config.optionalDependencies.begin(); it != config.optionalDependencies.end(); it++){
			if (checkIsFile(homeDir + "/.config/" + *it + ".accept-license"))
				config.dependencies.push_back(*it);
		}
		config.optionalDependencies.clear();
	}

	return true;
}
