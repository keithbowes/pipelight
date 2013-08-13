#include <iostream>

#include "pluginloader.h"

extern char strUserAgent[1024];
extern HandleManager handlemanager;
extern bool isWindowlessMode;

void pokeString(std::string str, char *dest, unsigned int maxLength){
	if(maxLength > 0){
		unsigned int length = std::min((unsigned int)str.length(), maxLength-1);

		// Always at least one byte to copy (nullbyte)
		memcpy(dest, str.c_str(), length);
		dest[length] = 0;
	}
}

NPError NP_LOADDS NPN_GetURL(NPP instance, const char* url, const char* window){
	EnterFunction();

	writeString(window);
	writeString(url);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_GET_URL);

	NPError result = readResultInt32();
	return result;
}

NPError NP_LOADDS NPN_PostURL(NPP instance, const char* url, const char* window, uint32_t len, const char* buf, NPBool file){
	EnterFunction();

	// File upload would require to convert the wine path to a linux path - too complicated as this function isnt used in many plugins
	if(file){
		NotImplemented();
		return NPERR_FILE_NOT_FOUND;
	}

	writeInt32(file);
	writeMemory(buf, len);
	writeString(window);
	writeString(url);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_POST_URL);

	NPError result = readResultInt32();
	return result;
}

NPError NP_LOADDS NPN_RequestRead(NPStream* stream, NPByteRange* rangeList){
	EnterFunction();

	// Count the number of elements in the linked list
	uint32_t rangeCount = 0;

	while(rangeList){
		rangeCount++;

		writeInt32(rangeList->length);
		writeInt32(rangeList->offset);

		rangeList = rangeList->next;
	}

	writeInt32(rangeCount);
	writeHandleStream(stream, HANDLE_SHOULD_EXIST);
	callFunction(FUNCTION_NPN_REQUEST_READ);

	NPError result = readResultInt32();

	return result;
}

NPError NP_LOADDS NPN_NewStream(NPP instance, NPMIMEType type, const char* window, NPStream** stream){
	EnterFunction();

	writeString(window);
	writeString(type);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_NEW_STREAM);

	std::vector<ParameterInfo> stack;
	readCommands(stack);

	NPError result = readInt32(stack);

	if(result == NPERR_NO_ERROR)
		*stream 	= readHandleStream(stack);

	return NPERR_NO_ERROR;		
}

int32_t NP_LOADDS NPN_Write(NPP instance, NPStream* stream, int32_t len, void* buffer){
	EnterFunction();

	writeMemory((char*)buffer, len);
	writeHandleStream(stream, HANDLE_SHOULD_EXIST);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_WRITE);

	NPError result = readResultInt32();
	return result;
}

NPError NP_LOADDS NPN_DestroyStream(NPP instance, NPStream* stream, NPReason reason){
	EnterFunction();

	writeInt32(reason);
	writeHandleStream(stream, HANDLE_SHOULD_EXIST);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_DESTROY_STREAM);

	NPError result = readResultInt32();

	return result;	
}

// Verified, everything okay
void NP_LOADDS NPN_Status(NPP instance, const char* message){
	EnterFunction();

	writeString(message);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_STATUS);
	waitReturn();
}

// Verified, everything okay
const char*  NP_LOADDS NPN_UserAgent(NPP instance){
	EnterFunction();

	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_USERAGENT);

	std::string result = readResultString();

	// TODO: Remove this if it doesnt cause problems
	result = "Mozilla/5.0 (Windows NT 5.1; rv:18.0) Gecko/20100101 Firefox/18.0";

	pokeString(result, strUserAgent, sizeof(strUserAgent));
	return strUserAgent;
}

void* NP_LOADDS NPN_MemAlloc(uint32_t size){
	return malloc(size);
}

void NP_LOADDS NPN_MemFree(void* ptr){
	if (ptr){
		free(ptr);
	}
}

// MacOS only, returns number of freed bytes
uint32_t NP_LOADDS NPN_MemFlush(uint32_t size){
	NotImplemented();
	return 0;
}

// Would allow to force the browser to reload plugins, not really necessary
void NP_LOADDS NPN_ReloadPlugins(NPBool reloadPages){
	NotImplemented();
}

// Java is disabled of course!
void* NP_LOADDS NPN_GetJavaEnv(void){
	NotImplemented();
	return NULL;		
}

// Java is disabled of course!
void* NP_LOADDS NPN_GetJavaPeer(NPP instance){
	NotImplemented();
	return NULL;		
}

// Verified, everything okay
NPError NPN_GetURLNotify(NPP instance, const  char* url, const char* target, void* notifyData){
	EnterFunction();

	writeHandleNotify(notifyData);
	writeString(target);
	writeString(url);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_GET_URL_NOTIFY);

	NPError result = readResultInt32();
	return result;
}

NPError NPN_PostURLNotify(NPP instance, const char* url, const char* target, uint32_t len, const char* buf, NPBool file, void* notifyData){
	EnterFunction();

	// File upload would require to convert the wine path to a linux path - too complicated as this function isnt used in many plugins
	if(file){
		NotImplemented();
		return NPERR_FILE_NOT_FOUND;
	}

	writeHandleNotify(notifyData);
	writeInt32(file);
	writeMemory(buf, len);
	writeString(target);
	writeString(url);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_POST_URL_NOTIFY);

	NPError result = readResultInt32();
	return result;

}

// Verified, everything okay
NPError NPN_GetValue(NPP instance, NPNVariable variable, void *value){
	EnterFunction();

	NPError result = NPERR_GENERIC_ERROR;
	std::vector<ParameterInfo> stack;

	// TODO: Deduplicate this code! One function for bool/obj is enough!

	switch (variable){

		case NPNVPluginElementNPObject:
		case NPNVWindowNPObject:
			writeInt32(variable);
			writeHandleInstance(instance);
			callFunction(FUNCTION_NPN_GETVALUE_OBJECT);
			readCommands(stack);

			result = readInt32(stack);

			if(result == NPERR_NO_ERROR)
				*((NPObject**)value) 	= readHandleObjIncRef(stack);
			
			break;

		case NPNVprivateModeBool:
			writeInt32(variable);
			writeHandleInstance(instance);
			callFunction(FUNCTION_NPN_GETVALUE_BOOL);

			readCommands(stack);

			result = readInt32(stack);

			if(result == NPERR_NO_ERROR)
				*((NPBool*)value) 	= (NPBool)readInt32(stack);

			break;

		case NPNVnetscapeWindow:
			{
				NetscapeData* ndata = (NetscapeData*)instance->ndata;
				if(ndata && ndata->hWnd){
					result = NPERR_NO_ERROR;
					*((HWND*)value) = ndata->hWnd;

				}else{
					result = NPERR_GENERIC_ERROR;
				}
			}
			break;

		default:
			NotImplemented();
			break;
	}

	return result;		
}

// So far we dont allow overwriting these values
NPError NPN_SetValue(NPP instance, NPPVariable variable, void *value){
	NotImplemented();
	return NPERR_NO_ERROR;		
}

void NP_LOADDS NPN_InvalidateRect(NPP instance, NPRect *rect){
	EnterFunction();

	NetscapeData* ndata = (NetscapeData*)instance->ndata;
	if(ndata){
		if(ndata->hWnd){
			if(isWindowlessMode){
				RECT r;
				r.left 		= rect->left;
				r.top 		= rect->top;
				r.right 	= rect->right;
				r.bottom 	= rect->bottom;
				InvalidateRect(ndata->hWnd, &r, false);

			// As far as I have noticed the rect value is incorrect in windowed mode - invalidating the whole region necessary
			// TODO: Completely disable this?
			}else{
				InvalidateRect(ndata->hWnd, NULL, false);

			}
		}
	}
}

void NP_LOADDS NPN_InvalidateRegion(NPP instance, NPRegion region){
	EnterFunction();

	NetscapeData* ndata = (NetscapeData*)instance->ndata;
	if(ndata){
		if(ndata->hWnd){
			InvalidateRgn(ndata->hWnd, region, false);
		}
		//UpdateWindow(hwnd);
	}
}

void NP_LOADDS NPN_ForceRedraw(NPP instance){
	EnterFunction();

	NetscapeData* ndata = (NetscapeData*)instance->ndata;
	if(ndata){
		if(ndata->hWnd){
			UpdateWindow(ndata->hWnd);
		}
	}
}

// Verified, everything okay
NPIdentifier NP_LOADDS NPN_GetStringIdentifier(const NPUTF8* name){
	EnterFunction();

	writeString(name);
	callFunction(FUNCTION_NPN_GET_STRINGIDENTIFIER);

	std::vector<ParameterInfo> stack;
	readCommands(stack);

	return readHandleIdentifier(stack);
}

void NP_LOADDS NPN_GetStringIdentifiers(const NPUTF8** names, int32_t nameCount, NPIdentifier* identifiers){
	EnterFunction();

	// Lazy implementation ;-)
	for(int i = 0; i < nameCount; i++){
		identifiers[i] = NPN_GetStringIdentifier(names[i]);
	}

}

NPIdentifier NP_LOADDS NPN_GetIntIdentifier(int32_t intid){
	EnterFunction();

	writeInt32(intid);
	callFunction(FUNCTION_NPN_GET_INTIDENTIFIER);

	std::vector<ParameterInfo> stack;
	readCommands(stack);

	return readHandleIdentifier(stack);
}

// Verified, everything okay
bool NP_LOADDS NPN_IdentifierIsString(NPIdentifier identifier){
	EnterFunction();

	writeHandleIdentifier(identifier);
	callFunction(FUNCTION_NPN_IDENTIFIER_IS_STRING);

	bool result = (bool)readResultInt32();
	return result;

}

// Verified, everything okay
NPUTF8* NP_LOADDS NPN_UTF8FromIdentifier(NPIdentifier identifier){
	EnterFunction();

	writeHandleIdentifier(identifier);
	callFunction(FUNCTION_NPN_UTF8_FROM_IDENTIFIER);

	std::vector<ParameterInfo> stack;
	readCommands(stack);

	// The plugin is responsible for freeing this with NPN_MemFree() when done
	return readStringMalloc(stack);
}

// Verified, everything okay
int32_t NP_LOADDS NPN_IntFromIdentifier(NPIdentifier identifier){
	EnterFunction();

	writeHandleIdentifier(identifier);
	callFunction(FUNCTION_NPN_INT_FROM_IDENTIFIER);
	
	return readResultInt32();
}

// Verified, everything okay
NPObject* NP_LOADDS NPN_CreateObject(NPP npp, NPClass *aClass){
	EnterFunction();

	// The other side doesnt need to know aClass
	writeHandleInstance(npp);
	callFunction(FUNCTION_NPN_CREATE_OBJECT);

	std::vector<ParameterInfo> stack;
	readCommands(stack);	

	// When we get a object handle back, then allocate a local corresponding object
	// and initialize the refcounter to one before returning it.
	NPObject* result = readHandleObjIncRef(stack, npp, aClass);

	return result;

}

// Verified, everything okay
NPObject* NP_LOADDS NPN_RetainObject(NPObject *obj){
	EnterFunction();

	if (obj){

		if(obj->referenceCount != REFCOUNT_UNDEFINED)
			obj->referenceCount++;

		writeHandleObj(obj, HANDLE_SHOULD_EXIST);
		callFunction(FUNCTION_NPN_RETAINOBJECT);
		waitReturn();	
	}

	return obj;	
}

// Verified, everything okay
void NP_LOADDS NPN_ReleaseObject(NPObject *obj){
	EnterFunction();

	if (obj){	
		writeHandleObjDecRef(obj, HANDLE_SHOULD_EXIST);
		callFunction(FUNCTION_NPN_RELEASEOBJECT);
		waitReturn();
	}
}

// Verified, everything okay
bool NP_LOADDS NPN_Invoke(NPP npp, NPObject* obj, NPIdentifier methodName, const NPVariant *args, uint32_t argCount, NPVariant *result){
	EnterFunction();

	writeVariantArrayConst(args, argCount);
	writeInt32(argCount);
	writeHandleIdentifier(methodName);
	writeHandleObj(obj);
	writeHandleInstance(npp);
	callFunction(FUNCTION_NPN_INVOKE);

	std::vector<ParameterInfo> stack;
	readCommands(stack);

	bool resultBool = readInt32(stack);

	if(resultBool){
		readVariantIncRef(stack, *result); // no incref, as linux is responsible for refcounting!
	}else{
		result->type = NPVariantType_Null;
	}	

	return resultBool;
}

bool NP_LOADDS NPN_InvokeDefault(NPP npp, NPObject* obj, const NPVariant *args, uint32_t argCount, NPVariant *result){ // UNTESTED!
	EnterFunction();

	writeVariantArrayConst(args, argCount);
	writeInt32(argCount);
	writeHandleObj(obj);
	writeHandleInstance(npp);
	callFunction(FUNCTION_NPN_INVOKE_DEFAULT);

	std::vector<ParameterInfo> stack;
	readCommands(stack);

	bool resultBool = readInt32(stack);

	if(resultBool){
		readVariantIncRef(stack, *result); // no incref, as linux is responsible for refcounting!
	}else{
		result->type = NPVariantType_Null;
	}	

	return resultBool;
}

// Verified, everything okay
bool NP_LOADDS NPN_Evaluate(NPP npp, NPObject *obj, NPString *script, NPVariant *result){
	EnterFunction();

	writeNPString(script);
	writeHandleObj(obj);
	writeHandleInstance(npp);
	callFunction(FUNCTION_NPN_EVALUATE);

	std::vector<ParameterInfo> stack;
	readCommands(stack);

	bool resultBool = readInt32(stack);

	if(resultBool){
		readVariantIncRef(stack, *result); // no incref, as linux is responsible for refcounting!
	}else{
		result->type = NPVariantType_Null;
	}	

	return resultBool;
}

// Verified, everything okay
bool NP_LOADDS NPN_GetProperty(NPP npp, NPObject *obj, NPIdentifier propertyName, NPVariant *result){
	EnterFunction();

	writeHandleIdentifier(propertyName);
	writeHandleObj(obj);
	writeHandleInstance(npp);
	callFunction(FUNCTION_NPN_GET_PROPERTY);

	std::vector<ParameterInfo> stack;
	readCommands(stack);

	bool resultBool = readInt32(stack);

	if(resultBool){
		readVariantIncRef(stack, *result); // no incref, as linux is responsible for refcounting!
	}else{
		result->type = NPVariantType_Null;
	}

	return resultBool;

}

bool NP_LOADDS NPN_SetProperty(NPP npp, NPObject *obj, NPIdentifier propertyName, const NPVariant *value){
	EnterFunction();

	writeVariantConst(*value);
	writeHandleIdentifier(propertyName);
	writeHandleObj(obj);
	writeHandleInstance(npp);
	callFunction(FUNCTION_NPN_SET_PROPERTY);

	bool result = (bool)readResultInt32();
	return result;
}

bool NP_LOADDS NPN_RemoveProperty(NPP npp, NPObject *obj, NPIdentifier propertyName){
	EnterFunction();

	writeHandleIdentifier(propertyName);
	writeHandleObj(obj);
	writeHandleInstance(npp);
	callFunction(FUNCTION_NPN_REMOVE_PROPERTY);

	bool result = (bool)readResultInt32();
	return result;
}

bool NP_LOADDS NPN_HasProperty(NPP npp, NPObject *obj, NPIdentifier propertyName){
	EnterFunction();

	writeHandleIdentifier(propertyName);
	writeHandleObj(obj);
	writeHandleInstance(npp);
	callFunction(FUNCTION_NPN_HAS_PROPERTY);

	bool result = (bool)readResultInt32();
	return result;
}

bool NP_LOADDS NPN_HasMethod(NPP npp, NPObject *obj, NPIdentifier propertyName){
	EnterFunction();

	writeHandleIdentifier(propertyName);
	writeHandleObj(obj);
	writeHandleInstance(npp);
	callFunction(FUNCTION_NPN_HAS_METHOD);

	bool result = (bool)readResultInt32();
	return result;
}

// Verified, everything okay
void NP_LOADDS NPN_ReleaseVariantValue(NPVariant *variant){
	EnterFunction();

	switch(variant->type){

		case NPVariantType_String:
			if (variant->value.stringValue.UTF8Characters)
				free((char*)variant->value.stringValue.UTF8Characters);
			break;

		case NPVariantType_Object:
			NPN_ReleaseObject(variant->value.objectValue);
			break;	

		default:
			break;		
	}

	// Ensure that noone is reading that stuff again!
	variant->type = NPVariantType_Null;
}

void NP_LOADDS NPN_SetException(NPObject *obj, const NPUTF8 *message){
	EnterFunction();

	writeString(message);
	writeHandleObj(obj);
	callFunction(FUNCTION_NPN_SET_EXCEPTION);
	waitReturn();

}

// Not documented, doesnt seem to be important
void NP_LOADDS NPN_PushPopupsEnabledState(NPP npp, NPBool enabled){
	NotImplemented();
}

// Not documented, doesnt seem to be important
void NP_LOADDS NPN_PopPopupsEnabledState(NPP npp){
	NotImplemented();
}

bool NP_LOADDS NPN_Enumerate(NPP npp, NPObject *obj, NPIdentifier **identifier, uint32_t *count){
	EnterFunction();

	writeHandleObj(obj);
	writeHandleInstance(npp);
	callFunction(FUNCTION_NPN_ENUMERATE);

	std::vector<ParameterInfo> stack;
	readCommands(stack);

	bool 	 result                         = (bool)readInt32(stack);

	if(!result){
		return false;
	}

	uint32_t identifierCount 				= readInt32(stack);
	std::vector<NPIdentifier> identifiers 	= readIdentifierArray(stack, identifierCount);

	NPIdentifier* identifierTable = (NPIdentifier*)malloc(identifierCount * sizeof(NPIdentifier));
	if(!identifierTable){
		return false;
	}

	memcpy(identifierTable, identifiers.data(), sizeof(NPIdentifier) * identifierCount);

	*identifier = identifierTable;
	*count 		= identifierCount;
	return true;
}

// TODO: Global list with asynchronous calls executed when doing eventhandling?
void NP_LOADDS NPN_PluginThreadAsyncCall(NPP instance, void (*func)(void *), void *userData){
	NotImplemented();
}

// Hopefully not required
bool NP_LOADDS NPN_Construct(NPP npp, NPObject* obj, const NPVariant *args, uint32_t argCount, NPVariant *result){
	NotImplemented();
	return false;
}

NPError NP_LOADDS NPN_GetValueForURL(NPP npp, NPNURLVariable variable, const char *url, char **value, uint32_t *len){
	NotImplemented();
	return NPERR_NO_ERROR;
}

NPError NP_LOADDS NPN_SetValueForURL(NPP npp, NPNURLVariable variable, const char *url, const char *value, uint32_t len){
	NotImplemented();
	return NPERR_NO_ERROR;	
}

// This isn't implemented for security reasons
NPError NPN_GetAuthenticationInfo(NPP npp, const char *protocol, const char *host, int32_t port, const char *scheme, const char *realm, char **username, uint32_t *ulen, char **password, uint32_t *plen){
	NotImplemented();
	return NPERR_NO_ERROR;	
}

uint32_t NP_LOADDS NPN_ScheduleTimer(NPP instance, uint32_t interval, NPBool repeat, void (*timerFunc)(NPP npp, uint32_t timerID)){
	NotImplemented();
	return 0;
}

void NP_LOADDS NPN_UnscheduleTimer(NPP instance, uint32_t timerID){
	NotImplemented();
}

// I hope this one isn't important
NPError NP_LOADDS NPN_PopUpContextMenu(NPP instance, NPMenu* menu){
	NotImplemented();
	return NPERR_NO_ERROR;
}

// I hope this one isn't important
NPBool NP_LOADDS NPN_ConvertPoint(NPP instance, double sourceX, double sourceY, NPCoordinateSpace sourceSpace, double *destX, double *destY, NPCoordinateSpace destSpace){
	NotImplemented();
	return false;
}

// I hope this one isn't important
NPBool NP_LOADDS NPN_HandleEvent(NPP instance, void *event, NPBool handled){
	NotImplemented();
	return false;
}

// I hope this one isn't important
NPBool NP_LOADDS NPN_UnfocusInstance(NPP instance, NPFocusDirection direction){
	NotImplemented();
	return false;
}

// I hope this one isn't important
void NP_LOADDS NPN_URLRedirectResponse(NPP instance, void* notifyData, NPBool allow){
	NotImplemented();
}

// I hope this one isn't important
NPError NP_LOADDS NPN_InitAsyncSurface(NPP instance, NPSize *size, NPImageFormat format, void *initData, NPAsyncSurface *surface){
	NotImplemented();
	return NPERR_NO_ERROR;	
}

// I hope this one isn't important
NPError NP_LOADDS NPN_FinalizeAsyncSurface(NPP instance, NPAsyncSurface *surface){
	NotImplemented();
	return NPERR_NO_ERROR;	
}

// I hope this one isn't important
void NP_LOADDS NPN_SetCurrentAsyncSurface(NPP instance, NPAsyncSurface *surface, NPRect *changed){
	NotImplemented();
}

NPNetscapeFuncs browserFuncs = {
  sizeof(NPNetscapeFuncs),
  (NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR, //must be below 9 when using Quicktime to prevent a crash
  NPN_GetURL,
  NPN_PostURL,
  NPN_RequestRead,
  NPN_NewStream,
  NPN_Write,
  NPN_DestroyStream,
  NPN_Status,
  NPN_UserAgent,
  NPN_MemAlloc,
  NPN_MemFree,
  NPN_MemFlush,
  NPN_ReloadPlugins,
  NPN_GetJavaEnv,
  NPN_GetJavaPeer,
  NPN_GetURLNotify,
  NPN_PostURLNotify,
  NPN_GetValue,
  NPN_SetValue,
  NPN_InvalidateRect,
  NPN_InvalidateRegion,
  NPN_ForceRedraw,
  NPN_GetStringIdentifier,
  NPN_GetStringIdentifiers,
  NPN_GetIntIdentifier,
  NPN_IdentifierIsString,
  NPN_UTF8FromIdentifier,
  NPN_IntFromIdentifier,
  NPN_CreateObject,
  NPN_RetainObject,
  NPN_ReleaseObject,
  NPN_Invoke,
  NPN_InvokeDefault,
  NPN_Evaluate,
  NPN_GetProperty,
  NPN_SetProperty,
  NPN_RemoveProperty,
  NPN_HasProperty,
  NPN_HasMethod,
  NPN_ReleaseVariantValue,
  NPN_SetException,
  NPN_PushPopupsEnabledState,
  NPN_PopPopupsEnabledState,
  NPN_Enumerate,
  NPN_PluginThreadAsyncCall,
  NPN_Construct,
  NPN_GetValueForURL,
  NPN_SetValueForURL,
  NPN_GetAuthenticationInfo,
  NPN_ScheduleTimer,
  NPN_UnscheduleTimer,
  NPN_PopUpContextMenu,
  NPN_ConvertPoint,
  NPN_HandleEvent,
  NPN_UnfocusInstance,
  NPN_URLRedirectResponse,
  NPN_InitAsyncSurface,
  NPN_FinalizeAsyncSurface,
  NPN_SetCurrentAsyncSurface,
};