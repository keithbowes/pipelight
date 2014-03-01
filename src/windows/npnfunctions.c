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

#define __WINESRC__

#include "../common/common.h"
#include "pluginloader.h"

#include <windows.h>

/* Shockwave sometimes calls the function with a wrong instance? Is this a wine bug? */
NPP shockwaveInstanceBug = NULL;

#define shockwaveInstanceWorkaround() \
	do{ \
		if (shockwaveInstanceBug && instance == shockwaveInstanceBug){ \
			instance = handleManager_findInstance(); \
			DBG_TRACE("Replaced wrong instance %p with %p", shockwaveInstanceBug, instance); \
		} \
	}while(0)

/* NPN_GetURL */
NPError NP_LOADDS NPN_GetURL(NPP instance, const char* url, const char* window){
	DBG_TRACE("( instance=%p, url='%s', window='%s' )", instance, url, window);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	writeString(window);
	writeString(url);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_GET_URL);
	NPError result = readResultInt32();

	DBG_TRACE(" -> result=%d", result);
	return result;
}

/* NPN_PostURL */
NPError NP_LOADDS NPN_PostURL(NPP instance, const char* url, const char* window, uint32_t len, const char* buf, NPBool file){
	DBG_TRACE("( instance=%p, url='%s', window='%s', len=%d, buf=%p, file=%d )", instance, url, window, len, buf, file);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	NPError result;

	/* File upload would require to convert the wine path to a linux path - too complicated as this function isnt used in many plugins */
	if (file){
		NOTIMPLEMENTED("file argument not supported.");
		result = NPERR_FILE_NOT_FOUND;

	}else{
		writeInt32(file);
		writeMemory(buf, len);
		writeString(window);
		writeString(url);
		writeHandleInstance(instance);
		callFunction(FUNCTION_NPN_POST_URL);
		result = readResultInt32();
	}

	DBG_TRACE(" -> result=%d", result);
	return result;
}

/* NPN_RequestRead */
NPError NP_LOADDS NPN_RequestRead(NPStream* stream, NPByteRange* rangeList){
	DBG_TRACE("( stream=%p, rangeList=%p )", stream, rangeList);
	DBG_CHECKTHREAD();

	uint32_t rangeCount = 0;

	for (; rangeList; rangeList = rangeList->next, rangeCount++){
		writeInt32(rangeList->length);
		writeInt32(rangeList->offset);
	}

	writeInt32(rangeCount);
	writeHandleStream(stream, HMGR_SHOULD_EXIST);
	callFunction(FUNCTION_NPN_REQUEST_READ);
	NPError result = readResultInt32();

	DBG_TRACE(" -> result=%d", result);
	return result;
}

/* NPN_NewStream */
NPError NP_LOADDS NPN_NewStream(NPP instance, NPMIMEType type, const char* window, NPStream** stream){
	DBG_TRACE("( instance=%p, type='%s', window='%s', stream=%p )", instance, type, window, stream);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	writeString(window);
	writeString(type);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_NEW_STREAM);

	Stack stack;
	readCommands(stack);
	NPError result = readInt32(stack);
	if (result == NPERR_NO_ERROR)
		*stream 	= readHandleStream(stack);

	DBG_TRACE(" -> ( result=%d, ... )", result);
	return NPERR_NO_ERROR;
}

/* NPN_Write */
int32_t NP_LOADDS NPN_Write(NPP instance, NPStream* stream, int32_t len, void* buffer){
	DBG_TRACE("( instance=%p, stream=%p, len=%d, buffer=%p )", instance, stream, len, buffer);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	writeMemory((char*)buffer, len);
	writeHandleStream(stream, HMGR_SHOULD_EXIST);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_WRITE);
	NPError result = readResultInt32();

	DBG_TRACE(" -> result=%d", result);
	return result;
}

/* NPN_DestroyStream */
NPError NP_LOADDS NPN_DestroyStream(NPP instance, NPStream* stream, NPReason reason){
	DBG_TRACE("( instance=%p, stream=%p, reason=%d )", instance, stream, reason);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	writeInt32(reason);
	writeHandleStream(stream, HMGR_SHOULD_EXIST);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_DESTROY_STREAM);
	NPError result = readResultInt32();

	DBG_TRACE(" -> result=%d", result);
	return result;
}

/* NPN_Status */
void NP_LOADDS NPN_Status(NPP instance, const char* message){
	DBG_TRACE("( instance=%p, message='%s' )", instance, message);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

#if 1 /*def PIPELIGHT_SYNC*/
	writeString(message);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_STATUS);
	readResultVoid();
#else
	writeString(message);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_STATUS_ASYNC);
#endif

	DBG_TRACE(" -> void");
}

/* NPN_UserAgent */
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

	DBG_TRACE(" -> str='%s'", strUserAgent);
	return strUserAgent;
}

/* NPN_MemAlloc */
void* NP_LOADDS NPN_MemAlloc(uint32_t size){
	DBG_TRACE("( size=%d )", size);

	void *mem = malloc(size);

	DBG_TRACE(" -> mem=%p", mem);
	return mem;
}

/* NPN_MemFree */
void NP_LOADDS NPN_MemFree(void* ptr){
	DBG_TRACE("( ptr=%p )", ptr);

	if (ptr)
		free(ptr);

	DBG_TRACE(" -> void");
}

/* NPN_MemFlush, MacOS only, returns number of freed bytes */
uint32_t NP_LOADDS NPN_MemFlush(uint32_t size){
	DBG_TRACE("( size=%d )", size);
	NOTIMPLEMENTED();
	DBG_TRACE(" -> numBytes=0");
	return 0;
}

/* NPN_ReloadPlugins, would allow to force the browser to reload plugins, not really necessary */
void NP_LOADDS NPN_ReloadPlugins(NPBool reloadPages){
	DBG_TRACE("( reloadPages=%d )", reloadPages);
	NOTIMPLEMENTED();
	DBG_TRACE(" -> void");
}

/* NPN_GetJavaEnv, Java is disabled of course! */
void* NP_LOADDS NPN_GetJavaEnv(void){
	DBG_TRACE("()");
	NOTIMPLEMENTED();
	DBG_TRACE(" -> ptr=NULL");
	return NULL;
}

/* NPN_GetJavaPeer, Java is disabled of course! */
void* NP_LOADDS NPN_GetJavaPeer(NPP instance){
	DBG_TRACE("( instance=%p )", instance);
	NOTIMPLEMENTED();
	DBG_TRACE(" -> ptr=NULL");
	return NULL;
}

/* NPN_GetURLNotify */
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

	DBG_TRACE(" -> result=%d", result);
	return result;
}

/* NPN_PostURLNotify */
NPError NP_LOADDS NPN_PostURLNotify(NPP instance, const char* url, const char* target, uint32_t len, const char* buf, NPBool file, void* notifyData){
	DBG_TRACE("( instance=%p, url='%s', target='%s', len=%d, buf=%p, file=%d, notifyData=%p )", instance, url, target, len, buf, file, notifyData);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	NPError result;

	/* File upload would require to convert the wine path to a linux path - too complicated as this function isnt used in many plugins */
	if (file){
		NOTIMPLEMENTED("file argument not supported.");
		result = NPERR_FILE_NOT_FOUND;

	}else{
		writeHandleNotify(notifyData);
		writeInt32(file);
		writeMemory(buf, len);
		writeString(target);
		writeString(url);
		writeHandleInstance(instance);
		callFunction(FUNCTION_NPN_POST_URL_NOTIFY);
		result = readResultInt32();
	}

	DBG_TRACE(" -> result=%d", result);
	return result;
}

/* NPN_GetValue */
NPError NP_LOADDS NPN_GetValue(NPP instance, NPNVariable variable, void *value){
	DBG_TRACE("( instance=%p, variable=%d, value=%p )", instance, variable, value);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	NPError result = NPERR_GENERIC_ERROR;
	Stack stack;

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
				static int once = 0;
				NetscapeData* ndata = (NetscapeData*)instance->ndata;
				if (ndata){
					if (ndata->hWnd){
						result = NPERR_NO_ERROR;
						*((HWND*)value) = ndata->hWnd;
					}else if (ndata->hDC && !once++)
						NOTIMPLEMENTED("NPNVnetscapeWindow not implemented for linuxWindowlessMode");
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

	DBG_TRACE(" -> ( result=%d, ... )", result);
	return result;
}

/* NPN_SetValue */
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
					/* Update windowless mode */
					ndata->windowlessMode 	= ( value == NULL );
					result 					= NPERR_NO_ERROR;

					DBG_INFO("plugin instance switched windowless mode to %s.", (ndata->windowlessMode ? "on" : "off"));

					/* Update existing plugin window */
					if (ndata->hWnd){
						if (ndata->window.type == NPWindowTypeDrawable)
							ReleaseDC(ndata->hWnd, (HDC)ndata->window.window);

						if (ndata->windowlessMode){
							ndata->window.window 		= GetDC(ndata->hWnd);
							ndata->window.type 			= NPWindowTypeDrawable;
						}else{
							ndata->window.window 		= ndata->hWnd;
							ndata->window.type 			= NPWindowTypeWindow;
						}

						pluginFuncs.setwindow(instance, &ndata->window);
					}
				}
			}
			break;

		default:
			NOTIMPLEMENTED("( variable=%d )", variable);
			break;
	}

	DBG_TRACE(" -> result=%d", result);
	return result;
}

/* NPN_InvalidateRect */
void NP_LOADDS NPN_InvalidateRect(NPP instance, NPRect *rect){
	DBG_TRACE("( instance=%p, rect=%p )", instance, rect);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	NetscapeData* ndata = (NetscapeData*)instance->ndata;
	if (ndata){
		if (ndata->hWnd){
			if (ndata->windowlessMode && rect){
				RECT r;
				r.left 		= rect->left;
				r.top 		= rect->top;
				r.right 	= rect->right;
				r.bottom 	= rect->bottom;
				InvalidateRect(ndata->hWnd, &r, false);

			}else
				InvalidateRect(ndata->hWnd, NULL, false);

		}else if (ndata->hDC){

			if (!rect)
				ndata->invalidate = INVALIDATE_EVERYTHING;

			else if (!ndata->invalidate){
				memcpy(&ndata->invalidateRect, rect, sizeof(*rect));
				ndata->invalidate = INVALIDATE_RECT;

			}else if (ndata->invalidate == INVALIDATE_RECT){

				/* Merge the NPRects */
				if (rect->top < ndata->invalidateRect.top)
					ndata->invalidateRect.top = rect->top;
				if (rect->left < ndata->invalidateRect.left)
					ndata->invalidateRect.left = rect->left;
				if (rect->bottom > ndata->invalidateRect.bottom)
					ndata->invalidateRect.bottom = rect->bottom;
				if (rect->right > ndata->invalidateRect.right)
					ndata->invalidateRect.right = rect->right;

			}

			pendingInvalidateLinuxWindowless = true;
		}
	}

	DBG_TRACE(" -> void");
}

/* NPN_InvalidateRegion */
void NP_LOADDS NPN_InvalidateRegion(NPP instance, NPRegion region){
	DBG_TRACE("( instance=%p, region=%p )", instance, region);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	NetscapeData* ndata = (NetscapeData*)instance->ndata;
	if (ndata){
		if (ndata->hWnd)
			InvalidateRgn(ndata->hWnd, region, false);

		else if (ndata->hDC){
			ndata->invalidate = INVALIDATE_EVERYTHING;
			pendingInvalidateLinuxWindowless = true;
		}
	}

	DBG_TRACE(" -> void");
}

/* NPN_ForceRedraw */
void NP_LOADDS NPN_ForceRedraw(NPP instance){
	DBG_TRACE("( instance=%p )", instance);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	NetscapeData* ndata = (NetscapeData*)instance->ndata;
	if (ndata){
		if (ndata->hWnd)
			UpdateWindow(ndata->hWnd);

		else if (ndata->hDC)
			NOTIMPLEMENTED("not implemented for linuxWindowlessMode");
	}

	DBG_TRACE(" -> void");
}

/* NPN_GetStringIdentifier */
NPIdentifier NP_LOADDS NPN_GetStringIdentifier(const NPUTF8* name){
	DBG_TRACE("( name='%s' )", name);
	DBG_CHECKTHREAD();

	NPIdentifier identifier;

#ifdef PIPELIGHT_NOCACHE
	writeString(name);
	callFunction(FUNCTION_NPN_GET_STRINGIDENTIFIER);

	Stack stack;
	readCommands(stack);
	identifier = readHandleIdentifierCreate(stack);

#else
	identifier = handleManager_lookupIdentifier(IDENT_TYPE_String, (void *)name);

	if (!identifier){
		NPIdentifierDescription *ident = (NPIdentifierDescription *)malloc(sizeof(NPIdentifierDescription));
		DBG_ASSERT(ident != NULL, "could not create identifier.");

		ident->type 		= IDENT_TYPE_String;
		ident->value.name 	= strdup(name);

		identifier = (NPIdentifier)ident;
		handleManager_updateIdentifier(identifier);
	}
#endif

	DBG_TRACE(" -> identifier=%p", identifier);
	return identifier;
}

/* NPN_GetStringIdentifiers */
void NP_LOADDS NPN_GetStringIdentifiers(const NPUTF8** names, int32_t nameCount, NPIdentifier* identifiers){
	DBG_TRACE("( names=%p, nameCount=%d, identifier=%p )", names, nameCount, identifiers);
	DBG_CHECKTHREAD();

	/* Lazy implementation ;-) */
	for (int i = 0; i < nameCount; i++)
		identifiers[i] = names[i] ? NPN_GetStringIdentifier(names[i]) : NULL;

	DBG_TRACE(" -> void");
}

/* NPN_GetIntIdentifier */
NPIdentifier NP_LOADDS NPN_GetIntIdentifier(int32_t intid){
	DBG_TRACE("( intid=%d )", intid);
	DBG_CHECKTHREAD();

	NPIdentifier identifier;

#ifdef PIPELIGHT_NOCACHE
	writeInt32(intid);
	callFunction(FUNCTION_NPN_GET_INTIDENTIFIER);

	Stack stack;
	readCommands(stack);
	identifier = readHandleIdentifierCreate(stack);

#else
	identifier = handleManager_lookupIdentifier(IDENT_TYPE_Integer, (void *)intid);

	if (!identifier){
		NPIdentifierDescription *ident = (NPIdentifierDescription *)malloc(sizeof(NPIdentifierDescription));
		DBG_ASSERT(ident != NULL, "could not create identifier.");

		ident->type 		= IDENT_TYPE_Integer;
		ident->value.intid 	= intid;

		identifier = (NPIdentifier)ident;
		handleManager_updateIdentifier(identifier);
	}
#endif

	DBG_TRACE(" -> identifier=%p", identifier);
	return identifier;
}

/* NPN_IdentifierIsString */
bool NP_LOADDS NPN_IdentifierIsString(NPIdentifier identifier){
	DBG_TRACE("( identifier=%p )", identifier);
	DBG_CHECKTHREAD();

	bool result;

#ifdef PIPELIGHT_NOCACHE
	writeHandleIdentifier(identifier);
	callFunction(FUNCTION_NPN_IDENTIFIER_IS_STRING);
	result = (bool)readResultInt32();

#else
	NPIdentifierDescription *ident = (NPIdentifierDescription *)identifier;
	DBG_ASSERT(ident != NULL, "got NULL identifier.");
	result = (ident->type == IDENT_TYPE_String);
#endif

	DBG_TRACE(" -> result=%d", result);
	return result;
}

/* NPN_UTF8FromIdentifier */
NPUTF8* NP_LOADDS NPN_UTF8FromIdentifier(NPIdentifier identifier){
	DBG_TRACE("( identifier=%p )", identifier);
	DBG_CHECKTHREAD();

	NPUTF8 *str;

#ifdef PIPELIGHT_NOCACHE
	writeHandleIdentifier(identifier);
	callFunction(FUNCTION_NPN_UTF8_FROM_IDENTIFIER);

	Stack stack;
	readCommands(stack);
	str = readStringMalloc(stack);

#else
	NPIdentifierDescription *ident = (NPIdentifierDescription *)identifier;
	DBG_ASSERT(ident != NULL, "got NULL identifier.");
	str = (ident->type == IDENT_TYPE_String && ident->value.name != NULL) ? strdup(ident->value.name) : NULL;
#endif

	DBG_TRACE(" -> str='%s'", str);
	return str;
}

/* NPN_IntFromIdentifier */
int32_t NP_LOADDS NPN_IntFromIdentifier(NPIdentifier identifier){
	DBG_TRACE("( identifier=%p )", identifier);
	DBG_CHECKTHREAD();

	int32_t result;

#ifdef PIPELIGHT_NOCACHE
	writeHandleIdentifier(identifier);
	callFunction(FUNCTION_NPN_INT_FROM_IDENTIFIER);
	result = readResultInt32();

#else
	NPIdentifierDescription *ident = (NPIdentifierDescription *)identifier;
	DBG_ASSERT(ident != NULL, "got NULL identifier.");
	result = (ident->type == IDENT_TYPE_Integer) ? ident->value.intid  : 0;
#endif

	DBG_TRACE(" -> result=%d", result);
	return result;
}

/* NPN_CreateObject */
NPObject* NP_LOADDS NPN_CreateObject(NPP instance, NPClass *aClass){
	DBG_TRACE("( instance=%p, aClass=%p )", instance, aClass);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	/* the other side doesnt need to know aClass */
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_CREATE_OBJECT);

	Stack stack;
	readCommands(stack);
	NPObject *result = readHandleObjIncRefCreate(stack, instance, aClass);

	DBG_TRACE(" -> obj=%p", result);
	return result;
}

/* NPN_RetainObject */
NPObject* NP_LOADDS NPN_RetainObject(NPObject *obj){
	DBG_TRACE("( obj=%p )", obj);
	DBG_CHECKTHREAD();

	if (obj){
		obj->referenceCount++;

	#if 1 /*def PIPELIGHT_SYNC*/
		writeInt32(obj->referenceCount);
		writeHandleObj(obj, HMGR_SHOULD_EXIST);
		callFunction(FUNCTION_NPN_RETAINOBJECT);
		readResultVoid();
	#else
		writeInt32(obj->referenceCount);
		writeHandleObj(obj, HMGR_SHOULD_EXIST);
		callFunction(FUNCTION_NPN_RETAINOBJECT_ASYNC);
	#endif
	}

	DBG_TRACE(" -> obj=%p", obj);
	return obj;
}

/* NPN_ReleaseObject */
void NP_LOADDS NPN_ReleaseObject(NPObject *obj){
	DBG_TRACE("( obj=%p )", obj);
	DBG_CHECKTHREAD();

	if (obj){
	#if 1 /*def PIPELIGHT_SYNC*/
		writeInt32(obj->referenceCount);
		writeHandleObjDecRef(obj, HMGR_SHOULD_EXIST);
		callFunction(FUNCTION_NPN_RELEASEOBJECT);
		readResultVoid();

	#else
		/* even without PIPELIGHT_SYNC we need a synchronized call in some cases (when a callback might happen) */
		if ((obj->referenceCount & REFCOUNT_MASK) == 1){
			writeInt32(obj->referenceCount);
			writeHandleObjDecRef(obj, HMGR_SHOULD_EXIST);
			callFunction(FUNCTION_NPN_RELEASEOBJECT);
			readResultVoid();

		}else{
			writeInt32(obj->referenceCount);
			writeHandleObjDecRef(obj, HMGR_SHOULD_EXIST);
			callFunction(FUNCTION_NPN_RELEASEOBJECT_ASYNC);
		}
	#endif
	}

	DBG_TRACE(" -> void");
}

/* NPN_Invoke */
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

	Stack stack;
	readCommands(stack);
	bool resultBool = readInt32(stack);
	if (resultBool)
		readVariantIncRef(stack, *result);
	else{
		result->type 				= NPVariantType_Void;
		result->value.objectValue 	= NULL;
	}

	DBG_TRACE(" -> ( result=%d, ... )", resultBool);
	return resultBool;
}

/* NPN_InvokeDefault */
bool NP_LOADDS NPN_InvokeDefault(NPP instance, NPObject* obj, const NPVariant *args, uint32_t argCount, NPVariant *result){ // UNTESTED!
	DBG_TRACE("( instance=%p, obj=%p, args=%p, argCount=%d, result=%p )", instance, obj, args, argCount, result);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	writeVariantArrayConst(args, argCount);
	writeInt32(argCount);
	writeHandleObj(obj);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_INVOKE_DEFAULT);

	Stack stack;
	readCommands(stack);
	bool resultBool = readInt32(stack);
	if (resultBool)
		readVariantIncRef(stack, *result);
	else{
		result->type 				= NPVariantType_Void;
		result->value.objectValue 	= NULL;
	}

	DBG_TRACE(" -> ( result=%d, ... )", resultBool);
	return resultBool;
}

/* NPN_Evaluate */
bool NP_LOADDS NPN_Evaluate(NPP instance, NPObject *obj, NPString *script, NPVariant *result){
	DBG_TRACE("( instance=%p, obj=%p, script=%p, result=%p )", instance, obj, script, result);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	writeNPString(script);
	writeHandleObj(obj);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_EVALUATE);

	Stack stack;
	readCommands(stack);
	bool resultBool = readInt32(stack);
	if (resultBool)
		readVariantIncRef(stack, *result);
	else{
		result->type 				= NPVariantType_Void;
		result->value.objectValue 	= NULL;
	}

	DBG_TRACE(" -> ( result=%d, ... )", resultBool);
	return resultBool;
}

/* NPN_GetProperty */
bool NP_LOADDS NPN_GetProperty(NPP instance, NPObject *obj, NPIdentifier propertyName, NPVariant *result){
	DBG_TRACE("( instance=%p, obj=%p, propertyName=%p, result=%p )", instance, obj, propertyName, result);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	/* Fake result of pluginElementNPObject :: clientWidth such that the plugin resultion is not messed up */
	NetscapeData* ndata = (NetscapeData*)instance->ndata;
	if (ndata && ndata->hWnd){
		if (obj == ndata->cache_pluginElementNPObject && propertyName == ndata->cache_clientWidthIdentifier){
			RECT rect;
			if (GetClientRect(ndata->hWnd, &rect)){
				result->type 			= NPVariantType_Int32;
				result->value.intValue 	= rect.right - rect.left;
				DBG_TRACE(" -> ( result=1, ... ) (faked)");
				return true;
			}
		}
	}

	writeHandleIdentifier(propertyName);
	writeHandleObj(obj);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_GET_PROPERTY);

	Stack stack;
	readCommands(stack);
	bool resultBool = readInt32(stack);
	if (resultBool)
		readVariantIncRef(stack, *result);
	else{
		result->type 				= NPVariantType_Void;
		result->value.objectValue 	= NULL;
	}

	DBG_TRACE(" -> ( result=%d, ... )", resultBool);
	return resultBool;
}

/* NPN_SetProperty */
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

	DBG_TRACE(" -> result=%d", result);
	return result;
}

/* NPN_RemoveProperty */
bool NP_LOADDS NPN_RemoveProperty(NPP instance, NPObject *obj, NPIdentifier propertyName){
	DBG_TRACE("( instance=%p, obj=%p, propertyName=%p )", instance, obj, propertyName);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	writeHandleIdentifier(propertyName);
	writeHandleObj(obj);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_REMOVE_PROPERTY);
	bool result = (bool)readResultInt32();

	DBG_TRACE(" -> result=%d", result);
	return result;
}

/* NPN_HasProperty */
bool NP_LOADDS NPN_HasProperty(NPP instance, NPObject *obj, NPIdentifier propertyName){
	DBG_TRACE("( instance=%p, obj=%p, propertyName=%p )", instance, obj, propertyName);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	writeHandleIdentifier(propertyName);
	writeHandleObj(obj);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_HAS_PROPERTY);
	bool result = (bool)readResultInt32();

	DBG_TRACE(" -> result=%d", result);
	return result;
}

/* NPN_HasMethod */
bool NP_LOADDS NPN_HasMethod(NPP instance, NPObject *obj, NPIdentifier propertyName){
	DBG_TRACE("( instance=%p, obj=%p, propertyName=%p )", instance, obj, propertyName);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	writeHandleIdentifier(propertyName);
	writeHandleObj(obj);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_HAS_METHOD);
	bool result = (bool)readResultInt32();

	DBG_TRACE(" -> result=%d", result);
	return result;
}

/* NPN_ReleaseVariantValue */
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

	DBG_TRACE(" -> void");
}

/* NPN_SetException */
void NP_LOADDS NPN_SetException(NPObject *obj, const NPUTF8 *message){
	DBG_TRACE("( obj=%p, message='%s' )", obj, message);
	DBG_CHECKTHREAD();

#if 1 /*def PIPELIGHT_SYNC*/
	writeString(message);
	writeHandleObj(obj);
	callFunction(FUNCTION_NPN_SET_EXCEPTION);
	readResultVoid();
#else
	writeString(message);
	writeHandleObj(obj);
	callFunction(FUNCTION_NPN_SET_EXCEPTION_ASYNC);
#endif

	DBG_TRACE(" -> void");
}

/* NPN_PushPopupsEnabledState */
void NP_LOADDS NPN_PushPopupsEnabledState(NPP instance, NPBool enabled){
	DBG_TRACE("( instance=%p, enabled=%d )", instance, enabled);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

#if 1 /*def PIPELIGHT_SYNC*/
	writeInt32(enabled);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_PUSH_POPUPS_ENABLED_STATE);
	readResultVoid();
#else
	writeInt32(enabled);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_PUSH_POPUPS_ENABLED_STATE_ASYNC);
#endif

	DBG_TRACE(" -> void");
}

/* NPN_PopPopupsEnabledState */
void NP_LOADDS NPN_PopPopupsEnabledState(NPP instance){
	DBG_TRACE("( instance=%p )", instance);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

#if 1 /*def PIPELIGHT_SYNC*/
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_POP_POPUPS_ENABLED_STATE);
	readResultVoid();
#else
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_POP_POPUPS_ENABLED_STATE_ASYNC);
#endif

	DBG_TRACE(" -> void");
}

/* NPN_Enumerate */
bool NP_LOADDS NPN_Enumerate(NPP instance, NPObject *obj, NPIdentifier **identifier, uint32_t *count){
	DBG_TRACE("( instance=%p, obj=%p, identifier=%p, count=%p )", instance, obj, identifier, count);
	DBG_CHECKTHREAD();

	shockwaveInstanceWorkaround();

	writeHandleObj(obj);
	writeHandleInstance(instance);
	callFunction(FUNCTION_NPN_ENUMERATE);

	Stack stack;
	readCommands(stack);
	bool result = (bool)readInt32(stack);
	if (result){
		uint32_t identifierCount = readInt32(stack);
		if (identifierCount == 0){
			*identifier = NULL;
			*count 		= 0;

		}else{
			std::vector<NPIdentifier> identifiers 	= readIdentifierArray(stack, identifierCount);
			NPIdentifier* identifierTable 			= (NPIdentifier*)malloc(identifierCount * sizeof(NPIdentifier));

			if (identifierTable){
				memcpy(identifierTable, identifiers.data(), sizeof(NPIdentifier) * identifierCount);
				*identifier = identifierTable;
				*count 		= identifierCount;

			}else
				result = false;
		}
	}

	DBG_TRACE(" -> ( result=%d, ... )", result);
	return result;
}

/* NPN_PluginThreadAsyncCall */
void NP_LOADDS NPN_PluginThreadAsyncCall(NPP instance, void (*func)(void *), void *userData){
	DBG_TRACE("( instance=%p, func=%p, userData=%p )", instance, func, userData);

	NetscapeData* ndata = (NetscapeData *)instance->ndata;
	if (ndata){
		AsyncCallback *asyncCall, *nextAsyncCall;
		asyncCall = (AsyncCallback *)malloc(sizeof(AsyncCallback));
		DBG_ASSERT(asyncCall, "unable to schedule async call, out of memory.");

		asyncCall->func 	= func;
		asyncCall->userData	= userData;

		/* append at the end of the list */
		do{
			nextAsyncCall	= ndata->asyncCalls;
			asyncCall->next	= nextAsyncCall;
		}while ((AsyncCallback *)InterlockedCompareExchangePointer((void **)&ndata->asyncCalls, (void *)asyncCall, (void *)nextAsyncCall) != nextAsyncCall);

		/* notify main thread that we've added something */
		InterlockedIncrement(&pendingAsyncCalls);
	}

	DBG_TRACE(" -> void");
}

/* NPN_Construct */
bool NP_LOADDS NPN_Construct(NPP instance, NPObject* obj, const NPVariant *args, uint32_t argCount, NPVariant *result){
	DBG_TRACE("( instance=%p, obj=%p, args=%p, argCount=%d, result=%p )", instance, obj, args, argCount, result);
	NOTIMPLEMENTED();
	DBG_TRACE(" -> result=0");
	return false;
}

/* NPN_GetValueForURL */
NPError NP_LOADDS NPN_GetValueForURL(NPP instance, NPNURLVariable variable, const char *url, char **value, uint32_t *len){
	DBG_TRACE("( instance=%p, variable=%d, url='%s', value=%p, len=%p )", instance, variable, url, value, len);
	NOTIMPLEMENTED();
	DBG_TRACE(" -> result=0");
	return NPERR_NO_ERROR;
}

/* NPN_SetValueForURL */
NPError NP_LOADDS NPN_SetValueForURL(NPP instance, NPNURLVariable variable, const char *url, const char *value, uint32_t len){
	DBG_TRACE("( instance=%p, variable=%d, url='%s', value=%p, len=%d )", instance, variable, url, value, len);
	NOTIMPLEMENTED();
	DBG_TRACE(" -> result=0");
	return NPERR_NO_ERROR;
}

/* NPN_GetAuthenticationInfo */
NPError NPN_GetAuthenticationInfo(NPP instance, const char *protocol, const char *host, int32_t port, const char *scheme, const char *realm, char **username, uint32_t *ulen, char **password, uint32_t *plen){
	DBG_TRACE("( instance=%p, protocol='%s', host='%s', port=%d, scheme='%s', realm='%s', username=%p, ulen=%p, password=%p, plen=%p )", \
			instance, protocol, host, port, scheme, realm, username, ulen, password, plen);
	NOTIMPLEMENTED();
	DBG_TRACE(" -> result=0");
	return NPERR_NO_ERROR;
}

/* NPN_ScheduleTimer */
uint32_t NP_LOADDS NPN_ScheduleTimer(NPP instance, uint32_t interval, NPBool repeat, void (*timerFunc)(NPP npp, uint32_t timerID)){
	DBG_TRACE("( instance=%p, interval=%d, repeat=%d, timerFunc=%p )", instance, interval, repeat, timerFunc);
	NOTIMPLEMENTED();
	DBG_TRACE(" -> timerID=0");
	return 0;
}

/* NPN_UnscheduleTimer */
void NP_LOADDS NPN_UnscheduleTimer(NPP instance, uint32_t timerID){
	DBG_TRACE("( instance=%p, timerID=%d )", instance, timerID);
	NOTIMPLEMENTED();
	DBG_TRACE(" -> void");
}

/* NPN_PopUpContextMenu */
NPError NP_LOADDS NPN_PopUpContextMenu(NPP instance, NPMenu* menu){
	DBG_TRACE("( instance=%p, menu=%p )", instance, menu);
	NOTIMPLEMENTED();
	DBG_TRACE(" -> result=0");
	return NPERR_NO_ERROR;
}

/* NPN_ConvertPoint */
NPBool NP_LOADDS NPN_ConvertPoint(NPP instance, double sourceX, double sourceY, NPCoordinateSpace sourceSpace, double *destX, double *destY, NPCoordinateSpace destSpace){
	DBG_TRACE("( instance=%p, sourceX=%f, sourceY=%f, sourceSpace=%d, destX=%p, destY=%p, destSpace=%d )", \
			instance, sourceX, sourceY, sourceSpace, destX, destY, destSpace);
	NOTIMPLEMENTED();
	DBG_TRACE(" -> result=0");
	return false;
}

/* NPN_HandleEvent */
NPBool NP_LOADDS NPN_HandleEvent(NPP instance, void *event, NPBool handled){
	DBG_TRACE("( instance=%p, event=%p, handled=%d )", instance, event, handled);
	NOTIMPLEMENTED();
	DBG_TRACE(" -> result=0");
	return false;
}

/* NPN_UnfocusInstance */
NPBool NP_LOADDS NPN_UnfocusInstance(NPP instance, NPFocusDirection direction){
	DBG_TRACE("( instance=%p, direction=%d )", instance, direction);
	NOTIMPLEMENTED();
	DBG_TRACE(" -> result=0");
	return false;
}

/* NPN_URLRedirectResponse */
void NP_LOADDS NPN_URLRedirectResponse(NPP instance, void* notifyData, NPBool allow){
	DBG_TRACE("( instance=%p, notifyData=%p, allow=%d )", instance, notifyData, allow);
	NOTIMPLEMENTED();
	DBG_TRACE(" -> void");
}

/* NPN_InitAsyncSurface */
NPError NP_LOADDS NPN_InitAsyncSurface(NPP instance, NPSize *size, NPImageFormat format, void *initData, NPAsyncSurface *surface){
	DBG_TRACE("( instance=%p, size=%p, format=%d, initData=%p, surface=%p )", instance, size, format, initData, surface);
	NOTIMPLEMENTED();
	DBG_TRACE(" -> result=0");
	return NPERR_NO_ERROR;
}

/* NPN_FinalizeAsyncSurface */
NPError NP_LOADDS NPN_FinalizeAsyncSurface(NPP instance, NPAsyncSurface *surface){
	DBG_TRACE("( instance=%p, surface=%p )", instance, surface);
	NOTIMPLEMENTED();
	DBG_TRACE(" -> result=0");
	return NPERR_NO_ERROR;
}

/* NPN_SetCurrentAsyncSurface */
void NP_LOADDS NPN_SetCurrentAsyncSurface(NPP instance, NPAsyncSurface *surface, NPRect *changed){
	DBG_TRACE("( instance=%p, surface=%p, changed=%p )", instance, surface, changed);
	NOTIMPLEMENTED();
	DBG_TRACE(" -> void");
}

NPNetscapeFuncs browserFuncs = {
  sizeof(NPNetscapeFuncs),
  (NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR,
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