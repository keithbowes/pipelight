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

#include <fstream>								// for std::ifstream

#include "../common/common.h"
#include "basicplugin.h"
#include "diagnostic.h"

/* debugSection */
bool debugSection(NPP instance, std::string name){
	NPObject 		*windowObj;
	NPIdentifier 	functionName;

	NPVariant		arg;
	arg.type = NPVariantType_String;
	arg.value.stringValue.UTF8Characters 	= name.c_str();
	arg.value.stringValue.UTF8Length		= name.size();

	NPVariant resultVariant;
	resultVariant.type 					= NPVariantType_Null;
	resultVariant.value.objectValue 	= NULL;

	bool resultBool = false;

	if (sBrowserFuncs->getvalue(instance, NPNVWindowNPObject, &windowObj) == NPERR_NO_ERROR){
		functionName = sBrowserFuncs->getstringidentifier("debugSection");

		if (sBrowserFuncs->invoke(instance, windowObj, functionName, &arg, 1, &resultVariant) == NPERR_NO_ERROR){
			sBrowserFuncs->releasevariantvalue(&resultVariant);
			resultBool = true;			
		}

		sBrowserFuncs->releaseobject(windowObj);
	}

	return resultBool;
}

/* debugSimpleMessage */
bool debugSimpleMessage(NPP instance, std::string message){
	NPObject 		*windowObj;
	NPIdentifier 	functionName;

	NPVariant		arg;
	arg.type = NPVariantType_String;
	arg.value.stringValue.UTF8Characters 	= message.c_str();
	arg.value.stringValue.UTF8Length		= message.size();

	NPVariant resultVariant;
	resultVariant.type 					= NPVariantType_Null;
	resultVariant.value.objectValue 	= NULL;

	bool resultBool = false;

	if (sBrowserFuncs->getvalue(instance, NPNVWindowNPObject, &windowObj) == NPERR_NO_ERROR){
		functionName = sBrowserFuncs->getstringidentifier("debugSimpleMessage");

		if (sBrowserFuncs->invoke(instance, windowObj, functionName, &arg, 1, &resultVariant) == NPERR_NO_ERROR){
			sBrowserFuncs->releasevariantvalue(&resultVariant);
			resultBool = true;			
		}

		sBrowserFuncs->releaseobject(windowObj);
	}

	return resultBool;
}

/* debugStatusMessage */
bool debugStatusMessage(NPP instance, std::string name, std::string result, std::string additionalMessage = ""){
	NPObject 		*windowObj;
	NPIdentifier 	functionName;

	NPVariant		args[3];
	args[0].type = NPVariantType_String;
	args[0].value.stringValue.UTF8Characters 	= name.c_str();
	args[0].value.stringValue.UTF8Length		= name.size();

	args[1].type = NPVariantType_String;
	args[1].value.stringValue.UTF8Characters 	= result.c_str();
	args[1].value.stringValue.UTF8Length		= result.size();

	if (additionalMessage != ""){
		args[2].type = NPVariantType_String;
		args[2].value.stringValue.UTF8Characters 	= additionalMessage.c_str();
		args[2].value.stringValue.UTF8Length		= additionalMessage.size();
	}else{
		args[2].type 				= NPVariantType_Null;
		args[2].value.objectValue 	= NULL;
	}

	NPVariant resultVariant;
	resultVariant.type 					= NPVariantType_Null;
	resultVariant.value.objectValue 	= NULL;

	bool resultBool = false;

	if (sBrowserFuncs->getvalue(instance, NPNVWindowNPObject, &windowObj) == NPERR_NO_ERROR){
		functionName = sBrowserFuncs->getstringidentifier("debugStatusMessage");

		if (sBrowserFuncs->invoke(instance, windowObj, functionName, (NPVariant*)&args, 3, &resultVariant) == NPERR_NO_ERROR){
			sBrowserFuncs->releasevariantvalue(&resultVariant);
			resultBool = true;
		}

		sBrowserFuncs->releaseobject(windowObj);
	}

	return resultBool;
}

/* debugFile, prints the content of a specific file as a debug message */
void debugFile(NPP instance,std::string filename){
	std::ifstream file;
	file.open(filename);

	debugStatusMessage(instance, "Loading file " + filename, file.is_open() ? "okay": "failed");
	if (!file.is_open()) return;

	while (file.good()){
		std::string line;
		getline(file, line);
		debugSimpleMessage(instance, line);
	}
}

/* runDiagnostic */
void runDiagnostic(NPP instance){
	DBG_INFO("running diagnostic checks.");

	/* initialization okay, but plugin cache still contains an error */
	if (initOkay){
		debugStatusMessage(instance, \
			"Valid browser plugin cache", \
			"failed", \
			"Pipelight is correctly installed, but you still need to clear the plugin cache!" );

		debugSimpleMessage(instance, "Take a look at the FAQ section on how to do this.");
		return;
	}

	debugSection(instance, "Configuration of Pipelight");

	/* no configuration found? */
	if (config.configPath == "" || !checkIfExists(config.configPath)){
		debugStatusMessage(instance, \
			"Checking if config exists", \
			"failed", \
			(config.configPath != "") ? ("Please install a default configuration file at " + config.configPath) : "" );

		return;
	}

	debugStatusMessage(instance, \
		"Checking if config exists", \
		"okay", \
		config.configPath );

	/* Check pluginLoaderPath */
	debugStatusMessage(instance, \
		"Checking if pluginLoaderPath is set and exists", \
		(config.pluginLoaderPath != "" && checkIfExists(config.pluginLoaderPath)) ? "okay" : "failed", \
		(config.pluginLoaderPath != "") ? config.pluginLoaderPath : "not set" );

	/* Check winePath */
	debugStatusMessage(instance, \
		"Checking if winePath is set and exists", \
		(config.winePath != "" && checkIfExists(config.winePath)) ? "okay" : "failed", \
		(config.winePath != "") ? config.winePath : "not set" );

	/* Check if wine exists */
	if (config.winePath != ""){

		debugStatusMessage(instance, \
			"Checking if wine exists", \
			checkIfExists(config.winePath) ? "okay" : "failed", \
			config.winePath );

	}

	/* Check winePrefix */
	bool winePrefixFound = (config.winePrefix != "" && checkIfExists(config.winePrefix));

	debugStatusMessage(instance, \
		"Checking if winePrefix is set and exists", \
		winePrefixFound ? "okay" : "failed", \
		(config.winePrefix != "") ? config.winePrefix : "not set");

	if (config.winePrefix == ""){
		debugSimpleMessage(instance, "As you have no winePrefix defined the environment variable WINEPREFIX will be used.");
		debugSimpleMessage(instance, "This script is not able to check if everything is okay with your wine prefix!");	
	}

	/* Check dllPath / dllName / regKey */
	std::string unixPath 	= "";
	bool dllPathSet 		= (config.dllPath != "" && config.dllName != "");
	if (dllPathSet) unixPath= convertWinePath(config.dllPath + "\\" + config.dllName);
	bool dllPathFound 		= (unixPath != "" && checkIfExists(unixPath));

	if (config.regKey == ""){
		debugStatusMessage(instance, "Checking if dllPath/dllname is set and exists", \
			dllPathFound ? "okay" : "failed");
		debugSimpleMessage(instance, "You need to define either a valid dllPath/dllName or regKey");

		if (config.winePrefix != "" && !checkIfExists(config.winePrefix)){
			debugSimpleMessage(instance, "The whole wine prefix " + config.winePrefix + " doesn't exist");

		}else if (unixPath == ""){
			debugSimpleMessage(instance, "Unable to verify if the DLL exists, please check this manually!");

		}else{
			debugSimpleMessage(instance, unixPath);
		}

		debugSimpleMessage(instance, "(dllPath = " + config.dllPath + ")");
		debugSimpleMessage(instance, "(dllName = " + config.dllName + ")");

	}else{
		debugStatusMessage(instance, "Checking if dllPath/dllname is set and exists", "not used");
		debugSimpleMessage(instance, "You defined the regKey option which is preferred over dllPath/dllName");

		debugStatusMessage(instance, "Checking if regKey is set and exists", "unknown");
		debugSimpleMessage(instance, "The regKey is set, but you need to manually check if the key exists");
		debugSimpleMessage(instance, "(regKey = " + config.regKey + ")");
	}

	bool dependencyInstallerFound = (config.dependencyInstaller != "" && checkIfExists(config.dependencyInstaller));

	debugStatusMessage(instance, "Checking if dependencyInstaller is set and exists", \
		dependencyInstallerFound ? "okay" : "failed", \
		(config.dependencyInstaller != "") ? config.dependencyInstaller : "not set");

	if (dependencyInstallerFound){
		debugStatusMessage(instance, "Checking if dependencies are defined", \
			config.dependencies.empty() ? "failed" : "okay", \
			config.dependencies.empty() ? "not set" : "");

	}else{
		debugSimpleMessage(instance, "You either have to install the dependencyInstaller script or setup your wine prefix manually.");
		debugSimpleMessage(instance, "Depending on the distribution you probably will have to execute some post-installation script to do that.");
	}

	debugSection(instance, "Distribution");
	debugFile(instance, "/etc/issue");

	debugSection(instance, "Content of file: " + config.configPath);
	debugFile(instance, config.configPath);
}