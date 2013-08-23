
#include <fstream>								// for std::ifstream
#include <iostream>								// for std::cerr

#include "basicplugin.h"
#include "configloader.h"

extern bool 		initOkay;

extern PluginConfig config;

bool debugSection(NPP instance, std::string name){
	NPObject 		*windowObj;
	NPIdentifier 	functionName;

	NPVariant		arg;
	arg.type = NPVariantType_String;
	arg.value.stringValue.UTF8Characters 	= name.c_str();
	arg.value.stringValue.UTF8Length		= name.size();

	NPVariant resultVariant;
	resultVariant.type = NPVariantType_Null;

	bool resultBool = false;

	if( sBrowserFuncs->getvalue(instance, NPNVWindowNPObject, &windowObj) == NPERR_NO_ERROR ){
		functionName = sBrowserFuncs->getstringidentifier("debugSection");

		if( sBrowserFuncs->invoke(instance, windowObj, functionName, &arg, 1, &resultVariant) == NPERR_NO_ERROR ){
			sBrowserFuncs->releasevariantvalue(&resultVariant);

			resultBool = true;			
		}

		sBrowserFuncs->releaseobject(windowObj);
	}

	return resultBool;
}

bool debugSimpleMessage(NPP instance, std::string message){
	NPObject 		*windowObj;
	NPIdentifier 	functionName;

	NPVariant		arg;
	arg.type = NPVariantType_String;
	arg.value.stringValue.UTF8Characters 	= message.c_str();
	arg.value.stringValue.UTF8Length		= message.size();

	NPVariant resultVariant;
	resultVariant.type = NPVariantType_Null;

	bool resultBool = false;

	if( sBrowserFuncs->getvalue(instance, NPNVWindowNPObject, &windowObj) == NPERR_NO_ERROR ){
		functionName = sBrowserFuncs->getstringidentifier("debugSimpleMessage");

		if( sBrowserFuncs->invoke(instance, windowObj, functionName, &arg, 1, &resultVariant) == NPERR_NO_ERROR ){
			sBrowserFuncs->releasevariantvalue(&resultVariant);

			resultBool = true;			
		}

		sBrowserFuncs->releaseobject(windowObj);
	}

	return resultBool;
}

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

	if(additionalMessage != ""){
		args[2].type = NPVariantType_String;
		args[2].value.stringValue.UTF8Characters 	= additionalMessage.c_str();
		args[2].value.stringValue.UTF8Length		= additionalMessage.size();
	}else{
		args[2].type = NPVariantType_Null;
	}

	NPVariant resultVariant;
	resultVariant.type = NPVariantType_Null;

	bool resultBool = false;

	if( sBrowserFuncs->getvalue(instance, NPNVWindowNPObject, &windowObj) == NPERR_NO_ERROR ){
		functionName = sBrowserFuncs->getstringidentifier("debugStatusMessage");

		if( sBrowserFuncs->invoke(instance, windowObj, functionName, (NPVariant*)&args, 3, &resultVariant) == NPERR_NO_ERROR ){
			sBrowserFuncs->releasevariantvalue(&resultVariant);

			resultBool = true;
		}

		sBrowserFuncs->releaseobject(windowObj);
	}

	return resultBool;
}

void debugFile(NPP instance,std::string filename){
	debugSection(instance, "Content of file: " + filename);

	std::ifstream 	file;
	file.open(filename);

	debugStatusMessage(instance, "Loading file " + filename, file.is_open() ? "okay": "failed");

	if(!file.is_open()) return;

	while (file.good()){
		std::string line;
		getline(file, line);

		debugSimpleMessage(instance, line);
	}
}

void runDiagnostic(NPP instance){
	std::cerr << "[PIPELIGHT] Diagnostic" << std::endl;

	if(initOkay){
		debugSimpleMessage(instance, \
			"Pipelight is working, but you still need to clear the plugin cache data");
		return;
	}

	debugFile(instance, config.configPath);

	// TODO: Add here additional diagnostic checks

}