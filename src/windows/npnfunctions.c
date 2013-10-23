/*
#include "pluginloader.h"

extern HandleManager handlemanager;

extern NPPluginFuncs pluginFuncs;
*/

#include "../common/common.h"
#include "pluginloader.h"

#include <windows.h>

// Shockwave sometimes calls the function with a wrong instance? Is this a wine bug?
NPP shockwaveInstanceBug = NULL;

#define shockwaveInstanceWorkaround() \
	do{ \
		if (shockwaveInstanceBug && instance == shockwaveInstanceBug){ \
			instance = handleManager_findInstance(); \
			DBG_TRACE("Replaced wrong instance %p with %p", shockwaveInstanceBug, instance); \
		} \
	}while(0)

NPError NP_LOADDS NPN_GetURL(NPP instance, const char* url, const char* window){
	DBG_TRACE("( instance=%p, url='%s', window='%s' )", instance, url, window);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	writeString(window);
	writeString(url);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_GET_URL);

	NPError result = readResultInt32();
	return result;
}

NPError NP_LOADDS NPN_PostURL(NPP instance, const char* url, const char* window, uint32_t len, const char* buf, NPBool file){
	DBG_TRACE("( instance=%p, url='%s', window='%s', len=%d, buf=%p, file=%d )", instance, url, window, len, buf, file);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	// File upload would require to convert the wine path to a linux path - too complicated as this function isnt used in many plugins
	if (file){
		NOTIMPLEMENTED("file argument not supported.");
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
	DBG_TRACE("( stream=%p, rangeList=%p )", stream, rangeList);
	DBG_CHECKTHREAD();

	// Count the number of elements in the linked list
	uint32_t rangeCount = 0;

	while (rangeList){
		rangeCount++;

		writeInt32(rangeList->length);
		writeInt32(rangeList->offset);

		rangeList = rangeList->next;
	}

	writeInt32(rangeCount);
	writeHandleStream(stream, HMGR_SHOULD_EXIST);
	callFunction(FUNCTION_NPN_REQUEST_READ);

	NPError result = readResultInt32();
	return result;
}

NPError NP_LOADDS NPN_NewStream(NPP instance, NPMIMEType type, const char* window, NPStream** stream){
	DBG_TRACE("( instance=%p, type='%s', window='%s', stream=%p )", instance, type, window, stream);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	writeString(window);
	writeString(type);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_NEW_STREAM);

	std::vector<ParameterInfo> stack;
	readCommands(stack);

	NPError result = readInt32(stack);

	if (result == NPERR_NO_ERROR)
		*stream 	= readHandleStream(stack);

	return NPERR_NO_ERROR;		
}

int32_t NP_LOADDS NPN_Write(NPP instance, NPStream* stream, int32_t len, void* buffer){
	DBG_TRACE("( instance=%p, stream=%p, len=%d, buffer=%p )", instance, stream, len, buffer);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	writeMemory((char*)buffer, len);
	writeHandleStream(stream, HMGR_SHOULD_EXIST);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_WRITE);

	NPError result = readResultInt32();
	return result;
}

NPError NP_LOADDS NPN_DestroyStream(NPP instance, NPStream* stream, NPReason reason){
	DBG_TRACE("( instance=%p, stream=%p, reason=%d )", instance, stream, reason);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	writeInt32(reason);
	writeHandleStream(stream, HMGR_SHOULD_EXIST);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_DESTROY_STREAM);

	NPError result = readResultInt32();
	return result;	
}

// Verified, everything okay
void NP_LOADDS NPN_Status(NPP instance, const char* message){
	DBG_TRACE("( instance=%p, message='%s' )", instance, message);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	writeString(message);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_STATUS);
	readResultVoid();
}

// Verified, everything okay
const char*  NP_LOADDS NPN_UserAgent(NPP instance){
	DBG_TRACE("( instance=%p )", instance);
	DBG_CHECKTHREAD();

	/*
		The following code is not used currently since we set a hardcoded user agent.
		Some plugins like Flash pass a NULL handle as instance which would cause our
		internal error checking mechanism to think something wents wrong and terminate
		our process, so just keep it commented out till we allow NULL instances.

		writeHandleInstance(instance);
		callFunction(FUNCTION_NPN_USERAGENT);

		std::string result = readResultString();
	*/

	if (instance && !handleManager_existsByPtr(HMGR_TYPE_NPPInstance, instance)){
		DBG_ERROR("Shockwave player wrong instance bug - called with unknown instance %p.", instance);
		shockwaveInstanceBug = instance;
	}

	std::string result = "Mozilla/5.0 (Windows NT 5.1; rv:18.0) Gecko/20100101 Firefox/18.0";

	pokeString(strUserAgent, result, sizeof(strUserAgent));
	return strUserAgent;
}

void* NP_LOADDS NPN_MemAlloc(uint32_t size){
	DBG_TRACE("( size=%d )", size);

	return malloc(size);
}

void NP_LOADDS NPN_MemFree(void* ptr){
	DBG_TRACE("( ptr=%p )", ptr);

	if (ptr)
		free(ptr);
}

// MacOS only, returns number of freed bytes
uint32_t NP_LOADDS NPN_MemFlush(uint32_t size){
	DBG_TRACE("( size=%d )", size);
	NOTIMPLEMENTED();
	return 0;
}

// Would allow to force the browser to reload plugins, not really necessary
void NP_LOADDS NPN_ReloadPlugins(NPBool reloadPages){
	DBG_TRACE("( reloadPages=%d )", reloadPages);
	NOTIMPLEMENTED();
}

// Java is disabled of course!
void* NP_LOADDS NPN_GetJavaEnv(void){
	DBG_TRACE("()");
	NOTIMPLEMENTED();
	return NULL;		
}

// Java is disabled of course!
void* NP_LOADDS NPN_GetJavaPeer(NPP instance){
	DBG_TRACE("( instance=%p )", instance);
	NOTIMPLEMENTED();
	return NULL;
}

// Verified, everything okay
NPError NP_LOADDS NPN_GetURLNotify(NPP instance, const  char* url, const char* target, void* notifyData){
	DBG_TRACE("( instance=%p, url='%s', target='%s', notifyData=%p )", instance, url, target, notifyData);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	writeHandleNotify(notifyData);
	writeString(target);
	writeString(url);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_GET_URL_NOTIFY);

	NPError result = readResultInt32();
	return result;
}

NPError NP_LOADDS NPN_PostURLNotify(NPP instance, const char* url, const char* target, uint32_t len, const char* buf, NPBool file, void* notifyData){
	DBG_TRACE("( instance=%p, url='%s', target='%s', len=%d, buf=%p, file=%d, notifyData=%p )", instance, url, target, len, buf, file, notifyData);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	// File upload would require to convert the wine path to a linux path - too complicated as this function isnt used in many plugins
	if (file){
		NOTIMPLEMENTED("file argument not supported.");
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

NPError NP_LOADDS NPN_GetValue(NPP instance, NPNVariable variable, void *value){
	DBG_TRACE("( instance=%p, variable=%d, value=%p )", instance, variable, value);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	NPError result = NPERR_GENERIC_ERROR;
	std::vector<ParameterInfo> stack;

	switch (variable){

		case NPNVPluginElementNPObject:
		case NPNVWindowNPObject:
			writeInt32(variable);
			writeHandleInstance(instance);
			callFunction(FUNCTION_NPN_GETVALUE_OBJECT);
			readCommands(stack);

			result = readInt32(stack);

			if (result == NPERR_NO_ERROR)
				*((NPObject**)value) 	= readHandleObjIncRef(stack);

			break;

		case NPNVprivateModeBool:
			writeInt32(variable);
			writeHandleInstance(instance);
			callFunction(FUNCTION_NPN_GETVALUE_BOOL);
			readCommands(stack);

			result = readInt32(stack);

			if (result == NPERR_NO_ERROR)
				*((NPBool*)value) 	= (NPBool)readInt32(stack);

			break;

		case NPNVdocumentOrigin:
			writeInt32(variable);
			writeHandleInstance(instance);
			callFunction(FUNCTION_NPN_GETVALUE_STRING);
			readCommands(stack);

			result = readInt32(stack);

			if (result == NPERR_NO_ERROR)
				*((char**)value) 	= readStringMalloc(stack);

			break;

		case NPNVnetscapeWindow:
			{
				NetscapeData* ndata = (NetscapeData*)instance->ndata;
				if (ndata && ndata->hWnd){
					result = NPERR_NO_ERROR;
					*((HWND*)value) = ndata->hWnd;

				}else{
					result = NPERR_GENERIC_ERROR;
				}
			}
			break;

		case NPNVSupportsWindowless:
			result = NPERR_NO_ERROR;
			*((NPBool*)value) = true;
			break;

		default:
			NOTIMPLEMENTED("( variable=%d )", variable);
			break;
	}

	return result;		
}

NPError NP_LOADDS NPN_SetValue(NPP instance, NPPVariable variable, void *value){
	DBG_TRACE("( instance=%p, variable=%d, value=%p )", instance, variable, value);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	NPError result = NPERR_GENERIC_ERROR;

	switch (variable){

		case NPPVpluginWindowBool:
			{
				NetscapeData* ndata = (NetscapeData*)instance->ndata;
				if (ndata){

					// Update windowless mode
					ndata->windowlessMode 	= ( value == NULL );
					result 					= NPERR_NO_ERROR;

					DBG_INFO("plugin instance switched windowless mode to %s.", (ndata->windowlessMode ? "on" : "off"));

					// Update existing plugin window
					if (ndata->hWnd && ndata->window){
						NPWindow* window = ndata->window;

						if (window->type == NPWindowTypeDrawable)
							ReleaseDC(ndata->hWnd, (HDC)ndata->window->window);

						if (ndata->windowlessMode){
							window->window 			= GetDC(ndata->hWnd);
							window->type 			= NPWindowTypeDrawable;
						}else{
							window->window 			= ndata->hWnd;
							window->type 			= NPWindowTypeWindow;
						}

						pluginFuncs.setwindow(instance, window);
					}
				}
			}
			break;

		default:
			NOTIMPLEMENTED("( variable=%d )", variable);
			break;

	}

	return result;		
}

void NP_LOADDS NPN_InvalidateRect(NPP instance, NPRect *rect){
	DBG_TRACE("( instance=%p, rect=%p )", instance, rect);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	NetscapeData* ndata = (NetscapeData*)instance->ndata;
	if (ndata){
		if (ndata->hWnd){
			if (ndata->windowlessMode){
				RECT r;
				r.left 		= rect->left;
				r.top 		= rect->top;
				r.right 	= rect->right;
				r.bottom 	= rect->bottom;
				InvalidateRect(ndata->hWnd, &r, false);

			// As far as I have noticed the rect value is incorrect in windowed mode - invalidating the whole region necessary
			}else{
				InvalidateRect(ndata->hWnd, NULL, false);

			}
		}
	}
}

void NP_LOADDS NPN_InvalidateRegion(NPP instance, NPRegion region){
	DBG_TRACE("( instance=%p, region=%p )", instance, region);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	NetscapeData* ndata = (NetscapeData*)instance->ndata;
	if (ndata){
		if (ndata->hWnd){
			InvalidateRgn(ndata->hWnd, region, false);
		}
	}
}

void NP_LOADDS NPN_ForceRedraw(NPP instance){
	DBG_TRACE("( instance=%p )", instance);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	NetscapeData* ndata = (NetscapeData*)instance->ndata;
	if (ndata){
		if (ndata->hWnd){
			UpdateWindow(ndata->hWnd);
		}
	}
}

NPIdentifier NP_LOADDS NPN_GetStringIdentifier(const NPUTF8* name){
	DBG_TRACE("( name='%s' )", name);
	DBG_CHECKTHREAD();

	writeString(name);
	callFunction(FUNCTION_NPN_GET_STRINGIDENTIFIER);

	std::vector<ParameterInfo> stack;
	readCommands(stack);

	return readHandleIdentifier(stack);
}

void NP_LOADDS NPN_GetStringIdentifiers(const NPUTF8** names, int32_t nameCount, NPIdentifier* identifiers){
	DBG_TRACE("( names=%p, nameCount=%d, identifier=%p )", names, nameCount, identifiers);
	DBG_CHECKTHREAD();

	// Lazy implementation ;-)
	for (int i = 0; i < nameCount; i++){
		identifiers[i] = names[i] ? NPN_GetStringIdentifier(names[i]) : NULL;
	}
}

NPIdentifier NP_LOADDS NPN_GetIntIdentifier(int32_t intid){
	DBG_TRACE("( intid=%d )", intid);
	DBG_CHECKTHREAD();

	writeInt32(intid);
	callFunction(FUNCTION_NPN_GET_INTIDENTIFIER);

	std::vector<ParameterInfo> stack;
	readCommands(stack);

	return readHandleIdentifier(stack);
}

bool NP_LOADDS NPN_IdentifierIsString(NPIdentifier identifier){
	DBG_TRACE("( identifier=%p )", identifier);
	DBG_CHECKTHREAD();

	writeHandleIdentifier(identifier);
	callFunction(FUNCTION_NPN_IDENTIFIER_IS_STRING);

	bool result = (bool)readResultInt32();
	return result;

}

NPUTF8* NP_LOADDS NPN_UTF8FromIdentifier(NPIdentifier identifier){
	DBG_TRACE("( identifier=%p )", identifier);
	DBG_CHECKTHREAD();

	writeHandleIdentifier(identifier);
	callFunction(FUNCTION_NPN_UTF8_FROM_IDENTIFIER);

	std::vector<ParameterInfo> stack;
	readCommands(stack);

	// The plugin is responsible for freeing this with NPN_MemFree() when done
	return readStringMalloc(stack);
}

int32_t NP_LOADDS NPN_IntFromIdentifier(NPIdentifier identifier){
	DBG_TRACE("( identifier=%p )", identifier);
	DBG_CHECKTHREAD();

	writeHandleIdentifier(identifier);
	callFunction(FUNCTION_NPN_INT_FROM_IDENTIFIER);
	
	return readResultInt32();
}

NPObject* NP_LOADDS NPN_CreateObject(NPP instance, NPClass *aClass){
	DBG_TRACE("( instance=%p, aClass=%p )", instance, aClass);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	// The other side doesnt need to know aClass
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_CREATE_OBJECT);

	std::vector<ParameterInfo> stack;
	readCommands(stack);	

	// When we get a object handle back, then allocate a local corresponding object
	// and initialize the refcounter to 1 before returning it.
	NPObject* result = readHandleObjIncRef(stack, instance, aClass);

	return result;
}

NPObject* NP_LOADDS NPN_RetainObject(NPObject *obj){
	DBG_TRACE("( obj=%p )", obj);
	DBG_CHECKTHREAD();

	if (obj){

		if(obj->referenceCount != REFCOUNT_UNDEFINED)
			obj->referenceCount++;

		// Required to check if the reference counting is still appropriate
		// (only used when DEBUG_LOG_HANDLES is on)
		writeInt32(obj->referenceCount);

		writeHandleObj(obj, HMGR_SHOULD_EXIST);
		callFunction(FUNCTION_NPN_RETAINOBJECT);
		readResultVoid();	
	}

	return obj;	
}

void NP_LOADDS NPN_ReleaseObject(NPObject *obj){
	DBG_TRACE("( obj=%p )", obj);
	DBG_CHECKTHREAD();

	if (obj){

		writeHandleObjDecRef(obj, HMGR_SHOULD_EXIST);
		callFunction(FUNCTION_NPN_RELEASEOBJECT);
		readResultVoid();
	}
}

bool NP_LOADDS NPN_Invoke(NPP instance, NPObject* obj, NPIdentifier methodName, const NPVariant *args, uint32_t argCount, NPVariant *result){
	DBG_TRACE("( instance=%p, obj=%p, methodName=%p, args=%p, argCount=%d, result=%p )", instance, obj, methodName, args, argCount, result);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	writeVariantArrayConst(args, argCount);
	writeInt32(argCount);
	writeHandleIdentifier(methodName);
	writeHandleObj(obj);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_INVOKE);

	std::vector<ParameterInfo> stack;
	readCommands(stack);

	bool resultBool = readInt32(stack);

	if (resultBool){
		readVariantIncRef(stack, *result); // Refcount already incremented by invoke()
	}else{
		result->type 				= NPVariantType_Void;
		result->value.objectValue 	= NULL;
	}	

	return resultBool;
}

bool NP_LOADDS NPN_InvokeDefault(NPP instance, NPObject* obj, const NPVariant *args, uint32_t argCount, NPVariant *result){ // UNTESTED!
	DBG_TRACE("( instance=%p, obj=%p, args=%p, argCount=%d, result=%p )", instance, obj, args, argCount, result);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	writeVariantArrayConst(args, argCount);
	writeInt32(argCount);
	writeHandleObj(obj);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_INVOKE_DEFAULT);

	std::vector<ParameterInfo> stack;
	readCommands(stack);

	bool resultBool = readInt32(stack);

	if (resultBool){
		readVariantIncRef(stack, *result); // Refcount already incremented by invoke()
	}else{
		result->type 				= NPVariantType_Void;
		result->value.objectValue 	= NULL;
	}	

	return resultBool;
}

bool NP_LOADDS NPN_Evaluate(NPP instance, NPObject *obj, NPString *script, NPVariant *result){
	DBG_TRACE("( instance=%p, obj=%p, script=%p, result=%p )", instance, obj, script, result);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	writeNPString(script);
	writeHandleObj(obj);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_EVALUATE);

	std::vector<ParameterInfo> stack;
	readCommands(stack);

	bool resultBool = readInt32(stack);

	if (resultBool){
		readVariantIncRef(stack, *result); // Refcount already incremented by evaluate()
	}else{
		result->type 				= NPVariantType_Void;
		result->value.objectValue 	= NULL;
	}	

	return resultBool;
}

bool NP_LOADDS NPN_GetProperty(NPP instance, NPObject *obj, NPIdentifier propertyName, NPVariant *result){
	DBG_TRACE("( instance=%p, obj=%p, propertyName=%p, result=%p )", instance, obj, propertyName, result);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	writeHandleIdentifier(propertyName);
	writeHandleObj(obj);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_GET_PROPERTY);

	std::vector<ParameterInfo> stack;
	readCommands(stack);

	bool resultBool = readInt32(stack);

	if (resultBool){
		readVariantIncRef(stack, *result); // Refcount already incremented by getProperty()
	}else{
		result->type 				= NPVariantType_Void;
		result->value.objectValue 	= NULL;
	}

	return resultBool;

}

bool NP_LOADDS NPN_SetProperty(NPP instance, NPObject *obj, NPIdentifier propertyName, const NPVariant *value){
	DBG_TRACE("( instance=%p, obj=%p, propertyName=%p, value=%p )", instance, obj, propertyName, value);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	writeVariantConst(*value);
	writeHandleIdentifier(propertyName);
	writeHandleObj(obj);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_SET_PROPERTY);

	bool result = (bool)readResultInt32();
	return result;
}

bool NP_LOADDS NPN_RemoveProperty(NPP instance, NPObject *obj, NPIdentifier propertyName){
	DBG_TRACE("( instance=%p, obj=%p, propertyName=%p )", instance, obj, propertyName);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	writeHandleIdentifier(propertyName);
	writeHandleObj(obj);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_REMOVE_PROPERTY);

	bool result = (bool)readResultInt32();
	return result;
}

bool NP_LOADDS NPN_HasProperty(NPP instance, NPObject *obj, NPIdentifier propertyName){
	DBG_TRACE("( instance=%p, obj=%p, propertyName=%p )", instance, obj, propertyName);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	writeHandleIdentifier(propertyName);
	writeHandleObj(obj);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_HAS_PROPERTY);

	bool result = (bool)readResultInt32();
	return result;
}

bool NP_LOADDS NPN_HasMethod(NPP instance, NPObject *obj, NPIdentifier propertyName){
	DBG_TRACE("( instance=%p, obj=%p, propertyName=%p )", instance, obj, propertyName);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	writeHandleIdentifier(propertyName);
	writeHandleObj(obj);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_HAS_METHOD);

	bool result = (bool)readResultInt32();
	return result;
}

void NP_LOADDS NPN_ReleaseVariantValue(NPVariant *variant){
	DBG_TRACE("( variant=%p )", variant);
	DBG_CHECKTHREAD();

	switch (variant->type){

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

	variant->type 				= NPVariantType_Void;
	variant->value.objectValue 	= NULL;
}

void NP_LOADDS NPN_SetException(NPObject *obj, const NPUTF8 *message){
	DBG_TRACE("( obj=%p, message='%s' )", obj, message);
	DBG_CHECKTHREAD();

	writeString(message);
	writeHandleObj(obj);
	callFunction(FUNCTION_NPN_SET_EXCEPTION);
	readResultVoid();
}

void NP_LOADDS NPN_PushPopupsEnabledState(NPP instance, NPBool enabled){
	DBG_TRACE("( instance=%p, enabled=%d )", instance, enabled);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	writeInt32(enabled);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_PUSH_POPUPS_ENABLED_STATE);
	readResultVoid();
}

void NP_LOADDS NPN_PopPopupsEnabledState(NPP instance){
	DBG_TRACE("( instance=%p )", instance);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_POP_POPUPS_ENABLED_STATE);
	readResultVoid();
}

bool NP_LOADDS NPN_Enumerate(NPP instance, NPObject *obj, NPIdentifier **identifier, uint32_t *count){
	DBG_TRACE("( instance=%p, obj=%p, identifier=%p, count=%p )", instance, obj, identifier, count);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	writeHandleObj(obj);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_ENUMERATE);

	std::vector<ParameterInfo> stack;
	readCommands(stack);

	bool 	 result                         = (bool)readInt32(stack);
	if (!result){
		return false;
	}

	uint32_t identifierCount 				= readInt32(stack);
	if (identifierCount == 0){
		*identifier = NULL;
		*count 		= 0;
		return result;
	}

	std::vector<NPIdentifier> identifiers 	= readIdentifierArray(stack, identifierCount);

	NPIdentifier* identifierTable = (NPIdentifier*)malloc(identifierCount * sizeof(NPIdentifier));
	if (!identifierTable){
		return false;
	}

	memcpy(identifierTable, identifiers.data(), sizeof(NPIdentifier) * identifierCount);

	*identifier = identifierTable;
	*count 		= identifierCount;
	return true;
}

void NP_LOADDS NPN_PluginThreadAsyncCall(NPP instance, void (*func)(void *), void *userData){
	DBG_TRACE("( instance=%p, func=%p, userData=%p )", instance, func, userData);
	NOTIMPLEMENTED();
}

bool NP_LOADDS NPN_Construct(NPP instance, NPObject* obj, const NPVariant *args, uint32_t argCount, NPVariant *result){
	DBG_TRACE("( instance=%p, obj=%p, args=%p, argCount=%d, result=%p )", instance, obj, args, argCount, result);
	NOTIMPLEMENTED();
	return false;
}

NPError NP_LOADDS NPN_GetValueForURL(NPP instance, NPNURLVariable variable, const char *url, char **value, uint32_t *len){
	DBG_TRACE("( instance=%p, variable=%d, url='%s', value=%p, len=%p )", instance, variable, url, value, len);
	NOTIMPLEMENTED();
	return NPERR_NO_ERROR;
}

NPError NP_LOADDS NPN_SetValueForURL(NPP instance, NPNURLVariable variable, const char *url, const char *value, uint32_t len){
	DBG_TRACE("( instance=%p, variable=%d, url='%s', value=%p, len=%d )", instance, variable, url, value, len);
	NOTIMPLEMENTED();
	return NPERR_NO_ERROR;
}

NPError NPN_GetAuthenticationInfo(NPP instance, const char *protocol, const char *host, int32_t port, const char *scheme, const char *realm, char **username, uint32_t *ulen, char **password, uint32_t *plen){
	DBG_TRACE("( instance=%p, protocol='%s', host='%s', port=%d, scheme='%s', realm='%s', username=%p, ulen=%p, password=%p, plen=%p )", \
			instance, protocol, host, port, scheme, realm, username, ulen, password, plen);
	NOTIMPLEMENTED();
	return NPERR_NO_ERROR;
}

uint32_t NP_LOADDS NPN_ScheduleTimer(NPP instance, uint32_t interval, NPBool repeat, void (*timerFunc)(NPP npp, uint32_t timerID)){
	DBG_TRACE("( instance=%p, interval=%d, repeat=%d, timerFunc=%p )", instance, interval, repeat, timerFunc);
	NOTIMPLEMENTED();
	return 0;
}

void NP_LOADDS NPN_UnscheduleTimer(NPP instance, uint32_t timerID){
	DBG_TRACE("( instance=%p, timerID=%d )", instance, timerID);
	NOTIMPLEMENTED();
}

NPError NP_LOADDS NPN_PopUpContextMenu(NPP instance, NPMenu* menu){
	DBG_TRACE("( instance=%p, menu=%p )", instance, menu);
	NOTIMPLEMENTED();
	return NPERR_NO_ERROR;
}

NPBool NP_LOADDS NPN_ConvertPoint(NPP instance, double sourceX, double sourceY, NPCoordinateSpace sourceSpace, double *destX, double *destY, NPCoordinateSpace destSpace){
	DBG_TRACE("( instance=%p, sourceX=%f, sourceY=%f, sourceSpace=%d, destX=%p, destY=%p, destSpace=%d )", \
			instance, sourceX, sourceY, sourceSpace, destX, destY, destSpace);
	NOTIMPLEMENTED();
	return false;
}

NPBool NP_LOADDS NPN_HandleEvent(NPP instance, void *event, NPBool handled){
	DBG_TRACE("( instance=%p, event=%p, handled=%d )", instance, event, handled);
	NOTIMPLEMENTED();
	return false;
}

NPBool NP_LOADDS NPN_UnfocusInstance(NPP instance, NPFocusDirection direction){
	DBG_TRACE("( instance=%p, direction=%d )", instance, direction);
	NOTIMPLEMENTED();
	return false;
}

void NP_LOADDS NPN_URLRedirectResponse(NPP instance, void* notifyData, NPBool allow){
	DBG_TRACE("( instance=%p, notifyData=%p, allow=%d )", instance, notifyData, allow);
	NOTIMPLEMENTED();
}

NPError NP_LOADDS NPN_InitAsyncSurface(NPP instance, NPSize *size, NPImageFormat format, void *initData, NPAsyncSurface *surface){
	DBG_TRACE("( instance=%p, size=%p, format=%d, initData=%p, surface=%p )", instance, size, format, initData, surface);
	NOTIMPLEMENTED();
	return NPERR_NO_ERROR;	
}

NPError NP_LOADDS NPN_FinalizeAsyncSurface(NPP instance, NPAsyncSurface *surface){
	DBG_TRACE("( instance=%p, surface=%p )", instance, surface);
	NOTIMPLEMENTED();
	return NPERR_NO_ERROR;	
}

void NP_LOADDS NPN_SetCurrentAsyncSurface(NPP instance, NPAsyncSurface *surface, NPRect *changed){
	DBG_TRACE("( instance=%p, surface=%p, changed=%p )", instance, surface, changed);
	NOTIMPLEMENTED();
}

NPNetscapeFuncs browserFuncs = {
  sizeof(NPNetscapeFuncs),
  (NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR, // must be below 9 when using Quicktime to prevent a crash
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