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
#include <sys/wait.h>
#include <unistd.h>								// for POSIX api
#include <iostream>								// for std::ios_base
#include <string>								// for std::string
#include <X11/Xlib.h>							// for XSendEvent, ...
#include <X11/Xmd.h>							// for CARD32

#include "../common/common.h"
#include "basicplugin.h"

/* BEGIN GLOBAL VARIABLES

	Note: As global variables should be initialized properly BEFORE the attach function is called.
	Otherwise there might be some uninitialized variables...

*/

char strMimeType[2048] 				= {0};
char strPluginVersion[100]			= {0};
char strPluginName[256] 			= {0};
char strPluginDescription[1024]		= {0};

uint32_t  	eventTimerID 			= 0;
NPP 		eventTimerInstance 		= NULL;
pthread_t 	eventThread				= 0;

sem_t		eventThreadSemRequestAsyncCall;
sem_t		eventThreadSemScheduledAsyncCall;

pid_t 		winePid 				= -1;
bool 		initOkay 				= false;

// Browser functions
NPNetscapeFuncs* sBrowserFuncs 		= NULL;

// Global plugin configuration
PluginConfig config INIT_EARLY;

// Attach has to be called as the last step
void attach() CONSTRUCTOR;
void detach() DESTRUCTOR;

/* END GLOBAL VARIABLES */

void attach(){
	std::ios_base::sync_with_stdio(false);		/* Fix for Opera: Dont sync stdio */
	setbuf(stderr, NULL);						/* Disable stderr buffering */

	DBG_INFO("attached to process.");

	/* Initialize semaphores */
	sem_init(&eventThreadSemRequestAsyncCall, 0, 0);
	sem_init(&eventThreadSemScheduledAsyncCall, 0, 0);

	initOkay = false;

	/* load config file */
	if (!loadConfig(config)){
		DBG_ERROR("unable to load config file - aborting.");
		return;
	}

	/* ensure that all necessary keys are provided */
	if 	(	config.winePath == "" || ((config.dllPath == "" || config.dllName == "") && config.regKey == "") ||
			config.pluginLoaderPath == "" || config.winePrefix == "" ){
		DBG_ERROR("Your configuration file doesn't contain all necessary keys - aborting.");
		DBG_ERROR("please take a look at the original configuration file for more details.");
		return;
	}

	/* sandbox specified, but doesn't exist */
	if (config.sandboxPath != "" && !checkIfExists(config.sandboxPath)){
		DBG_WARN("sandbox not found / not installed!");
		config.sandboxPath = "";
	}

	/* check if hw acceleration should be used (only for Silverlight) */
	if (config.silverlightGraphicDriverCheck != ""){
		int gpuAcceleration = getEnvironmentInteger("PIPELIGHT_GPUACCELERATION", -1);

		if (gpuAcceleration == 0){
			DBG_INFO("enableGPUAcceleration set via commandline to 'false'");
			config.overwriteArgs["enableGPUAcceleration"] = "false";

		}else if (gpuAcceleration > 0){
			DBG_INFO("enableGPUAcceleration set via commandline to 'true'");
			config.overwriteArgs["enableGPUAcceleration"] = "true";
			if (gpuAcceleration > 1)
				config.experimental_renderTopLevelWindow = true;

		}else if (config.overwriteArgs.find("enableGPUAcceleration") == config.overwriteArgs.end()){
			if (!checkSilverlightGraphicDriver())
				config.overwriteArgs["enableGPUAcceleration"] = "false";

		}else{
			DBG_INFO("enableGPUAcceleration set manually - skipping compatibility check.");
		}
	}

	/* Check for correct installation */
	if (!checkPluginInstallation()){
		DBG_ERROR("plugin not correctly installed - aborting.");
		return;
	}

	/* Start wine process */
	if (!startWineProcess()){
		DBG_ERROR("could not start wine process - aborting.");
		return;
	}

	/* We want to be sure that wine is up and running until we return! */
	if (!pluginInitOkay()){
		DBG_ERROR("error during the initialization of the wine process - aborting.");
		return;
	}

 	/* tell the windows side that a sandbox is active */
	if (config.sandboxPath != ""){
		writeInt32( (config.sandboxPath != "") );
		callFunction( CHANGE_SANDBOX_STATE );
		readResultVoid();
	}

 	/* initialisation successful */
	initOkay = true;
}

void detach(){
	/* TODO: Deinitialize pointers etc. */
}

/* convertWinePath */
std::string convertWinePath(std::string path, bool direction){
	if (!checkIfExists(config.winePrefix)){
		DBG_WARN("wine prefix doesn't exist.");
		return "";
	}

	std::string resultPath;
	int tempPipeIn[2];

	if (pipe(tempPipeIn) == -1){
		DBG_ERROR("could not create pipes to communicate with winepath.exe.");
		return "";
	}

	pid_t pidWinePath = fork();
	if (pidWinePath == 0){

		close(0);
		close(tempPipeIn[0]);
		dup2(tempPipeIn[1], 1);

		/* Setup environment variables */
		setenv("WINEPREFIX", 			config.winePrefix.c_str(), 			true);

		if (config.wineArch != "")
			setenv("WINEARCH", 			config.wineArch.c_str(), 			true);

		if (config.wineDLLOverrides != "")
			setenv("WINEDLLOVERRIDES", 	config.wineDLLOverrides.c_str(), 	true);

		/* Generate argv array */
		std::vector<const char*> argv;
		std::string argument = direction ? "--windows" : "--unix";

		if (config.sandboxPath != "")
			argv.push_back( config.sandboxPath.c_str() );

		argv.push_back( config.winePath.c_str() );
		argv.push_back( "winepath.exe" );
		argv.push_back( argument.c_str() );
		argv.push_back( path.c_str() );
		argv.push_back(NULL);

		execvp(argv[0], (char**)argv.data());
		DBG_ABORT("error in execvp command - probably wine/sandbox not found or missing execute permission.");

	}else if (pidWinePath != -1){
		char resultPathBuffer[4096+1];

		close(tempPipeIn[1]);
		FILE *tempPipeInF = fdopen(tempPipeIn[0], "rb");

		if (tempPipeInF != NULL){

			if (fgets( (char*)&resultPathBuffer, sizeof(resultPathBuffer), tempPipeInF))
				resultPath = trim( std::string( (char*)&resultPathBuffer) );

			fclose(tempPipeInF);
		}

		int status;
		if (waitpid(pidWinePath, &status, 0) == -1 || !WIFEXITED(status) ){
			DBG_ERROR("winepath.exe did not run correctly (error occured).");
			return "";

		}else if (WEXITSTATUS(status) != 0){
			DBG_ERROR("winepath.exe did not run correctly (exitcode = %d).", WEXITSTATUS(status));
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

/* getWineVersion */
std::string getWineVersion(){
	std::string wineVersion;
	int tempPipeIn[2];

	if (pipe(tempPipeIn) == -1){
		DBG_ERROR("could not create pipes to communicate with wine.");
		return "";
	}

	pid_t pidWineVersion = fork();
	if (pidWineVersion == 0){

		close(0);
		close(tempPipeIn[0]);
		dup2(tempPipeIn[1], 1);

		/* Generate argv array */
		std::vector<const char*> argv;

		if (config.sandboxPath != "")
			argv.push_back( config.sandboxPath.c_str() );

		argv.push_back( config.winePath.c_str() );
		argv.push_back( "--version" );
		argv.push_back(NULL);

		execvp(argv[0], (char**)argv.data());
		DBG_ABORT("error in execvp command - probably wine/sandbox not found or missing execute permission.");

	}else if (pidWineVersion != -1){
		char resultPathBuffer[4096+1];

		close(tempPipeIn[1]);
		FILE *tempPipeInF = fdopen(tempPipeIn[0], "rb");

		if (tempPipeInF != NULL){

			if (fgets( resultPathBuffer, sizeof(resultPathBuffer), tempPipeInF))
				wineVersion = trim( std::string( resultPathBuffer) );

			fclose(tempPipeInF);
		}

		int status;
		if (waitpid(pidWineVersion, &status, 0) == -1 || !WIFEXITED(status) ){
			DBG_ERROR("wine did not run correctly (error occured).");
			return "";

		}else if (WEXITSTATUS(status) != 0){
			DBG_ERROR("wine did not run correctly (exitcode = %d).", WEXITSTATUS(status));
			return "";
		}

	}else{
		close(tempPipeIn[0]);
		close(tempPipeIn[1]);

		DBG_ERROR("unable to fork() - probably out of memory?");
		return "";
	}

	return wineVersion;
}

/* checkPluginInstallation */
bool checkPluginInstallation(){

	/* Output wine prefix */
	DBG_INFO("using wine prefix directory %s.", config.winePrefix.c_str());

	/* If there is no installer provided we cannot check the installation */
	if (config.dependencyInstaller == "" || config.dependencies.empty() || !checkIfExists(config.dependencyInstaller) )
		return checkIfExists(config.winePrefix);

	/* Run the installer ... */
	DBG_INFO("checking plugin installation - this might take some time.");

	/* When using a sandbox, we have to create the directory in advance */
	if (config.sandboxPath != ""){
		if (mkdir(config.winePrefix.c_str(), 0755) != 0 && errno != EEXIST){
			DBG_ERROR("unable to manually create wine prefix.");
			return false;
		}
	}

	pid_t pidInstall = fork();
	if (pidInstall == 0){

		close(0);

		/* Setup environment variables */
		setenv("WINEPREFIX", 			config.winePrefix.c_str(), 			true);
		setenv("WINE", 					config.winePath.c_str(), 			true);

		if (config.wineArch != "")
			setenv("WINEARCH", 			config.wineArch.c_str(), 			true);

		if (config.wineDLLOverrides != "")
			setenv("WINEDLLOVERRIDES", 	config.wineDLLOverrides.c_str(), 	true);

		if (config.quietInstallation)
			setenv("QUIETINSTALLATION",	"1", 								true);

		/* Generate argv array */
		std::vector<const char*> argv;

		if (config.sandboxPath != "")
			argv.push_back( config.sandboxPath.c_str() );

		argv.push_back( config.dependencyInstaller.c_str());

		for (std::vector<std::string>::iterator it = config.dependencies.begin(); it != config.dependencies.end(); it++)
			argv.push_back( it->c_str());

		argv.push_back(NULL);

		execvp(argv[0], (char**)argv.data());
		DBG_ABORT("error in execvp command - probably dependencyInstaller/sandbox not found or missing execute permission.");

	}else if (pidInstall != -1){

		int status;
		if (waitpid(pidInstall, &status, 0) == -1 || !WIFEXITED(status) ){
			DBG_ERROR("Plugin installer did not run correctly (error occured).");
			return false;

		}else if (WEXITSTATUS(status) != 0){
			DBG_ERROR("Plugin installer did not run correctly (exitcode = %d).", WEXITSTATUS(status));
			return false;
		}

	}else{
		DBG_ERROR("unable to fork() - probably out of memory?");
		return false;
	}

	return true;
}

/* checkSilverlightGraphicDriver */
bool checkSilverlightGraphicDriver(){

	if (config.silverlightGraphicDriverCheck == ""){
		DBG_ERROR("no GPU driver check script defined - treating test as failure.");
		return false;
	}

	/* Shortcuts */
	if (config.silverlightGraphicDriverCheck == "/bin/true"){
		DBG_INFO("GPU driver check - Manually set to /bin/true.");
		return true;
	}else if (config.silverlightGraphicDriverCheck == "/bin/false"){
		DBG_INFO("GPU driver check - Manually set to /bin/false.");
		return false;
	}

	if (!checkIfExists(config.silverlightGraphicDriverCheck)){
		DBG_ERROR("GPU driver check script not found - treating test as failure.");
		return false;
	}

	pid_t pidCheck = fork();
	if (pidCheck == 0){

		close(0);

		/* The graphic driver check doesn't need any environment variables or sandbox at all. */
		execlp(config.silverlightGraphicDriverCheck.c_str(), config.silverlightGraphicDriverCheck.c_str(), NULL);
		DBG_ABORT("error in execlp command - probably silverlightGraphicDriverCheck not found or missing execute permission.");

	}else if (pidCheck != -1){

		int status;
		if (waitpid(pidCheck, &status, 0) == -1 || !WIFEXITED(status)){
			DBG_ERROR("GPU driver check did not run correctly (error occured).");
			return false;

		}else if (WEXITSTATUS(status) == 0){
			DBG_INFO("GPU driver check - Your driver is supported, hardware acceleration enabled.");
			return true;

		}else if (WEXITSTATUS(status) == 1){
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

/* startWineProcess */
bool startWineProcess(){
	int tempPipeOut[2], tempPipeIn[2];

	if (pipe(tempPipeOut) == -1 || pipe(tempPipeIn) == -1){
		DBG_ERROR("could not create pipes to communicate with the plugin.");
		return false;
	}

	winePid = fork();
	if (winePid == 0){
		/* The child process will be replaced with wine */

		close(tempPipeIn[0]);
		close(tempPipeOut[1]);

		/* Assign to stdin/stdout */
		dup2(tempPipeOut[0],  0);
		dup2(tempPipeIn[1],   1);

		/* Setup environment variables */
		setenv("WINEPREFIX", 			config.winePrefix.c_str(), 			true);

		if (config.wineArch != "")
			setenv("WINEARCH", 			config.wineArch.c_str(), 			true);

		if (config.wineDLLOverrides != "")
			setenv("WINEDLLOVERRIDES", 	config.wineDLLOverrides.c_str(), 	true);

		if (config.gccRuntimeDLLs != ""){
			std::string runtime = getEnvironmentString("Path");
			runtime = config.gccRuntimeDLLs + ((runtime != "") ? (";" + runtime) : "");
			setenv("Path", runtime.c_str(), true);
		}

		/* Generate argv array */
		std::vector<const char*> argv;

		if (config.sandboxPath != "")
			argv.push_back( config.sandboxPath.c_str() );

		argv.push_back( config.winePath.c_str() );
		argv.push_back( config.pluginLoaderPath.c_str() );

		/* send the plugin name to the other side */
		argv.push_back( "--pluginName" );
		argv.push_back( strMultiPluginName );

		/* configuration options */
		if (config.dllPath != ""){
			argv.push_back( "--dllPath" );
			argv.push_back( config.dllPath.c_str() );
		}

		if (config.dllName != ""){
			argv.push_back( "--dllName" );
			argv.push_back( config.dllName.c_str() );
		}

		if (config.regKey != ""){
			argv.push_back( "--regKey" );
			argv.push_back( config.regKey.c_str() );
		}

		if (config.windowlessMode)
			argv.push_back( "--windowless" );

		if (config.embed)
			argv.push_back( "--embed" );

		if (config.experimental_unityHacks)
			argv.push_back( "--unityhacks" );

		if (config.experimental_windowClassHook)
			argv.push_back( "--windowclasshook" );

		if (config.experimental_renderTopLevelWindow)
			argv.push_back( "--rendertoplevelwindow" );

		if (config.experimental_linuxWindowlessMode)
			argv.push_back( "--linuxwindowless" );

		argv.push_back(NULL);

		/* Execute wine */
		execvp(argv[0], (char**)argv.data());
		DBG_ABORT("error in execvp command - probably wine/sandbox not found or missing execute permission.");

	}else if (winePid != -1){
		/* The parent process will return normally and use the pipes to communicate with the child process */

		close(tempPipeOut[0]);
		close(tempPipeIn[1]);

		if (!initCommPipes(tempPipeOut[1], tempPipeIn[0]))
			return false;

	}else{
		DBG_ERROR("unable to fork() - probably out of memory?");
		return false;
	}

	return true;
}

/* sendXembedMessage */
void sendXembedMessage(Display* display, Window win, long message, long detail, long data1, long data2){
	XEvent ev;
	memset(&ev, 0, sizeof(ev));

	ev.xclient.type 		= ClientMessage;
	ev.xclient.window 		= win;
	ev.xclient.message_type = XInternAtom(display, "_XEMBED", False);
	ev.xclient.format 		= 32;

	ev.xclient.data.l[0] 	= CurrentTime;
	ev.xclient.data.l[1] 	= message;
	ev.xclient.data.l[2] 	= detail;
	ev.xclient.data.l[3] 	= data1;
	ev.xclient.data.l[4] 	= data2;

	XSendEvent(display, win, False, NoEventMask, &ev);
	XSync(display, False);
}

/* setXembedWindowInfo */
void setXembedWindowInfo(Display* display, Window win, int flags){
	CARD32 list[2];
	Atom xembedInfo = XInternAtom(display, "_XEMBED_INFO", False);

	list[0] = 0;
	list[1] = flags;

	XChangeProperty(display, win, xembedInfo, xembedInfo, 32, PropModeReplace, (unsigned char *)list, 2);
	XSync(display, False);
}

/* dispatcher */
void dispatcher(int functionid, Stack &stack){
	DBG_ASSERT(sBrowserFuncs, "browser didn't correctly initialize the plugin!");

	switch (functionid){

		case LIN_HANDLE_MANAGER_REQUEST_STREAM_INFO:
			{
				NPStream* stream = readHandleStream(stack); /* shouldExist not necessary, Linux checks always */
				DBG_TRACE("LIN_HANDLE_MANAGER_REQUEST_STREAM_INFO( stream=%p )", stream);

				writeString(stream->headers);
				writeHandleNotify(stream->notifyData, HMGR_SHOULD_EXIST);
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

				handleManager_removeByPtr(HMGR_TYPE_NPObject, obj);

				DBG_TRACE("LIN_HANDLE_MANAGER_FREE_OBJECT -> void");
				returnCommand();
			}
			break;

		case GET_WINDOW_RECT:
			{
				Window win 				= (Window)readInt32(stack);
				bool result         	= false;
				XWindowAttributes winattr;
				Window dummy;
				DBG_TRACE("GET_WINDOW_RECT( win=%lu )", win);

				Display *display 		= XOpenDisplay(NULL);

				if (display){
					result 				= XGetWindowAttributes(display, win, &winattr);
					if (result) result 	= XTranslateCoordinates(display, win, RootWindow(display, 0), winattr.x, winattr.y, &winattr.x, &winattr.y, &dummy);

					XCloseDisplay(display);

				}else
					DBG_ERROR("could not open display!");

				if (result)
					writeRectXYWH(winattr.x, winattr.y, winattr.width, winattr.height);

				writeInt32(result);

				DBG_TRACE("GET_WINDOW_RECT -> ( result=%d, ... )", result);
				returnCommand();
			}
			break;

		case CHANGE_EMBEDDED_MODE:
			{
				NPP instance 			= readHandleInstance(stack);
				Window win 				= (Window)readInt32(stack);
				bool embed 				= (bool)readInt32(stack);
				DBG_TRACE("CHANGE_EMBEDDED_MODE( instance=%p, win=%lu, embed=%d )", instance, win, embed);

				PluginData *pdata = (PluginData*)instance->pdata;
				if (pdata){
					pdata->plugin = win;

					if (pdata->container){
						Display *display = XOpenDisplay(NULL);

						if (display){
							Window parentWindow;

							/* embed into child window */
							if (embed){
								parentWindow = (Window)getEnvironmentInteger("PIPELIGHT_X11WINDOW");
								if (!parentWindow)
									parentWindow = pdata->container;

							/* reparent to root window */
							}else
								parentWindow = RootWindow(display, 0);

							XReparentWindow(display, win, parentWindow, 0, 0);
							sendXembedMessage(display, win, XEMBED_EMBEDDED_NOTIFY, 0, parentWindow, 0);
							sendXembedMessage(display, win, XEMBED_FOCUS_OUT, 0, 0, 0);
							XCloseDisplay(display);

						}else
							DBG_ERROR("could not open display!");
					}

				}

				DBG_TRACE("CHANGE_EMBEDDED_MODE -> void");
				returnCommand();
			}
			break;

		case FUNCTION_NPN_CREATE_OBJECT:
			{
				NPP instance 			= readHandleInstance(stack);
				DBG_TRACE("FUNCTION_NPN_CREATE_OBJECT( instance=%p )", instance);

				NPObject* obj = sBrowserFuncs->createobject(instance, &myClass);
				writeHandleObj(obj); /* refcounter is hopefully 1 */

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
				NPError result;

				if (variable == NPNVprivateModeBool){
					result = sBrowserFuncs->getvalue(instance, variable, &resultBool);

				}else{
					DBG_WARN("FUNCTION_NPN_GETVALUE_BOOL - variable %d not allowed", variable);
					result = NPERR_GENERIC_ERROR;
				}

				if (result == NPERR_NO_ERROR)
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
				NPError result;

				if (variable == NPNVPluginElementNPObject || variable == NPNVWindowNPObject){
					result = sBrowserFuncs->getvalue(instance, variable, &obj);

				}else{
					DBG_WARN("FUNCTION_NPN_GETVALUE_OBJECT - variable %d not allowed", variable);
					result = NPERR_GENERIC_ERROR;
				}

				if (result == NPERR_NO_ERROR)
					writeHandleObj(obj); /* Refcount was already incremented by getValue */

				writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_GETVALUE_OBJECT -> ( result=%d, ... )", result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_GETVALUE_STRING:
			{
				NPP instance 			= readHandleInstance(stack);
				NPNVariable variable 	= (NPNVariable)readInt32(stack);
				DBG_TRACE("FUNCTION_NPN_GETVALUE_STRING( instance=%p, variable=%d )", instance, variable);

				char* str = NULL;
				NPError result;

				if (variable == NPNVdocumentOrigin){
					result = sBrowserFuncs->getvalue(instance, variable, &str);

				}else{
					DBG_WARN("FUNCTION_NPN_GETVALUE_STRING - variable %d not allowed", variable);
					result = NPERR_GENERIC_ERROR;
				}

				if (result == NPERR_NO_ERROR){
					writeString(str);
					if (str) sBrowserFuncs->memfree(str);
				}

				writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_GETVALUE_STRING -> ( result=%d, ... )", result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_RELEASEOBJECT:
			{
				NPObject* obj 				= readHandleObj(stack);
				uint32_t minReferenceCount 	= readInt32(stack);
				DBG_TRACE("FUNCTION_NPN_RELEASEOBJECT( obj=%p, minReferenceCount=%d )", obj, minReferenceCount);

				DBG_ASSERT( minReferenceCount == REFCOUNT_UNDEFINED || minReferenceCount <= obj->referenceCount, \
					"object referenceCount smaller than expected?");

				if (obj->referenceCount == 1 && handleManager_existsByPtr(HMGR_TYPE_NPObject, obj))
					DBG_ASSERT((minReferenceCount == REFCOUNT_UNDEFINED), "forgot to set killObject?");

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

				DBG_ASSERT( minReferenceCount == REFCOUNT_UNDEFINED || minReferenceCount <= obj->referenceCount, \
					"object referenceCount smaller than expected?");

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
				freeNPString(script); /* free the string */
				if (result)
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
				freeVariantArray(args); /* free the variant array */
				if (result)
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
				freeVariantArray(args); /* free the variant array */
				if (result)
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
				if (result)
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
				freeVariant(value);
				writeInt32(result);

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
				if (result){
					writeIdentifierArray(identifierTable, identifierCount);
					writeInt32(identifierCount);
					if (identifierTable) sBrowserFuncs->memfree(identifierTable);
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

				/* increase refcounter */
				if (notifyData)
					notifyData->referenceCount++;

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

				/* increase refcounter */
				if (notifyData)
					notifyData->referenceCount++;

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

		case FUNCTION_NPN_REQUEST_READ: /* UNTESTED! */
			{
				NPStream *stream 				= readHandleStream(stack);
				uint32_t rangeCount				= readInt32(stack);
				NPByteRange *byteRange 			= NULL;
				DBG_TRACE("FUNCTION_NPN_REQUEST_READ( stream=%p, rangeCount=%d, ... )", stream, rangeCount );

				for (unsigned int i = 0; i < rangeCount; i++){
					NPByteRange *newByteRange = (NPByteRange*)malloc(sizeof(NPByteRange));
					if (!newByteRange) break; /* Unable to send all requests, but shouldn't occur */

					newByteRange->offset = readInt32(stack);
					newByteRange->length = readInt32(stack);
					newByteRange->next   = byteRange;

					byteRange = newByteRange;
				}

				NPError result = sBrowserFuncs->requestread(stream, byteRange);

				/* Free the linked list */
				while (byteRange){
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
				if (result == NPERR_NO_ERROR)
					writeHandleStream(stream);
				writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_NEW_STREAM -> ( result=%d, ... )", result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_DESTROY_STREAM:
			{
				NPP instance 		= readHandleInstance(stack);
				NPStream *stream 	= readHandleStream(stack, HMGR_SHOULD_EXIST);
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

				DBG_TRACE("FUNCTION_NPN_STATUS -> void");
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
				if (str) sBrowserFuncs->memfree(str);

				DBG_TRACE("FUNCTION_NPN_UTF8_FROM_IDENTIFIER -> str='%s'", str );
				returnCommand();
			}
			break;

		case FUNCTION_NPN_INT_FROM_IDENTIFIER:
			{
				NPIdentifier identifier = readHandleIdentifier(stack);
				DBG_TRACE("FUNCTION_NPN_INT_FROM_IDENTIFIER( identifier=%p )", identifier );

				int32_t result = sBrowserFuncs->intfromidentifier(identifier);
				writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_INT_FROM_IDENTIFIER -> result=%d", result );
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

		case FUNCTION_NPN_PUSH_POPUPS_ENABLED_STATE:
			{
				NPP instance 					= readHandleInstance(stack);
				bool enabled 					= (bool)readInt32(stack);
				DBG_TRACE("FUNCTION_NPN_PUSH_POPUPS_ENABLED_STATE( instance=%p, enabled=%d )", instance, enabled );

				sBrowserFuncs->pushpopupsenabledstate(instance, enabled);

				DBG_TRACE("FUNCTION_NPN_PUSH_POPUPS_ENABLED_STATE -> void");
				returnCommand();
			}
			break;

		case FUNCTION_NPN_POP_POPUPS_ENABLED_STATE:
			{
				NPP instance 					= readHandleInstance(stack);
				DBG_TRACE("FUNCTION_NPN_POP_POPUPS_ENABLED_STATE( instance=%p )", instance );

				sBrowserFuncs->poppopupsenabledstate(instance);

				DBG_TRACE("FUNCTION_NPN_POP_POPUPS_ENABLED_STATE -> void");
				returnCommand();
			}
			break;

		default:
			DBG_ABORT("specified function %d not found!", functionid);
			break;
	}
}
