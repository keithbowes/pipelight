
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>								// waitpid, kill, ...
#include <string.h>								// for memcpy, ...

#include "../common/common.h"
#include "basicplugin.h"
#include "debug.h"

NP_EXPORT(NPError) NP_Initialize(NPNetscapeFuncs* bFuncs, NPPluginFuncs* pFuncs)
{
	DBG_TRACE("( bFuncs=%p, pFuncs=%p )", bFuncs, pFuncs);

	if (bFuncs == NULL || pFuncs == NULL)
		return NPERR_INVALID_PARAM;

	if ((bFuncs->version >> 8) > NP_VERSION_MAJOR){
		DBG_ERROR("incompatible browser version!");
		return NPERR_INCOMPATIBLE_VERSION_ERROR;
	}

	// Copy browser functions instead of saving the pointer
	if (!sBrowserFuncs){
		sBrowserFuncs = (NPNetscapeFuncs*)malloc( sizeof(NPNetscapeFuncs) );
	}

	if (!sBrowserFuncs){
		return NPERR_OUT_OF_MEMORY_ERROR;
	}

	memset(sBrowserFuncs, 0, sizeof(NPNetscapeFuncs));
	memcpy(sBrowserFuncs, bFuncs, ((bFuncs->size < sizeof(NPNetscapeFuncs)) ? bFuncs->size : sizeof(NPNetscapeFuncs)) );

	// Check if all required browser functions are available
	if (!sBrowserFuncs->geturl ||
		!sBrowserFuncs->posturl ||
		!sBrowserFuncs->requestread ||
		!sBrowserFuncs->newstream ||
		!sBrowserFuncs->write ||
		!sBrowserFuncs->destroystream ||
		!sBrowserFuncs->status ||
		!sBrowserFuncs->uagent ||
		!sBrowserFuncs->memalloc ||
		!sBrowserFuncs->memfree ||
		!sBrowserFuncs->geturlnotify ||
		!sBrowserFuncs->posturlnotify ||
		!sBrowserFuncs->getvalue ||
		!sBrowserFuncs->getstringidentifier ||
		!sBrowserFuncs->getintidentifier ||
		!sBrowserFuncs->identifierisstring ||
		!sBrowserFuncs->utf8fromidentifier ||
		!sBrowserFuncs->intfromidentifier ||
		!sBrowserFuncs->createobject ||
		!sBrowserFuncs->retainobject ||
		!sBrowserFuncs->releaseobject ||
		!sBrowserFuncs->invoke ||
		!sBrowserFuncs->invokeDefault ||
		!sBrowserFuncs->evaluate ||
		!sBrowserFuncs->getproperty ||
		!sBrowserFuncs->setproperty ||
		!sBrowserFuncs->removeproperty ||
		!sBrowserFuncs->hasproperty ||
		!sBrowserFuncs->releasevariantvalue ||
		!sBrowserFuncs->setexception ||
		!sBrowserFuncs->enumerate ){

		DBG_ERROR("your browser doesn't support all required functions!");
		return NPERR_INCOMPATIBLE_VERSION_ERROR;
	}

	if (pFuncs->size < (offsetof(NPPluginFuncs, setvalue) + sizeof(void*)))
		return NPERR_INVALID_FUNCTABLE_ERROR;

	// Select which event handling method should be used
	if (!config.eventAsyncCall && sBrowserFuncs->scheduletimer && sBrowserFuncs->unscheduletimer){
		DBG_INFO("using timer based event handling.");

	}else if (sBrowserFuncs->pluginthreadasynccall){
		DBG_INFO("using thread asynccall event handling.");
		config.eventAsyncCall = true;

	}else{
		DBG_ERROR("no eventhandling compatible with your browser available.");
		return NPERR_INCOMPATIBLE_VERSION_ERROR;
	}

	// Return the plugin function table
	pFuncs->version 		= (NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR;

	// Dont overwrite the size of the function table, it might be smaller than sizeof(NPPluginFuncs)
	pFuncs->newp 			= NPP_New;
	pFuncs->destroy 		= NPP_Destroy;
	pFuncs->setwindow 		= NPP_SetWindow;
	pFuncs->newstream 		= NPP_NewStream;
	pFuncs->destroystream 	= NPP_DestroyStream;
	pFuncs->asfile 			= NPP_StreamAsFile;
	pFuncs->writeready 		= NPP_WriteReady;
	pFuncs->write 			= NPP_Write;
	pFuncs->print 			= NPP_Print;
	pFuncs->event 			= NPP_HandleEvent;
	pFuncs->urlnotify 		= NPP_URLNotify;
	pFuncs->getvalue 		= NPP_GetValue;
	pFuncs->setvalue 		= NPP_SetValue;

	return NPERR_NO_ERROR;
}

NP_EXPORT(char*) NP_GetPluginVersion()
{
	DBG_TRACE("()");

	if (!initOkay){
		pokeString(strPluginversion, "0.0", sizeof(strPluginversion));
		return strPluginversion;
	}

	callFunction(FUNCTION_GET_VERSION);

	std::string result = readResultString();
	pokeString(strPluginversion, result, sizeof(strPluginversion));

	return strPluginversion;
}

NP_EXPORT(const char*) NP_GetMIMEDescription()
{
	DBG_TRACE("()");

	if (!initOkay){
		if(config.pluginName == ""){
			pokeString(strMimeType, "application/x-pipelight-error:pipelighterror:Error during initialization", sizeof(strMimeType));
		}else{
			pokeString(strMimeType, "application/x-pipelight-error-"+config.pluginName+":pipelighterror-"+config.pluginName+":Error during initialization", sizeof(strMimeType));
		}
		return strMimeType;
	}

	callFunction(FUNCTION_GET_MIMETYPE);

	std::string result = readResultString();
	pokeString(strMimeType, result, sizeof(strMimeType));

	return strMimeType;
}

NP_EXPORT(NPError) NP_GetValue(void* future, NPPVariable variable, void* value) {
	NPError result = NPERR_GENERIC_ERROR;
	std::string resultStr;

	DBG_TRACE("( future=%p, variable=%d, value=%p )", future, variable, value);

	switch (variable) {

		case NPPVpluginNameString:

			if (!initOkay){
				if(config.pluginName == ""){
					resultStr = "Pipelight Error!";
				}else{
					resultStr = "Pipelight Error (" + config.pluginName +")!";
				}
			}else{
				callFunction(FUNCTION_GET_NAME);
				resultStr = readResultString();
			}

			pokeString(strPluginName, resultStr, sizeof(strPluginName));

			*((char**)value) 	= strPluginName;
			result 				= NPERR_NO_ERROR;
			break;

		case NPPVpluginDescriptionString:

			if (!initOkay){
				resultStr = "Something went wrong, check the terminal output";
			}else if (config.fakeVersion != ""){
				resultStr = config.fakeVersion;
			}else{
				callFunction(FUNCTION_GET_DESCRIPTION);
				resultStr = readResultString();
			}

			pokeString(strPluginDescription, resultStr, sizeof(strPluginDescription));		

			*((char**)value) 	= strPluginDescription;
			result 				= NPERR_NO_ERROR;
			break;

		default:
			NOTIMPLEMENTED("( variable=%d )", variable);
			result = NPERR_INVALID_PARAM;
			break;

	}

	return result;
}

NP_EXPORT(NPError) NP_Shutdown() {
	DBG_TRACE("NP_Shutdown()");

	if (initOkay){
		callFunction(NP_SHUTDOWN);
		readResultVoid();
	}

	return NPERR_NO_ERROR;
}

void timerFunc(NPP instance, uint32_t timerID){

	writeInt64( handleManager_count() );
	callFunction(PROCESS_WINDOW_EVENTS);
	readResultVoid();
}

void timerThreadAsyncFunc(void* argument){

	// Has been cancelled if we cannot acquire this lock
	if (sem_trywait(&eventThreadSemScheduledAsyncCall)) return;

	// Update the window
	writeInt64( handleManager_count() );
	callFunction(PROCESS_WINDOW_EVENTS);
	readResultVoid();

	// Request event handling again
	sem_post(&eventThreadSemRequestAsyncCall);
}

void* timerThread(void* argument){
	while (true){
		sem_wait(&eventThreadSemRequestAsyncCall);

		// 10 ms of sleeping before requesting again
		usleep(10000);

		// If no instance is running, just terminate
		if (!eventTimerInstance){
			sem_wait(&eventThreadSemRequestAsyncCall);
			if (!eventTimerInstance) break;
		}

		// Request an asynccall
		sem_post(&eventThreadSemScheduledAsyncCall);
		sBrowserFuncs->pluginthreadasynccall(eventTimerInstance, timerThreadAsyncFunc, 0);
	}

	return NULL;
}

NPError NPP_New(NPMIMEType pluginType, NPP instance, uint16_t mode, int16_t argc, char* argn[], char* argv[], NPSavedData* saved) {
	std::string mimeType(pluginType);

	DBG_TRACE("( pluginType='%s', instance=%p, mode=%d, argc=%d, argn=%p, argv=%p, saved=%p )", pluginType, instance, mode, argc, argn, argv, saved);

	PluginData *pdata = (PluginData*)malloc(sizeof(PluginData));
	if (!pdata)
		return NPERR_OUT_OF_MEMORY_ERROR;

	bool invalidMimeType  	= (mimeType == "application/x-pipelight-error" || mimeType == "application/x-pipelight-error-" + config.pluginName);

	// Setup plugin data structure
	pdata->pipelightError 	= (!initOkay || invalidMimeType);
	pdata->container      	= 0;
	instance->pdata 		= pdata;

	if (pdata->pipelightError){

		// Run diagnostic stuff if its the wrong mimetype
		if (invalidMimeType && config.diagnosticMode)
			runDiagnostic(instance);

		return NPERR_GENERIC_ERROR;
	}

	bool startAsyncCall = false;

	// Detect opera browsers and set eventAsyncCall to true in this case
	if (!config.eventAsyncCall && config.operaDetection){
		if (std::string(sBrowserFuncs->uagent(instance)).find("Opera") != std::string::npos){
			config.eventAsyncCall = true;

			DBG_INFO("Opera browser detected, changed eventAsyncCall to true.");
		}
	}

	// Execute javascript if defined
	if (config.executeJavascript != ""){
		NPObject 		*windowObj;
		NPString		script;
		script.UTF8Characters 	= config.executeJavascript.c_str();
		script.UTF8Length		= config.executeJavascript.size();

		NPVariant resultVariant;
		resultVariant.type 				= NPVariantType_Void;
		resultVariant.value.objectValue = NULL;

		if (sBrowserFuncs->getvalue(instance, NPNVWindowNPObject, &windowObj) == NPERR_NO_ERROR){
			
			if (sBrowserFuncs->evaluate(instance, windowObj, &script, &resultVariant)){
				sBrowserFuncs->releasevariantvalue(&resultVariant);

				DBG_INFO("successfully executed JavaScript.");

			}else{
				DBG_ERROR("failed to execute JavaScript, take a look at the JS console.");
			}

			sBrowserFuncs->releaseobject(windowObj);
		}
	}

	// Setup eventhandling
	if (config.eventAsyncCall){
		if (!eventThread){
			eventTimerInstance = instance;

			if (pthread_create(&eventThread, NULL, timerThread, NULL) == 0){ // failed
				startAsyncCall = true;

			}else{
				eventThread = 0;
				DBG_ERROR("unable to start timer thread.");
			}

		}else{
			DBG_INFO("already one timer thread running.");
		}

	}else{
		// TODO: For Chrome this should be ~0, for Firefox a value of 5-10 is better.
		if (eventTimerInstance == NULL){
			eventTimerInstance 	= instance;
			eventTimerID 		= sBrowserFuncs->scheduletimer(instance, 5, true, timerFunc);
		}else{
			DBG_INFO("already one timer running.");
		}

	}

	if (saved){
		writeMemory((char*)saved->buf, saved->len);
	}else{
		writeMemory(NULL, 0);
	}

	// We can't use this function as we may need to fake some values
	// writeStringArray(argv, argc);
	// writeStringArray(argn, argc);

	int realArgCount = 0;
	std::map<std::string, std::string>::iterator it;

	// argv
	for (int i = argc - 1; i >= 0; i--){
		std::string key(argn[i]);

		it = config.overwriteArgs.find(key);
		if (it == config.overwriteArgs.end()){
			realArgCount++;
			writeString(argv[i]);
		}
	}

	for (it = config.overwriteArgs.begin(); it != config.overwriteArgs.end(); it++){
		realArgCount++;
		writeString(it->second);
	}

	//argn
	for (int i = argc - 1; i >= 0; i--){
		std::string key(argn[i]);

		it = config.overwriteArgs.find(key);
		if (it == config.overwriteArgs.end()){
			writeString(argn[i]);
		}
	}

	for (it = config.overwriteArgs.begin(); it != config.overwriteArgs.end(); it++){
		writeString(it->first);
	}

	writeInt32(realArgCount);
	writeInt32(mode);
	writeHandleInstance(instance);
	writeString(pluginType);
	callFunction(FUNCTION_NPP_NEW);

	NPError result = readResultInt32();

	// The plugin is responsible for freeing *saved
	// The other side has its own copy of this memory
	if (saved){
		sBrowserFuncs->memfree(saved->buf);
		saved->buf = NULL;
		saved->len = 0;
	}

	// Begin scheduling events
	if (startAsyncCall) sem_post(&eventThreadSemRequestAsyncCall);

	return result;
}

NPError NPP_Destroy(NPP instance, NPSavedData** save) {
	DBG_TRACE("( instance=%p, save=%p )", instance, save);

	// Initialization failed or diagnostic mode
	PluginData *pdata = (PluginData*)instance->pdata;
	if (!pdata)
		return NPERR_GENERIC_ERROR;

	bool pipelightError = pdata->pipelightError;

	// Free instance data, not required anymore
	free(pdata);
	instance->pdata = NULL;

	if (pipelightError)
		return NPERR_GENERIC_ERROR;

	bool unscheduleCurrentTimer = (eventTimerInstance && eventTimerInstance == instance);

	if (unscheduleCurrentTimer){
		if (config.eventAsyncCall){
			if (eventThread){
				
				// Do synchronization with the main thread
				sem_wait(&eventThreadSemScheduledAsyncCall);
				eventTimerInstance = NULL;
				sem_post(&eventThreadSemRequestAsyncCall);

				DBG_INFO("unscheduled event timer thread.");
			}

		}else{

			sBrowserFuncs->unscheduletimer(instance, eventTimerID);
			eventTimerInstance 	= NULL;
			eventTimerID 		= 0;

			DBG_INFO("unscheduled event timer.");

		}
	}

	writeHandleInstance(instance);
	callFunction(FUNCTION_NPP_DESTROY);

	Stack stack;

	if (!readCommands(stack, true, 5000)){
		DBG_ERROR("plugin did not deinitialize properly, killing it!");

		// Kill the wine process (if it still exists) ...
		int status;
		if (winePid > 0 && !waitpid(winePid, &status, WNOHANG)){
			kill(winePid, SIGTERM);
		}

		DBG_ABORT("terminating.");
	}

	NPError result 	= readInt32(stack);

	if (result == NPERR_NO_ERROR){
		if (save){
			size_t save_length;
			char* save_data = readMemoryBrowserAlloc(stack, save_length);

			if (save_data && save){
				*save = (NPSavedData*) sBrowserFuncs->memalloc(sizeof(NPSavedData));
				if (*save){

					(*save)->buf = save_data;
					(*save)->len = save_length;

				}else{
					sBrowserFuncs->memfree(save_data);
				}
			}else{
				sBrowserFuncs->memfree(save_data);
			}

		}else{ // Skip the saved data
			stack.pop_back();
		}

	}else if (save){
		*save = NULL; // Nothing to save
	}

	handleManager_removeByPtr(HMGR_TYPE_NPPInstance, instance);

	if (unscheduleCurrentTimer){
		NPP nextInstance = handleManager_findInstance();
		if (config.eventAsyncCall){
			if (eventThread){
				eventTimerInstance = nextInstance;
				
				// Start again requesting async calls
				sem_post(&eventThreadSemRequestAsyncCall);

				// If nextInstance == 0 then the thread will terminate itself as soon as it recognizes that eventTimerInstace == NULL
				if (nextInstance == 0){
					eventThread = 0;
				}else{
					DBG_INFO("started timer thread for instance %p.", nextInstance);
				}
			}


		}else{
			// In this event handling model we explicitly schedule a new timer
			if (nextInstance){
				eventTimerID 		= sBrowserFuncs->scheduletimer(nextInstance, 5, true, timerFunc);
				eventTimerInstance 	= nextInstance;

				DBG_INFO("started timer for instance %p.", nextInstance);
			}

		}
	}

	return result;
}

NPError NPP_SetWindow(NPP instance, NPWindow* window) {
	DBG_TRACE("( instance=%p, window=%p )", instance, window);

	PluginData *pdata = (PluginData*)instance->pdata;

	// Save the embed container
	if (pdata)
		pdata->container = (Window)window->window;

	// TODO: translate to Screen coordinates
	// TODO: Use all parameters

	writeInt32(window->height);
	writeInt32(window->width);
	writeInt32(window->y);
	writeInt32(window->x);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPP_SET_WINDOW);
	readResultVoid();

	return NPERR_NO_ERROR;
}

NPError NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16_t* stype) {
	DBG_TRACE("( instance=%p, type='%s', stream=%p, seekable=%d, stype=%p )", instance, type, stream, seekable, stype);

	writeInt32(seekable);
	writeHandleStream(stream);
	writeString(type);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPP_NEW_STREAM);

	Stack stack;
	readCommands(stack);	

	NPError result 	= readInt32(stack);

	if (result == NPERR_NO_ERROR){
		*stype 			= (uint16_t)readInt32(stack);

	}else{ // Handle is now invalid because of this error
		handleManager_removeByPtr(HMGR_TYPE_NPStream, stream);

		// We get another request using our notifyData after everything
	}

	return result;
}

NPError NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason) {
	DBG_TRACE("( instance=%p, stream=%p, reason=%d )", instance, stream, reason);

	if (!handleManager_existsByPtr(HMGR_TYPE_NPStream, stream)){
		DBG_TRACE("Opera use-after-free bug!");
		return NPERR_NO_ERROR;
	}

	writeInt32(reason);
	writeHandleStream(stream, HMGR_SHOULD_EXIST);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPP_DESTROY_STREAM);

	NPError result = readResultInt32();

	// Remove the handle by the corresponding stream real object
	handleManager_removeByPtr(HMGR_TYPE_NPStream, stream);

	return result;
}

int32_t NPP_WriteReady(NPP instance, NPStream* stream) {
	DBG_TRACE("( instance=%p, stream=%p )", instance, stream);

	if (!handleManager_existsByPtr(HMGR_TYPE_NPStream, stream)){
		DBG_TRACE("Chrome use-after-free bug!");
		return 0x7FFFFFFF;
	}

	writeHandleStream(stream, HMGR_SHOULD_EXIST);
	writeHandleInstance(instance);	
	callFunction(FUNCTION_NPP_WRITE_READY);
	
	int32_t result = readResultInt32();

	// Ensure that the program doesn't want too much data at once - this might cause the communication to hang
	if (result > 0xFFFFFF){
		result = 0xFFFFFF;
	}

	return result;
}

int32_t NPP_Write(NPP instance, NPStream* stream, int32_t offset, int32_t len, void* buffer) {
	DBG_TRACE("( instance=%p, stream=%p, offset=%d, len=%d, buffer=%p )", instance, stream, offset, len, buffer);

	if (!handleManager_existsByPtr(HMGR_TYPE_NPStream, stream)){
		DBG_TRACE("Chrome use-after-free bug!");
		return len;
	}

	writeMemory((char*)buffer, len);
	writeInt32(offset);
	writeHandleStream(stream, HMGR_SHOULD_EXIST);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPP_WRITE);
	
	return readResultInt32();
}

void NPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname) {
	DBG_TRACE("( instance=%p, stream=%p, fname=%p )", instance, stream, fname);

	writeString(fname);
	writeHandleStream(stream, HMGR_SHOULD_EXIST);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPP_STREAM_AS_FILE);
	readResultVoid();
}

void NPP_Print(NPP instance, NPPrint* platformPrint) {
	DBG_TRACE("( instance=%p, platformPrint=%p )", instance, platformPrint);
	NOTIMPLEMENTED();
}

int16_t NPP_HandleEvent(NPP instance, void* event) {
	DBG_TRACE("( instance=%p, event=%p )", instance, event);
	NOTIMPLEMENTED();
	return 0;
}

void NPP_URLNotify(NPP instance, const char* URL, NPReason reason, void* notifyData) {
	DBG_TRACE("( instance=%p, URL='%s', reason=%d, notifyData=%p )", instance, URL, reason, notifyData);

	writeHandleNotify(notifyData, HMGR_SHOULD_EXIST);
	writeInt32(reason);
	writeString(URL);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPP_URL_NOTIFY);
	readResultVoid();

	// Free all the notifydata stuff
	NotifyDataRefCount* myNotifyData = (NotifyDataRefCount*)notifyData;
	if (myNotifyData){
		DBG_ASSERT(myNotifyData->referenceCount != 0, "reference count is zero.");

		// Decrement refcount
		myNotifyData->referenceCount--;

		if (myNotifyData->referenceCount == 0){

			// Free everything
			writeHandleNotify(myNotifyData);
			callFunction(WIN_HANDLE_MANAGER_FREE_NOTIFY_DATA);
			readResultVoid();

			handleManager_removeByPtr(HMGR_TYPE_NotifyData, myNotifyData);

			free(myNotifyData);
		}
	}

}

NPError NPP_GetValue(NPP instance, NPPVariable variable, void *value) {
	DBG_TRACE("( instance=%p, variable=%d, value=%p )", instance, variable, value);

	NPError result = NPERR_GENERIC_ERROR;
	std::vector<ParameterInfo> stack;

	switch (variable){

		case NPPVpluginNeedsXEmbed:
			result 						= NPERR_NO_ERROR;
			*((PRBool *)value) 			= PR_TRUE;
			break;

		// Requested by Midori, but unknown if Silverlight supports this variable
		case NPPVpluginWantsAllNetworkStreams:
			result 						= NPERR_NO_ERROR;
			*((PRBool *)value) 			= PR_FALSE;
			break;

		// Boolean return value
		/*
			writeInt32(variable);
			writeHandleInstance(instance);
			callFunction(FUNCTION_NPP_GETVALUE_BOOL);
			readCommands(stack);

			result = (NPError)readInt32(stack);

			if(result == NPERR_NO_ERROR)
				*((PRBool *)value) 		= (PRBool)readInt32(stack);
			break;*/

		// Object return value
		case NPPVpluginScriptableNPObject:
			writeInt32(variable);
			writeHandleInstance(instance);
			callFunction(FUNCTION_NPP_GETVALUE_OBJECT);
			readCommands(stack);

			result 						= readInt32(stack);

			if (result == NPERR_NO_ERROR)
				*((NPObject**)value) 	= readHandleObj(stack);
			break;


		default:
			NOTIMPLEMENTED("( variable=%d )", variable);
			result = NPERR_INVALID_PARAM;
			break;
	}

	return result;
}

NPError NPP_SetValue(NPP instance, NPNVariable variable, void *value) {
	DBG_TRACE("( instance=%p, variable=%d, value=%p )", instance, variable, value);
	NOTIMPLEMENTED();
	return NPERR_GENERIC_ERROR;
}