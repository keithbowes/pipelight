#include "pluginloader.h"

NPError NP_LOADDS NPN_GetURL(NPP instance, const char* url, const char* window){
	output << ">>>>> STUB: NPN_GetURL" << std::endl;
	return NPERR_NO_ERROR;
}

NPError NP_LOADDS NPN_PostURL(NPP instance, const char* url, const char* window, uint32_t len, const char* buf, NPBool file){
	output << ">>>>> STUB: NPN_PostURL" << std::endl;
	return NPERR_NO_ERROR;	
}

NPError NP_LOADDS NPN_RequestRead(NPStream* stream, NPByteRange* rangeList){
	output << ">>>>> STUB: NPN_RequestRead" << std::endl;
	return NPERR_NO_ERROR;	
}

NPError NP_LOADDS NPN_NewStream(NPP instance, NPMIMEType type, const char* window, NPStream** stream){
	output << ">>>>> STUB: NPN_NewStream" << std::endl;
	return NPERR_NO_ERROR;		
}

int32_t NP_LOADDS NPN_Write(NPP instance, NPStream* stream, int32_t len, void* buffer){
	output << ">>>>> STUB: NPN_Write" << std::endl;
	return 0;		
}

NPError NP_LOADDS NPN_DestroyStream(NPP instance, NPStream* stream, NPReason reason){
	output << ">>>>> STUB: NPN_Write" << std::endl;
	return NPERR_NO_ERROR;	
}

void NP_LOADDS NPN_Status(NPP instance, const char* message){
	output << "NPN_Status: " << message <<  std::endl;
	
	writeString(message);
	writeHandle(instance);

	callFunction(FUNCTION_NPN_STATUS);
	waitReturn();
}

const char*  NP_LOADDS NPN_UserAgent(NPP instance){
	output << "NPN_UserAgent" << std::endl;
	return "Dark's virtual Browser";
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
	output << ">>>>> STUB: NPN_MemFlush" << std::endl;
	return 0;
}

void NP_LOADDS NPN_ReloadPlugins(NPBool reloadPages){
	output << ">>>>> STUB: NPN_ReloadPlugins" << std::endl;
}

void* NP_LOADDS NPN_GetJavaEnv(void){
	output << ">>>>> STUB: NPN_GetJavaEnv" << std::endl;
	return NULL;		
}

void* NP_LOADDS NPN_GetJavaPeer(NPP instance){
	output << ">>>>> STUB: NPN_GetJavaPeer" << std::endl;
	return NULL;		
}

NPError NPN_GetURLNotify(NPP instance, const  char* url, const char* target, void* notifyData){

	output << "NPN_GetURLNotify: " << url << std::endl;
	output << "NotifyData: " << notifyData << std::endl;

	writeHandleNotify(notifyData);
	writeString(target);
	writeString(url);
	writeHandle(instance);

	callFunction(FUNCTION_NPN_GET_URL_NOTIFY);

	NPError result = readResultInt32();
	output << "-- Returning: " << result << std::endl;
	return result;
}

NPError NPN_PostURLNotify(NPP instance, const char* url, const char* target, uint32_t len, const char* buf, NPBool file, void* notifyData){
	output << ">>>>> STUB: NPN_PostURLNotify" << std::endl;
	return NPERR_NO_ERROR;	
}

NPError NPN_GetValue(NPP instance, NPNVariable variable, void *value){
	
	output << "NPN_GetValue: " << variable << std::endl;

	NPError returnvalue = NPERR_GENERIC_ERROR;
	std::vector<ParameterInfo> stack;

	switch (variable){

		case NPNVWindowNPObject:

			output << "NPN_GetValue : NPNVWindowNPObject" << std::endl;

			writeHandle(instance);
			callFunction(FUNCTION_NPN_GET_WINDOWNPOBJECT);

			readCommands(stack);

			returnvalue 			= readInt32(stack);
			*((NPObject**)value) 	= readHandleObj(stack, true);
			break;

		case NPNVprivateModeBool:

			output << "NPN_GetValue : NPNVprivateModeBool" << std::endl;

			writeHandle(instance);
			callFunction(FUNCTION_NPN_GET_PRIVATEMODE);

			readCommands(stack);

			returnvalue  		= readInt32(stack);
			*((int32_t*)value) 	= readInt32(stack);
			break;

		case NPNVPluginElementNPObject:

			output << "NPN_GetValue : NPNVPluginElementNPObject" << std::endl;

			writeHandle(instance);
			callFunction(FUNCTION_NPN_GET_PLUGINELEMENTNPOBJECT);

			readCommands(stack);

			returnvalue 			= readInt32(stack);
			*((NPObject**)value) 	= readHandleObj(stack, true);
			break;

		default:
			output << "NPN_GetValue : Unknown" << std::endl;
			break;
	}

	return returnvalue;		
}

NPError NPN_SetValue(NPP instance, NPPVariable variable, void *value){
	output << ">>>>> STUB: NPN_SetValue" << std::endl;
	return NPERR_NO_ERROR;		
}

void NP_LOADDS NPN_InvalidateRect(NPP instance, NPRect *rect){
	//output << "NPN_InvalidateRect" << std::endl;
	InvalidateRect((HWND)instance->ndata, NULL, true);
}

void NP_LOADDS NPN_InvalidateRegion(NPP instance, NPRegion region){
	output << ">>>>> STUB: NPN_InvalidateRegion" << std::endl;
}

void NP_LOADDS NPN_ForceRedraw(NPP instance){
	output << ">>>>> STUB: NPN_ForceRedraw" << std::endl;
}

NPIdentifier NP_LOADDS NPN_GetStringIdentifier(const NPUTF8* name){
	output << "NPN_GetStringIdentifier: " << name << std::endl;
	
	writeString(name);
	callFunction(FUNCTION_NPP_GET_STRINGIDENTIFIER);

	std::vector<ParameterInfo> stack;
	readCommands(stack);	
	return readHandleIdentifier(stack);
}

void NP_LOADDS NPN_GetStringIdentifiers(const NPUTF8** names, int32_t nameCount, NPIdentifier* identifiers){
	output << ">>>>> STUB: NPN_GetStringIdentifiers" << std::endl;
}

NPIdentifier NP_LOADDS NPN_GetIntIdentifier(int32_t intid){
	output << ">>>>> STUB: NPN_GetIntIdentifier" << std::endl;
	return 0;
}

bool NP_LOADDS NPN_IdentifierIsString(NPIdentifier identifier){
	output << "NPN_IdentifierIsString" << std::endl;

	writeHandle(identifier);
	callFunction(FUNCTION_NPN_IDENTIFIER_IS_STRING);
	
	output << "NPN_IdentifierIsString got args" << std::endl;

	bool result = (bool)readResultInt32();

	output << "NPN_IdentifierIsString result:" << result << std::endl;


	return result;

}

NPUTF8* NP_LOADDS NPN_UTF8FromIdentifier(NPIdentifier identifier){
	output << "NPN_UTF8FromIdentifier" << std::endl;

	writeHandle(identifier);
	callFunction(FUNCTION_NPN_UTF8_FROM_IDENTIFIER);

	std::vector<ParameterInfo> stack;
	readCommands(stack);	
	return readStringAsMalloc(stack);
}

int32_t NP_LOADDS NPN_IntFromIdentifier(NPIdentifier identifier){
	output << "NPN_IntFromIdentifier" << std::endl;

	writeHandle(identifier);
	callFunction(FUNCTION_NPN_INT_FROM_IDENTIFIER);
	
	return readResultInt32();
}

NPObject* NP_LOADDS NPN_CreateObject(NPP npp, NPClass *aClass){

	output << "NPN_CreateObject" << std::endl;

	writeHandle(npp);
	callFunction(FUNCTION_NPN_CREATE_OBJECT);

	std::vector<ParameterInfo> stack;
	readCommands(stack);	

	NPObject* result = readHandleObj(stack, false, aClass, npp);

	return result;

}

NPObject* NP_LOADDS NPN_RetainObject(NPObject *obj){

	output << "NPN_RetainObject" << std::endl;
	
	if (obj){
		obj->referenceCount++;
		writeHandle(obj);

		callFunction(FUNCTION_NPN_RETAINOBJECT);
		waitReturn();		
	}

	return obj;	
}

void NP_LOADDS NPN_ReleaseObject(NPObject *obj){

	output << "NPN_ReleaseObject" << std::endl;

	if (obj){
		
		writeHandle(obj);
		callFunction(FUNCTION_NPN_RELEASEOBJECT);
		waitReturn();

		obj->referenceCount--;
	
	}
}

bool NP_LOADDS NPN_Invoke(NPP npp, NPObject* obj, NPIdentifier methodName, const NPVariant *args, uint32_t argCount, NPVariant *result){
	
	output << "NPN_Invoke - Parameter Count: " << argCount  << "result: " << (void*) result<< std::endl;
	
	/* 	Parameter order swapped!
		We need to know the argCount to 
		read the right amount of arguments
		from the stack */

	writeVariantArray(args, argCount);

	writeInt32(argCount);

	writeHandle(methodName);
	writeHandle(obj);
	writeHandle(npp);

	callFunction(FUNCTION_NPN_INVOKE);

	std::vector<ParameterInfo> stack;
	readCommands(stack);

	uint32_t resultBool = readInt32(stack);
	if(resultBool){
		readVariant(stack, *result);
	}else{
		result->type = NPVariantType_Null;
	}	

	output << "NPN_Invoke Result: " << resultBool << std::endl;

	return resultBool;
}

bool NP_LOADDS NPN_InvokeDefault(NPP npp, NPObject* obj, const NPVariant *args, uint32_t argCount, NPVariant *result){	
	output << ">>>>> STUB: NPN_InvokeDefault" << std::endl;
	return false;
}

bool NP_LOADDS NPN_Evaluate(NPP npp, NPObject *obj, NPString *script, NPVariant *result){

	output << "NPN_Evaluate" << std::endl;
	//output << script->UTF8Characters << std::endl;

	writeNPString(script);
	writeHandle(obj);
	writeHandle(npp);

	callFunction(FUNCTION_NPN_Evaluate);

	std::vector<ParameterInfo> stack;
	readCommands(stack);

	uint32_t resultBool = readInt32(stack);
	if(resultBool){
		readVariant(stack, *result, true);
	}	

	output << "NPN_Evaluate resultBool=" << resultBool << std::endl;

	return resultBool;
}

bool NP_LOADDS NPN_GetProperty(NPP npp, NPObject *obj, NPIdentifier propertyName, NPVariant *result){

	output << "NPN_GetProperty" << std::endl;

	writeHandle(propertyName);
	writeHandle(obj);
	writeHandle(npp);

	callFunction(FUNCTION_NPN_GET_PROPERTY);

	std::vector<ParameterInfo> stack;
	readCommands(stack);

	uint32_t resultBool = readInt32(stack);
	if(resultBool){
		readVariant(stack, *result, true);
	}
	output << "NPN_GetProperty Result " <<  resultBool << std::endl;
	return resultBool;

}

bool NP_LOADDS NPN_SetProperty(NPP npp, NPObject *obj, NPIdentifier propertyName, const NPVariant *value){
	output << ">>>>> STUB: NPN_SetProperty" << std::endl;
	return false;
}

bool NP_LOADDS NPN_RemoveProperty(NPP npp, NPObject *obj, NPIdentifier propertyName){
	output << ">>>>> STUB: NPN_RemoveProperty" << std::endl;
	return false;
}

bool NP_LOADDS NPN_HasProperty(NPP npp, NPObject *obj, NPIdentifier propertyName){
	output << ">>>>> STUB: NPN_HasProperty" << std::endl;
	return false;
}

bool NP_LOADDS NPN_HasMethod(NPP npp, NPObject *obj, NPIdentifier propertyName){
	output << ">>>>> STUB: NPN_HasProperty" << std::endl;
	return false;
}

void NP_LOADDS NPN_ReleaseVariantValue(NPVariant *variant){
	output << "NPN_ReleaseVariantValue: " << std::endl;

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

}

void NP_LOADDS NPN_SetException(NPObject *obj, const NPUTF8 *message){
	output << ">>>>> STUB: NPN_SetException" << std::endl;
}

void NP_LOADDS NPN_PushPopupsEnabledState(NPP npp, NPBool enabled){
	output << ">>>>> STUB: NPN_PushPopupsEnabledState" << std::endl;	
}

void NP_LOADDS NPN_PopPopupsEnabledState(NPP npp){
	output << ">>>>> STUB: NPN_PopPopupsEnabledState" << std::endl;	
}

bool NP_LOADDS NPN_Enumerate(NPP npp, NPObject *obj, NPIdentifier **identifier, uint32_t *count){
	output << ">>>>> STUB: NPN_Enumerate" << std::endl;	
	*count = 0;
	return false;
}

void NP_LOADDS NPN_PluginThreadAsyncCall(NPP instance, void (*func)(void *), void *userData){
	output << ">>>>> STUB: NPN_PluginThreadAsyncCall" << std::endl;	
}

bool NP_LOADDS NPN_Construct(NPP npp, NPObject* obj, const NPVariant *args, uint32_t argCount, NPVariant *result){
	output << ">>>>> STUB: NPN_Construct" << std::endl;
	return false;
}

NPError NP_LOADDS NPN_GetValueForURL(NPP npp, NPNURLVariable variable, const char *url, char **value, uint32_t *len){
	output << ">>>>> STUB: NPN_GetValueForURL" << std::endl;
	return NPERR_NO_ERROR;
}

NPError NP_LOADDS NPN_SetValueForURL(NPP npp, NPNURLVariable variable, const char *url, const char *value, uint32_t len){
	output << ">>>>> STUB: NPN_SetValueForURL" << std::endl;
	return NPERR_NO_ERROR;	
}

NPError NPN_GetAuthenticationInfo(NPP npp, const char *protocol, const char *host, int32_t port, const char *scheme, const char *realm, char **username, uint32_t *ulen, char **password, uint32_t *plen){
	output << ">>>>> STUB: NPN_GetAuthenticationInfo" << std::endl;
	return NPERR_NO_ERROR;	
}

uint32_t NP_LOADDS NPN_ScheduleTimer(NPP instance, uint32_t interval, NPBool repeat, void (*timerFunc)(NPP npp, uint32_t timerID)){
	output << ">>>>> STUB: NPN_ScheduleTimer" << std::endl;
	return 0;
}

void NP_LOADDS NPN_UnscheduleTimer(NPP instance, uint32_t timerID){
	output << ">>>>> STUB: NPN_UnscheduleTimer" << std::endl;
}

NPError NP_LOADDS NPN_PopUpContextMenu(NPP instance, NPMenu* menu){
	output << ">>>>> STUB: NPN_PopUpContextMenu" << std::endl;
	return NPERR_NO_ERROR;
}

NPBool NP_LOADDS NPN_ConvertPoint(NPP instance, double sourceX, double sourceY, NPCoordinateSpace sourceSpace, double *destX, double *destY, NPCoordinateSpace destSpace){
	output << ">>>>> STUB: NPN_ConvertPoint" << std::endl;
	return false;
}

NPBool NP_LOADDS NPN_HandleEvent(NPP instance, void *event, NPBool handled){
	output << ">>>>> STUB: NPN_HandleEvent" << std::endl;
	return false;
}

NPBool NP_LOADDS NPN_UnfocusInstance(NPP instance, NPFocusDirection direction){
	output << ">>>>> STUB: NPN_UnfocusInstance" << std::endl;
	return false;
}

void NP_LOADDS NPN_URLRedirectResponse(NPP instance, void* notifyData, NPBool allow){
	output << ">>>>> STUB: NPN_URLRedirectResponse" << std::endl;
}

NPError NP_LOADDS NPN_InitAsyncSurface(NPP instance, NPSize *size, NPImageFormat format, void *initData, NPAsyncSurface *surface){
	output << ">>>>> STUB: NPN_InitAsyncSurface" << std::endl;
	return NPERR_NO_ERROR;	
}

NPError NP_LOADDS NPN_FinalizeAsyncSurface(NPP instance, NPAsyncSurface *surface){
	output << ">>>>> STUB: NPN_InitAsyncSurface" << std::endl;
	return NPERR_NO_ERROR;	
}

void NP_LOADDS NPN_SetCurrentAsyncSurface(NPP instance, NPAsyncSurface *surface, NPRect *changed){
	output << ">>>>> STUB: NPN_SetCurrentAsyncSurface" << std::endl;
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