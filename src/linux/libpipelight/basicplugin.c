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
#include <grp.h>								// for initgroups()
#include <pwd.h>								// for struct passwd
#include <iostream>								// for std::ios_base
#include <string>								// for std::string
#include <errno.h>

#ifndef __APPLE__
#include <X11/Xlib.h>							// for XSendEvent, ...
#include <X11/Xmd.h>							// for CARD32
#endif

#include "basicplugin.h"

static bool checkPluginInstallation();
static bool startWineProcess();

/* BEGIN GLOBAL VARIABLES

	Note: As global variables should be initialized properly BEFORE the attach function is called.
	Otherwise there might be some uninitialized variables...

*/

char strMimeType[2048]				= {0};
char strPluginVersion[100]			= {0};
char strPluginName[256]				= {0};
char strPluginDescription[1024]		= {0};

uint32_t	eventTimerID			= 0;
NPP			eventTimerInstance		= NULL;
pthread_t	eventThread				= 0;

sem_t		eventThreadSemRequestAsyncCall;
sem_t		eventThreadSemScheduledAsyncCall;

pid_t		pidPluginloader			= -1;
bool		initOkay				= false;

// Browser functions
NPNetscapeFuncs* sBrowserFuncs		= NULL;

// Global plugin configuration
PluginConfig config INIT_EARLY;

// Attach has to be called as the last step
static void attach() CONSTRUCTOR;
static void detach() DESTRUCTOR;

/* END GLOBAL VARIABLES */

static void attach(){
	std::string result;
	Stack stack;

	std::ios_base::sync_with_stdio(false);		/* Fix for Opera: Dont sync stdio */
	setbuf(stderr, NULL);						/* Disable stderr buffering */

	DBG_INFO("attached to process.");

	/* Initialize semaphores */
	sem_init(&eventThreadSemRequestAsyncCall, 0, 0);
	sem_init(&eventThreadSemScheduledAsyncCall, 0, 0);

	initOkay = false;

	/* load config file */
	if (!loadConfig(config)){
		DBG_ERROR("unable to load configuration - aborting.");
		return;
	}

	/* check for correct installation */
	if (!checkPluginInstallation()){
		DBG_ERROR("plugin not correctly installed - aborting.");
		return;
	}

	/* start wine process */
	if (!startWineProcess()){
		DBG_ERROR("could not start wine process - aborting.");
		return;
	}

	/* we want to be sure that wine is up and running until we return! */
	if (!pluginInitOkay()){
		DBG_ERROR("error during the initialization of the wine process - aborting.");

		if (!loadPluginInformation()){
			if(config.pluginName == ""){
				pokeString(strMimeType, "application/x-pipelight-error:pipelighterror:Error during initialization");
				pokeString(strPluginName, "Pipelight Error!");
			}else{
				pokeString(strMimeType, "application/x-pipelight-error-"+config.pluginName+":pipelighterror-"+config.pluginName+":Error during initialization");
				pokeString(strPluginName, "Pipelight Error (" + config.pluginName +")!");
			}
			pokeString(strPluginDescription, "Something went wrong, check the terminal output");
			pokeString(strPluginVersion, "0.0");
		}

		return;
	}

	/* do we have to disable graphic acceleration for Silverlight? */
	if (config.silverlightGraphicDriverCheck)
	{
		ctx->callFunction( SILVERLIGHT_IS_GRAPHIC_DRIVER_SUPPORTED );
		if (!ctx->readResultInt32())
			config.overwriteArgs["enableGPUAcceleration"] = "false";
	}

	ctx->callFunction(FUNCTION_GET_PLUGIN_INFO);
	ctx->readCommands(stack);

	/* mime types */
	result = readString(stack);
	for (std::vector<MimeInfo>::iterator it = config.fakeMIMEtypes.begin(); it != config.fakeMIMEtypes.end(); it++)
		result += ";" + it->mimeType + ":" + it->extension + ":" + it->description;
	pokeString(strMimeType, result);

	/* plugin name */
	result = readString(stack);
	pokeString(strPluginName, result);

	/* plugin description */
	result = readString(stack);
	if (config.fakeVersion != "")
		result = config.fakeVersion;
	pokeString(strPluginDescription, result);

	/* plugin version */
	result = readString(stack);
	if (config.fakeVersion != "")
		result = config.fakeVersion;
	pokeString(strPluginVersion, result);

	savePluginInformation();

	/* initialisation successful */
	initOkay = true;
}

static void detach(){
	/* TODO: Deinitialize pointers etc. */
}

/* checkPermissions */
static void checkPermissions(){
	bool result = true;
	uid_t uid  = getuid();
	uid_t euid = geteuid();
	gid_t gid  = getgid();
	gid_t egid = getegid();
	passwd* user = NULL;

	if (euid == 0 || egid == 0){
		DBG_WARN("-------------------------------------------------------");
		DBG_WARN("WARNING! YOU ARE RUNNING THIS PIPELIGHT PLUGIN AS ROOT!");
		DBG_WARN("THIS IS USUALLY NOT A GOOD IDEA! YOU HAVE BEEN WARNED!");
		DBG_WARN("-------------------------------------------------------");
	}

	/* When dropping privileges from root, the initgroups() call will
	 * remove any extraneous groups and just use the groups the real
	 * user is a member of.  If we don't call this, then even though
	 * our gid or uid has dropped, we may still have group-permissions
	 * that enable us to do super-user things.  This will fail if we
	 * aren't root or could not properly acquire the user's credentials.
	 */

	if (!(user = getpwuid(uid)))
		DBG_ERROR("call to getpwuid() failed.");

	if (user && gid != egid && (euid == 0 || egid == 0))
		if (initgroups(user->pw_name, user->pw_gid))
			DBG_ERROR("failed to drop group-privileges by calling initgroups().");

	if (gid != egid)
		result = (!setgid(gid) && getegid() == gid);

	if (uid != euid)
		result = (!setuid(uid) && geteuid() == uid && result);

	if (!result){
		DBG_ERROR("failed to set permissions to uid=%d, gid=%d.", uid, gid);
		DBG_ERROR("running with uid=%d, gid=%d.", geteuid(), getegid());
	}
}

/* checkPluginInstallation */
static bool checkPluginInstallation(){
	std::string dependencyInstaller = PIPELIGHT_SHARE_PATH "/install-plugin";

	/* Output wine prefix */
	DBG_INFO("using wine prefix directory %s.", config.winePrefix.c_str());

	/* If there is no installer provided we cannot check the installation */
	if (config.dependencies.empty() || !checkIfExists(dependencyInstaller))
		return checkIfExists(config.winePrefix);

	/* Run the installer ... */
	DBG_INFO("checking plugin installation - this might take some time.");

	pid_t pidInstall = fork();
	if (pidInstall == 0){

		close(0);

		checkPermissions();

		/* Setup environment variables */
		setenv("WINEPREFIX",			config.winePrefix.c_str(),			true);
		setenv("WINE",					config.winePath.c_str(),			true);

		if (config.wineArch != "")
			setenv("WINEARCH",			config.wineArch.c_str(),			true);

		if (config.wineDLLOverrides != "")
			setenv("WINEDLLOVERRIDES",	config.wineDLLOverrides.c_str(),	true);

		if (config.quietInstallation)
			setenv("QUIETINSTALLATION",	"1",								true);

		/* Generate argv array */
		std::vector<const char*> argv;

		argv.push_back(dependencyInstaller.c_str());
		argv.push_back(config.configPath.c_str());

		for (std::vector<std::string>::iterator it = config.dependencies.begin(); it != config.dependencies.end(); it++)
			argv.push_back( it->c_str());

		argv.push_back(NULL);

		execvp(argv[0], (char**)argv.data());
		DBG_ABORT("error in execvp command - probably dependencyInstaller not found or missing execute permission.");

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

/* startWineProcess */
static bool startWineProcess(){
	int tempPipeOut[2], tempPipeIn[2];

	if (pipe(tempPipeOut) == -1 || pipe(tempPipeIn) == -1){
		DBG_ERROR("could not create pipes to communicate with the plugin.");
		return false;
	}

	pidPluginloader = fork();
	if (pidPluginloader == 0){
		/* The child process will be replaced with wine */

		close(tempPipeIn[0]);
		close(tempPipeOut[1]);

		/* Assign to stdin/stdout */
		dup2(tempPipeOut[0],  0);
		dup2(tempPipeIn[1],   1);

		checkPermissions();

		/* Setup environment variables */
		setenv("WINEPREFIX",			config.winePrefix.c_str(),			true);

		if (config.wineArch != "")
			setenv("WINEARCH",			config.wineArch.c_str(),			true);

		if (config.wineDLLOverrides != "")
			setenv("WINEDLLOVERRIDES",	config.wineDLLOverrides.c_str(),	true);

		if (config.gccRuntimeDLLs != ""){
			std::string runtime = getEnvironmentString("Path");
			runtime = config.gccRuntimeDLLs + ((runtime != "") ? (";" + runtime) : "");
			setenv("Path", runtime.c_str(), true);
		}

		/* Generate argv array */
		std::vector<const char*> argv;

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

		if (config.linuxWindowlessMode)
			argv.push_back( "--linuxWindowless" );

		if (config.embed)
			argv.push_back( "--embed" );

		if (config.experimental_forceSetWindow)
			argv.push_back( "--forceSetWindow" );

		if (config.experimental_windowClassHook)
			argv.push_back( "--windowClassHook" );

		if (config.experimental_strictDrawOrdering)
			argv.push_back( "--strictDrawOrdering" );

		argv.push_back(NULL);

		/* Execute wine */
		execvp(argv[0], (char**)argv.data());
		DBG_ABORT("error in execvp command - probably wine not found or missing execute permission.");

	}else if (pidPluginloader != -1){
		/* The parent process will return normally and use the pipes to communicate with the child process */

		close(tempPipeOut[0]);
		close(tempPipeIn[1]);

		if (!ctx->initCommPipes(tempPipeOut[1], tempPipeIn[0]))
			return false;

	}else{
		DBG_ERROR("unable to fork() - probably out of memory?");
		return false;
	}

	return true;
}

#ifndef __APPLE__
/* sendXembedMessage */
static void sendXembedMessage(Display* display, Window win, long message, long detail, long data1, long data2){
	XEvent ev;
	memset(&ev, 0, sizeof(ev));

	ev.xclient.type			= ClientMessage;
	ev.xclient.window		= win;
	ev.xclient.message_type = XInternAtom(display, "_XEMBED", False);
	ev.xclient.format		= 32;

	ev.xclient.data.l[0]	= CurrentTime;
	ev.xclient.data.l[1]	= message;
	ev.xclient.data.l[2]	= detail;
	ev.xclient.data.l[3]	= data1;
	ev.xclient.data.l[4]	= data2;

	XSendEvent(display, win, False, NoEventMask, &ev);
	XSync(display, False);
}
#endif

/* dispatcher */
void dispatcher(int functionid, Stack &stack){
	DBG_ASSERT(sBrowserFuncs, "browser didn't correctly initialize the plugin!");

	switch (functionid){

		case LIN_HANDLE_MANAGER_REQUEST_STREAM_INFO:
			{
				NPStream* stream = readHandleStream(stack); /* shouldExist not necessary, Linux checks always */
				DBG_TRACE("LIN_HANDLE_MANAGER_REQUEST_STREAM_INFO( stream=%p )", stream);

				ctx->writeString(stream->headers);
				ctx->writeHandleNotify(stream->notifyData, HMGR_SHOULD_EXIST);
				ctx->writeInt32(stream->lastmodified);
				ctx->writeInt32(stream->end);
				ctx->writeString(stream->url);

				DBG_TRACE("LIN_HANDLE_MANAGER_REQUEST_STREAM_INFO -> ( headers='%s', notifyData=%p, lastmodified=%d, end=%d, url='%s' )", \
						stream->headers, stream->notifyData, stream->lastmodified, stream->end, stream->url);
				ctx->returnCommand();
			}
			break;

		case LIN_HANDLE_MANAGER_FREE_OBJECT:
			{
				NPObject* obj		= readHandleObj(stack);
				DBG_TRACE("LIN_HANDLE_MANAGER_FREE_OBJECT( obj=%p )", obj);
				DBG_TRACE("LIN_HANDLE_MANAGER_FREE_OBJECT -> void");
				ctx->returnCommand();

				/* ASYNC */
				handleManager_removeByPtr(HMGR_TYPE_NPObject, obj);
			}
			break;

		case LIN_HANDLE_MANAGER_FREE_OBJECT_ASYNC:
			{
				NPObject* obj		= readHandleObj(stack);
				DBG_TRACE("LIN_HANDLE_MANAGER_FREE_OBJECT_ASYNC( obj=%p ) -> void", obj);
				handleManager_removeByPtr(HMGR_TYPE_NPObject, obj);
			}
			break;

	#ifndef __APPLE__

		case CHANGE_EMBEDDED_MODE:
			{
				NPP instance			= readHandleInstance(stack);
				Window win				= (Window)readInt32(stack);
				bool embed				= (bool)readInt32(stack);
				DBG_TRACE("CHANGE_EMBEDDED_MODE( instance=%p, win=%lu, embed=%d )", instance, win, embed);

				PluginData *pdata = (PluginData*)instance->pdata;
				if (pdata){
					pdata->plugin = win;

					if (pdata->containerType == NPWindowTypeWindow && pdata->container){
						Display *display = XOpenDisplay(NULL);

						if (display){
							Window parentWindow;

							/* embed into child window */
							if (embed){
								parentWindow = config.x11WindowID;
								if (!parentWindow)
									parentWindow = (Window)pdata->container;

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
				ctx->returnCommand();
			}
			break;

	#endif

		case FUNCTION_NPN_CREATE_OBJECT:
			{
				NPP instance			= readHandleInstance(stack);
				DBG_TRACE("FUNCTION_NPN_CREATE_OBJECT( instance=%p )", instance);

				NPObject* obj = sBrowserFuncs->createobject(instance, &myClass);
				ctx->writeHandleObj(obj); /* refcounter is hopefully 1 */

				DBG_TRACE("FUNCTION_NPN_CREATE_OBJECT -> obj=%p", obj);
				ctx->returnCommand();
			}
			break;


		case FUNCTION_NPN_GETVALUE_BOOL:
			{
				NPP instance			= readHandleInstance(stack);
				NPNVariable variable	= (NPNVariable)readInt32(stack);
				DBG_TRACE("FUNCTION_NPN_GETVALUE_BOOL( instance=%p, variable=%d )", instance, variable);

				NPBool resultBool = 0;
				NPError result;

				if (variable == NPNVprivateModeBool)
					result = sBrowserFuncs->getvalue(instance, variable, &resultBool);
				else{
					DBG_WARN("FUNCTION_NPN_GETVALUE_BOOL - variable %d not allowed", variable);
					result = NPERR_GENERIC_ERROR;
				}
				if (result == NPERR_NO_ERROR)
					ctx->writeInt32(resultBool);
				ctx->writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_GETVALUE_BOOL -> ( result=%d, ... )", result);
				ctx->returnCommand();
			}
			break;


		case FUNCTION_NPN_GETVALUE_OBJECT:
			{
				NPP instance			= readHandleInstance(stack);
				NPNVariable variable	= (NPNVariable)readInt32(stack);
				DBG_TRACE("FUNCTION_NPN_GETVALUE_OBJECT( instance=%p, variable=%d )", instance, variable);

				NPObject* obj = NULL;
				NPError result;

				if (variable == NPNVPluginElementNPObject || variable == NPNVWindowNPObject)
					result = sBrowserFuncs->getvalue(instance, variable, &obj);
				else{
					DBG_WARN("FUNCTION_NPN_GETVALUE_OBJECT - variable %d not allowed", variable);
					result = NPERR_GENERIC_ERROR;
				}
				if (result == NPERR_NO_ERROR)
					ctx->writeHandleObj(obj);
				ctx->writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_GETVALUE_OBJECT -> ( result=%d, ... )", result);
				ctx->returnCommand();
			}
			break;

		case FUNCTION_NPN_GETVALUE_STRING:
			{
				NPP instance			= readHandleInstance(stack);
				NPNVariable variable	= (NPNVariable)readInt32(stack);
				DBG_TRACE("FUNCTION_NPN_GETVALUE_STRING( instance=%p, variable=%d )", instance, variable);

				char* str = NULL;
				NPError result;

				if (variable == NPNVdocumentOrigin)
					result = sBrowserFuncs->getvalue(instance, variable, &str);
				else{
					DBG_WARN("FUNCTION_NPN_GETVALUE_STRING - variable %d not allowed", variable);
					result = NPERR_GENERIC_ERROR;
				}
				if (result == NPERR_NO_ERROR)
					ctx->writeString(str);
				ctx->writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_GETVALUE_STRING -> ( result=%d, ... )", result);
				ctx->returnCommand();

				/* ASYNC */
				if (str) sBrowserFuncs->memfree(str);
			}
			break;

		case FUNCTION_NPN_RELEASEOBJECT:
			{
				NPObject* obj				= readHandleObj(stack);
				uint32_t minReferenceCount	= readInt32(stack);
				DBG_TRACE("FUNCTION_NPN_RELEASEOBJECT( obj=%p, minReferenceCount=%d )", obj, minReferenceCount);

				DBG_ASSERT( (minReferenceCount & REFCOUNT_MASK) <= obj->referenceCount, "object referenceCount smaller than expected?");
				if (obj->referenceCount == 1 && handleManager_existsByPtr(HMGR_TYPE_NPObject, obj))
					DBG_ASSERT(minReferenceCount & REFCOUNT_CUSTOM, "forgot to set killObject?");

				sBrowserFuncs->releaseobject(obj);

				DBG_TRACE("FUNCTION_NPN_RELEASEOBJECT -> void");
				ctx->returnCommand();
			}
			break;

		case FUNCTION_NPN_RELEASEOBJECT_ASYNC:
			{
				NPObject* obj				= readHandleObj(stack);
				uint32_t minReferenceCount	= readInt32(stack);
				DBG_TRACE("FUNCTION_NPN_RELEASEOBJECT_ASYNC( obj=%p, minReferenceCount=%d ) -> void", obj, minReferenceCount);

				DBG_ASSERT( (minReferenceCount & REFCOUNT_MASK) <= obj->referenceCount, "object referenceCount smaller than expected?");
				if (obj->referenceCount == 1 && handleManager_existsByPtr(HMGR_TYPE_NPObject, obj))
					DBG_ASSERT(minReferenceCount & REFCOUNT_CUSTOM, "forgot to set killObject?");

				sBrowserFuncs->releaseobject(obj);
			}
			break;

		case FUNCTION_NPN_RETAINOBJECT:
			{
				NPObject* obj				= readHandleObj(stack);
				uint32_t minReferenceCount	= readInt32(stack);
				DBG_TRACE("FUNCTION_NPN_RETAINOBJECT( obj=%p, minReferenceCount=%d )", obj, minReferenceCount);

				sBrowserFuncs->retainobject(obj);

				DBG_ASSERT( (minReferenceCount & REFCOUNT_MASK) <= obj->referenceCount, "object referenceCount smaller than expected?");

				DBG_TRACE("FUNCTION_NPN_RETAINOBJECT -> void");
				ctx->returnCommand();
			}
			break;

		case FUNCTION_NPN_RETAINOBJECT_ASYNC:
			{
				NPObject* obj				= readHandleObj(stack);
				uint32_t minReferenceCount	= readInt32(stack);
				DBG_TRACE("FUNCTION_NPN_RETAINOBJECT_ASYNC( obj=%p, minReferenceCount=%d ) -> void", obj, minReferenceCount);

				sBrowserFuncs->retainobject(obj);

				DBG_ASSERT( (minReferenceCount & REFCOUNT_MASK) <= obj->referenceCount, "object referenceCount smaller than expected?");
			}
			break;

		case FUNCTION_NPN_EVALUATE:
			{
				NPP instance					= readHandleInstance(stack);
				NPObject* obj					= readHandleObj(stack);
				size_t len;
				std::shared_ptr<char> buffer	= readStringAsMemory(stack, len);
				NPVariant resultVariant;
				resultVariant.type				= NPVariantType_Void;
				resultVariant.value.objectValue = NULL;
				DBG_TRACE("FUNCTION_NPN_EVALUATE( instance=%p, obj=%p, buffer='%s' )", instance, obj, buffer.get());

				bool result;

				if (config.replaceJavascript.empty()){
					NPString script;
					script.UTF8Characters = buffer.get();
					script.UTF8Length     = len;
					result = sBrowserFuncs->evaluate(instance, obj, &script, &resultVariant);

				}else{
					std::string scriptStr = std::string(buffer.get(), len);
					size_t startPos = 0;

					for(;;){
						std::map<std::string, std::string>::iterator firstMatch;
						size_t firstPos = std::string::npos;

						for (std::map<std::string, std::string>::iterator it = config.replaceJavascript.begin(); it != config.replaceJavascript.end(); it++){
							size_t pos = scriptStr.find(it->first, startPos);
							if (pos != std::string::npos && (firstPos == std::string::npos || pos < firstPos)){
								firstMatch = it;
								firstPos = pos;
							}
						}

						if (firstPos == std::string::npos)
							break;

						scriptStr.replace(firstPos, firstMatch->first.size(), firstMatch->second);
						startPos = firstPos + firstMatch->second.size();
					}

					DBG_TRACE("replaced script='%s'", scriptStr.c_str());

					NPString script;
					script.UTF8Characters = scriptStr.c_str();
					script.UTF8Length     = scriptStr.size();
					result = sBrowserFuncs->evaluate(instance, obj, &script, &resultVariant);
				}

				if (result)
					ctx->writeVariantRelease(resultVariant);
				ctx->writeInt32( result );

				DBG_TRACE("FUNCTION_NPN_EVALUATE -> ( result=%d, ... )", result);
				ctx->returnCommand();
			}
			break;

		case FUNCTION_NPN_INVOKE:
			{
				NPP instance					= readHandleInstance(stack);
				NPObject* obj					= readHandleObj(stack);
				NPIdentifier identifier			= readHandleIdentifier(stack);
				int32_t argCount				= readInt32(stack);
				std::vector<NPVariant> args		= readVariantArray(stack, argCount);
				NPVariant resultVariant;
				resultVariant.type				= NPVariantType_Void;
				resultVariant.value.objectValue = NULL;
				DBG_TRACE("FUNCTION_NPN_INVOKE( instance=%p, obj=%p, identifier=%p, argCount=%d, ... )", instance, obj, identifier, argCount);

				bool result = sBrowserFuncs->invoke(instance, obj, identifier, args.data(), argCount, &resultVariant);
				freeVariantArray(args);
				if (result)
					ctx->writeVariantRelease(resultVariant);
				ctx->writeInt32( result );

				DBG_TRACE("FUNCTION_NPN_INVOKE -> ( result=%d, ... )", result);
				ctx->returnCommand();
			}
			break;

		case FUNCTION_NPN_INVOKE_DEFAULT:
			{
				NPP instance					= readHandleInstance(stack);
				NPObject* obj					= readHandleObj(stack);
				int32_t argCount				= readInt32(stack);
				std::vector<NPVariant> args		= readVariantArray(stack, argCount);
				NPVariant resultVariant;
				resultVariant.type				= NPVariantType_Void;
				resultVariant.value.objectValue = NULL;
				DBG_TRACE("FUNCTION_NPN_INVOKE_DEFAULT( instance=%p, obj=%p, argCount=%d, ... )", instance, obj, argCount);

				bool result = sBrowserFuncs->invokeDefault(instance, obj, args.data(), argCount, &resultVariant);
				freeVariantArray(args); /* free the variant array */
				if (result)
					ctx->writeVariantRelease(resultVariant);
				ctx->writeInt32( result );

				DBG_TRACE("FUNCTION_NPN_INVOKE_DEFAULT -> ( result=%d, ... )", result);
				ctx->returnCommand();
			}
			break;

		case FUNCTION_NPN_HAS_PROPERTY:
			{
				NPP instance				= readHandleInstance(stack);
				NPObject* obj				= readHandleObj(stack);
				NPIdentifier identifier		= readHandleIdentifier(stack);
				DBG_TRACE("FUNCTION_NPN_HAS_PROPERTY( instance=%p, obj=%p, identifier=%p )", instance, obj, identifier);

				bool result = sBrowserFuncs->hasproperty(instance, obj, identifier);
				ctx->writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_HAS_PROPERTY -> result=%d", result);
				ctx->returnCommand();
			}
			break;

		case FUNCTION_NPN_HAS_METHOD:
			{
				NPP instance				= readHandleInstance(stack);
				NPObject* obj				= readHandleObj(stack);
				NPIdentifier identifier		= readHandleIdentifier(stack);
				DBG_TRACE("FUNCTION_NPN_HAS_METHOD( instance=%p, obj=%p, identifier=%p )", instance, obj, identifier);

				bool result = sBrowserFuncs->hasmethod(instance, obj, identifier);
				ctx->writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_HAS_METHOD -> result=%d", result);
				ctx->returnCommand();
			}
			break;

		case FUNCTION_NPN_GET_PROPERTY:
			{
				NPP instance				= readHandleInstance(stack);
				NPObject*  obj				= readHandleObj(stack);
				NPIdentifier propertyName	= readHandleIdentifier(stack);
				NPVariant resultVariant;
				resultVariant.type					= NPVariantType_Void;
				resultVariant.value.objectValue		= NULL;
				DBG_TRACE("FUNCTION_NPN_GET_PROPERTY( instance=%p, obj=%p, propertyName=%p )", instance, obj, propertyName);

				bool result = sBrowserFuncs->getproperty(instance, obj, propertyName, &resultVariant);
				if (result)
					ctx->writeVariantRelease(resultVariant);
				ctx->writeInt32( result );

				DBG_TRACE("FUNCTION_NPN_GET_PROPERTY -> ( result=%d, ... )", result);
				ctx->returnCommand();
			}
			break;

		case FUNCTION_NPN_SET_PROPERTY:
			{
				NPP instance					= readHandleInstance(stack);
				NPObject* obj					= readHandleObj(stack);
				NPIdentifier propertyName		= readHandleIdentifier(stack);
				NPVariant value;
				readVariant(stack, value);
				DBG_TRACE("FUNCTION_NPN_SET_PROPERTY( instance=%p, obj=%p, propertyName=%p, value=%p )", instance, obj, propertyName, &value);

				bool result = sBrowserFuncs->setproperty(instance, obj, propertyName, &value);
				freeVariant(value);
				ctx->writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_SET_PROPERTY -> result=%d", result);
				ctx->returnCommand();
			}
			break;

		case FUNCTION_NPN_REMOVE_PROPERTY:
			{
				NPP instance					= readHandleInstance(stack);
				NPObject* obj					= readHandleObj(stack);
				NPIdentifier propertyName		= readHandleIdentifier(stack);
				DBG_TRACE("FUNCTION_NPN_REMOVE_PROPERTY( instance=%p, obj=%p, propertyName=%p )", instance, obj, propertyName);

				bool result = sBrowserFuncs->removeproperty(instance, obj, propertyName);
				ctx->writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_REMOVE_PROPERTY -> result=%d", result);
				ctx->returnCommand();
			}
			break;

		case FUNCTION_NPN_ENUMERATE:
			{
				NPP instance					= readHandleInstance(stack);
				NPObject *obj					= readHandleObj(stack);
				NPIdentifier*   identifierTable = NULL;
				uint32_t		identifierCount = 0;
				DBG_TRACE("FUNCTION_NPN_ENUMERATE( instance=%p, obj=%p )", instance, obj);

				bool result = sBrowserFuncs->enumerate(instance, obj, &identifierTable, &identifierCount);
				if (result){
					ctx->writeIdentifierArray(identifierTable, identifierCount);
					ctx->writeInt32(identifierCount);
				}
				ctx->writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_ENUMERATE -> ( result=%d, ... )", result);
				ctx->returnCommand();

				/* ASYNC */
				if (identifierTable) sBrowserFuncs->memfree(identifierTable);
			}
			break;

		case FUNCTION_NPN_SET_EXCEPTION:
			{
				NPObject* obj					= readHandleObj(stack);
				std::shared_ptr<char> message	= readStringAsMemory(stack);
				DBG_TRACE("FUNCTION_NPN_SET_EXCEPTION( obj=%p, message='%s' )", obj, message.get());

				sBrowserFuncs->setexception(obj, message.get());

				DBG_TRACE("FUNCTION_NPN_SET_EXCEPTION -> void");
				ctx->returnCommand();
			}
			break;

		case FUNCTION_NPN_SET_EXCEPTION_ASYNC:
			{
				NPObject* obj					= readHandleObj(stack);
				std::shared_ptr<char> message	= readStringAsMemory(stack);
				DBG_TRACE("FUNCTION_NPN_SET_EXCEPTION_ASYNC( obj=%p, message='%s' ) -> void", obj, message.get());
				sBrowserFuncs->setexception(obj, message.get());
			}
			break;

		case FUNCTION_NPN_GET_URL_NOTIFY:
			{
				NPP instance					= readHandleInstance(stack);
				std::shared_ptr<char> url		= readStringAsMemory(stack);
				std::shared_ptr<char> target	= readStringAsMemory(stack);
				NotifyDataRefCount* notifyData	= (NotifyDataRefCount*)readHandleNotify(stack);
				DBG_TRACE("FUNCTION_NPN_GET_URL_NOTIFY( instance=%p, url='%s', target='%s', notifyData=%p )", instance, url.get(), target.get(), notifyData);

				/* increase refcounter */
				if (notifyData)
					notifyData->referenceCount++;

				NPError result = sBrowserFuncs->geturlnotify(instance, url.get(), target.get(), notifyData);
				ctx->writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_GET_URL_NOTIFY -> result=%d", result);
				ctx->returnCommand();
			}
			break;

		case FUNCTION_NPN_POST_URL_NOTIFY:
			{
				NPP instance					= readHandleInstance(stack);
				std::shared_ptr<char> url		= readStringAsMemory(stack);
				std::shared_ptr<char> target	= readStringAsMemory(stack);
				size_t len;
				std::shared_ptr<char> buffer	= readMemory(stack, len);
				bool file						= (bool)readInt32(stack);
				NotifyDataRefCount* notifyData	= (NotifyDataRefCount*)readHandleNotify(stack);
				DBG_TRACE("FUNCTION_NPN_POST_URL_NOTIFY( instance=%p, url='%s', target='%s', buffer=%p, len=%zd, file=%d, notifyData=%p )", instance, url.get(), target.get(), buffer.get(), len, file, notifyData);

				/* increase refcounter */
				if (notifyData)
					notifyData->referenceCount++;

				NPError result = sBrowserFuncs->posturlnotify(instance, url.get(), target.get(), len, buffer.get(), file, notifyData);
				ctx->writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_POST_URL_NOTIFY -> result=%d", result);
				ctx->returnCommand();
			}
			break;

		case FUNCTION_NPN_GET_URL:
			{
				NPP instance					= readHandleInstance(stack);
				std::shared_ptr<char> url		= readStringAsMemory(stack);
				std::shared_ptr<char> target	= readStringAsMemory(stack);
				DBG_TRACE("FUNCTION_NPN_GET_URL( instance=%p, url='%s', target='%s' )", instance, url.get(), target.get());

				NPError result = sBrowserFuncs->geturl(instance, url.get(), target.get());
				ctx->writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_GET_URL -> result=%d", result);
				ctx->returnCommand();
			}
			break;

		case FUNCTION_NPN_POST_URL:
			{
				NPP instance					= readHandleInstance(stack);
				std::shared_ptr<char> url		= readStringAsMemory(stack);
				std::shared_ptr<char> target	= readStringAsMemory(stack);
				size_t len;
				std::shared_ptr<char> buffer	= readMemory(stack, len);
				bool file						= (bool)readInt32(stack);
				DBG_TRACE("FUNCTION_NPN_POST_URL( instance=%p, url='%s', target='%s', buffer=%p, len=%zd, file=%d )", instance, url.get(), target.get(), buffer.get(), len, file );

				NPError result = sBrowserFuncs->posturl(instance, url.get(), target.get(), len, buffer.get(), file);
				ctx->writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_POST_URL -> result=%d", result);
				ctx->returnCommand();
			}
			break;

		case FUNCTION_NPN_REQUEST_READ:
			{
				NPStream *stream				= readHandleStream(stack);
				uint32_t rangeCount				= readInt32(stack);
				NPByteRange *byteRange			= NULL;
				DBG_TRACE("FUNCTION_NPN_REQUEST_READ( stream=%p, rangeCount=%d, ... )", stream, rangeCount );

				if (rangeCount){
					byteRange = (NPByteRange *)malloc(sizeof(NPByteRange) * rangeCount);
					DBG_ASSERT(byteRange != NULL, "failed to allocate memory.");

					NPByteRange *range = byteRange + (rangeCount - 1);
					for (unsigned int i = 0; i < rangeCount; i++, range--){
						range->offset = readInt32(stack);
						range->length = readInt32(stack);
						range->next   = i ? (range + 1) : NULL;
					}
				}

				NPError result = sBrowserFuncs->requestread(stream, byteRange);
				ctx->writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_REQUEST_READ -> result=%d", result);
				ctx->returnCommand();

				/* ASYNC */
				if (byteRange) free(byteRange);
			}
			break;

		case FUNCTION_NPN_WRITE:
			{
				size_t len;
				NPP instance					= readHandleInstance(stack);
				NPStream *stream				= readHandleStream(stack);
				std::shared_ptr<char> buffer	= readMemory(stack, len);
				DBG_TRACE("FUNCTION_NPN_WRITE( instance=%p, stream=%p, buffer=%p, len=%zd )", instance, stream, buffer.get(), len );

				int32_t result = sBrowserFuncs->write(instance, stream, len, buffer.get());
				ctx->writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_WRITE -> result=%d", result);
				ctx->returnCommand();
			}
			break;

		case FUNCTION_NPN_NEW_STREAM:
			{
				NPP instance					= readHandleInstance(stack);
				std::shared_ptr<char> type		= readStringAsMemory(stack);
				std::shared_ptr<char> target	= readStringAsMemory(stack);
				DBG_TRACE("FUNCTION_NPN_NEW_STREAM( instance=%p, type='%s', target='%s' )", instance, type.get(), target.get() );

				NPStream* stream = NULL;
				NPError result = sBrowserFuncs->newstream(instance, type.get(), target.get(), &stream);
				if (result == NPERR_NO_ERROR)
					ctx->writeHandleStream(stream);
				ctx->writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_NEW_STREAM -> ( result=%d, ... )", result);
				ctx->returnCommand();
			}
			break;

		case FUNCTION_NPN_DESTROY_STREAM:
			{
				NPP instance		= readHandleInstance(stack);
				NPStream *stream	= readHandleStream(stack, HMGR_SHOULD_EXIST);
				NPReason reason		= (NPReason) readInt32(stack);
				DBG_TRACE("FUNCTION_NPN_DESTROY_STREAM( instance=%p, stream=%p, reason=%d )", instance, stream, reason );

				NPError result = sBrowserFuncs->destroystream(instance, stream, reason);
				ctx->writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_DESTROY_STREAM -> result=%d", result);
				ctx->returnCommand();
			}
			break;

		case FUNCTION_NPN_STATUS:
			{
				NPP instance					= readHandleInstance(stack);
				std::shared_ptr<char> message	= readStringAsMemory(stack);
				DBG_TRACE("FUNCTION_NPN_STATUS( instance=%p, message='%s' )", instance, message.get() );

				sBrowserFuncs->status(instance, message.get());

				DBG_TRACE("FUNCTION_NPN_STATUS -> void");
				ctx->returnCommand();
			}
			break;

		case FUNCTION_NPN_STATUS_ASYNC:
			{
				NPP instance					= readHandleInstance(stack);
				std::shared_ptr<char> message	= readStringAsMemory(stack);
				DBG_TRACE("FUNCTION_NPN_STATUS_ASYNC( instance=%p, message='%s' ) -> void", instance, message.get() );
				sBrowserFuncs->status(instance, message.get());
			}
			break;

		case FUNCTION_NPN_USERAGENT:
			{
				NPP instance					= readHandleInstance(stack);
				DBG_TRACE("FUNCTION_NPN_USERAGENT( instance=%p )", instance );

				const char *uagent = sBrowserFuncs->uagent(instance);
				ctx->writeString(uagent);

				DBG_TRACE("FUNCTION_NPN_USERAGENT -> uagent='%s'", uagent);
				ctx->returnCommand();
			}
			break;

	#ifdef PIPELIGHT_NOCACHE

		case FUNCTION_NPN_IDENTIFIER_IS_STRING:
			{
				NPIdentifier identifier			= readHandleIdentifier(stack);
				DBG_TRACE("FUNCTION_NPN_IDENTIFIER_IS_STRING( identifier=%p )", identifier );

				bool result = sBrowserFuncs->identifierisstring(identifier);
				ctx->writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_IDENTIFIER_IS_STRING -> result=%d", result );
				ctx->returnCommand();
			}
			break;

		case FUNCTION_NPN_UTF8_FROM_IDENTIFIER:
			{
				NPIdentifier identifier			= readHandleIdentifier(stack);
				DBG_TRACE("FUNCTION_NPN_UTF8_FROM_IDENTIFIER( identifier=%p )", identifier );

				NPUTF8 *str = sBrowserFuncs->utf8fromidentifier(identifier);
				ctx->writeString((char *)str);

				DBG_TRACE("FUNCTION_NPN_UTF8_FROM_IDENTIFIER -> str='%s'", str );
				ctx->returnCommand();

				/* ASYNC */
				if (str) sBrowserFuncs->memfree(str);
			}
			break;

		case FUNCTION_NPN_INT_FROM_IDENTIFIER:
			{
				NPIdentifier identifier			= readHandleIdentifier(stack);
				DBG_TRACE("FUNCTION_NPN_INT_FROM_IDENTIFIER( identifier=%p )", identifier );

				int32_t result = sBrowserFuncs->intfromidentifier(identifier);
				ctx->writeInt32(result);

				DBG_TRACE("FUNCTION_NPN_INT_FROM_IDENTIFIER -> result=%d", result );
				ctx->returnCommand();
			}
			break;

		case FUNCTION_NPN_GET_STRINGIDENTIFIER:
			{
				std::shared_ptr<char> utf8name	= readStringAsMemory(stack);
				DBG_TRACE("FUNCTION_NPN_GET_STRINGIDENTIFIER( utf8name='%s' )", utf8name.get() );

				NPIdentifier identifier = sBrowserFuncs->getstringidentifier((NPUTF8 *)utf8name.get());
				ctx->writeHandleIdentifier(identifier);

				DBG_TRACE("FUNCTION_NPN_GET_STRINGIDENTIFIER -> identifier=%p", identifier );
				ctx->returnCommand();
			}
			break;

		case FUNCTION_NPN_GET_INTIDENTIFIER:
			{
				int32_t intid					= readInt32(stack);
				DBG_TRACE("FUNCTION_NPN_GET_INTIDENTIFIER( intid='%d' )", intid );

				NPIdentifier identifier = sBrowserFuncs->getintidentifier(intid);
				ctx->writeHandleIdentifier(identifier);

				DBG_TRACE("FUNCTION_NPN_GET_INTIDENTIFIER -> identifier=%p", identifier );
				ctx->returnCommand();
			}
			break;

	#endif

		case FUNCTION_NPN_PUSH_POPUPS_ENABLED_STATE:
			{
				NPP instance					= readHandleInstance(stack);
				bool enabled					= (bool)readInt32(stack);
				DBG_TRACE("FUNCTION_NPN_PUSH_POPUPS_ENABLED_STATE( instance=%p, enabled=%d )", instance, enabled );
				DBG_TRACE("FUNCTION_NPN_PUSH_POPUPS_ENABLED_STATE -> void");
				ctx->returnCommand();

				/* ASYNC */
				sBrowserFuncs->pushpopupsenabledstate(instance, enabled);
			}
			break;

		case FUNCTION_NPN_PUSH_POPUPS_ENABLED_STATE_ASYNC:
			{
				NPP instance					= readHandleInstance(stack);
				bool enabled					= (bool)readInt32(stack);
				DBG_TRACE("FUNCTION_NPN_PUSH_POPUPS_ENABLED_STATE_ASYNC( instance=%p, enabled=%d ) -> void", instance, enabled );
				sBrowserFuncs->pushpopupsenabledstate(instance, enabled);
			}
			break;

		case FUNCTION_NPN_POP_POPUPS_ENABLED_STATE:
			{
				NPP instance					= readHandleInstance(stack);
				DBG_TRACE("FUNCTION_NPN_POP_POPUPS_ENABLED_STATE( instance=%p )", instance );
				DBG_TRACE("FUNCTION_NPN_POP_POPUPS_ENABLED_STATE -> void");
				ctx->returnCommand();

				/* ASYNC */
				sBrowserFuncs->poppopupsenabledstate(instance);
			}
			break;

		case FUNCTION_NPN_POP_POPUPS_ENABLED_STATE_ASYNC:
			{
				NPP instance					= readHandleInstance(stack);
				DBG_TRACE("FUNCTION_NPN_POP_POPUPS_ENABLED_STATE_ASYNC( instance=%p ) -> void", instance );
				sBrowserFuncs->poppopupsenabledstate(instance);
			}
			break;

		default:
			DBG_ABORT("specified function %d not found!", functionid);
			break;
	}
}
