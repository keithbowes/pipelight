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
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>								// waitpid, kill, ...
#include <string.h>								// for memcpy, ...

#include "common/common.h"
#include "basicplugin.h"

static std::string pipelightErrorJS =
	"if (!window.__pipelight_error){\n"
	"	if (confirm(\""
			"Pipelight\\n\\n"
			"There was an error during the plugin initialization!\\n\\n"
			"Press OK to open a page with additional information."
		"\")){\n"
	"		window.open(\"http://pipelight.net/cms/faqs/faq-pipelight-error-in-aboutplugins.html\",'_blank');\n"
	"	}\n"
	"	window.__pipelight_error = true;\n"
	"}";

/* NP_Initialize */
NP_EXPORT(NPError) NP_Initialize(NPNetscapeFuncs *bFuncs, NPPluginFuncs* pFuncs){
	DBG_TRACE("( bFuncs=%p, pFuncs=%p )", bFuncs, pFuncs);

	if (bFuncs == NULL || pFuncs == NULL){
		DBG_TRACE(" -> result=NPERR_INVALID_PARAM");
		return NPERR_INVALID_PARAM;
	}

	if ((bFuncs->version >> 8) > NP_VERSION_MAJOR){
		DBG_ERROR("incompatible browser version!");
		DBG_TRACE(" -> result=%d", NPERR_INCOMPATIBLE_VERSION_ERROR);
		return NPERR_INCOMPATIBLE_VERSION_ERROR;
	}

	/* copy browser functions instead of saving the pointer */
	if (!sBrowserFuncs)
		sBrowserFuncs = (NPNetscapeFuncs*)malloc( sizeof(NPNetscapeFuncs) );

	if (!sBrowserFuncs){
		DBG_TRACE(" -> result=%d", NPERR_OUT_OF_MEMORY_ERROR);
		return NPERR_OUT_OF_MEMORY_ERROR;
	}

	memset(sBrowserFuncs, 0, sizeof(NPNetscapeFuncs));
	memcpy(sBrowserFuncs, bFuncs, ((bFuncs->size < sizeof(NPNetscapeFuncs)) ? bFuncs->size : sizeof(NPNetscapeFuncs)) );

	/* Check if all required browser functions are available */
	if (	!sBrowserFuncs->createobject ||
			!sBrowserFuncs->destroystream ||
			!sBrowserFuncs->enumerate ||
			!sBrowserFuncs->evaluate ||
			!sBrowserFuncs->getintidentifier ||
			!sBrowserFuncs->getproperty ||
			!sBrowserFuncs->getstringidentifier ||
			!sBrowserFuncs->geturl ||
			!sBrowserFuncs->geturlnotify ||
			!sBrowserFuncs->getvalue ||
			!sBrowserFuncs->hasmethod ||
			!sBrowserFuncs->hasproperty ||
			!sBrowserFuncs->identifierisstring ||
			!sBrowserFuncs->intfromidentifier ||
			!sBrowserFuncs->invalidaterect ||
			!sBrowserFuncs->invoke ||
			!sBrowserFuncs->invokeDefault ||
			!sBrowserFuncs->memalloc ||
			!sBrowserFuncs->memfree ||
			!sBrowserFuncs->newstream ||
			/* !sBrowserFuncs->pluginthreadasynccall || */
			!sBrowserFuncs->poppopupsenabledstate ||
			!sBrowserFuncs->posturl ||
			!sBrowserFuncs->posturlnotify ||
			!sBrowserFuncs->pushpopupsenabledstate ||
			!sBrowserFuncs->releaseobject ||
			!sBrowserFuncs->releasevariantvalue ||
			!sBrowserFuncs->removeproperty ||
			!sBrowserFuncs->requestread ||
			!sBrowserFuncs->retainobject ||
			/* !sBrowserFuncs->scheduletimer || */
			!sBrowserFuncs->setexception ||
			!sBrowserFuncs->setproperty ||
			!sBrowserFuncs->setvalue ||
			!sBrowserFuncs->status ||
			!sBrowserFuncs->uagent ||
			/* !sBrowserFuncs->unscheduletimer || */
			!sBrowserFuncs->utf8fromidentifier ||
			!sBrowserFuncs->write ){
		DBG_ERROR("your browser doesn't support all required functions!");
		DBG_TRACE(" -> result=%d", NPERR_INCOMPATIBLE_VERSION_ERROR);
		return NPERR_INCOMPATIBLE_VERSION_ERROR;
	}

	if (pFuncs->size < (offsetof(NPPluginFuncs, setvalue) + sizeof(void*))){
		DBG_TRACE(" -> result=%d", NPERR_INVALID_FUNCTABLE_ERROR);
		return NPERR_INVALID_FUNCTABLE_ERROR;
	}

	/* select which event handling method should be used */
	if (!config.eventAsyncCall && sBrowserFuncs->scheduletimer && sBrowserFuncs->unscheduletimer){
		DBG_INFO("using timer based event handling.");

	}else if (sBrowserFuncs->pluginthreadasynccall){
		DBG_INFO("using thread asynccall event handling.");
		config.eventAsyncCall = true;

	}else{
		DBG_ERROR("no eventhandling compatible with your browser available.");
		DBG_TRACE(" -> result=%d", NPERR_INCOMPATIBLE_VERSION_ERROR);
		return NPERR_INCOMPATIBLE_VERSION_ERROR;
	}

	/* clear the structure before writing the values */
	memset(&pFuncs->newp, 0, pFuncs->size - offsetof(NPPluginFuncs, newp));

	/* return the plugin function table */
	pFuncs->version			= (NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR;
	pFuncs->newp			= NPP_New;
	pFuncs->destroy			= NPP_Destroy;
	pFuncs->setwindow		= NPP_SetWindow;
	pFuncs->newstream		= NPP_NewStream;
	pFuncs->destroystream	= NPP_DestroyStream;
	pFuncs->asfile			= NPP_StreamAsFile;
	pFuncs->writeready		= NPP_WriteReady;
	pFuncs->write			= NPP_Write;
	pFuncs->print			= NPP_Print;
	pFuncs->event			= NPP_HandleEvent;
	pFuncs->urlnotify		= NPP_URLNotify;
	pFuncs->getvalue		= NPP_GetValue;
	pFuncs->setvalue		= NPP_SetValue;

	DBG_TRACE(" -> result=0");
	return NPERR_NO_ERROR;
}

/* NP_GetPluginVersion */
NP_EXPORT(/*const*/ char*) NP_GetPluginVersion(){
	DBG_TRACE("()");
	DBG_TRACE(" -> version='%s'", strPluginVersion);
	return strPluginVersion;
}

/* NP_GetMIMEDescription */
NP_EXPORT(const char*) NP_GetMIMEDescription(){
	DBG_TRACE("()");
	DBG_TRACE(" -> mimeType='%s'", strMimeType);
	return strMimeType;
}

/* NP_GetValue */
NP_EXPORT(NPError) NP_GetValue(void *future, NPPVariable variable, void *value){
	NPError result = NPERR_GENERIC_ERROR;
	std::string resultStr;

	DBG_TRACE("( future=%p, variable=%d, value=%p )", future, variable, value);

	switch (variable) {

		case NPPVpluginNameString:
			*((char**)value)	= strPluginName;
			result				= NPERR_NO_ERROR;
			break;

		case NPPVpluginDescriptionString:
			*((char**)value)	= strPluginDescription;
			result				= NPERR_NO_ERROR;
			break;

		default:
			NOTIMPLEMENTED("( variable=%d )", variable);
			result = NPERR_INVALID_PARAM;
			break;

	}

	DBG_TRACE(" -> result=%d", result);
	return result;
}

/* NP_Shutdown */
NP_EXPORT(NPError) NP_Shutdown(){
	DBG_TRACE("NP_Shutdown()");

	if (initOkay){
		callFunction(NP_SHUTDOWN);
		readResultVoid();
	}

	DBG_TRACE(" -> result=0");
	return NPERR_NO_ERROR;
}

inline void timerFunc(NPP __instance, uint32_t __timerID){
	/* Update the window */
#if 0
	writeInt64( handleManager_count() );
#endif
	callFunction(PROCESS_WINDOW_EVENTS);

	Stack stack;
	readCommands(stack);

	if (!config.linuxWindowlessMode)
		return;

	uint32_t invalidateCount = readInt32(stack);
	while (invalidateCount--){
		NPP instance   = readHandleInstance(stack);

		switch (readInt32(stack)){
			case INVALIDATE_RECT:
				{
					NPRect rect;
					readNPRect(stack, rect);
					sBrowserFuncs->invalidaterect(instance, &rect);
				}
				break;

			case INVALIDATE_EVERYTHING:
				sBrowserFuncs->invalidaterect(instance, NULL);
				break;

			default:
				DBG_ABORT("PROCESS_WINDOW_EVENTS returned unsupported invalidate action.");

		}
	}

}

static void timerThreadAsyncFunc(void* argument){

	/* has been cancelled if we cannot acquire this lock */
	if (sem_trywait(&eventThreadSemScheduledAsyncCall)) return;

	/* Update the window */
	timerFunc(NULL, 0);

	/* request event handling again */
	sem_post(&eventThreadSemRequestAsyncCall);
}

static void* timerThread(void *argument){
	while (true){
		sem_wait(&eventThreadSemRequestAsyncCall);

		/* 10 ms of sleeping before requesting again */
		usleep(10000);

		/* If no instance is running, just terminate */
		if (!eventTimerInstance){
			sem_wait(&eventThreadSemRequestAsyncCall);
			if (!eventTimerInstance) break;
		}

		/* Request an asynccall */
		sem_post(&eventThreadSemScheduledAsyncCall);
		sBrowserFuncs->pluginthreadasynccall(eventTimerInstance, timerThreadAsyncFunc, 0);
	}

	return NULL;
}

static void executeJS(NPP instance, std::string code)
{
	NPObject *windowObj;
	NPString script;

	script.UTF8Characters	= code.c_str();
	script.UTF8Length		= code.size();

	NPVariant resultVariant;
	resultVariant.type				= NPVariantType_Void;
	resultVariant.value.objectValue = NULL;

	if (sBrowserFuncs->getvalue(instance, NPNVWindowNPObject, &windowObj) == NPERR_NO_ERROR){
		if (sBrowserFuncs->evaluate(instance, windowObj, &script, &resultVariant)){
			sBrowserFuncs->releasevariantvalue(&resultVariant);
			DBG_INFO("successfully executed JavaScript.");
		}else
			DBG_ERROR("failed to execute JavaScript, take a look at the JS console.");
		sBrowserFuncs->releaseobject(windowObj);
	}
}

/* NPP_New */
NPError NPP_New(NPMIMEType pluginType, NPP instance, uint16_t mode, int16_t argc, char *argn[], char *argv[], NPSavedData* saved){
	std::string mimeType(pluginType);

	DBG_TRACE("( pluginType='%s', instance=%p, mode=%d, argc=%d, argn=%p, argv=%p, saved=%p )", pluginType, instance, mode, argc, argn, argv, saved);

	PluginData *pdata = (PluginData*)malloc(sizeof(PluginData));
	if (!pdata){
		DBG_TRACE(" -> result=%d", NPERR_OUT_OF_MEMORY_ERROR);
		return NPERR_OUT_OF_MEMORY_ERROR;
	}

	bool invalidMimeType	= (mimeType == "application/x-pipelight-error" || mimeType == "application/x-pipelight-error-" + config.pluginName);

	/* setup plugin data structure */
	pdata->pipelightError	= (!initOkay || invalidMimeType);
	pdata->containerType    = 0;
	pdata->container		= NULL;
#ifndef __APPLE__
	pdata->plugin			= 0;
#endif
	instance->pdata			= pdata;

	if (pdata->pipelightError){
		/* show error message */
		if (sBrowserFuncs->pushpopupsenabledstate)
			sBrowserFuncs->pushpopupsenabledstate(instance, true);
		executeJS(instance, pipelightErrorJS);
		if (sBrowserFuncs->poppopupsenabledstate)
			sBrowserFuncs->poppopupsenabledstate(instance);

		DBG_TRACE(" -> result=%d", NPERR_GENERIC_ERROR);
		return NPERR_GENERIC_ERROR;
	}

	/* Replace fake mimetypes */
	for (std::vector<MimeInfo>::iterator it = config.fakeMIMEtypes.begin(); it != config.fakeMIMEtypes.end(); it++){
		if (it->mimeType == mimeType){
			mimeType = it->originalMime;
			break;
		}
	}

	bool startAsyncCall = false;

	/* Detect Opera browsers and set eventAsyncCall to true in this case */
	if (!config.eventAsyncCall && config.operaDetection && sBrowserFuncs->pluginthreadasynccall){
		if (std::string(sBrowserFuncs->uagent(instance)).find("Opera") != std::string::npos){
			config.eventAsyncCall = true;
			DBG_INFO("Opera browser detected, changed eventAsyncCall to true.");
		}
	}

	/* Execute Javascript if defined */
	if (config.executeJavascript != "")
		executeJS(instance, config.executeJavascript);

	/* Setup eventhandling */
	if (config.eventAsyncCall){
		if (!eventThread){
			eventTimerInstance = instance;
			if (pthread_create(&eventThread, NULL, timerThread, NULL) == 0)
				startAsyncCall = true;
			else{
				eventThread = 0;
				DBG_ERROR("unable to start timer thread.");
			}
		}else
			DBG_INFO("already one timer thread running.");

	}else{
		/* TODO: For Chrome this should be ~0, for Firefox a value of 5-10 is better. */
		if (eventTimerInstance == NULL){
			eventTimerInstance	= instance;
			eventTimerID		= sBrowserFuncs->scheduletimer(instance, 5, true, timerFunc);
		}else
			DBG_INFO("already one timer running.");
	}

	/*
	We can't use this function as we may need to fake some values
	writeStringArray(argv, argc);
	writeStringArray(argn, argc);
	*/

	std::map<std::string, std::string, stringInsensitiveCompare> tempArgs;
	std::map<std::string, std::string, stringInsensitiveCompare>::iterator it;
	for (int i = 0; i < argc; i++){
		if (!argn[i] || !argv[i])
			DBG_ERROR("malformed argument '%s' -> '%s'", argn[i], argv[i]);
		if (argn[i])
			tempArgs[std::string(argn[i])] = argv[i] ? std::string(argv[i]) : "";
	}
	for (it = config.overwriteArgs.begin(); it != config.overwriteArgs.end(); it++)
		tempArgs[it->first] = it->second;
	if (config.windowlessMode){
		for (it = config.windowlessOverwriteArgs.begin(); it != config.windowlessOverwriteArgs.end(); it++)
			tempArgs[it->first] = it->second;
	}

	if (saved)
		writeMemory((char*)saved->buf, saved->len);
	else
		writeMemory(NULL, 0);

	for (it = tempArgs.begin(); it != tempArgs.end(); it++)
		writeString(it->second);
	for (it = tempArgs.begin(); it != tempArgs.end(); it++)
		writeString(it->first);
	writeInt32(tempArgs.size());

	writeInt32(mode);
	writeHandleInstance(instance);
	writeString(mimeType);
	callFunction(FUNCTION_NPP_NEW);
	NPError result = readResultInt32();

	/* The plugin is responsible for freeing *saved. The other side has its own copy of this memory. */
	if (saved){
		sBrowserFuncs->memfree(saved->buf);
		saved->buf = NULL;
		saved->len = 0;
	}

	if (config.linuxWindowlessMode)
		sBrowserFuncs->setvalue(instance, NPPVpluginWindowBool, NULL);

#ifndef __APPLE__
	if (result == NPERR_NO_ERROR && config.x11WindowID)
		NPP_SetWindow(instance, NULL);
#endif

	/* Begin scheduling events */
	if (startAsyncCall) sem_post(&eventThreadSemRequestAsyncCall);

	DBG_TRACE(" -> result=%d", result);
	return result;
}

/* NPP_Destroy */
NPError NPP_Destroy(NPP instance, NPSavedData** save){
	DBG_TRACE("( instance=%p, save=%p )", instance, save);

	/* Initialization failed */
	PluginData *pdata = (PluginData*)instance->pdata;
	if (!pdata){
		DBG_TRACE(" -> result=%d", NPERR_GENERIC_ERROR);
		return NPERR_GENERIC_ERROR;
	}

	bool pipelightError = pdata->pipelightError;

	/* Free instance data, not required anymore */
	free(pdata);
	instance->pdata = NULL;

	if (pipelightError){
		DBG_TRACE(" -> result=%d", NPERR_GENERIC_ERROR);
		return NPERR_GENERIC_ERROR;
	}

	bool unscheduleCurrentTimer = (eventTimerInstance && eventTimerInstance == instance);
	if (unscheduleCurrentTimer){
		if (config.eventAsyncCall){
			if (eventThread){
				/* Do synchronization with the main thread */
				sem_wait(&eventThreadSemScheduledAsyncCall);
				eventTimerInstance = NULL;
				sem_post(&eventThreadSemRequestAsyncCall);
				DBG_INFO("unscheduled event timer thread.");
			}

		}else{
			sBrowserFuncs->unscheduletimer(instance, eventTimerID);
			eventTimerInstance	= NULL;
			eventTimerID		= 0;
			DBG_INFO("unscheduled event timer.");
		}
	}

	writeHandleInstance(instance);
	callFunction(FUNCTION_NPP_DESTROY);

	Stack stack;
	if (!readCommands(stack, true, 5000)){ /* wait maximum 5sec for result */
		int status;
		DBG_ERROR("plugin did not deinitialize properly, killing it!");
		if (pidPluginloader > 0 && !waitpid(pidPluginloader, &status, WNOHANG))
			kill(pidPluginloader, SIGTERM);
		DBG_ABORT("terminating.");
	}

	NPError result = readInt32(stack);

	/* reset the pointer, if there is nothing to save */
	if (save)
		*save = NULL;

	if (result == NPERR_NO_ERROR){

		/* browser has provided memory to save the result */
		if (save){
			size_t save_length;
			char *save_data = readMemoryBrowserAlloc(stack, save_length);
			if (save_data){
				if ((*save = (NPSavedData *)sBrowserFuncs->memalloc(sizeof(NPSavedData)))){
					(*save)->buf = save_data;
					(*save)->len = save_length;
				}else
					sBrowserFuncs->memfree(save_data);
			}

		}else /* skip the saved data */
			stack.pop_back();

	}

	handleManager_removeByPtr(HMGR_TYPE_NPPInstance, instance);

	if (unscheduleCurrentTimer){
		NPP nextInstance = handleManager_findInstance();
		if (config.eventAsyncCall){
			if (eventThread){
				/* start again requesting async calls */
				eventTimerInstance = nextInstance;
				sem_post(&eventThreadSemRequestAsyncCall);
				/* if nextInstance == 0 then the thread will terminate itself as soon as it recognizes that eventTimerInstace == NULL */
				if (nextInstance == 0)
					eventThread = 0;
				else
					DBG_INFO("started timer thread for instance %p.", nextInstance);
			}

		}else{
			/* In this event handling model we explicitly schedule a new timer */
			if (nextInstance){
				eventTimerID		= sBrowserFuncs->scheduletimer(nextInstance, 5, true, timerFunc);
				eventTimerInstance	= nextInstance;
				DBG_INFO("started timer for instance %p.", nextInstance);
			}

		}
	}

	DBG_TRACE(" -> result=%d", result);
	return result;
}

/* NPP_SetWindow */
NPError NPP_SetWindow(NPP instance, NPWindow* window){
	DBG_TRACE("( instance=%p, window=%p )", instance, window);

	PluginData *pdata = (PluginData*)instance->pdata;

#ifndef __APPLE__
	NPWindow windowOverride;
	if (config.x11WindowID){
		if (!window){
			Display *display = XOpenDisplay(NULL);
			if (display)
			{
				unsigned int border, depth;
				Window root;
				if (XGetGeometry(display, config.x11WindowID, &root, &windowOverride.x, &windowOverride.y,
						&windowOverride.width, &windowOverride.height, &border, &depth)){
					windowOverride.window = (void *)config.x11WindowID;
					window = &windowOverride;
				}
				XCloseDisplay(display);
			}
		}else{
			DBG_TRACE(" -> result=0");
			return NPERR_NO_ERROR;
		}
	}
#endif

	if (window){
		/* save the embed container */
		if (pdata){
			pdata->containerType	= window->type;
			pdata->container		= window->window;
		}

		writeRectXYWH(window->x, window->y, window->width, window->height);
		writeInt32((window->type == NPWindowTypeWindow && window->window) ? 1 : 0);
		writeHandleInstance(instance);
		callFunction(FUNCTION_NPP_SET_WINDOW);
		readResultVoid();
	}

	DBG_TRACE(" -> result=0");
	return NPERR_NO_ERROR;
}

/* NPP_NewStream */
NPError NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16_t* stype){
	DBG_TRACE("( instance=%p, type='%s', stream=%p, seekable=%d, stype=%p )", instance, type, stream, seekable, stype);

	if (handleManager_existsByPtr(HMGR_TYPE_NPStream, stream)){
		DBG_ERROR("Chrome notification for existing stream bug!");
		NPP_DestroyStream(instance, stream, NPRES_DONE);
	}

	writeInt32(seekable);
	writeHandleStream(stream);
	writeString(type);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPP_NEW_STREAM);

	Stack stack;
	readCommands(stack);
	NPError result	= readInt32(stack);

	if (result == NPERR_NO_ERROR)
		*stype = (uint16_t)readInt32(stack);
	else /* handle is now invalid because of this error, we get another request using our notifyData */
		handleManager_removeByPtr(HMGR_TYPE_NPStream, stream);

	DBG_TRACE(" -> result=%d", result);
	return result;
}

/* NPP_DestroyStream */
NPError NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason){
	DBG_TRACE("( instance=%p, stream=%p, reason=%d )", instance, stream, reason);

	if (!handleManager_existsByPtr(HMGR_TYPE_NPStream, stream)){
		DBG_TRACE("Opera use-after-free bug!");
		DBG_TRACE(" -> result=0");
		return NPERR_NO_ERROR;
	}

	writeInt32(reason);
	writeHandleStream(stream, HMGR_SHOULD_EXIST);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPP_DESTROY_STREAM);
	NPError result = readResultInt32();

	/* remove the handle by the corresponding stream real object */
	handleManager_removeByPtr(HMGR_TYPE_NPStream, stream);

	DBG_TRACE(" -> result=%d", result);
	return result;
}

/* NPP_WriteReady */
int32_t NPP_WriteReady(NPP instance, NPStream* stream){
	DBG_TRACE("( instance=%p, stream=%p )", instance, stream);

	int32_t result;

	if (!handleManager_existsByPtr(HMGR_TYPE_NPStream, stream)){
		DBG_TRACE("Chrome use-after-free bug!");
		result = 0x7FFFFFFF;

	}else{
		writeHandleStream(stream, HMGR_SHOULD_EXIST);
		writeHandleInstance(instance);
		callFunction(FUNCTION_NPP_WRITE_READY);
		result = readResultInt32();

		/* ensure that the program doesn't want too much data at once - this might cause communication errors */
		if (result > 0xFFFFFF)
			result = 0xFFFFFF;
	}

	DBG_TRACE(" -> result=%d", result);
	return result;
}

/* NPP_Write */
int32_t NPP_Write(NPP instance, NPStream* stream, int32_t offset, int32_t len, void* buffer){
	DBG_TRACE("( instance=%p, stream=%p, offset=%d, len=%d, buffer=%p )", instance, stream, offset, len, buffer);

	if (!handleManager_existsByPtr(HMGR_TYPE_NPStream, stream))
		DBG_TRACE("Chrome use-after-free bug!");

	else{
		writeMemory((char*)buffer, len);
		writeInt32(offset);
		writeHandleStream(stream, HMGR_SHOULD_EXIST);
		writeHandleInstance(instance);
		callFunction(FUNCTION_NPP_WRITE);

		len = readResultInt32();
	}

	DBG_TRACE(" -> result=%d", len);
	return len;
}

/* NPP_StreamAsFile */
void NPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname){
	DBG_TRACE("( instance=%p, stream=%p, fname=%p )", instance, stream, fname);

	writeString(fname);
	writeHandleStream(stream, HMGR_SHOULD_EXIST);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPP_STREAM_AS_FILE);
	readResultVoid();

	DBG_TRACE(" -> void");
}

/* NPP_Print */
void NPP_Print(NPP instance, NPPrint* platformPrint){
	DBG_TRACE("( instance=%p, platformPrint=%p )", instance, platformPrint);
	NOTIMPLEMENTED();
	DBG_TRACE(" -> void");
}

/* NPP_HandleEvent */
int16_t NPP_HandleEvent(NPP instance, void* event){
	DBG_TRACE("( instance=%p, event=%p )", instance, event);

	int16_t res = kNPEventNotHandled;

#ifndef __APPLE__
	if (config.linuxWindowlessMode && event){
		XEvent *xevent   = (XEvent *)event;
		/* Display *display = xevent->xany.display; */

		PluginData *pdata = (PluginData*)instance->pdata;
		if (pdata){
			if (xevent->type == GraphicsExpose){
				writeRectXYWH(xevent->xgraphicsexpose.x, xevent->xgraphicsexpose.y,
					xevent->xgraphicsexpose.width, xevent->xgraphicsexpose.height);
				writeInt32(xevent->xgraphicsexpose.drawable);
				writeHandleInstance(instance);
				callFunction(WINDOWLESS_EVENT_PAINT);
				readResultVoid();
				res = kNPEventHandled;

			}else if (xevent->type == MotionNotify){
				writePointXY(xevent->xmotion.x, xevent->xmotion.y);
				writeInt32(xevent->xmotion.state);
				writeHandleInstance(instance);
				callFunction(WINDOWLESS_EVENT_MOUSEMOVE);
				readResultVoid();
				res = kNPEventHandled;

			}else if (xevent->type == ButtonPress || xevent->type == ButtonRelease){
				writePointXY(xevent->xbutton.x, xevent->xbutton.y);
				writeInt32(xevent->xbutton.button);
				writeInt32(xevent->xbutton.state);
				writeInt32((xevent->type == ButtonPress));
				writeHandleInstance(instance);
				callFunction(WINDOWLESS_EVENT_MOUSEBUTTON);
				readResultVoid();
				res = kNPEventHandled;

			}else if (xevent->type == KeyPress || xevent->type == KeyRelease){
				writeInt32(xevent->xkey.keycode);
				writeInt32(xevent->xkey.state);
				writeInt32((xevent->type == KeyPress));
				writeHandleInstance(instance);
				callFunction(WINDOWLESS_EVENT_KEYBOARD);
				readResultVoid();
				res = kNPEventHandled;

			}
		}

	}else
		NOTIMPLEMENTED("ignoring unexpected callback.");
#endif

	DBG_TRACE(" -> result=%d", res);
	return res;
}

/* NPP_URLNotify */
void NPP_URLNotify(NPP instance, const char* URL, NPReason reason, void* notifyData){
	DBG_TRACE("( instance=%p, URL='%s', reason=%d, notifyData=%p )", instance, URL, reason, notifyData);

	writeHandleNotify(notifyData, HMGR_SHOULD_EXIST);
	writeInt32(reason);
	writeString(URL);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPP_URL_NOTIFY);
	readResultVoid();

	/* free all the notifydata stuff */
	NotifyDataRefCount* myNotifyData = (NotifyDataRefCount*)notifyData;
	if (myNotifyData){
		DBG_ASSERT(myNotifyData->referenceCount != 0, "reference count is zero.");

		/* decrement refcount */
		if (--myNotifyData->referenceCount == 0){
		#ifdef PIPELIGHT_SYNC
			writeHandleNotify(myNotifyData);
			callFunction(WIN_HANDLE_MANAGER_FREE_NOTIFY_DATA);
			readResultVoid();
		#else
			writeHandleNotify(myNotifyData);
			callFunction(WIN_HANDLE_MANAGER_FREE_NOTIFY_DATA_ASYNC);
		#endif

			handleManager_removeByPtr(HMGR_TYPE_NotifyData, myNotifyData);

			free(myNotifyData);
		}
	}

	DBG_TRACE(" -> void");
}

/* NPP_GetValue */
NPError NPP_GetValue(NPP instance, NPPVariable variable, void *value){
	DBG_TRACE("( instance=%p, variable=%d, value=%p )", instance, variable, value);

	NPError result = NPERR_GENERIC_ERROR;
	Stack stack;

	switch (variable){

		case NPPVpluginNeedsXEmbed:
			result						= NPERR_NO_ERROR;
			*((PRBool *)value)			= config.linuxWindowlessMode ? PR_FALSE : PR_TRUE;
			break;

		/* Requested by Midori, but unknown if Silverlight supports this variable */
		case NPPVpluginWantsAllNetworkStreams:
			result						= NPERR_NO_ERROR;
			*((PRBool *)value)			= PR_FALSE;
			break;

		/* Boolean return value */
		/*
			writeInt32(variable);
			writeHandleInstance(instance);
			callFunction(FUNCTION_NPP_GETVALUE_BOOL);
			readCommands(stack);
			result = (NPError)readInt32(stack);
			if(result == NPERR_NO_ERROR)
				*((PRBool *)value) = (PRBool)readInt32(stack);
			break;*/

		/* Object return value */
		case NPPVpluginScriptableNPObject:
			writeInt32(variable);
			writeHandleInstance(instance);
			callFunction(FUNCTION_NPP_GETVALUE_OBJECT);
			readCommands(stack);
			result = readInt32(stack);
			if (result == NPERR_NO_ERROR)
				*((NPObject**)value) = readHandleObj(stack);
			break;

		default:
			NOTIMPLEMENTED("( variable=%d )", variable);
			result = NPERR_INVALID_PARAM;
			break;
	}

	DBG_TRACE(" -> ( result=%d, ... )", result);
	return result;
}

/* NPP_SetValue */
NPError NPP_SetValue(NPP instance, NPNVariable variable, void *value){
	DBG_TRACE("( instance=%p, variable=%d, value=%p )", instance, variable, value);
	NOTIMPLEMENTED();
	DBG_TRACE(" -> result=%d", NPERR_GENERIC_ERROR);
	return NPERR_GENERIC_ERROR;
}