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

#ifndef PluginLoader_h_
#define PluginLoader_h_

#include <map>
#include <set>

#include "../common/common.h"

extern std::map<HWND, NPP> hwndToInstance;
extern std::set<NPP> instanceList;

/* variables */
extern bool isWindowlessMode;
extern bool isLinuxWindowlessMode;
extern bool isEmbeddedMode;
extern bool stayInFullscreen;
extern bool isSandboxed;

/* hooks */
extern bool usermodeTimer;
extern bool unityHacks;
extern bool windowClassHook;

/* not implemented yet */
extern bool renderTopLevelWindow;

/* linux windowless */
extern bool invalidateLinuxWindowless;

/* user agent and plugin data */
extern char strUserAgent[1024];

extern std::string np_MimeType;
extern std::string np_FileExtents;
extern std::string np_FileOpenName;
extern std::string np_ProductName;
extern std::string np_FileDescription;
extern std::string np_Language;

extern NPPluginFuncs pluginFuncs;
extern NPNetscapeFuncs browserFuncs;

/*
	NP Class functions
*/
void NPInvalidateFunction(NPObject *npobj);
bool NPHasMethodFunction(NPObject *npobj, NPIdentifier name);
bool NPInvokeFunction(NPObject *npobj, NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result);
bool NPInvokeDefaultFunction(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result);
bool NPHasPropertyFunction(NPObject *npobj, NPIdentifier name);
bool NPGetPropertyFunction(NPObject *npobj, NPIdentifier name, NPVariant *result);
bool NPSetPropertyFunction(NPObject *npobj, NPIdentifier name, const NPVariant *value);
bool NPRemovePropertyFunction(NPObject *npobj, NPIdentifier name);
bool NPEnumerationFunction(NPObject *npobj, NPIdentifier **value, uint32_t *count);
bool NPConstructFunction(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result);
NPObject * NPAllocateFunction(NPP npp, NPClass *aClass);
void NPDeallocateFunction(NPObject *npobj);

/*
	NPN Browser functions
*/
NPError NP_LOADDS NPN_GetURL(NPP instance, const char* url, const char* window);
NPError NP_LOADDS NPN_PostURL(NPP instance, const char* url, const char* window, uint32_t len, const char* buf, NPBool file);
NPError NP_LOADDS NPN_RequestRead(NPStream* stream, NPByteRange* rangeList);
NPError NP_LOADDS NPN_NewStream(NPP instance, NPMIMEType type, const char* window, NPStream** stream);
int32_t NP_LOADDS NPN_Write(NPP instance, NPStream* stream, int32_t len, void* buffer);
NPError NP_LOADDS NPN_DestroyStream(NPP instance, NPStream* stream, NPReason reason);
void NP_LOADDS NPN_Status(NPP instance, const char* message);
const char*  NP_LOADDS NPN_UserAgent(NPP instance);
void* NP_LOADDS NPN_MemAlloc(uint32_t size);
void NP_LOADDS NPN_MemFree(void* ptr);
uint32_t NP_LOADDS NPN_MemFlush(uint32_t size);
void NP_LOADDS NPN_ReloadPlugins(NPBool reloadPages);
void* NP_LOADDS NPN_GetJavaEnv(void);
void* NP_LOADDS NPN_GetJavaPeer(NPP instance);
NPError NP_LOADDS NPN_GetURLNotify(NPP instance, const  char* url, const char* target, void* notifyData);
NPError NP_LOADDS NPN_PostURLNotify(NPP instance, const char* url, const char* target, uint32_t len, const char* buf, NPBool file, void* notifyData);
NPError NP_LOADDS NPN_GetValue(NPP instance, NPNVariable variable, void *value);
NPError NP_LOADDS NPN_SetValue(NPP instance, NPPVariable variable, void *value);
void NP_LOADDS NPN_InvalidateRect(NPP instance, NPRect *rect);
void NP_LOADDS NPN_InvalidateRegion(NPP instance, NPRegion region);
void NP_LOADDS NPN_ForceRedraw(NPP instance);
NPIdentifier NP_LOADDS NPN_GetStringIdentifier(const NPUTF8* name);
void NP_LOADDS NPN_GetStringIdentifiers(const NPUTF8** names, int32_t nameCount, NPIdentifier* identifiers);
NPIdentifier NP_LOADDS NPN_GetIntIdentifier(int32_t intid);
bool NP_LOADDS NPN_IdentifierIsString(NPIdentifier identifier);
NPUTF8* NP_LOADDS NPN_UTF8FromIdentifier(NPIdentifier identifier);
int32_t NP_LOADDS NPN_IntFromIdentifier(NPIdentifier identifier);
NPObject* NP_LOADDS NPN_CreateObject(NPP npp, NPClass *aClass);
NPObject* NP_LOADDS NPN_RetainObject(NPObject *obj);
void NP_LOADDS NPN_ReleaseObject(NPObject *obj);
bool NP_LOADDS NPN_Invoke(NPP npp, NPObject* obj, NPIdentifier methodName, const NPVariant *args, uint32_t argCount, NPVariant *result);
bool NP_LOADDS NPN_InvokeDefault(NPP npp, NPObject* obj, const NPVariant *args, uint32_t argCount, NPVariant *result);
bool NP_LOADDS NPN_Evaluate(NPP npp, NPObject *obj, NPString *script, NPVariant *result);
bool NP_LOADDS NPN_GetProperty(NPP npp, NPObject *obj, NPIdentifier propertyName, NPVariant *result);
bool NP_LOADDS NPN_SetProperty(NPP npp, NPObject *obj, NPIdentifier propertyName, const NPVariant *value);
bool NP_LOADDS NPN_RemoveProperty(NPP npp, NPObject *obj, NPIdentifier propertyName);
bool NP_LOADDS NPN_HasProperty(NPP npp, NPObject *obj, NPIdentifier propertyName);
bool NP_LOADDS NPN_HasMethod(NPP npp, NPObject *obj, NPIdentifier propertyName);
void NP_LOADDS NPN_SetException(NPObject *obj, const NPUTF8 *message);
void NP_LOADDS NPN_PushPopupsEnabledState(NPP npp, NPBool enabled);
void NP_LOADDS NPN_PopPopupsEnabledState(NPP npp);
bool NP_LOADDS NPN_Enumerate(NPP npp, NPObject *obj, NPIdentifier **identifier, uint32_t *count);
void NP_LOADDS NPN_PluginThreadAsyncCall(NPP instance, void (*func)(void *), void *userData);
bool NP_LOADDS NPN_Construct(NPP npp, NPObject* obj, const NPVariant *args, uint32_t argCount, NPVariant *result);
NPError NP_LOADDS NPN_GetValueForURL(NPP npp, NPNURLVariable variable, const char *url, char **value, uint32_t *len);
NPError NP_LOADDS NPN_SetValueForURL(NPP npp, NPNURLVariable variable, const char *url, const char *value, uint32_t len);
NPError NP_LOADDS NPN_GetAuthenticationInfo(NPP npp, const char *protocol, const char *host, int32_t port, const char *scheme, const char *realm, char **username, uint32_t *ulen, char **password, uint32_t *plen);
uint32_t NP_LOADDS NPN_ScheduleTimer(NPP instance, uint32_t interval, NPBool repeat, void (*timerFunc)(NPP npp, uint32_t timerID));
void NP_LOADDS NPN_UnscheduleTimer(NPP instance, uint32_t timerID);
NPError NP_LOADDS NPN_PopUpContextMenu(NPP instance, NPMenu* menu);
NPBool NP_LOADDS NPN_ConvertPoint(NPP instance, double sourceX, double sourceY, NPCoordinateSpace sourceSpace, double *destX, double *destY, NPCoordinateSpace destSpace);
NPBool NP_LOADDS NPN_HandleEvent(NPP instance, void *event, NPBool handled);
NPBool NP_LOADDS NPN_UnfocusInstance(NPP instance, NPFocusDirection direction);
void NP_LOADDS NPN_URLRedirectResponse(NPP instance, void* notifyData, NPBool allow);
NPError NP_LOADDS NPN_InitAsyncSurface(NPP instance, NPSize *size, NPImageFormat format, void *initData, NPAsyncSurface *surface);
NPError NP_LOADDS NPN_FinalizeAsyncSurface(NPP instance, NPAsyncSurface *surface);
void NP_LOADDS NPN_SetCurrentAsyncSurface(NPP instance, NPAsyncSurface *surface, NPRect *changed);

/* public */
typedef unsigned long int XID;

struct NetscapeData{
	bool		windowlessMode;
	bool		embeddedMode;

	NPObject*   	cache_pluginElementNPObject;
	NPIdentifier 	cache_clientWidthIdentifier;

	RECT 		browser;

	/* regular mode */
	HWND 		hWnd;

	/* linux windowless mode */
	HDC 		hDC;
	XID			lastDrawableDC;

	NPWindow 	window;

	int 		invalidate;
	NPRect 		invalidateRect;
};

extern void changeEmbeddedMode(bool newEmbed);
extern std::string getWineVersion();

#endif // PluginLoader_h_