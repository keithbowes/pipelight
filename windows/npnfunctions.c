#include "pluginloader.h"

char strUserAgent[1024] 			= {0};
extern HandleManager handlemanager;

void pokeString(std::string str, char *dest, unsigned int maxLength){
	if(maxLength > 0){
		unsigned int length = std::min((unsigned int)str.length(), maxLength-1);

		// Always at least one byte to copy (nullbyte)
		memcpy(dest, str.c_str(), length);
		dest[length] = 0;
	}
}

NPError NP_LOADDS NPN_GetURL(NPP instance, const char* url, const char* window){
	debugNotImplemented("NPN_GetURL");
	return NPERR_NO_ERROR;
}

NPError NP_LOADDS NPN_PostURL(NPP instance, const char* url, const char* window, uint32_t len, const char* buf, NPBool file){
	debugNotImplemented("NPN_PostURL");
	return NPERR_NO_ERROR;	
}

NPError NP_LOADDS NPN_RequestRead(NPStream* stream, NPByteRange* rangeList){
	debugNotImplemented("NPN_RequestRead");
	return NPERR_NO_ERROR;	
}

NPError NP_LOADDS NPN_NewStream(NPP instance, NPMIMEType type, const char* window, NPStream** stream){
	debugNotImplemented("NPN_NewStream");
	return NPERR_NO_ERROR;		
}

int32_t NP_LOADDS NPN_Write(NPP instance, NPStream* stream, int32_t len, void* buffer){
	debugEnterFunction("NPN_Write");

	writeMemory((char*)buffer, len);
	writeHandleStream(stream);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_WRITE);

	NPError result = readResultInt32();
	return result;
}

NPError NP_LOADDS NPN_DestroyStream(NPP instance, NPStream* stream, NPReason reason){
	debugEnterFunction("NPN_DestroyStream");

	writeInt32(reason);
	writeHandleStream(stream);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_DESTROY_STREAM);

	NPError result = readResultInt32();

	// Free Data
	// TODO: Is this required?
	/*if(stream){
		if(stream->url) free((char*)stream->url);
		if(stream->headers) free((char*)stream->headers);
		free(stream);
	}

	handlemanager.removeHandleByReal((uint64_t)stream, TYPE_NPStream);
	*/

	return result;	
}

// Verified, everything okay
void NP_LOADDS NPN_Status(NPP instance, const char* message){
	debugEnterFunction("NPN_Status");

	writeString(message);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_STATUS);
	waitReturn();
}

// Verified, everything okay
const char*  NP_LOADDS NPN_UserAgent(NPP instance){
	debugEnterFunction("NPN_UserAgent");

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

uint32_t NP_LOADDS NPN_MemFlush(uint32_t size){
	debugNotImplemented("NPN_MemFlush");
	return 0;
}

void NP_LOADDS NPN_ReloadPlugins(NPBool reloadPages){
	debugNotImplemented("NPN_ReloadPlugins");
}

void* NP_LOADDS NPN_GetJavaEnv(void){
	debugNotImplemented("NPN_GetJavaEnv");
	return NULL;		
}

void* NP_LOADDS NPN_GetJavaPeer(NPP instance){
	debugNotImplemented("NPN_GetJavaPeer");
	return NULL;		
}

// Verified, everything okay
NPError NPN_GetURLNotify(NPP instance, const  char* url, const char* target, void* notifyData){
	debugEnterFunction("NPN_GetURLNotify");

	writeHandleNotify(notifyData);
	writeString(target);
	writeString(url);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_GET_URL_NOTIFY);

	NPError result = readResultInt32();
	return result;
}

NPError NPN_PostURLNotify(NPP instance, const char* url, const char* target, uint32_t len, const char* buf, NPBool file, void* notifyData){
	debugEnterFunction("NPN_PostURLNotify");

	// File upload would require to convert the wine path to a linux path - too complicated as this function isnt used in many plugins
	if(file){
		debugNotImplemented("NPN_PostURLNotify - file upload not supported yet");
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
	debugEnterFunction("NPN_GetValue");

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

		default:
			debugNotImplemented("NPN_GetValue (several variables)");
			break;
	}

	return result;		
}

NPError NPN_SetValue(NPP instance, NPPVariable variable, void *value){
	debugNotImplemented("NPN_SetValue");
	return NPERR_NO_ERROR;		
}

void NP_LOADDS NPN_InvalidateRect(NPP instance, NPRect *rect){
	debugEnterFunction("NPN_InvalidateRect");

	HWND hwnd = (HWND)instance->ndata;
	if(hwnd){
		InvalidateRect(hwnd, NULL, true);
	}
}

void NP_LOADDS NPN_InvalidateRegion(NPP instance, NPRegion region){
	debugNotImplemented("NPN_InvalidateRegion");
}

void NP_LOADDS NPN_ForceRedraw(NPP instance){
	debugNotImplemented("NPN_ForceRedraw");
}

// Verified, everything okay
NPIdentifier NP_LOADDS NPN_GetStringIdentifier(const NPUTF8* name){
	debugEnterFunction("NPN_GetStringIdentifier");

	writeString(name);
	callFunction(FUNCTION_NPP_GET_STRINGIDENTIFIER);

	std::vector<ParameterInfo> stack;
	readCommands(stack);

	return readHandleIdentifier(stack);
}

void NP_LOADDS NPN_GetStringIdentifiers(const NPUTF8** names, int32_t nameCount, NPIdentifier* identifiers){
	debugNotImplemented("NPN_GetStringIdentifiers");
}

NPIdentifier NP_LOADDS NPN_GetIntIdentifier(int32_t intid){
	debugNotImplemented("NPN_GetIntIdentifier");
	return 0;
}

// Verified, everything okay
bool NP_LOADDS NPN_IdentifierIsString(NPIdentifier identifier){
	debugEnterFunction("NPN_IdentifierIsString");

	writeHandleIdentifier(identifier);
	callFunction(FUNCTION_NPN_IDENTIFIER_IS_STRING);

	bool result = (bool)readResultInt32();
	return result;

}

// Verified, everything okay
NPUTF8* NP_LOADDS NPN_UTF8FromIdentifier(NPIdentifier identifier){
	debugEnterFunction("NPN_UTF8FromIdentifier");

	writeHandleIdentifier(identifier);
	callFunction(FUNCTION_NPN_UTF8_FROM_IDENTIFIER);

	std::vector<ParameterInfo> stack;
	readCommands(stack);

	// The plugin is responsible for freeing this with NPN_MemFree() when done
	return readStringMalloc(stack);
}

// Verified, everything okay
int32_t NP_LOADDS NPN_IntFromIdentifier(NPIdentifier identifier){
	debugEnterFunction("NPN_IntFromIdentifier");

	writeHandleIdentifier(identifier);
	callFunction(FUNCTION_NPN_INT_FROM_IDENTIFIER);
	
	return readResultInt32();
}

// Verified, everything okay
NPObject* NP_LOADDS NPN_CreateObject(NPP npp, NPClass *aClass){
	debugEnterFunction("NPN_CreateObject");

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
	debugEnterFunction("NPN_RetainObject");

	if (obj){

		if(obj->referenceCount != REFCOUNT_UNDEFINED)
			obj->referenceCount++;

		writeHandleObj(obj, true);
		callFunction(FUNCTION_NPN_RETAINOBJECT);
		waitReturn();	
	}

	return obj;	
}

// Verified, everything okay
void NP_LOADDS NPN_ReleaseObject(NPObject *obj){
	debugEnterFunction("NPN_ReleaseObject");

	if (obj){
		if(obj->referenceCount == 0) throw std::runtime_error("Reference count is zero when calling ReleaseObject!");

		if(obj->referenceCount != REFCOUNT_UNDEFINED)
			obj->referenceCount--;

		writeInt32( (obj->referenceCount == 0) );
		writeHandleObj(obj, true);
		callFunction(FUNCTION_NPN_RELEASEOBJECT);
		waitReturn();

		// Can never occur for user-created objects
		// For such objects the other side calls KILL_OBJECT
		if(obj->referenceCount == 0){

			// Remove the object locally
			if(obj->_class->deallocate){
				output << "call deallocate function " << (void*)obj->_class->deallocate << std::endl;

				obj->_class->deallocate(obj);
			}else{
				output << "call default dealloc function " << std::endl;

				free((char*)obj);
			}

			output << "removeHandleByReal (NPN_ReleaseObject): " << (uint64_t)obj << " or " << (void*)obj << std::endl;

			// Remove it in the handle manager
			handlemanager.removeHandleByReal((uint64_t)obj, TYPE_NPObject);

		}

	}
}

// Verified, everything okay
bool NP_LOADDS NPN_Invoke(NPP npp, NPObject* obj, NPIdentifier methodName, const NPVariant *args, uint32_t argCount, NPVariant *result){
	debugEnterFunction("NPN_Invoke");

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

bool NP_LOADDS NPN_InvokeDefault(NPP npp, NPObject* obj, const NPVariant *args, uint32_t argCount, NPVariant *result){
	debugNotImplemented("NPN_InvokeDefault");
	return false;
}

// Verified, everything okay
bool NP_LOADDS NPN_Evaluate(NPP npp, NPObject *obj, NPString *script, NPVariant *result){
	debugEnterFunction("NPN_Evaluate");

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
	debugEnterFunction("NPN_GetProperty");

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
	debugNotImplemented("NPN_SetProperty");
	return false;
}

bool NP_LOADDS NPN_RemoveProperty(NPP npp, NPObject *obj, NPIdentifier propertyName){
	debugNotImplemented("NPN_RemoveProperty");
	return false;
}

bool NP_LOADDS NPN_HasProperty(NPP npp, NPObject *obj, NPIdentifier propertyName){
	debugNotImplemented("NPN_HasProperty");
	return false;
}

bool NP_LOADDS NPN_HasMethod(NPP npp, NPObject *obj, NPIdentifier propertyName){
	debugNotImplemented("NPN_HasMethod");
	return false;
}

// Verified, everything okay
void NP_LOADDS NPN_ReleaseVariantValue(NPVariant *variant){
	debugEnterFunction("NPN_ReleaseVariantValue");

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
	debugNotImplemented("NPN_SetException");
}

void NP_LOADDS NPN_PushPopupsEnabledState(NPP npp, NPBool enabled){
	debugNotImplemented("NPN_PushPopupsEnabledState");	
}

void NP_LOADDS NPN_PopPopupsEnabledState(NPP npp){
	debugNotImplemented("NPN_PopPopupsEnabledState");
}

bool NP_LOADDS NPN_Enumerate(NPP npp, NPObject *obj, NPIdentifier **identifier, uint32_t *count){
	debugNotImplemented("NPN_Enumerate");
	*count = 0;
	return false;
}

void NP_LOADDS NPN_PluginThreadAsyncCall(NPP instance, void (*func)(void *), void *userData){
	debugNotImplemented("NPN_PluginThreadAsyncCall");
}

bool NP_LOADDS NPN_Construct(NPP npp, NPObject* obj, const NPVariant *args, uint32_t argCount, NPVariant *result){
	debugNotImplemented("NPN_Construct");
	return false;
}

NPError NP_LOADDS NPN_GetValueForURL(NPP npp, NPNURLVariable variable, const char *url, char **value, uint32_t *len){
	debugNotImplemented("NPN_GetValueForURL");
	return NPERR_NO_ERROR;
}

NPError NP_LOADDS NPN_SetValueForURL(NPP npp, NPNURLVariable variable, const char *url, const char *value, uint32_t len){
	debugNotImplemented("NPN_SetValueForURL");
	return NPERR_NO_ERROR;	
}

NPError NPN_GetAuthenticationInfo(NPP npp, const char *protocol, const char *host, int32_t port, const char *scheme, const char *realm, char **username, uint32_t *ulen, char **password, uint32_t *plen){
	debugNotImplemented("NPN_GetAuthenticationInfo");
	return NPERR_NO_ERROR;	
}

uint32_t NP_LOADDS NPN_ScheduleTimer(NPP instance, uint32_t interval, NPBool repeat, void (*timerFunc)(NPP npp, uint32_t timerID)){
	debugNotImplemented("NPN_ScheduleTimer");
	return 0;
}

void NP_LOADDS NPN_UnscheduleTimer(NPP instance, uint32_t timerID){
	debugNotImplemented("NPN_UnscheduleTimer");
}

NPError NP_LOADDS NPN_PopUpContextMenu(NPP instance, NPMenu* menu){
	debugNotImplemented("NPN_PopUpContextMenu");
	return NPERR_NO_ERROR;
}

NPBool NP_LOADDS NPN_ConvertPoint(NPP instance, double sourceX, double sourceY, NPCoordinateSpace sourceSpace, double *destX, double *destY, NPCoordinateSpace destSpace){
	debugNotImplemented("NPN_ConvertPoint");
	return false;
}

NPBool NP_LOADDS NPN_HandleEvent(NPP instance, void *event, NPBool handled){
	debugNotImplemented("NPN_HandleEvent");
	return false;
}

NPBool NP_LOADDS NPN_UnfocusInstance(NPP instance, NPFocusDirection direction){
	debugNotImplemented("NPN_UnfocusInstance");
	return false;
}

void NP_LOADDS NPN_URLRedirectResponse(NPP instance, void* notifyData, NPBool allow){
	debugNotImplemented("NPN_URLRedirectResponse");
}

NPError NP_LOADDS NPN_InitAsyncSurface(NPP instance, NPSize *size, NPImageFormat format, void *initData, NPAsyncSurface *surface){
	debugNotImplemented("NPN_InitAsyncSurface");
	return NPERR_NO_ERROR;	
}

NPError NP_LOADDS NPN_FinalizeAsyncSurface(NPP instance, NPAsyncSurface *surface){
	debugNotImplemented("NPN_FinalizeAsyncSurface");
	return NPERR_NO_ERROR;	
}

void NP_LOADDS NPN_SetCurrentAsyncSurface(NPP instance, NPAsyncSurface *surface, NPRect *changed){
	debugNotImplemented("NPN_SetCurrentAsyncSurface");
}

NPNetscapeFuncs browserFuncs = {
  sizeof(NPNetscapeFuncs),
  NP_VERSION_MINOR,
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