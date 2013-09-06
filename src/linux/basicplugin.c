/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Josh Aas <josh@mozilla.com>
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

/*	Pipelight - License
 *
 *	The source code is based on mozilla.org code and is licensed under
 *	MPL 1.1/GPL 2.0/LGPL 2.1 as stated above. Modifications to create 
 * 	Pipelight are done by:
 *
 *	Contributor(s):
 *		Michael Müller <michael@fds-team.de>
 *		Sebastian Lackner <sebastian@fds-team.de>
 *
 */
 
#include <stdlib.h>								// for getenv, ...
#include <iostream>								// for std::cerr
#include <unistd.h>								// for POSIX api
#include <string>								// for std::string
#include <stdexcept>							// for std::runtime_error

#include "basicplugin.h"
#include "configloader.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <pthread.h>							// alternative to ScheduleTimer etc.
#include <semaphore.h>

/* BEGIN GLOBAL VARIABLES

	Note: As global variables should be initialized properly BEFORE the attach function is called.
	Otherwise there might be some uninitialized variables...

*/

// Description etc.
char strMimeType[2048] 			= {0};
char strPluginversion[100]		= {0};
char strPluginName[256] 		= {0};
char strPluginDescription[1024]	= {0};

// Instance responsible for triggering the timer
uint32_t  			eventTimerID 			= 0;
NPP 				eventTimerInstance 		= NULL;
pthread_t 			eventThread				= 0;

sem_t				eventThreadSemRequestAsyncCall;
sem_t				eventThreadSemScheduledAsyncCall;


// Pipes to communicate with the wine process
int pipeOut[2] 	= {0, 0};
int pipeIn[2] 	= {0, 0};
FILE * pipeOutF = NULL;
FILE * pipeInF	= NULL;

// winePid if wine has already been started
pid_t winePid = -1;
bool initOkay = false;

// Browser functions
NPNetscapeFuncs* sBrowserFuncs = NULL;

// Handlemanager
HandleManager handlemanager __attribute__((init_priority(101)));

// Global plugin configuration
PluginConfig config __attribute__((init_priority(101)));

// Attach has to be called as a last step
void attach() __attribute__((constructor(102)));
void detach() __attribute__((destructor));

/* END GLOBAL VARIABLES */

void attach(){

	// Fix for Opera: Dont sync stdio
	std::ios_base::sync_with_stdio(false);

	std::cerr << "[PIPELIGHT] Attached to process" << std::endl;

	// Initialize semaphore
	sem_init(&eventThreadSemRequestAsyncCall, 0, 0);
	sem_init(&eventThreadSemScheduledAsyncCall, 0, 0);

	initOkay = false;

	if(!loadConfig(config)){
		std::cerr << "[PIPELIGHT] Unable to load config file - aborting" << std::endl;
		return;
	}

	if( config.winePath 		== "" ||	// We have to know where wine is installed (default: wine)
		config.dllPath 			== "" ||	// We need the path and name of the plugin DLL
		config.dllName 			== "" ||
		config.pluginLoaderPath == "" ){	// Without pluginloader.exe this doesn't work

		std::cerr << "[PIPELIGHT] Your configuration file doesn't contain all necessary keys - aborting" << std::endl;
		std::cerr << "[PIPELIGHT] Please take a look at the original configuration file for more details." << std::endl;
		return;
	}

	// Check if we should enable hardware acceleration
	if(config.overwriteArgs.find("enableGPUAcceleration") == config.overwriteArgs.end()){
		if(!checkGraphicDriver())
			config.overwriteArgs["enableGPUAcceleration"] = "false";
	}else{
		std::cerr << "[PIPELIGHT] enableGPUAcceleration set manually - skipping compatibility check" << std::endl;
	}

	// Check for correct installation
	if(!checkSilverlightInstallation()){
		std::cerr << "[PIPELIGHT] Silverlight not correctly installed - aborting" << std::endl;
		return;
	}

	// Start wine process
	if(!startWineProcess()){
		std::cerr << "[PIPELIGHT] Could not start wine process - aborting" << std::endl;
		return;
	}

	// We want to be sure that wine is up and running until we return!
	try {
		callFunction(INIT_OKAY);
		waitReturn();
	} catch(std::runtime_error error){
		std::cerr << "[PIPELIGHT] Error during the initialization of the wine process - aborting" << std::endl;
		return;
	}
 
 	// Initialisation successful
	initOkay = true;
}

void detach(){
	// TODO: Deinitialize pointers etc.
}

bool checkIfExists(std::string path){
	struct stat info;
	if(stat(path.c_str(), &info) == 0){
		return true;
	}
	return false;
}

std::string getEnvironmentString(const char* variable){
	char *str = getenv(variable);
	return str ? std::string(str) : "";
}

std::string convertWinePath(std::string path, bool direction){

	// Not possible to call /bin/winepath if the path is deprecated
	if( config.winePathIsDeprecated ){
		std::cerr << "[PIPELIGHT] Don't know where /bin/winepath is" << std::endl;
		return "";

	}else if( config.winePrefix != "" && !checkIfExists(config.winePrefix) ){
		std::cerr << "[PIPELIGHT] Wine prefix doesn't exist" << std::endl;
		return "";
	}

	int tempPipeIn[2];
	std::string resultPath;

	if( pipe(tempPipeIn) == -1 ){
		std::cerr << "[PIPELIGHT] Could not create pipes to communicate with /bin/winepath" << std::endl;
		return "";
	}

	pid_t pidWinePath = fork();
	if(pidWinePath == 0){

		close(0);
		close(tempPipeIn[0]);
		dup2(tempPipeIn[1], 1);

		// Run winepath with the correct environment variables

		std::string winePathBinary		= config.winePath + "/bin/winepath";

		if (config.winePrefix != "")
			setenv("WINEPREFIX", 		config.winePrefix.c_str(), 	true);

		if(config.wineArch != "")
			setenv("WINEARCH", 			config.wineArch.c_str(), 	true);

		if(config.wineDLLOverrides != "")
			setenv("WINEDLLOVERRIDES", 	config.wineDLLOverrides.c_str(), true);

		std::string argument = direction ? "--windows" : "--unix";

		execlp(winePathBinary.c_str(), winePathBinary.c_str(), argument.c_str(), path.c_str(), NULL);
		throw std::runtime_error("Error in execlp command - probably /bin/sh not found?");

	}else if(pidWinePath != -1){
		char resultPathBuffer[4096+1];

		close(tempPipeIn[1]);
		FILE * tempPipeInF = fdopen(tempPipeIn[0], "rb");

		if(tempPipeInF != NULL){

			if( fgets( (char*)&resultPathBuffer, sizeof(resultPathBuffer), tempPipeInF) ){
				resultPath = trim( std::string( (char*)&resultPathBuffer) );
			}

			fclose(tempPipeInF);
		}

		int status;
		if(waitpid(pidWinePath, &status, 0) == -1 || !WIFEXITED(status) ){
			std::cerr << "[PIPELIGHT] /bin/winepath did not run correctly (error occured)" << std::endl;
			return "";

		}else if(WEXITSTATUS(status) != 0){
			std::cerr << "[PIPELIGHT] /bin/winepath did not run correctly (exitcode = " << WEXITSTATUS(status) << ")" << std::endl;
			return "";
		}

	}else{

		close(tempPipeIn[0]);
		close(tempPipeIn[1]);	

		std::cerr << "[PIPELIGHT] Unable to fork() - probably out of memory?" << std::endl;
		return "";

	}

	return resultPath;
}

bool checkSilverlightInstallation(){

	// Checking the silverlight installation is only possible if the user has defined a winePrefix
	if( config.winePrefix == "" ){
		std::cerr << "[PIPELIGHT] No winePrefix defined - unable to check Silverlight installation" << std::endl;
		return true;
	}

	// Output wine prefix
	std::cerr << "[PIPELIGHT] Using wine prefix directory " << config.winePrefix << std::endl;

	// Check if the prefix exists?
	if( checkIfExists(config.winePrefix) ){
		return true;
	}

	// If there is no installer provided we cannot fix this issue!
	if( config.dependencyInstaller == "" || config.silverlightVersion == "" || 
		!checkIfExists(config.dependencyInstaller) ){
		return false;
	}

	// Run the installer ...
	std::cerr << "[PIPELIGHT] Silverlight not installed. Starting installation - this might take some time" << std::endl;

	pid_t pidInstall = fork();
	if(pidInstall == 0){

		close(0);

		// Run the installer with the correct environment variables

		std::string wineBinary		= config.winePathIsDeprecated ? config.winePath : (config.winePath + "/bin/wine");

		setenv("WINEPREFIX", 	config.winePrefix.c_str(), 	true);
		setenv("WINE", 			wineBinary.c_str(), 		true);

		if(config.wineArch != "")
			setenv("WINEARCH", 	config.wineArch.c_str(), 	true);

		if(config.wineDLLOverrides != "")
			setenv("WINEDLLOVERRIDES", config.wineDLLOverrides.c_str(), true);

		std::string argument = "wine-" + config.silverlightVersion + "-installer";

		execlp(config.dependencyInstaller.c_str(), config.dependencyInstaller.c_str(), argument.c_str(), NULL);
		throw std::runtime_error("Error in execlp command - probably /bin/sh not found?");

	}else if(pidInstall != -1){

		int status;
		if(waitpid(pidInstall, &status, 0) == -1 || !WIFEXITED(status) ){
			std::cerr << "[PIPELIGHT] Silverlight installer did not run correctly (error occured)" << std::endl;
			return false;

		}else if(WEXITSTATUS(status) != 0){
			std::cerr << "[PIPELIGHT] Silverlight installer did not run correctly (exitcode = " << WEXITSTATUS(status) << ")" << std::endl;
			return false;
		}


	}else{
		std::cerr << "[PIPELIGHT] Unable to fork() - probably out of memory?" << std::endl;
		return false;

	}

	return true;
}

bool checkGraphicDriver(){

	// Checking the silverlight installation is only possible if the user has defined a winePrefix
	if( config.graphicDriverCheck == "" ){
		std::cerr << "[PIPELIGHT] No GPU driver check script defined - treating test as failure" << std::endl;
		return false;
	}

	if( !checkIfExists(config.graphicDriverCheck) ){
		std::cerr << "[PIPELIGHT] GPU driver check script not found - treating test as failure" << std::endl;
		return false;
	}

	pid_t pidCheck = fork();
	if(pidCheck == 0){

		close(0);

		// We set all enviroments variables for Wine although the GPU check script shouldn't need them

		std::string wineBinary		= config.winePathIsDeprecated ? config.winePath : (config.winePath + "/bin/wine");

		setenv("WINE", 					wineBinary.c_str(), 		true);

		if (config.winePrefix != "")
			setenv("WINEPREFIX", 		config.winePrefix.c_str(), 	true);

		if(config.wineArch != "")
			setenv("WINEARCH", 			config.wineArch.c_str(), 	true);

		if(config.wineDLLOverrides != "")
			setenv("WINEDLLOVERRIDES", 	config.wineDLLOverrides.c_str(), true);

		execlp(config.graphicDriverCheck.c_str(), config.graphicDriverCheck.c_str(), NULL);
		throw std::runtime_error("Error in execlp command - probably /bin/sh not found?");

	}else if(pidCheck != -1){

		int status;
		if(waitpid(pidCheck, &status, 0) == -1 || !WIFEXITED(status) ){
			std::cerr << "[PIPELIGHT] GPU driver check script failed to execute - treating test as failure" << std::endl;
			return false;

		}else if(WEXITSTATUS(status) == 0){
			std::cerr << "[PIPELIGHT] GPU driver check - Your driver is supported, hardware acceleration enabled" << std::endl;
			return true;

		}else if(WEXITSTATUS(status) == 1){
			std::cerr << "[PIPELIGHT] GPU driver check - Your driver is not in the whitelist, hardware acceleration disabled" << std::endl;
			return false;

		}else{
			std::cerr << "[PIPELIGHT] GPU driver check did not run correctly (exitcode = " << WEXITSTATUS(status) << ")" << std::endl;
			return false;
		}

	}else{
		std::cerr << "[PIPELIGHT] Unable to fork() - probably out of memory?" << std::endl;
		return false;
	}

	return false;
}

bool startWineProcess(){

	if( pipe(pipeOut) == -1 || pipe(pipeIn) == -1 ){
		std::cerr << "[PIPELIGHT] Could not create pipes to communicate with the plugin" << std::endl;
		return false;
	}


	winePid = fork();
	if (winePid == 0){
		// The child process will be replaced with wine

		close(PIPE_BROWSER_READ);
		close(PIPE_BROWSER_WRITE);

		// Assign to stdin/stdout
		dup2(PIPE_PLUGIN_READ,  0);
		dup2(PIPE_PLUGIN_WRITE, 1);
		
		// Runt he pluginloader with the correct environment variables

		if (config.winePrefix != "")
			setenv("WINEPREFIX", 		config.winePrefix.c_str(), 	true);

		if (config.wineArch != "")
			setenv("WINEARCH", 			config.wineArch.c_str(), 	true);

		if(config.wineDLLOverrides != "")
			setenv("WINEDLLOVERRIDES", 	config.wineDLLOverrides.c_str(), 	true);

		if(config.gccRuntimeDLLs != ""){
			std::string runtime = getEnvironmentString("Path");
			if(runtime != "") runtime += ";";
			runtime += config.gccRuntimeDLLs;
			setenv("Path", runtime.c_str(), true);
		}

		// Put together the flags
		std::string windowMode 		= config.windowlessMode 			? "--windowless" 	: "";
		std::string embedMode 		= config.embed 						? "--embed" 		: "";
		std::string usermodeTimer	= config.experimental_usermodeTimer ? "--usermodetimer" : "";

		std::string wineBinary		= config.winePathIsDeprecated ? config.winePath : (config.winePath + "/bin/wine");

		// Execute wine
		execlp(wineBinary.c_str(), wineBinary.c_str(), config.pluginLoaderPath.c_str(), config.dllPath.c_str(), config.dllName.c_str(), windowMode.c_str(), embedMode.c_str(), usermodeTimer.c_str(), NULL);	
		throw std::runtime_error("Error in execlp command - probably wine not found?");

	}else if (winePid != -1){
		// The parent process will return normally and use the pipes to communicate with the child process

		close(PIPE_PLUGIN_READ);
		close(PIPE_PLUGIN_WRITE);		

		pipeOutF 	= fdopen(PIPE_BROWSER_WRITE, 	"wb");
		pipeInF		= fdopen(PIPE_BROWSER_READ, 	"rb");

		// In case something goes wrong ...
		if(pipeOutF == NULL || pipeInF == NULL){
			if(pipeOutF) fclose(pipeOutF);
			if(pipeInF) fclose(pipeInF);

			return false;
		}

		// Disable buffering for input pipe
		// (To allow waiting for a pipe)
		setbuf(pipeInF, NULL);


	}else{
		std::cerr << "[PIPELIGHT] Unable to fork() - probably out of memory?" << std::endl;
		return false;
	}

	return true;
}


void dispatcher(int functionid, Stack &stack){
	if(!sBrowserFuncs) throw std::runtime_error("Browser didn't correctly initialize the plugin!");

	switch(functionid){
		
		// OBJECT_KILL not implemented

		case HANDLE_MANAGER_REQUEST_STREAM_INFO:
			{
				NPStream* stream = readHandleStream(stack); // shouldExist not necessary, Linux checks always

				writeString(stream->headers);
				writeHandleNotify(stream->notifyData, HANDLE_SHOULD_EXIST);
				writeInt32(stream->lastmodified);
				writeInt32(stream->end);
				writeString(stream->url);
				returnCommand();
			}
			break;

		// PROCESS_WINDOW_EVENTS not implemented

		case GET_WINDOW_RECT:
			{
				Window win 			= (Window)readInt32(stack);
				XWindowAttributes winattr;
				bool result         = false;
				Window dummy;

				Display *display 	= XOpenDisplay(NULL);

				if(display){
					result 				= XGetWindowAttributes(display, win, &winattr);
					if(result) result 	= XTranslateCoordinates(display, win, RootWindow(display, 0), winattr.x, winattr.y, &winattr.x, &winattr.y, &dummy);


					XCloseDisplay(display);

				}else{
					std::cerr << "[PIPELIGHT] Could not open Display" << std::endl;
				}

				if(result){
					/*writeInt32(winattr.height);
					writeInt32(winattr.width);*/
					writeInt32(winattr.y);
					writeInt32(winattr.x);
				}

				writeInt32(result);
				returnCommand();
			}
			break;

		// Plugin specific commands (_GET_, _NP_ and _NPP_) not implemented

		case FUNCTION_NPN_CREATE_OBJECT: // Verified, everything okay
			{
				NPObject* obj = sBrowserFuncs->createobject(readHandleInstance(stack), &myClass);

				#ifdef DEBUG_LOG_HANDLES
					std::cerr << "[PIPELIGHT:LINUX] FUNCTION_NPN_CREATE_OBJECT created " << (void*)obj << std::endl;
				#endif

				writeHandleObj(obj); // refcounter is hopefully 1
				returnCommand();
			}
			break;


		case FUNCTION_NPN_GETVALUE_BOOL: // Verified, everything okay
			{
				NPP instance 			= readHandleInstance(stack);
				NPNVariable variable 	= (NPNVariable)readInt32(stack);

				NPBool resultBool;
				NPError result = sBrowserFuncs->getvalue(instance, variable, &resultBool);

				if(result == NPERR_NO_ERROR)
					writeInt32(resultBool);

				writeInt32(result);
				returnCommand();
			}
			break;


		case FUNCTION_NPN_GETVALUE_OBJECT: // Verified, everything okay
			{
				NPP instance 			= readHandleInstance(stack);
				NPNVariable variable 	= (NPNVariable)readInt32(stack);

				NPObject* obj = NULL;
				NPError result = sBrowserFuncs->getvalue(instance, variable, &obj);

				if(result == NPERR_NO_ERROR)
					writeHandleObj(obj); // Refcount was already incremented by getValue

				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_RELEASEOBJECT: // Verified, everything okay
			{
				NPObject* obj 		= readHandleObj(stack);

				#ifdef DEBUG_LOG_HANDLES
					std::cerr << "[PIPELIGHT:LINUX] FUNCTION_NPN_RELEASEOBJECT(" << (void*)obj << ")" << std::endl;

					if(obj->referenceCount == 1 && handlemanager.existsHandleByReal( (uint64_t)obj, TYPE_NPObject) ){

						writeHandleObj(obj);
						callFunction(OBJECT_IS_CUSTOM);

						if( !(bool)readResultInt32() ){
							throw std::runtime_error("Forgot to set killObject?");
						}
						
					}
				#endif

				sBrowserFuncs->releaseobject(obj);

				returnCommand();
			}
			break;

		case FUNCTION_NPN_RETAINOBJECT: // Verified, everything okay
			{
				NPObject* obj 				= readHandleObj(stack);
				uint32_t minReferenceCount 	= readInt32(stack);


				#ifdef DEBUG_LOG_HANDLES
					std::cerr << "[PIPELIGHT:LINUX] FUNCTION_NPN_RETAINOBJECT(" << (void*)obj << ")" << std::endl;
				#endif

				sBrowserFuncs->retainobject(obj);

				#ifdef DEBUG_LOG_HANDLES
					if( minReferenceCount != REFCOUNT_UNDEFINED && obj->referenceCount < minReferenceCount ){
						throw std::runtime_error("Object referencecount smaller than expected?");
					}

				#else
					(void)minReferenceCount; // UNUSED
				#endif

				returnCommand();
			}
			break;


		case FUNCTION_NPN_EVALUATE: // Verified, everything okay
			{
				NPString script;

				NPP instance 		= readHandleInstance(stack);
				NPObject* obj 		= readHandleObj(stack);	
				readNPString(stack, script);

				// Reset variant type
				NPVariant resultVariant;
				resultVariant.type 					= NPVariantType_Void;
				resultVariant.value.objectValue 	= NULL;

				bool result = sBrowserFuncs->evaluate(instance, obj, &script, &resultVariant);	
				
				// Free the string
				freeNPString(script);

				if(result)
					writeVariantRelease(resultVariant);

				writeInt32( result );
				returnCommand();
			}
			break;
	
		case FUNCTION_NPN_INVOKE:
			{
				NPP instance 					= readHandleInstance(stack);
				NPObject* obj 					= readHandleObj(stack);
				NPIdentifier identifier			= readHandleIdentifier(stack);
				int32_t argCount				= readInt32(stack);
				std::vector<NPVariant> args 	= readVariantArray(stack, argCount);
				// refcount is not incremented here!

				NPVariant resultVariant;
				resultVariant.type 					= NPVariantType_Void;
				resultVariant.value.objectValue 	= NULL;
				
				bool result = sBrowserFuncs->invoke(instance, obj, identifier, args.data(), argCount, &resultVariant);

				// Free the variant array
				freeVariantArray(args);

				if(result)
					writeVariantRelease(resultVariant);

				writeInt32( result );
				returnCommand();	

			}
			break;

		case FUNCTION_NPN_INVOKE_DEFAULT: // UNTESTED!
			{
				NPP instance 					= readHandleInstance(stack);
				NPObject* obj 					= readHandleObj(stack);
				int32_t argCount				= readInt32(stack);
				std::vector<NPVariant> args 	= readVariantArray(stack, argCount);
				// refcount is not incremented here!

				NPVariant resultVariant;
				resultVariant.type 					= NPVariantType_Void;
				resultVariant.value.objectValue 	= NULL;
				
				bool result = sBrowserFuncs->invokeDefault(instance, obj, args.data(), argCount, &resultVariant);

				// Free the variant array
				freeVariantArray(args);

				if(result)
					writeVariantRelease(resultVariant);

				writeInt32( result );
				returnCommand();	

			}
			break;

		case FUNCTION_NPN_HAS_PROPERTY: // UNTESTED!
			{
				NPP instance 					= readHandleInstance(stack);
				NPObject* obj 					= readHandleObj(stack);
				NPIdentifier identifier			= readHandleIdentifier(stack);

				bool result = sBrowserFuncs->hasproperty(instance, obj, identifier);

				writeInt32(result);
				returnCommand();	
			}
			break;

		case FUNCTION_NPN_HAS_METHOD: // UNTESTED!
			{
				NPP instance 					= readHandleInstance(stack);
				NPObject* obj 					= readHandleObj(stack);
				NPIdentifier identifier			= readHandleIdentifier(stack);

				bool result = sBrowserFuncs->hasmethod(instance, obj, identifier);

				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_GET_PROPERTY: // Verified, everything okay
			{
				NPP instance 				= readHandleInstance(stack);
				NPObject*  obj 				= readHandleObj(stack);
				NPIdentifier propertyName	= readHandleIdentifier(stack);

				NPVariant resultVariant;
				resultVariant.type 					= NPVariantType_Void;
				resultVariant.value.objectValue 	= NULL;

				bool result = sBrowserFuncs->getproperty(instance, obj, propertyName, &resultVariant);

				if(result)
					writeVariantRelease(resultVariant); // free variant (except contained objects)

				writeInt32( result );
				returnCommand();
			}
			break;

		case FUNCTION_NPN_SET_PROPERTY: // UNTESTED!
			{
				NPP instance 					= readHandleInstance(stack);
				NPObject* obj 					= readHandleObj(stack);
				NPIdentifier identifier			= readHandleIdentifier(stack);

				NPVariant value;
				readVariant(stack, value);

				bool result = sBrowserFuncs->setproperty(instance, obj, identifier, &value);

				freeVariant(value);

				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_REMOVE_PROPERTY: // UNTESTED!
			{
				NPP instance 					= readHandleInstance(stack);
				NPObject* obj 					= readHandleObj(stack);
				NPIdentifier identifier			= readHandleIdentifier(stack);

				bool result = sBrowserFuncs->removeproperty(instance, obj, identifier);

				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_ENUMERATE: // UNTESTED!
			{
				NPP instance 					= readHandleInstance(stack);
				NPObject 		*obj 			= readHandleObj(stack);

				NPIdentifier*   identifierTable  = NULL;
				uint32_t 		identifierCount  = 0;

				bool result = sBrowserFuncs->enumerate(instance, obj, &identifierTable, &identifierCount);

				if(result){
					writeIdentifierArray(identifierTable, identifierCount);
					writeInt32(identifierCount);
					
					// Free the memory for the table
					if(identifierTable)
						sBrowserFuncs->memfree(identifierTable);
				}

				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_SET_EXCEPTION: // UNTESTED!
			{
				NPObject* obj 					= readHandleObj(stack);
				std::shared_ptr<char> message 	= readStringAsMemory(stack);

				sBrowserFuncs->setexception(obj, message.get());

				returnCommand();
			}
			break;

		case FUNCTION_NPN_GET_URL_NOTIFY:
			{
				NPP instance 					= readHandleInstance(stack);
				std::shared_ptr<char> url 		= readStringAsMemory(stack);
				std::shared_ptr<char> target 	= readStringAsMemory(stack);
				NotifyDataRefCount* notifyData 	= (NotifyDataRefCount*)readHandleNotify(stack);

				// Increase refcounter
				if(notifyData){
					notifyData->referenceCount++;
				}

				NPError result = sBrowserFuncs->geturlnotify(instance, url.get(), target.get(), notifyData);

				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_POST_URL_NOTIFY:
			{
				NPP instance 					= readHandleInstance(stack);
				std::shared_ptr<char> url 		= readStringAsMemory(stack);
				std::shared_ptr<char> target 	= readStringAsMemory(stack);

				size_t len;
				std::shared_ptr<char> buffer	= readMemory(stack, len);
				bool file 						= (bool)readInt32(stack);
				NotifyDataRefCount* notifyData 	= (NotifyDataRefCount*)readHandleNotify(stack);

				// Increase refcounter
				if(notifyData){
					notifyData->referenceCount++;
				}

				NPError result = sBrowserFuncs->posturlnotify(instance, url.get(), target.get(), len, buffer.get(), file, notifyData);

				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_GET_URL:
			{
				NPP instance 					= readHandleInstance(stack);
				std::shared_ptr<char> url 		= readStringAsMemory(stack);
				std::shared_ptr<char> target 	= readStringAsMemory(stack);

				NPError result = sBrowserFuncs->geturl(instance, url.get(), target.get());

				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_POST_URL:
			{
				NPP instance 					= readHandleInstance(stack);
				std::shared_ptr<char> url 		= readStringAsMemory(stack);
				std::shared_ptr<char> target 	= readStringAsMemory(stack);

				size_t len;
				std::shared_ptr<char> buffer	= readMemory(stack, len);
				bool file 						= (bool)readInt32(stack);

				NPError result = sBrowserFuncs->posturl(instance, url.get(), target.get(), len, buffer.get(), file);

				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_REQUEST_READ: // UNTESTED!
			{
				NPStream *stream 				= readHandleStream(stack);
				uint32_t rangeCount				= readInt32(stack);

				// TODO: Verify that this is correct!
				std::vector<NPByteRange> rangeVector;

				for(unsigned int i = 0; i < rangeCount; i++){
					NPByteRange range;
					range.offset = readInt32(stack);
					range.length = readInt32(stack);
					range.next   = NULL;
					rangeVector.push_back(range);
				}

				// The last element is the latest one, we have to create the links between them...
				std::vector<NPByteRange>::reverse_iterator lastObject = rangeVector.rend();
				NPByteRange* rangeList = NULL;

				for(std::vector<NPByteRange>::reverse_iterator it = rangeVector.rbegin(); it != rangeVector.rend(); it++){
					if(lastObject != rangeVector.rend()){
						lastObject->next = &(*it);
					}else{
						rangeList        = &(*it);
					}
					lastObject = it;
				}

				NPError result = sBrowserFuncs->requestread(stream, rangeList);

				// As soon as the vector is deallocated everything else is gone, too

				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_WRITE:
			{
				size_t len;

				NPP instance 					= readHandleInstance(stack);
				NPStream *stream 				= readHandleStream(stack);
				std::shared_ptr<char> buffer	= readMemory(stack, len);	

				int32_t result = sBrowserFuncs->write(instance, stream, len, buffer.get());
				
				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_NEW_STREAM:
			{
				NPP instance 					= readHandleInstance(stack);
				std::shared_ptr<char> type 		= readStringAsMemory(stack);
				std::shared_ptr<char> target 	= readStringAsMemory(stack);

				NPStream* stream = NULL;
				NPError result = sBrowserFuncs->newstream(instance, type.get(), target.get(), &stream);

				if(result == NPERR_NO_ERROR)
					writeHandleStream(stream);

				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_DESTROY_STREAM:
			{
				NPP instance 		= readHandleInstance(stack);
				NPStream *stream 	= readHandleStream(stack, HANDLE_SHOULD_EXIST);
				NPReason reason 	= (NPReason) readInt32(stack);

				NPError result = sBrowserFuncs->destroystream(instance, stream, reason);
				
				// Let the handlemanager remove this one
				// TODO: Is this necessary?
				//handlemanager.removeHandleByReal((uint64_t)stream, TYPE_NPStream);

				writeInt32(result);
				returnCommand();
			}
			break;		

		case FUNCTION_NPN_STATUS:
			{		
				NPP instance 					= readHandleInstance(stack);
				std::shared_ptr<char> message	= readStringAsMemory(stack);

				sBrowserFuncs->status(instance, message.get());
				returnCommand();
			}
			break;

		case FUNCTION_NPN_USERAGENT: // Verified, everything okay
			{
				writeString( sBrowserFuncs->uagent(readHandleInstance(stack)) );
				returnCommand();
			}
			break;

		case FUNCTION_NPN_IDENTIFIER_IS_STRING:
			{
				NPIdentifier identifier = readHandleIdentifier(stack);
				bool result = sBrowserFuncs->identifierisstring(identifier);

				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_UTF8_FROM_IDENTIFIER:
			{
				NPIdentifier identifier	= readHandleIdentifier(stack);
				NPUTF8 *str = sBrowserFuncs->utf8fromidentifier(identifier);

				writeString((char*) str);

				// Free the string
				if(str)
					sBrowserFuncs->memfree(str);

				returnCommand();
			}
			break;


		case FUNCTION_NPN_INT_FROM_IDENTIFIER:
			{
				NPIdentifier identifier = readHandleIdentifier(stack);
				int32_t result = sBrowserFuncs->intfromidentifier(identifier);
				
				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_GET_STRINGIDENTIFIER: // Verified, everything okay
			{
				std::shared_ptr<char> utf8name 	= readStringAsMemory(stack);
				NPIdentifier identifier 		= sBrowserFuncs->getstringidentifier((NPUTF8*) utf8name.get());

				writeHandleIdentifier(identifier);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_GET_INTIDENTIFIER:
			{
				int32_t intid 					= readInt32(stack);
				NPIdentifier identifier 		= sBrowserFuncs->getintidentifier(intid);

				writeHandleIdentifier(identifier);
				returnCommand();
			}
			break;

		default:
			throw std::runtime_error("Specified function not found!");
			break;
	}
}
