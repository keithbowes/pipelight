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

	DBG_INFO("attached to process.");

	// Initialize semaphore
	sem_init(&eventThreadSemRequestAsyncCall, 0, 0);
	sem_init(&eventThreadSemScheduledAsyncCall, 0, 0);

	initOkay = false;

	if(!loadConfig(config)){
		DBG_ERROR("unable to load config file - aborting.");
		return;
	}

	if( config.winePath 		== "" ||	// We have to know where wine is installed (default: wine)
		config.dllPath 			== "" ||	// We need the path and name of the plugin DLL
		config.dllName 			== "" ||
		config.pluginLoaderPath == "" ){	// Without pluginloader.exe this doesn't work

		DBG_ERROR("your configuration file doesn't contain all necessary keys - aborting.");
		DBG_ERROR("please take a look at the original configuration file for more details.");
		return;
	}

	// Check if we should enable hardware acceleration
	if(config.overwriteArgs.find("enableGPUAcceleration") == config.overwriteArgs.end()){
		if(!checkGraphicDriver())
			config.overwriteArgs["enableGPUAcceleration"] = "false";
	}else{
		DBG_INFO("enableGPUAcceleration set manually - skipping compatibility check.");
	}

	// Check for correct installation
	if(!checkSilverlightInstallation()){
		DBG_ERROR("Silverlight not correctly installed - aborting.");
		return;
	}

	// Start wine process
	if(!startWineProcess()){
		DBG_ERROR("could not start wine process - aborting.");
		return;
	}

	// We want to be sure that wine is up and running until we return!
	try {
		callFunction(INIT_OKAY);
		waitReturn();
	} catch(std::runtime_error error){
		DBG_ERROR("error during the initialization of the wine process - aborting.");
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
		DBG_WARN("don't know where /bin/winepath is.");
		return "";

	}else if( config.winePrefix != "" && !checkIfExists(config.winePrefix) ){
		DBG_WARN("wine prefix doesn't exist.");
		return "";
	}

	int tempPipeIn[2];
	std::string resultPath;

	if( pipe(tempPipeIn) == -1 ){
		DBG_ERROR("could not create pipes to communicate with /bin/winepath.");
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
			DBG_ERROR("/bin/winepath did not run correctly (error occured).");
			return "";

		}else if(WEXITSTATUS(status) != 0){
			DBG_ERROR("/bin/winepath did not run correctly (exitcode = %d).", WEXITSTATUS(status));
			return "";
		}

	}else{

		close(tempPipeIn[0]);
		close(tempPipeIn[1]);	

		DBG_ERROR("unable to fork() - probably out of memory?");
		return "";

	}

	return resultPath;
}

bool checkSilverlightInstallation(){

	// Checking the silverlight installation is only possible if the user has defined a winePrefix
	if( config.winePrefix == "" ){
		DBG_WARN("no winePrefix defined - unable to check Silverlight installation.");
		return true;
	}

	// Output wine prefix
	DBG_INFO("using wine prefix directory %s.", config.winePrefix.c_str());

	// If there is no installer provided we cannot check the installation
	if( config.dependencyInstaller == "" || config.dependencies.size() == 0 || 
		!checkIfExists(config.dependencyInstaller) ){

		return checkIfExists(config.winePrefix);
	}

	// Run the installer ...
	DBG_INFO("checking Silverlight installation - this might take some time.");

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

		// Generate argv array
		std::vector<char*> argv;
		argv.push_back( (char*)config.dependencyInstaller.c_str());

		for(std::string &dep: config.dependencies){
			argv.push_back( (char*)dep.c_str());
		}

		argv.push_back(NULL);

		execvp(config.dependencyInstaller.c_str(), argv.data() );
		throw std::runtime_error("Error in execlp command - probably /bin/sh not found?");

	}else if(pidInstall != -1){

		int status;
		if(waitpid(pidInstall, &status, 0) == -1 || !WIFEXITED(status) ){
			DBG_ERROR("Silverlight installer did not run correctly (error occured).");
			return false;

		}else if(WEXITSTATUS(status) != 0){
			DBG_ERROR("Silverlight installer did not run correctly (exitcode = %d).", WEXITSTATUS(status));
			return false;
		}


	}else{
		DBG_ERROR("unable to fork() - probably out of memory?");
		return false;

	}

	return true;
}

bool checkGraphicDriver(){

	// Checking the silverlight installation is only possible if the user has defined a winePrefix
	if( config.graphicDriverCheck == "" ){
		DBG_ERROR("no GPU driver check script defined - treating test as failure.");
		return false;
	}

	if( !checkIfExists(config.graphicDriverCheck) ){
		DBG_ERROR("GPU driver check script not found - treating test as failure.");
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
			DBG_ERROR("GPU driver check did not run correctly (error occured).");
			return false;

		}else if(WEXITSTATUS(status) == 0){
			DBG_ERROR("GPU driver check - Your driver is supported, hardware acceleration enabled.");
			return true;

		}else if(WEXITSTATUS(status) == 1){
			DBG_ERROR("GPU driver check - Your driver is not in the whitelist, hardware acceleration disabled.");
			return false;

		}else{
			DBG_ERROR("GPU driver check did not run correctly (exitcode = %d).", WEXITSTATUS(status));
			return false;
		}

	}else{
		DBG_ERROR("unable to fork() - probably out of memory?");
		return false;
	}

	return false;
}

bool startWineProcess(){

	if( pipe(pipeOut) == -1 || pipe(pipeIn) == -1 ){
		DBG_ERROR("could not create pipes to communicate with the plugin.");
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

		// Disable buffering for input pipe (to allow waiting for a pipe)
		setbuf(pipeInF, NULL);


	}else{
		DBG_ERROR("unable to fork() - probably out of memory?");
		return false;
	}

	return true;
}


void dispatcher(int functionid, Stack &stack){
	if(!sBrowserFuncs) throw std::runtime_error("Browser didn't correctly initialize the plugin!");

	switch(functionid){
		
		case LIN_HANDLE_MANAGER_REQUEST_STREAM_INFO:
			{
				NPStream* stream = readHandleStream(stack); // shouldExist not necessary, Linux checks always
				DBG_TRACE("LIN_HANDLE_MANAGER_REQUEST_STREAM_INFO( stream=%p )", stream);

				writeString(stream->headers);
				writeHandleNotify(stream->notifyData, HANDLE_SHOULD_EXIST);
				writeInt32(stream->lastmodified);
				writeInt32(stream->end);
				writeString(stream->url);

				DBG_TRACE("LIN_HANDLE_MANAGER_REQUEST_STREAM_INFO -> ( headers='%s', notifyData=%p, lastmodified=%d, end=%d, url='%s' )", \
						stream->headers, stream->notifyData, stream->lastmodified, stream->end, stream->url);
				returnCommand();
			}
			break;

		case LIN_HANDLE_MANAGER_FREE_OBJECT:
			{
				NPObject* obj 		= readHandleObj(stack);
				DBG_TRACE("LIN_HANDLE_MANAGER_FREE_OBJECT( obj=%p )", obj);

				handlemanager.removeHandleByReal((uint64_t)obj, TYPE_NPObject);

				DBG_TRACE("LIN_HANDLE_MANAGER_FREE_OBJECT -> void");
				returnCommand();
			}
			break;

		case GET_WINDOW_RECT:
			{
				Window win 			= (Window)readInt32(stack);
				XWindowAttributes winattr;
				bool result         = false;
				Window dummy;
				DBG_TRACE("GET_WINDOW_RECT( win=%lu )", win);

				Display *display 	= XOpenDisplay(NULL);

				if(display){
					result 				= XGetWindowAttributes(display, win, &winattr);
					if(result) result 	= XTranslateCoordinates(display, win, RootWindow(display, 0), winattr.x, winattr.y, &winattr.x, &winattr.y, &dummy);


					XCloseDisplay(display);

				}else{
					DBG_ERROR("could not open Display!");
				}

				if(result){
					/*writeInt32(winattr.height);
					writeInt32(winattr.width);*/
					writeInt32(winattr.y);
					writeInt32(winattr.x);
				}

				writeInt32(result);

				DBG_TRACE("GET_WINDOW_RECT -> ( result=%d, ... )", result);
				returnCommand();
			}
			break;

		// Plugin specific commands (_GET_, _NP_ and _NPP_) not implemented

		case FUNCTION_NPN_CREATE_OBJECT:
			{
				NPP instance 			= readHandleInstance(stack);
				DBG_TRACE("FUNCTION_NPN_CREATE_OBJECT( instance=%p )", instance);

				NPObject* obj = sBrowserFuncs->createobject(instance, &myClass);
				writeHandleObj(obj); // refcounter is hopefully 1

				DBG_TRACE("FUNCTION_NPN_CREATE_OBJECT -> obj=%p", obj);
				returnCommand();
			}
			break;


		case FUNCTION_NPN_GETVALUE_BOOL:
			{
				NPP instance 			= readHandleInstance(stack);
				NPNVariable variable 	= (NPNVariable)readInt32(stack);
				DBG_TRACE("FUNCTION_NPN_GETVALUE_BOOL( instance=%p, variable=%d )", instance, variable);

				NPBool resultBool = 0;
				NPError result = sBrowserFuncs->getvalue(instance, variable, &resultBool);

				if(result == NPERR_NO_ERROR)
					writeInt32(resultBool);

				writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_GETVALUE_BOOL -> ( result=%d, ... )", result);
				returnCommand();
			}
			break;


		case FUNCTION_NPN_GETVALUE_OBJECT:
			{
				NPP instance 			= readHandleInstance(stack);
				NPNVariable variable 	= (NPNVariable)readInt32(stack);
				DBG_TRACE("FUNCTION_NPN_GETVALUE_OBJECT( instance=%p, variable=%d )", instance, variable);

				NPObject* obj = NULL;
				NPError result = sBrowserFuncs->getvalue(instance, variable, &obj);

				if(result == NPERR_NO_ERROR)
					writeHandleObj(obj); // Refcount was already incremented by getValue

				writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_GETVALUE_OBJECT -> ( result=%d, ... )", result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_GETVALUE_STRING:
			{
				NPP instance 			= readHandleInstance(stack);
				NPNVariable variable 	= (NPNVariable)readInt32(stack);

				char* str = NULL;
				NPError result = sBrowserFuncs->getvalue(instance, variable, &str);

				if(result == NPERR_NO_ERROR){
					writeString(str);

					if(str)
						sBrowserFuncs->memfree(str);
				}

				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_RELEASEOBJECT:
			{
				NPObject* obj 		= readHandleObj(stack);
				DBG_TRACE("FUNCTION_NPN_GETVALUE_OBJECT( obj=%p )", obj);

				// We do this check always, although its not really required, but this makes it easier to find errors
				if(obj->referenceCount == 1 && handlemanager.existsHandleByReal( (uint64_t)obj, TYPE_NPObject) ){
					writeHandleObj(obj);
					callFunction(WIN_HANDLE_MANAGER_OBJECT_IS_CUSTOM);

					if( !(bool)readResultInt32() ){
						throw std::runtime_error("Forgot to set killObject?");
					}
				}

				sBrowserFuncs->releaseobject(obj);

				DBG_TRACE("FUNCTION_NPN_RELEASEOBJECT -> void");
				returnCommand();
			}
			break;

		case FUNCTION_NPN_RETAINOBJECT:
			{
				NPObject* obj 				= readHandleObj(stack);
				uint32_t minReferenceCount 	= readInt32(stack);
				DBG_TRACE("FUNCTION_NPN_RETAINOBJECT( obj=%p, minReferenceCount=%d )", obj, minReferenceCount);

				sBrowserFuncs->retainobject(obj);

				if( minReferenceCount != REFCOUNT_UNDEFINED && obj->referenceCount < minReferenceCount ){
					throw std::runtime_error("Object referencecount smaller than expected?");
				}

				DBG_TRACE("FUNCTION_NPN_RETAINOBJECT -> void");
				returnCommand();
			}
			break;


		case FUNCTION_NPN_EVALUATE:
			{
				NPString script;

				NPP instance 		= readHandleInstance(stack);
				NPObject* obj 		= readHandleObj(stack);	
				readNPString(stack, script);
				NPVariant resultVariant;
				resultVariant.type 					= NPVariantType_Void;
				resultVariant.value.objectValue 	= NULL;
				DBG_TRACE("FUNCTION_NPN_EVALUATE( instance=%p, obj=%p )", instance, obj);

				bool result = sBrowserFuncs->evaluate(instance, obj, &script, &resultVariant);	
				
				// Free the string
				freeNPString(script);

				if(result)
					writeVariantRelease(resultVariant);

				writeInt32( result );

				DBG_TRACE("FUNCTION_NPN_EVALUATE -> ( result=%d, ... )", result);
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
				NPVariant resultVariant;
				resultVariant.type 					= NPVariantType_Void;
				resultVariant.value.objectValue 	= NULL;
				DBG_TRACE("FUNCTION_NPN_INVOKE( instance=%p, obj=%p, identifier=%p, argCount=%d, ... )", instance, obj, identifier, argCount);

				bool result = sBrowserFuncs->invoke(instance, obj, identifier, args.data(), argCount, &resultVariant);

				// Free the variant array
				freeVariantArray(args);

				if(result)
					writeVariantRelease(resultVariant);

				writeInt32( result );

				DBG_TRACE("FUNCTION_NPN_INVOKE -> ( result=%d, ... )", result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_INVOKE_DEFAULT:
			{
				NPP instance 					= readHandleInstance(stack);
				NPObject* obj 					= readHandleObj(stack);
				int32_t argCount				= readInt32(stack);
				std::vector<NPVariant> args 	= readVariantArray(stack, argCount);
				NPVariant resultVariant;
				resultVariant.type 					= NPVariantType_Void;
				resultVariant.value.objectValue 	= NULL;
				DBG_TRACE("FUNCTION_NPN_INVOKE_DEFAULT( instance=%p, obj=%p, argCount=%d, ... )", instance, obj, argCount);

				bool result = sBrowserFuncs->invokeDefault(instance, obj, args.data(), argCount, &resultVariant);

				// Free the variant array
				freeVariantArray(args);

				if(result)
					writeVariantRelease(resultVariant);

				writeInt32( result );

				DBG_TRACE("FUNCTION_NPN_INVOKE_DEFAULT -> ( result=%d, ... )", result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_HAS_PROPERTY:
			{
				NPP instance 					= readHandleInstance(stack);
				NPObject* obj 					= readHandleObj(stack);
				NPIdentifier identifier			= readHandleIdentifier(stack);
				DBG_TRACE("FUNCTION_NPN_HAS_PROPERTY( instance=%p, obj=%p, identifier=%p )", instance, obj, identifier);

				bool result = sBrowserFuncs->hasproperty(instance, obj, identifier);
				writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_HAS_PROPERTY -> result=%d", result);
				returnCommand();	
			}
			break;

		case FUNCTION_NPN_HAS_METHOD:
			{
				NPP instance 					= readHandleInstance(stack);
				NPObject* obj 					= readHandleObj(stack);
				NPIdentifier identifier			= readHandleIdentifier(stack);
				DBG_TRACE("FUNCTION_NPN_HAS_METHOD( instance=%p, obj=%p, identifier=%p )", instance, obj, identifier);

				bool result = sBrowserFuncs->hasmethod(instance, obj, identifier);
				writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_HAS_METHOD -> result=%d", result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_GET_PROPERTY:
			{
				NPP instance 				= readHandleInstance(stack);
				NPObject*  obj 				= readHandleObj(stack);
				NPIdentifier propertyName	= readHandleIdentifier(stack);
				NPVariant resultVariant;
				resultVariant.type 					= NPVariantType_Void;
				resultVariant.value.objectValue 	= NULL;
				DBG_TRACE("FUNCTION_NPN_GET_PROPERTY( instance=%p, obj=%p, propertyName=%p )", instance, obj, propertyName);

				bool result = sBrowserFuncs->getproperty(instance, obj, propertyName, &resultVariant);
				if(result)
					writeVariantRelease(resultVariant);
				writeInt32( result );

				DBG_TRACE("FUNCTION_NPN_GET_PROPERTY -> ( result=%d, ... )", result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_SET_PROPERTY:
			{
				NPP instance 					= readHandleInstance(stack);
				NPObject* obj 					= readHandleObj(stack);
				NPIdentifier propertyName		= readHandleIdentifier(stack);
				NPVariant value;
				readVariant(stack, value);
				DBG_TRACE("FUNCTION_NPN_SET_PROPERTY( instance=%p, obj=%p, propertyName=%p, value=%p )", instance, obj, propertyName, &value);

				bool result = sBrowserFuncs->setproperty(instance, obj, propertyName, &value);
				writeInt32(result);
				freeVariant(value);

				DBG_TRACE("FUNCTION_NPN_SET_PROPERTY -> result=%d", result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_REMOVE_PROPERTY:
			{
				NPP instance 					= readHandleInstance(stack);
				NPObject* obj 					= readHandleObj(stack);
				NPIdentifier propertyName		= readHandleIdentifier(stack);
				DBG_TRACE("FUNCTION_NPN_REMOVE_PROPERTY( instance=%p, obj=%p, propertyName=%p )", instance, obj, propertyName);

				bool result = sBrowserFuncs->removeproperty(instance, obj, propertyName);
				writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_REMOVE_PROPERTY -> result=%d", result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_ENUMERATE:
			{
				NPP instance 					= readHandleInstance(stack);
				NPObject *obj 					= readHandleObj(stack);
				NPIdentifier*   identifierTable  = NULL;
				uint32_t 		identifierCount  = 0;
				DBG_TRACE("FUNCTION_NPN_ENUMERATE( instance=%p, obj=%p )", instance, obj);

				bool result = sBrowserFuncs->enumerate(instance, obj, &identifierTable, &identifierCount);

				if(result){
					writeIdentifierArray(identifierTable, identifierCount);
					writeInt32(identifierCount);
					
					// Free the memory for the table
					if(identifierTable)
						sBrowserFuncs->memfree(identifierTable);
				}

				writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_ENUMERATE -> ( result=%d, ... )", result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_SET_EXCEPTION:
			{
				NPObject* obj 					= readHandleObj(stack);
				std::shared_ptr<char> message 	= readStringAsMemory(stack);
				DBG_TRACE("FUNCTION_NPN_SET_EXCEPTION( instance=%p, obj=%p )", obj, message.get());

				sBrowserFuncs->setexception(obj, message.get());

				DBG_TRACE("FUNCTION_NPN_SET_EXCEPTION -> void");
				returnCommand();
			}
			break;

		case FUNCTION_NPN_GET_URL_NOTIFY:
			{
				NPP instance 					= readHandleInstance(stack);
				std::shared_ptr<char> url 		= readStringAsMemory(stack);
				std::shared_ptr<char> target 	= readStringAsMemory(stack);
				NotifyDataRefCount* notifyData 	= (NotifyDataRefCount*)readHandleNotify(stack);
				DBG_TRACE("FUNCTION_NPN_GET_URL_NOTIFY( instance=%p, url='%s', target='%s', notifyData=%p )", instance, url.get(), target.get(), notifyData);

				// Increase refcounter
				if(notifyData){
					notifyData->referenceCount++;
				}

				NPError result = sBrowserFuncs->geturlnotify(instance, url.get(), target.get(), notifyData);
				writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_GET_URL_NOTIFY -> result=%d", result);
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
				DBG_TRACE("FUNCTION_NPN_POST_URL_NOTIFY( instance=%p, url='%s', target='%s', buffer=%p, len=%lu, file=%d, notifyData=%p )", instance, url.get(), target.get(), buffer.get(), len, file, notifyData);

				// Increase refcounter
				if(notifyData){
					notifyData->referenceCount++;
				}

				NPError result = sBrowserFuncs->posturlnotify(instance, url.get(), target.get(), len, buffer.get(), file, notifyData);
				writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_POST_URL_NOTIFY -> result=%d", result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_GET_URL:
			{
				NPP instance 					= readHandleInstance(stack);
				std::shared_ptr<char> url 		= readStringAsMemory(stack);
				std::shared_ptr<char> target 	= readStringAsMemory(stack);
				DBG_TRACE("FUNCTION_NPN_GET_URL( instance=%p, url='%s', target='%s' )", instance, url.get(), target.get());

				NPError result = sBrowserFuncs->geturl(instance, url.get(), target.get());
				writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_GET_URL -> result=%d", result);
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
				DBG_TRACE("FUNCTION_NPN_POST_URL( instance=%p, url='%s', target='%s', buffer=%p, len=%lu, file=%d )", instance, url.get(), target.get(), buffer.get(), len, file );

				NPError result = sBrowserFuncs->posturl(instance, url.get(), target.get(), len, buffer.get(), file);
				writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_POST_URL -> result=%d", result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_REQUEST_READ: // UNTESTED!
			{
				NPStream *stream 				= readHandleStream(stack);
				uint32_t rangeCount				= readInt32(stack);
				NPByteRange *byteRange 			= NULL;
				DBG_TRACE("FUNCTION_NPN_REQUEST_READ( stream=%p, rangeCount=%d, ... )", stream, rangeCount );

				for(unsigned int i = 0; i < rangeCount; i++){
					NPByteRange *newByteRange = (NPByteRange*)malloc(sizeof(NPByteRange));
					if(!newByteRange) break; // Unable to send all requests, but shouldn't occur

					newByteRange->offset = readInt32(stack);
					newByteRange->length = readInt32(stack);
					newByteRange->next   = byteRange;

					byteRange = newByteRange;
				}

				NPError result = sBrowserFuncs->requestread(stream, byteRange);

				// Free the linked list
				while(byteRange){
					NPByteRange *nextByteRange = byteRange->next;
					free(byteRange);
					byteRange = nextByteRange;
				}

				writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_REQUEST_READ -> result=%d", result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_WRITE:
			{
				size_t len;
				NPP instance 					= readHandleInstance(stack);
				NPStream *stream 				= readHandleStream(stack);
				std::shared_ptr<char> buffer	= readMemory(stack, len);	
				DBG_TRACE("FUNCTION_NPN_WRITE( instance=%p, stream=%p, buffer=%p, len=%lu )", instance, stream, buffer.get(), len );

				int32_t result = sBrowserFuncs->write(instance, stream, len, buffer.get());
				writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_WRITE -> result=%d", result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_NEW_STREAM:
			{
				NPP instance 					= readHandleInstance(stack);
				std::shared_ptr<char> type 		= readStringAsMemory(stack);
				std::shared_ptr<char> target 	= readStringAsMemory(stack);
				DBG_TRACE("FUNCTION_NPN_NEW_STREAM( instance=%p, type='%s', target='%s' )", instance, type.get(), target.get() );

				NPStream* stream = NULL;
				NPError result = sBrowserFuncs->newstream(instance, type.get(), target.get(), &stream);
				if(result == NPERR_NO_ERROR)
					writeHandleStream(stream);
				writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_NEW_STREAM -> ( result=%d, ... )", result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_DESTROY_STREAM:
			{
				NPP instance 		= readHandleInstance(stack);
				NPStream *stream 	= readHandleStream(stack, HANDLE_SHOULD_EXIST);
				NPReason reason 	= (NPReason) readInt32(stack);
				DBG_TRACE("FUNCTION_NPN_DESTROY_STREAM( instance=%p, stream=%p, reason=%d )", instance, stream, reason );

				NPError result = sBrowserFuncs->destroystream(instance, stream, reason);
				writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_DESTROY_STREAM -> result=%d", result);
				returnCommand();
			}
			break;		

		case FUNCTION_NPN_STATUS:
			{		
				NPP instance 					= readHandleInstance(stack);
				std::shared_ptr<char> message	= readStringAsMemory(stack);

				DBG_TRACE("FUNCTION_NPN_STATUS( instance=%p, message='%s' )", instance, message.get() );
				sBrowserFuncs->status(instance, message.get());

				DBG_TRACE("FUNCTION_NPN_DESTROY_STREAM -> void");
				returnCommand();
			}
			break;

		case FUNCTION_NPN_USERAGENT:
			{
				NPP instance 					= readHandleInstance(stack);
				DBG_TRACE("FUNCTION_NPN_USERAGENT( instance=%p )", instance );

				const char* uagent = sBrowserFuncs->uagent(instance);
				writeString(uagent);

				DBG_TRACE("FUNCTION_NPN_USERAGENT -> uagent='%s'", uagent);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_IDENTIFIER_IS_STRING:
			{
				NPIdentifier identifier = readHandleIdentifier(stack);
				DBG_TRACE("FUNCTION_NPN_IDENTIFIER_IS_STRING( identifier=%p )", identifier );

				bool result = sBrowserFuncs->identifierisstring(identifier);
				writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_IDENTIFIER_IS_STRING -> result=%d", result );
				returnCommand();
			}
			break;

		case FUNCTION_NPN_UTF8_FROM_IDENTIFIER:
			{
				NPIdentifier identifier	= readHandleIdentifier(stack);
				DBG_TRACE("FUNCTION_NPN_UTF8_FROM_IDENTIFIER( identifier=%p )", identifier );

				NPUTF8 *str = sBrowserFuncs->utf8fromidentifier(identifier);
				writeString((char*) str);

				DBG_TRACE("FUNCTION_NPN_UTF8_FROM_IDENTIFIER -> str='%s'", str );

				// Free the string
				if(str)
					sBrowserFuncs->memfree(str);

				returnCommand();
			}
			break;


		case FUNCTION_NPN_INT_FROM_IDENTIFIER:
			{
				NPIdentifier identifier = readHandleIdentifier(stack);
				DBG_TRACE("FUNCTION_NPN_IDENTIFIER_IS_STRING( identifier=%p )", identifier );

				int32_t result = sBrowserFuncs->intfromidentifier(identifier);
				writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_IDENTIFIER_IS_STRING -> result=%d", result );
				returnCommand();
			}
			break;

		case FUNCTION_NPN_GET_STRINGIDENTIFIER:
			{
				std::shared_ptr<char> utf8name 	= readStringAsMemory(stack);
				DBG_TRACE("FUNCTION_NPN_GET_STRINGIDENTIFIER( utf8name='%s' )", utf8name.get() );

				NPIdentifier identifier 		= sBrowserFuncs->getstringidentifier((NPUTF8*) utf8name.get());
				writeHandleIdentifier(identifier);

				DBG_TRACE("FUNCTION_NPN_GET_STRINGIDENTIFIER -> identifier=%p", identifier );
				returnCommand();
			}
			break;

		case FUNCTION_NPN_GET_INTIDENTIFIER:
			{
				int32_t intid 					= readInt32(stack);
				DBG_TRACE("FUNCTION_NPN_GET_INTIDENTIFIER( intid='%d' )", intid );

				NPIdentifier identifier 		= sBrowserFuncs->getintidentifier(intid);
				writeHandleIdentifier(identifier);

				DBG_TRACE("FUNCTION_NPN_GET_INTIDENTIFIER -> identifier=%p", identifier );
				returnCommand();
			}
			break;

		default:
			throw std::runtime_error("Specified function not found!");
			break;
	}
}
