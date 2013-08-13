
#include <iostream>								// for std::cerr
#include <algorithm>							// for std::transform
#include <X11/Xlib.h>							// for XSendEvent, ...
#include <stdexcept>							// for std::runtime_error
#include <string.h>								// for memcpy, ...

#include <signal.h>								// waitpid, kill, ...
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "basicplugin.h"
#include "configloader.h"

extern char strMimeType[2048];
extern char strPluginversion[100];
extern char strPluginName[256];
extern char strPluginDescription[1024];

extern uint32_t  	EventTimerID;
extern NPP 			EventTimerInstance;

extern PluginConfig config;
extern bool 		initOkay;

#define XEMBED_EMBEDDED_NOTIFY		0

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

void pokeString(std::string str, char *dest, unsigned int maxLength){
	if(maxLength > 0){
		unsigned int length = std::min((unsigned int)str.length(), maxLength-1);

		// Always at least one byte to copy (nullbyte)
		memcpy(dest, str.c_str(), length);
		dest[length] = 0;
	}
}

// Verified, everything okay
NP_EXPORT(NPError)
NP_Initialize(NPNetscapeFuncs* bFuncs, NPPluginFuncs* pFuncs)
{
	EnterFunction();

	if( bFuncs == NULL || pFuncs == NULL )
		return NPERR_INVALID_PARAM;

	if( (bFuncs->version >> 8) > NP_VERSION_MAJOR ){
		std::cerr << "[PIPELIGHT] Incompatible browser version!" << std::endl;
		return NPERR_INCOMPATIBLE_VERSION_ERROR;
	}

	// sBrowserFuncs = bFuncs;
	// Copy browser functions instead of saving the pointer

	if(!sBrowserFuncs){
		sBrowserFuncs = (NPNetscapeFuncs*)malloc( sizeof(NPNetscapeFuncs) );
	}

	if(!sBrowserFuncs){
		return NPERR_OUT_OF_MEMORY_ERROR;
	}

	memset(sBrowserFuncs, 0, sizeof(NPNetscapeFuncs));
	memcpy(sBrowserFuncs, bFuncs, ((bFuncs->size < sizeof(NPNetscapeFuncs)) ? bFuncs->size : sizeof(NPNetscapeFuncs)) );

	// Check if all required browser functions are available
	if( !sBrowserFuncs->geturl ||
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
		!sBrowserFuncs->enumerate ||
		!sBrowserFuncs->scheduletimer ||
		!sBrowserFuncs->unscheduletimer ){
		std::cerr << "[PIPELIGHT] Your browser doesn't support all required functions!" << std::endl;
		return NPERR_INCOMPATIBLE_VERSION_ERROR;
	}


	if( pFuncs->size < (offsetof(NPPluginFuncs, setvalue) + sizeof(void*)) )
		return NPERR_INVALID_FUNCTABLE_ERROR;

	// Return the plugin function table
	pFuncs->version 		= (NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR;

	// Dont overwrite the size of the function table, it might be smaller than usual
	// pFuncs->size 		= sizeof(NPPluginFuncs);

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

// Verified, everything okay
NP_EXPORT(char*)
NP_GetPluginVersion()
{
	EnterFunction();

	if(!initOkay){
		pokeString("0.0", strPluginversion, sizeof(strPluginversion));
		return strPluginversion;
	}

	callFunction(FUNCTION_GET_VERSION);

	std::string result = readResultString();
	pokeString(result, strPluginversion, sizeof(strPluginversion));

	return strPluginversion;
}

// Verified, everything okay
NP_EXPORT(const char*)
NP_GetMIMEDescription()
{
	EnterFunction();

	if(!initOkay){
		pokeString("application/x-pipelight-error:pipelighterror:Error during initialization", strMimeType, sizeof(strMimeType));
		return strMimeType;
	}

	callFunction(FUNCTION_GET_MIMETYPE);

	std::string result = readResultString();
	pokeString(result, strMimeType, sizeof(strMimeType));

	return strMimeType;
}

// Verified, everything okay
NP_EXPORT(NPError)
NP_GetValue(void* future, NPPVariable aVariable, void* aValue) {
	EnterFunction();
	
	NPError result = NPERR_GENERIC_ERROR;
	std::string resultStr;

	switch (aVariable) {

		case NPPVpluginNameString:

			if(!initOkay){
				resultStr = "Pipelight Error!";
			}else{
				callFunction(FUNCTION_GET_NAME);
				resultStr = readResultString();
			}

			pokeString(resultStr, strPluginName, sizeof(strPluginName));

			*((char**)aValue) = strPluginName;
			result = NPERR_NO_ERROR;
			break;

		case NPPVpluginDescriptionString:

			if(!initOkay){
				resultStr = "Something went wrong, check the terminal output";
			}else if(config.fakeVersion != ""){
				resultStr = config.fakeVersion;
			}else{
				callFunction(FUNCTION_GET_DESCRIPTION);
				resultStr = readResultString();
			}

			pokeString(resultStr, strPluginDescription, sizeof(strPluginDescription));		

			*((char**)aValue) = strPluginDescription;
			result = NPERR_NO_ERROR;
			break;

		default:
			NotImplemented();
			result = NPERR_INVALID_PARAM;
			break;

	}

	return result;
}

// TODO: Is this type correct? Does an errorcode make sense?
NP_EXPORT(NPError)
NP_Shutdown() {
	EnterFunction();

	if(initOkay){
		callFunction(NP_SHUTDOWN);
		waitReturn();
	}

	return NPERR_NO_ERROR;
}

void timerFunc(NPP instance, uint32_t timerID){
	callFunction(PROCESS_WINDOW_EVENTS);
	waitReturn();
}

// Verified, everything okay
NPError
NPP_New(NPMIMEType pluginType, NPP instance, uint16_t mode, int16_t argc, char* argn[], char* argv[], NPSavedData* saved) {
	EnterFunction();

	if(!initOkay)
		return NPERR_GENERIC_ERROR;

	// TODO: For Chrome this should be ~0, for Firefox a value of 5-10 is better.

	if( EventTimerInstance == NULL ){
		EventTimerID 		= sBrowserFuncs->scheduletimer(instance, 5, true, timerFunc);
		EventTimerInstance 	= instance;
	}else{
		std::cerr << "[PIPELIGHT] Already one running timer" << std::endl;
	}

	if(saved){
		writeMemory((char*)saved->buf, saved->len);
	}else{
		writeMemory(NULL, 0);
	}

	// We can't use this function as we may need to fake some values
	//writeStringArray(argv, argc);
	for(int i = argc - 1; i >= 0; i--){

		std::string key(argn[i]);
		std::transform(key.begin(), key.end(), key.begin(), ::tolower);

		std::map<std::string, std::string>::iterator it = config.overwriteArgs.find(key);

		if(it != config.overwriteArgs.end()){
			writeString(it->second);
		}else{
			writeString(argv[i]);
		}
	}

	writeStringArray(argn, argc);
	writeInt32(argc);
	writeInt32(mode);
	writeHandleInstance(instance);
	writeString(pluginType);
	callFunction(FUNCTION_NPP_NEW);

	NPError result = readResultInt32();

	// The plugin is responsible for freeing *saved
	// The other side has its own copy of this memory
	if(saved){
		sBrowserFuncs->memfree(saved->buf);
		saved->buf = NULL;
		saved->len = 0;
	}

	return result;
}

NPError
NPP_Destroy(NPP instance, NPSavedData** save) {
	EnterFunction();

	if( EventTimerInstance && EventTimerInstance == instance ){
		sBrowserFuncs->unscheduletimer(instance, EventTimerID);
		EventTimerInstance 	= NULL;
		EventTimerID 		= 0;

		std::cerr << "[PIPELIGHT] Unscheduled event timer" << std::endl;
	}

	writeHandleInstance(instance);
	callFunction(FUNCTION_NPP_DESTROY);

	Stack stack;
	readCommands(stack);	

	NPError result 	= readInt32(stack);

	if(result == NPERR_NO_ERROR){
		if(save){
			size_t save_length;
			char* save_data = readMemoryBrowserAlloc(stack, save_length);

			if(save_data && save){
				*save = (NPSavedData*) sBrowserFuncs->memalloc(sizeof(NPSavedData));
				if(*save){

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

	}else if(save){
		*save = NULL; // Nothing to save
	}

	handlemanager.removeHandleByReal((uint64_t)instance, TYPE_NPPInstance);

	if( EventTimerInstance == NULL ){
		NPP nextInstance = handlemanager.findInstance();
		if( nextInstance ){
			EventTimerID 		= sBrowserFuncs->scheduletimer(nextInstance, 5, true, timerFunc);
			EventTimerInstance 	= instance;

			std::cerr << "[PIPELIGHT] Started timer in instance " << (void*)nextInstance << std::endl;

		}else{
			std::cerr << "[PIPELIGHT] No more instance found, timer stays stopped" << std::endl;
		}
	}

	return result;
}

// Verified, everything okay
NPError
NPP_SetWindow(NPP instance, NPWindow* window) {
	EnterFunction();

	// TODO: translate to Screen coordinates
	// TODO: Use all parameters

	writeInt32(window->height);
	writeInt32(window->width);
	writeInt32(window->y);
	writeInt32(window->x);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPP_SET_WINDOW);

	// Embed window if we get a valid window
	Window win = (Window)readResultInt32();

	if(win){

		if(window->window){

			Display *display = XOpenDisplay(NULL);

			if(display){
				XReparentWindow(display, win, (Window)window->window, 0, 0);
				sendXembedMessage(display, win, XEMBED_EMBEDDED_NOTIFY, 0, (Window)window->window, 0);
				XCloseDisplay(display);
			}else{
				std::cerr << "[PIPELIGHT] Could not open Display" << std::endl;
			}
		}
	}
	return NPERR_NO_ERROR;
}

// Verified, everything okay
NPError
NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16_t* stype) {
	EnterFunction();

	writeInt32(seekable);
	writeHandleStream(stream);
	writeString(type);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPP_NEW_STREAM);

	Stack stack;
	readCommands(stack);	

	NPError result 	= readInt32(stack);

	if(result == NPERR_NO_ERROR){
		*stype 			= (uint16_t)readInt32(stack);

	}else{ // Handle is now invalid because of this error
		handlemanager.removeHandleByReal((uint64_t)stream, TYPE_NPStream);

		// We get another request using our notifyData after everything
	}

	return result;
}

// Verified, everything okay
NPError
NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason) {
	EnterFunction();

	writeInt32(reason);
	writeHandleStream(stream, HANDLE_SHOULD_EXIST);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPP_DESTROY_STREAM);

	NPError result = readResultInt32();

	// Remove the handle by the corresponding stream real object
	handlemanager.removeHandleByReal((uint64_t)stream, TYPE_NPStream);

	// We get another request using our notifyData after everything

	return result;
}

// Verified, everything okay
int32_t
NPP_WriteReady(NPP instance, NPStream* stream) {
	EnterFunction();
	
	if( !handlemanager.existsHandleByReal((uint64_t)stream, TYPE_NPStream) ){
		//std::cerr << "[PIPELIGHT] Browser Use-After-Free bug in NPP_WriteReady" << std::endl;
		return -1;
	}

	writeHandleStream(stream, HANDLE_SHOULD_EXIST);
	writeHandleInstance(instance);	
	callFunction(FUNCTION_NPP_WRITE_READY);
	
	int32_t result = readResultInt32();

	return result;
}

// Verified, everything okay
int32_t
NPP_Write(NPP instance, NPStream* stream, int32_t offset, int32_t len, void* buffer) {
	EnterFunction();

	if( !handlemanager.existsHandleByReal((uint64_t)stream, TYPE_NPStream) ){
		//std::cerr << "[PIPELIGHT] Browser Use-After-Free bug in NPP_WriteReady" << std::endl;
		return len;
	}

	writeMemory((char*)buffer, len);
	writeInt32(offset);
	writeHandleStream(stream, HANDLE_SHOULD_EXIST);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPP_WRITE);
	
	return readResultInt32();
}

// Not implemented as it doesnt make sense to pass filenames between both the windows and linux instances
void
NPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname) {
	NotImplemented();
}

// Platform-specific print operation also isn't well defined between two different platforma
void
NPP_Print(NPP instance, NPPrint* platformPrint) {
	NotImplemented();
}

// Delivers platform-specific events.. but again this doesnt make much sense, as long as a translation function is missing
int16_t
NPP_HandleEvent(NPP instance, void* event) {
	NotImplemented();
	return 0;
}

// Verified, everything okay
void
NPP_URLNotify(NPP instance, const char* URL, NPReason reason, void* notifyData) {
	EnterFunction();

	writeHandleNotify(notifyData, HANDLE_SHOULD_EXIST);
	writeInt32(reason);
	writeString(URL);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPP_URL_NOTIFY);
	waitReturn();

	// Free all the notifydata stuff
	NotifyDataRefCount* myNotifyData = (NotifyDataRefCount*)notifyData;
	if(myNotifyData){

		if(myNotifyData->referenceCount == 0){
			throw std::runtime_error("Reference count is zero when calling NPP_URLNotify!");
		}

		// Decrement refcount
		myNotifyData->referenceCount--;

		if(myNotifyData->referenceCount == 0){

			// Free everything
			writeHandleNotify(myNotifyData);
			callFunction(HANDLE_MANAGER_FREE_NOTIFY_DATA);
			waitReturn();

			handlemanager.removeHandleByReal((uint64_t)myNotifyData, TYPE_NotifyData);

			free(myNotifyData);
		}
	}

}

// Verified, everything okay
NPError
NPP_GetValue(NPP instance, NPPVariable variable, void *value) {
	EnterFunction();

	NPError result = NPERR_GENERIC_ERROR;
	std::vector<ParameterInfo> stack;

	switch(variable){

		case NPPVpluginNeedsXEmbed:
			result 				= NPERR_NO_ERROR;
			*((PRBool *)value) 	= PR_TRUE;

			/*writeInt32(variable);
			writeHandleInstance(instance);
			callFunction(FUNCTION_NPP_GETVALUE_BOOL);
			readCommands(stack);

			result = (NPError)readInt32(stack);

			if(result == NPERR_NO_ERROR)
				*((PRBool *)value) = (PRBool)readInt32(stack);*/
			break;

		case NPPVpluginScriptableNPObject:
			writeInt32(variable);
			writeHandleInstance(instance);
			callFunction(FUNCTION_NPP_GETVALUE_OBJECT);
			readCommands(stack);

			result 					= readInt32(stack);

			if(result == NPERR_NO_ERROR)
				*((NPObject**)value) 	= readHandleObj(stack);
			break;


		default:
			NotImplemented();
			result = NPERR_INVALID_PARAM;
			break;
	}

	return result;
}

// As the size of the value depends on the specific variable, this also isn't easy to implement
NPError
NPP_SetValue(NPP instance, NPNVariable variable, void *value) {
	NotImplemented();
	return NPERR_GENERIC_ERROR;
}