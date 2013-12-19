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

#include <cstdlib>								// for malloc, ...
#include <string>								// for std::string
#include <vector>								// for std::vector
#include <algorithm>							// for std::transform
#include <stdio.h>								// for _fdopen
#include <float.h>								// _controlfp_s

#include "../common/common.h"
#include "pluginloader.h"
#include "apihook.h"

#include <windows.h>
#include <objbase.h>							// for CoInitializeEx

/* BEGIN GLOBAL VARIABLES */

char clsName[] = "VirtualBrowser";

std::map<HWND, NPP> hwndToInstance;
std::set<NPP> instanceList;

/* variables */
bool isWindowlessMode	= false;
bool isLinuxWindowlessMode = false;
bool isEmbeddedMode		= false;
bool stayInFullscreen 	= false;
bool isSandboxed 		= false;

/* hooks */
bool unityHacks 		= false;
bool windowClassHook 	= false;

/* not implemented yet */
bool renderTopLevelWindow = false;

/* linux windowless */
bool invalidateLinuxWindowless = false;

/* user agent and plugin data */
char strUserAgent[1024] = {0};

std::string np_MimeType;
std::string np_FileExtents;
std::string np_FileOpenName;
std::string np_ProductName;
std::string np_FileDescription;
std::string np_Language;

NPPluginFuncs pluginFuncs = {sizeof(pluginFuncs), (NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR};

/* END GLOBAL VARIABLES */

/* wine specific definitions */
typedef WCHAR* (* CDECL wine_get_dos_file_namePtr)(LPCSTR str);
typedef const char* (* CDECL wine_get_versionPtr)();
#define CP_UNIXCP 65010

#define X11DRV_ESCAPE 6789
#define X11DRV_SET_DRAWABLE 0

struct x11drv_escape_set_drawable{
	int  code;
	HWND hwnd;
	XID  drawable;
	int  mode;
	RECT dc_rect;
	XID  fbconfig_id;
};

/* convertToWindowsPath */
std::string convertToWindowsPath(const std::string &linux_path){
	static wine_get_dos_file_namePtr wine_get_dos_file_name = NULL;
	WCHAR* windows_path;
	char path[MAX_PATH];

	if (!wine_get_dos_file_name)
		wine_get_dos_file_name = (wine_get_dos_file_namePtr)GetProcAddress(GetModuleHandleA("kernel32.dll"), "wine_get_dos_file_name");

	if (!wine_get_dos_file_name){
		DBG_ERROR("Unable to find wine function 'wine_get_dos_file_name'.");
		return "";
	}

	if (!(windows_path = wine_get_dos_file_name(linux_path.c_str()))){
		DBG_ERROR("Unable to convert '%s' to a windows path.", linux_path.c_str());
		return "";
	}

	WideCharToMultiByte(CP_UNIXCP, 0, windows_path, -1, path, sizeof(path), NULL, NULL);
	HeapFree(GetProcessHeap(), 0, windows_path);

	return std::string(path);
}

/* getWineVersion */
std::string getWineVersion(){
	static wine_get_versionPtr wine_get_version = NULL;
	const char *wine_version;

	if (!wine_get_version)
		wine_get_version = (wine_get_versionPtr)GetProcAddress(GetModuleHandleA("ntdll.dll"), "wine_get_version");

	if (!wine_get_version){
		DBG_ERROR("Unable to find wine function 'wine_get_version'.");
		return "";
	}

	if (!(wine_version = wine_get_version())){
		DBG_ERROR("Unable to determine wine version.");
		return "";
	}

	return std::string(wine_version);
}

/* wndProcedure */
LRESULT CALLBACK wndProcedure(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam){

	/* Only messages with a hwnd can be relevant in windowlessmode mode */
	if (hWnd){
		std::map<HWND, NPP>::iterator it = hwndToInstance.find(hWnd);
		if (it != hwndToInstance.end()){
			NPP instance 		= it->second;
			NetscapeData* ndata = (NetscapeData*)instance->ndata;

			/* In windowless mode handle paint and all other keyboard/mouse events */
			if (ndata && ndata->windowlessMode){

				/* Paint event */
				if (Msg == WM_PAINT){
					if (ndata->window.type == NPWindowTypeDrawable){
						PAINTSTRUCT paint;
						HDC hDC = BeginPaint(hWnd, &paint);
						if (hDC != NULL){
							HDC previousDC = (HDC)ndata->window.window;
							ndata->window.window = hDC;

							NPEvent event;
							event.event 	= WM_PAINT;
							event.wParam 	= (uintptr_t)hDC;
							event.lParam 	= (uintptr_t)&paint.rcPaint;
							pluginFuncs.event(instance, &event);

							ndata->window.window = previousDC;
							EndPaint(hWnd, &paint);
						}
						return 0;
					}

				/* All other events */
				}else{

					/*
						Workaround for Silverlight - the events are not correctly handled if window->window is nonzero
						Set it to zero before calling the event handler in this case.
					*/

					HDC previousDC = NULL;

					if 	( ndata->window.type == NPWindowTypeDrawable &&
							((Msg >= WM_KEYFIRST && Msg <= WM_KEYLAST) || (Msg >= WM_MOUSEFIRST && Msg <= WM_MOUSELAST )) ){
						previousDC = (HDC)ndata->window.window;
						ndata->window.window = NULL;
					}

					/* Request the focus for the plugin window */
					if (Msg == WM_LBUTTONDOWN)
						SetFocus(hWnd);

					NPEvent event;
					event.event 	= Msg;
					event.wParam 	= wParam;
					event.lParam 	= lParam;
					int16_t result = pluginFuncs.event(instance, &event);

					if (previousDC)
						ndata->window.window = previousDC;

					if (result == kNPEventHandled) return 0;

				}

			}

		}
	}

	/* Otherwise we only have to handle several regular events */
	if (Msg == WM_DESTROY){
		return 0;

	}else if (Msg == WM_CLOSE){
		return 0;

	}else if (Msg == WM_SIZE){
		InvalidateRect(hWnd, NULL, false);
		return 0;

	}else{
		return DefWindowProcA(hWnd, Msg, wParam, lParam);
	}
}

/* freeSharedPtrMemory */
void freeSharedPtrMemory(void *memory){
	if (memory)
		free(memory);
}

/* splitMimeType */
std::vector<std::string> splitMimeType(std::string input){
	std::vector<std::string> result;

	int start 			= 0;
	unsigned int i 		= 0;

	while (i < input.length()){
		while (i < input.length() && input[i] != '|')
			i++;

		if (i - start > 0)
			result.push_back(input.substr(start, i-start));

		i++;
		start = i;
	}

	return result;
}

/* createLinuxCompatibleMimeType */
std::string createLinuxCompatibleMimeType(){
	std::vector<std::string> mimeTypes 		= splitMimeType(np_MimeType);
	std::vector<std::string> fileExtensions = splitMimeType(np_FileExtents);
	std::vector<std::string> extDescription = splitMimeType(np_FileOpenName);

	std::string result = "";

	for (unsigned int i = 0; i < mimeTypes.size(); i++){

		if (i != 0)
			result += ";";

		result += mimeTypes[i];

		result += ":";
		if (i < fileExtensions.size())
			result += fileExtensions[i];

		result += ":";
		if (i < extDescription.size())
			result += extDescription[i];
	}

	return result;
}

/* initDLL */
bool initDLL(std::string dllPath, std::string dllName){

	/* Silverlight doesn't call this, so we have to do it */
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	if (!SetDllDirectoryA(dllPath.c_str())){
		DBG_ERROR("failed to set DLL directory.");
	}

	HMODULE dll = LoadLibraryA(dllName.c_str());
	if (!dll){
		DBG_ERROR("could not load library '%s' (last error = %lu).", dllName.c_str(), (unsigned long)GetLastError());
		return false;
	}

	int requiredBytes = GetFileVersionInfoSizeA(dllName.c_str(), NULL);
	if (!requiredBytes){
		DBG_ERROR("could not load version information.");
		FreeLibrary(dll);
		return false;
	}

	std::unique_ptr<void, void (*)(void *)> data(malloc(requiredBytes), freeSharedPtrMemory);
	if (!data){
		DBG_ERROR("failed to allocate memory.");
		FreeLibrary(dll);
		return false;
	}

	if (!GetFileVersionInfoA(dllName.c_str(), 0, requiredBytes, data.get())){
		DBG_ERROR("failed to get file version.");
		FreeLibrary(dll);
		return false;
	}

	char *info = NULL;
	UINT size = 0;

	if (VerQueryValueA(data.get(), "\\StringFileInfo\\040904E4\\MIMEType", (void**)&info, &size)){
		while( size > 0 && info[size-1] == 0) size--;
		np_MimeType = std::string(info, size);
	}

	if (VerQueryValueA(data.get(), "\\StringFileInfo\\040904E4\\FileExtents", (void**)&info, &size)){
		while( size > 0 && info[size-1] == 0) size--;
		np_FileExtents = std::string(info, size);
	}

	if (VerQueryValueA(data.get(), "\\StringFileInfo\\040904E4\\FileOpenName", (void**)&info, &size)){
		while( size > 0 && info[size-1] == 0) size--;
		np_FileOpenName = std::string(info, size);
	}

	if (VerQueryValueA(data.get(), "\\StringFileInfo\\040904E4\\ProductName", (void**)&info, &size)){
		while( size > 0 && info[size-1] == 0) size--;
		np_ProductName = std::string(info, size);
	}

	if (VerQueryValueA(data.get(), "\\StringFileInfo\\040904E4\\FileDescription", (void**)&info, &size)){
		while( size > 0 && info[size-1] == 0) size--;
		np_FileDescription = std::string(info, size);
	}

	if (VerQueryValueA(data.get(), "\\StringFileInfo\\040904E4\\Language", (void**)&info, &size)){
		while( size > 0 && info[size-1] == 0) size--;
		np_Language = std::string(info, size);
	}

	NP_GetEntryPointsFunc 	NP_GetEntryPoints 	= (NP_GetEntryPointsFunc) 	GetProcAddress(dll, "NP_GetEntryPoints");
	NP_InitializeFunc 		NP_Initialize 		= (NP_InitializeFunc) 		GetProcAddress(dll, "NP_Initialize");

	if (NP_GetEntryPoints && NP_Initialize){
		if (NP_GetEntryPoints(&pluginFuncs) == NPERR_NO_ERROR){
			if (NP_Initialize(&browserFuncs) == NPERR_NO_ERROR){
				return true;

			}else{
				DBG_ERROR("failed to initialize plugin.");
			}
		}else{
			DBG_ERROR("failed to get entry points for plugin functions.");
		}
	}else{
		DBG_ERROR("could not load entry points from DLL!");
	}

	FreeLibrary(dll);
	return false;
}

/* readPathFromRegistry */
std::string readPathFromRegistry(HKEY hKey, std::string regKey){

	std::string fullKey = "Software\\MozillaPlugins\\" + regKey + "\\";

	DWORD type;
	DWORD length;

	/* check if the value exists and get required size */
	if (RegGetValueA(hKey, fullKey.c_str(), "Path", RRF_RT_ANY, &type, NULL, &length) != ERROR_SUCCESS)
		return "";

	/* check if the value is a string and the length is > 0 */
	if (type != REG_SZ || !length)
		return "";

	char *path = (char*)malloc(length);
	if (!path)
		return "";

	if (RegGetValueA(hKey, fullKey.c_str(), "Path", RRF_RT_REG_SZ, NULL, path, &length) != ERROR_SUCCESS){
		free(path);
		return "";
	}

	std::string result(path);
	free(path);

	return result;
}

/* main */
int main(int argc, char *argv[]){

	/* get the main thread ID */
	mainThreadID = GetCurrentThreadId();

	/*
		When compiling with wineg++ the _controlfp_s isn't available
		We should find a workaround for this (asm implementation) as soon as wineg++ support works properly
	*/
	#ifndef __WINE__
		unsigned int control_word;
		_controlfp_s(&control_word, _CW_DEFAULT, MCW_PC);
	#endif

	setbuf(stderr, NULL); /* Disable stderr buffering */

	std::string dllPath;
	std::string dllName;
	std::string regKey;

	for (int i = 1; i < argc; i++){
		std::string arg = std::string(argv[i]);
		std::transform(arg.begin(), arg.end(), arg.begin(), c_tolower);

		if       (arg == "--pluginname"){
			if (i + 1 >= argc) break;
			setMultiPluginName(argv[++i]);

		}else if (arg == "--dllpath"){
			if (i + 1 >= argc) break;
			dllPath = std::string(argv[++i]);

		}else if (arg == "--dllname"){
			if (i + 1 >= argc) break;
			dllName = std::string(argv[++i]);

		}else if (arg == "--regkey"){
			if (i + 1 >= argc) break;
			regKey  = std::string(argv[++i]);

		/* variables */
		}else if (arg == "--windowless"){
			isWindowlessMode = true;

		}else if (arg == "--linuxwindowless"){
			isLinuxWindowlessMode = true;

		}else if (arg == "--embed"){
			isEmbeddedMode = true;

		/* hooks */
		}else if (arg == "--unityhacks"){
			unityHacks = true;

		}else if (arg == "--windowclasshook"){
			windowClassHook = true;

		}else if (arg == "--rendertoplevelwindow"){
			renderTopLevelWindow = true;

		}
	}

	/* required arguments available? */
	if (regKey == "" && (dllPath == "" || dllName == "")){
		DBG_ERROR("you must at least specify --regKey or --dllPath and --dllName.");
		return 1;
	}

	/* read arguments from the registry if necessary */
	if (dllPath == "" || dllName == ""){
		std::string path = readPathFromRegistry(HKEY_CURRENT_USER, regKey);

		if (path == "")
			path = readPathFromRegistry(HKEY_LOCAL_MACHINE, regKey);

		if (path == ""){
			DBG_ERROR("Couldn't read dllPath and dllName from registry.");
			return 1;
		}

		size_t pos = path.find_last_of('\\');
		if (pos == std::string::npos || pos == 0 || pos >= path.length() - 1){
			DBG_ERROR("Registry value for dllPath and dllName is invalid.");
			return 1;
		}

		dllPath = path.substr(0, pos);
		dllName = path.substr(pos+1, std::string::npos);
		DBG_INFO("Read dllPath '%s' and dllName '%s' from registry", dllPath.c_str(), dllName.c_str());
	}

	/* debug info */
	DBG_INFO("windowless mode       is %s.", (isWindowlessMode ? "on" : "off"));
	DBG_INFO("linux windowless mode is %s.", (isLinuxWindowlessMode ? "on" : "off"));
	DBG_INFO("embedded mode         is %s.", (isEmbeddedMode ? "on" : "off"));
	DBG_INFO("unity hacks           is %s.", (unityHacks ? "on" : "off"));
	DBG_INFO("window class hook     is %s.", (windowClassHook ? "on" : "off"));
	DBG_INFO("render toplevelwindow is %s.", (renderTopLevelWindow ? "on" : "off"));

	DBG_ASSERT(initCommIO(), "unable to initialize communication channel.");

	/* Redirect STDOUT to STDERR */
	SetStdHandle(STD_OUTPUT_HANDLE, GetStdHandle(STD_ERROR_HANDLE));

	/* Create the application window */
	WNDCLASSEXA WndClsEx;
	WndClsEx.cbSize        = sizeof(WndClsEx);
	WndClsEx.style         = CS_HREDRAW | CS_VREDRAW;
	WndClsEx.lpfnWndProc   = &wndProcedure;
	WndClsEx.cbClsExtra    = 0;
	WndClsEx.cbWndExtra    = 0;
	WndClsEx.hIcon         = LoadIconA(NULL, (LPCSTR)IDI_APPLICATION);
	WndClsEx.hCursor       = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
	WndClsEx.hbrBackground = NULL; /* (HBRUSH)GetStockObject(LTGRAY_BRUSH); */
	WndClsEx.lpszMenuName  = NULL;
	WndClsEx.lpszClassName = clsName;
	WndClsEx.hInstance     = GetModuleHandleA(NULL);
	WndClsEx.hIconSm       = LoadIconA(NULL, (LPCSTR)IDI_APPLICATION);

	ATOM classAtom = RegisterClassExA(&WndClsEx);
	if (!classAtom){
		DBG_ERROR("failed to register class.");
		return 1;
	}

	/* Install hooks */
	if (unityHacks) 		installUnityHooks();
	if (windowClassHook) 	installWindowClassHook();

	installPopupHook();

	/* Load the DLL */
	if (!initDLL(dllPath, dllName)){
		DBG_ERROR("failed to initialize DLL.");
		return 1;
	}

	DBG_INFO("init successful!");

	Stack stack;
	readCommands(stack, false);

	return 0;
}

/* makeWindowEmbedded */
bool makeWindowEmbedded(NPP instance, HWND hWnd, bool embed = true){
	XID windowIDX11 = (XID)GetPropA(hWnd, "__wine_x11_whole_window");

	if (!windowIDX11){
		DBG_ERROR("Unable to find X11 window ID, embedding not possible");
		return false;
	}

	/* Request change of embedded mode */
	writeInt32(embed);
	writeInt32(windowIDX11);
	writeHandleInstance(instance);

	callFunction(CHANGE_EMBEDDED_MODE);
	readResultVoid();

	return true;
}

/* changeEmbeddedMode */
void changeEmbeddedMode(bool newEmbed){
	if (isEmbeddedMode == newEmbed)
		return;

	/*
		TODO: The following code should theoretically work, but doesn't work yet because of additional wine bugs
		when they are fixed we can allow to toggle embedded mode on-the-fly without restart
	*/

	/*for (std::map<HWND, NPP>::iterator it = hwndToInstance.begin(); it != hwndToInstance.end(); it++){
		HWND hWnd = it->first;

		ShowWindow(hWnd, SW_HIDE);

		makeWindowEmbedded(it->second, hWnd, newEmbed);

		ShowWindow(hWnd, SW_SHOW);
		UpdateWindow(hWnd);

	}*/

	isEmbeddedMode = newEmbed;
}

/* dispatcher */
void dispatcher(int functionid, Stack &stack){
	switch (functionid){

		case INIT_OKAY:
			{
				DBG_TRACE("INIT_OKAY()");

				writeInt32(PIPELIGHT_PROTOCOL_VERSION);

				DBG_TRACE("INIT_OKAY -> %x", PIPELIGHT_PROTOCOL_VERSION);
				returnCommand();
			}
			break;

		case WIN_HANDLE_MANAGER_FREE_NOTIFY_DATA:
			{
				void *notifyData = readHandleNotify(stack, HMGR_SHOULD_EXIST);
				DBG_TRACE("WIN_HANDLE_MANAGER_FREE_NOTIFY_DATA( notifyData=%p )", notifyData);

				handleManager_removeByPtr(HMGR_TYPE_NotifyData, notifyData);

				DBG_TRACE("WIN_HANDLE_MANAGER_FREE_NOTIFY_DATA -> void");
				returnCommand();
			}
			break;

		case WIN_HANDLE_MANAGER_FREE_OBJECT:
			{
				NPObject 	*obj = readHandleObjIncRef(stack, NULL, 0, HMGR_SHOULD_EXIST);
				DBG_TRACE("WIN_HANDLE_MANAGER_FREE_OBJECT( obj=%p )", obj);

				objectKill(obj);

				DBG_TRACE("WIN_HANDLE_MANAGER_FREE_OBJECT -> void");
				returnCommand();
			}
			break;

		case CHANGE_SANDBOX_STATE:
			{
				bool state 			= (bool)readInt32(stack);
				DBG_TRACE("CHANGE_SANDBOX_STATE( state=%d )", state);

				isSandboxed = state;

				DBG_TRACE("CHANGE_SANDBOX_STATE -> void");
				returnCommand();
			}
			break;

		case PROCESS_WINDOW_EVENTS:
			{
				uint64_t remoteHandleCount = readInt64(stack);
				DBG_ASSERT(remoteHandleCount == handleManager_count(), "remote handle count doesn't match local one.");
				MSG msg;
				/* DBG_TRACE("PROCESS_WINDOW_EVENTS()"); */

				DWORD abortTime = GetTickCount() + 80;
				while (!invalidateLinuxWindowless && GetTickCount() < abortTime){
					if (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)){
						TranslateMessage(&msg);
						DispatchMessageA(&msg);

					}else{
						break;
					}
				}

				/* Invalidate rects after returning from the event handler */
				if (isLinuxWindowlessMode){
					writeInt32(INVALIDATE_NOTHING);

					if (invalidateLinuxWindowless){
						for (std::set<NPP>::iterator it = instanceList.begin(); it != instanceList.end(); it++){
							NPP instance 		= *it;
							NetscapeData* ndata = (NetscapeData*)instance->ndata;
							if (!ndata || !ndata->invalidate) continue;

							if (ndata->invalidate == INVALIDATE_RECT)
								writeNPRect(ndata->invalidateRect);

							writeHandleInstance(instance);
							writeInt32(ndata->invalidate);

							ndata->invalidate = INVALIDATE_NOTHING;
						}

						invalidateLinuxWindowless = false;
					}
				}

				/* DBG_TRACE("PROCESS_WINDOW_EVENTS -> void"); */
				returnCommand();
			}
			break;

		case WINDOWLESS_EVENT_REDRAW:
			{
				RECT rect;
				NPP instance 				= readHandleInstance(stack);
				XID drawable 				= readInt32(stack);
				readRECT(stack, rect);
				DBG_TRACE("WINDOWLESS_EVENT_REDRAW( instance=%p, drawable=%lu, left=%ld, top=%ld, right=%ld, bottom=%ld )",
					instance, drawable, rect.left, rect.top, rect.right, rect.bottom);

				NetscapeData* ndata = (NetscapeData*)instance->ndata;
				if (ndata){
					if (ndata->hDC){

						/* update drawable if necessary */
						if (drawable != ndata->lastDrawableDC){
							x11drv_escape_set_drawable escape;

							escape.code 		= X11DRV_SET_DRAWABLE;
							escape.hwnd 		= 0;
							escape.drawable 	= drawable;
							escape.mode 		= 1; /* IncludeInferiors */
							memcpy(&escape.dc_rect, &rect, sizeof(rect));
							escape.fbconfig_id	= 0;

							if (ExtEscape(ndata->hDC, X11DRV_ESCAPE, sizeof(escape), (char *)&escape, 0, NULL))
								ndata->lastDrawableDC = drawable;
							else
								DBG_ERROR("ExtEscape(X11DRV_ESCAPE, ...) failed");
						}

						NPEvent event;
						event.event 	= WM_PAINT;
						event.wParam 	= (uintptr_t)ndata->hDC;
						event.lParam 	= (uintptr_t)&rect;
						pluginFuncs.event(instance, &event);
					}
				}

				DBG_TRACE("WINDOWLESS_EVENT_REDRAW -> void");
				returnCommand();
			}
			break;

		case FUNCTION_GET_VERSION:
			{
				DBG_TRACE("FUNCTION_GET_VERSION()");

				writeString(np_FileDescription);

				DBG_TRACE("FUNCTION_GET_VERSION -> str='%s'", np_FileDescription.c_str());
				returnCommand();
			}
			break;

		case FUNCTION_GET_MIMETYPE:
			{
				DBG_TRACE("FUNCTION_GET_MIMETYPE()");

				std::string mimeType = createLinuxCompatibleMimeType();
				writeString(mimeType);

				DBG_TRACE("FUNCTION_GET_MIMETYPE -> str='%s'", mimeType.c_str());
				returnCommand();
			}
			break;

		case FUNCTION_GET_NAME:
			{
				DBG_TRACE("FUNCTION_GET_NAME()");

				writeString(np_ProductName);

				DBG_TRACE("FUNCTION_GET_NAME -> str='%s'", np_ProductName.c_str());
				returnCommand();
			}
			break;

		case FUNCTION_GET_DESCRIPTION:
			{
				DBG_TRACE("FUNCTION_GET_DESCRIPTION()");

				writeString(np_FileDescription);

				DBG_TRACE("FUNCTION_GET_DESCRIPTION -> str='%s'", np_FileDescription.c_str());
				returnCommand();
			}
			break;


		case FUNCTION_NP_INVOKE:
			{
				NPObject 	*obj 			= readHandleObjIncRef(stack);
				NPIdentifier name 			= readHandleIdentifier(stack);
				uint32_t argCount 			= readInt32(stack);
				std::vector<NPVariant> args = readVariantArrayIncRef(stack, argCount);
				NPVariant resultVariant;
				resultVariant.type 				= NPVariantType_Void;
				resultVariant.value.objectValue = NULL;
				DBG_TRACE("FUNCTION_NP_INVOKE( obj=%p, name=%p, argCount=%d, args=%p )", obj, name, argCount, args.data());

				bool result = obj->_class->invoke(obj, name, args.data(), argCount, &resultVariant);

				/*
					The objects refcount has been incremented by invoke
					Return the variant without modifying the objects refcount
				*/
				if (result)
					writeVariantReleaseDecRef(resultVariant);

				/* This frees ONLY all the strings! */
				freeVariantArrayDecRef(args);
				objectDecRef(obj);

				writeInt32(result);

				DBG_TRACE("FUNCTION_NP_INVOKE -> ( result=%d, ... )", result);
				returnCommand();
			}
			break;

		case FUNCTION_NP_INVOKE_DEFAULT:
			{
				NPObject 	*obj 			= readHandleObjIncRef(stack);
				uint32_t argCount 			= readInt32(stack);
				std::vector<NPVariant> args = readVariantArrayIncRef(stack, argCount);
				NPVariant resultVariant;
				resultVariant.type 				= NPVariantType_Void;
				resultVariant.value.objectValue = NULL;
				DBG_TRACE("FUNCTION_NP_INVOKE_DEFAULT( obj=%p, argCount=%d, args=%p )", obj, argCount, args.data());

				bool result = obj->_class->invokeDefault(obj, args.data(), argCount, &resultVariant);

				/*
					The objects refcount has been incremented by invoke
					Return the variant without modifying the objects refcount
				*/
				if (result)
					writeVariantReleaseDecRef(resultVariant);

				/* This frees ONLY all the strings! */
				freeVariantArrayDecRef(args);
				writeInt32(result);
				objectDecRef(obj);

				DBG_TRACE("FUNCTION_NP_INVOKE_DEFAULT -> ( result=%d, ... )", result);
				returnCommand();
			}
			break;

		case FUNCTION_NP_HAS_PROPERTY:
			{
				NPObject *obj 		= readHandleObjIncRef(stack);
				NPIdentifier name 	= readHandleIdentifier(stack);
				DBG_TRACE("FUNCTION_NP_HAS_PROPERTY( obj=%p, name=%p )", obj, name);

				bool result = obj->_class->hasProperty(obj, name);
				writeInt32(result);
				objectDecRef(obj);

				DBG_TRACE("FUNCTION_NP_HAS_PROPERTY -> result=%d", result);
				returnCommand();
			}
			break;

		case FUNCTION_NP_HAS_METHOD:
			{
				NPObject *obj 		= readHandleObjIncRef(stack);
				NPIdentifier name 	= readHandleIdentifier(stack);
				DBG_TRACE("FUNCTION_NP_HAS_METHOD( obj=%p, name=%p )", obj, name);

				bool result = obj->_class->hasMethod(obj, name);
				writeInt32(result);
				objectDecRef(obj);

				DBG_TRACE("FUNCTION_NP_HAS_METHOD -> result=%d", result);
				returnCommand();
			}
			break;

		case FUNCTION_NP_GET_PROPERTY:
			{
				NPObject *obj 		= readHandleObjIncRef(stack);
				NPIdentifier name 	= readHandleIdentifier(stack);
				NPVariant resultVariant;
				resultVariant.type 				= NPVariantType_Void;
				resultVariant.value.objectValue = NULL;
				DBG_TRACE("FUNCTION_NP_GET_PROPERTY( obj=%p, name=%p )", obj, name);

				bool result = obj->_class->getProperty(obj, name, &resultVariant);

				if (result)
					writeVariantReleaseDecRef(resultVariant);

				objectDecRef(obj);
				writeInt32(result);

				DBG_TRACE("FUNCTION_NP_GET_PROPERTY -> ( result=%d, ... )", result);
				returnCommand();
			}
			break;

		case FUNCTION_NP_SET_PROPERTY:
			{
				NPObject 		*obj 		= readHandleObjIncRef(stack);
				NPIdentifier 	name 		= readHandleIdentifier(stack);
				NPVariant 		variant;
				readVariantIncRef(stack, variant);
				DBG_TRACE("FUNCTION_NP_SET_PROPERTY( obj=%p, name=%p, variant=%p )", obj, name, &variant);

				bool result = obj->_class->setProperty(obj, name, &variant);
				freeVariantDecRef(variant);
				writeInt32(result);
				objectDecRef(obj);

				DBG_TRACE("FUNCTION_NP_SET_PROPERTY -> result=%d", result);
				returnCommand();
			}
			break;

		case FUNCTION_NP_REMOVE_PROPERTY:
			{
				NPObject 		*obj 		= readHandleObjIncRef(stack);
				NPIdentifier 	name 		= readHandleIdentifier(stack);
				DBG_TRACE("FUNCTION_NP_REMOVE_PROPERTY( obj=%p, name=%p )", obj, name);

				bool result = obj->_class->removeProperty(obj, name);
				writeInt32(result);
				objectDecRef(obj);

				DBG_TRACE("FUNCTION_NP_REMOVE_PROPERTY -> result=%d", result);
				returnCommand();
			}
			break;

		case FUNCTION_NP_ENUMERATE:
			{
				NPObject 		*obj 		= readHandleObjIncRef(stack);
				NPIdentifier*   identifierTable  = NULL;
				uint32_t 		identifierCount  = 0;
				DBG_TRACE("FUNCTION_NP_ENUMERATE( obj=%p )", obj);

				bool result = obj->_class->enumerate && obj->_class->enumerate(obj, &identifierTable, &identifierCount);

				if (result){
					writeIdentifierArray(identifierTable, identifierCount);
					writeInt32(identifierCount);

					/* Free the memory for the table */
					if (identifierTable)
						free(identifierTable);
				}

				objectDecRef(obj);
				writeInt32(result);

				DBG_TRACE("FUNCTION_NP_ENUMERATE -> ( result=%d, ... )", result);
				returnCommand();
			}
			break;

		case FUNCTION_NP_INVALIDATE:
			{
				NPObject *obj = readHandleObjIncRef(stack);
				DBG_TRACE("FUNCTION_NP_INVALIDATE( obj=%p )", obj);

				obj->_class->invalidate(obj);
				objectDecRef(obj);

				DBG_TRACE("FUNCTION_NP_INVALIDATE -> void");
				returnCommand();
			}
			break;


		case FUNCTION_NPP_NEW:
			{
				std::shared_ptr<char> mimeType		= readStringAsMemory(stack);
				NPP instance						= readHandleInstance(stack);
				uint16_t mode 						= readInt32(stack);
				int16_t argc 						= readInt32(stack);
				std::vector<char*> argn 			= readStringArray(stack, argc);
				std::vector<char*> argv 			= readStringArray(stack, argc);

				size_t saved_length;
				char* saved_data 					= readMemoryMalloc(stack, saved_length);

				NPSavedData saved;
				NPSavedData* savedPtr = NULL;

				/* NOTE: The plugin is responsible for freeing the saved memory when not required anymore! */
				if (saved_data){
					saved.buf 	= saved_data;
					saved.len 	= saved_length;
					savedPtr 	= &saved;
				}

				DBG_TRACE("FUNCTION_NPP_NEW( mimeType='%s', instance=%p, mode=%d, argc=%d, argn=%p, argv=%p, saved=%p )", \
						mimeType.get(), instance, mode, argc, argn.data(), argv.data(), savedPtr);

				/* Most plugins only support windowlessMode in combination with NP_EMBED */
				if (isWindowlessMode)
					mode = NP_EMBED;

				/* Set privata data before calling the plugin, it might already use some commands */
				NetscapeData* ndata = (NetscapeData*)malloc(sizeof(NetscapeData));
				if (ndata){
					instance->ndata = ndata;

					memset(ndata, 0, sizeof(*ndata));
					ndata->windowlessMode 				= isWindowlessMode;
					ndata->embeddedMode 				= isEmbeddedMode;
					ndata->cache_pluginElementNPObject 	= NULL;
					ndata->cache_clientWidthIdentifier  = NULL;
					ndata->hWnd 						= NULL;
					ndata->hDC 							= NULL;

					if (NPN_GetValue(instance, NPNVPluginElementNPObject, &ndata->cache_pluginElementNPObject) != NPERR_NO_ERROR)
						DBG_ERROR("unable to get plugin element NPObject.");

					if (!(ndata->cache_clientWidthIdentifier = NPN_GetStringIdentifier("clientWidth")))
						DBG_ERROR("unable to get clientWidth identifier.");

				}else{
					instance->ndata = NULL;
					DBG_ERROR("unable to allocate memory for private data.");
				}

				/* append to the instance list */
				instanceList.insert(instance);

				NPError result = pluginFuncs.newp(mimeType.get(), instance, mode, argc, argn.data(), argv.data(), savedPtr);

				/* TODO: Do we have to deallocate the privata data or does NPP_DESTROY get called? */

				/* Free the arrays before returning */
				freeStringArray(argn);
				freeStringArray(argv);

				writeInt32(result);

				DBG_TRACE("FUNCTION_NPP_NEW -> result=%d", result);
				returnCommand();
			}
			break;

		case FUNCTION_NPP_DESTROY:
			{
				NPSavedData* saved 	= NULL;
				NPP instance 		= readHandleInstance(stack);
				DBG_TRACE("FUNCTION_NPP_DESTROY( instance=%p )", instance);

				NPError result 		= pluginFuncs.destroy(instance, &saved);

				/* Destroy the pointers */
				NetscapeData* ndata = (NetscapeData*)instance->ndata;
				if (ndata){

					/* Destroy the window itself and any allocated DCs */
					if (ndata->hWnd){
						hwndToInstance.erase(ndata->hWnd);

						if (ndata->window.type == NPWindowTypeDrawable)
							ReleaseDC(ndata->hWnd, (HDC)ndata->window.window);

						DestroyWindow(ndata->hWnd);
					}

					if (ndata->hDC)
						DeleteDC(ndata->hDC);

					if (ndata->cache_pluginElementNPObject)
						NPN_ReleaseObject(ndata->cache_pluginElementNPObject);

					/* not necessary to free the value ndata->cache_clientWidthIdentifier */

					/* Free this structure */
					free(ndata);
					instance->ndata = NULL;
				}

				/* remove from to the instance list */
				instanceList.erase(instance);

				handleManager_removeByPtr(HMGR_TYPE_NPPInstance, instance);

				free(instance);

				if (result == NPERR_NO_ERROR){
					if (saved){
						writeMemory((char*)saved->buf, saved->len);

						if (saved->buf) free((char*)saved->buf);
						free(saved);

					}else{
						writeMemory(NULL, 0);
					}
				}

				writeInt32(result);

				DBG_TRACE("FUNCTION_NPP_DESTROY -> ( result=%d, ... )", result);
				returnCommand();
			}
			break;

		case FUNCTION_NPP_GETVALUE_BOOL:
			{
				NPP instance 			= readHandleInstance(stack);
				NPPVariable variable 	= (NPPVariable)readInt32(stack);
				DBG_TRACE("FUNCTION_NPP_GETVALUE_BOOL( instance=%p, variable=%d )", instance, variable);

				PRBool boolValue;
				NPError result;

				if (false){ /* not used at the moment */
					result = pluginFuncs.getvalue(instance, variable, &boolValue);

				}else{
					DBG_WARN("FUNCTION_NPP_GETVALUE_BOOL - variable %d not allowed", variable);
					result = NPERR_GENERIC_ERROR;
				}

				if(result == NPERR_NO_ERROR)
					writeInt32(boolValue);

				writeInt32(result);

				DBG_TRACE("FUNCTION_NPP_GETVALUE_BOOL -> ( result=%d, ... )", result);
				returnCommand();
			}
			break;

		case FUNCTION_NPP_GETVALUE_OBJECT:
			{
				NPP instance 			= readHandleInstance(stack);
				NPPVariable variable 	= (NPPVariable)readInt32(stack);
				DBG_TRACE("FUNCTION_NPP_GETVALUE_OBJECT( instance=%p, variable=%d )", instance, variable);

				NPObject *objectValue;
				NPError result;

				if (variable == NPPVpluginScriptableNPObject){
					result = pluginFuncs.getvalue(instance, variable, &objectValue);

				}else{
					DBG_WARN("FUNCTION_NPP_GETVALUE_OBJECT - variable %d not allowed", variable);
					result = NPERR_GENERIC_ERROR;
				}

				if (result == NPERR_NO_ERROR)
					writeHandleObjDecRef(objectValue);

				writeInt32(result);

				DBG_TRACE("FUNCTION_NPP_GETVALUE_OBJECT -> ( result=%d, ... )", result);
				returnCommand();
			}
			break;

		case FUNCTION_NPP_SET_WINDOW:
			{
				POINT pt;
				NPP instance 		= readHandleInstance(stack);
				readPOINT(stack, pt);
				DBG_TRACE("FUNCTION_NPP_SET_WINDOW( instance=%p, width=%d, height=%d )", instance, pt.x, pt.y);

				NetscapeData* ndata = (NetscapeData*)instance->ndata;
				if (ndata){
					if (!isLinuxWindowlessMode){ /* regular mode */

						DWORD style, extStyle;
						int posX, posY;
						RECT rect;

						/* Get style flags */
						if (ndata->embeddedMode){
							style 		= WS_POPUP;
							extStyle 	= WS_EX_TOOLWINDOW;
							posX		= 0;
							posY		= 0;
						}else{
							style 		= WS_TILEDWINDOW;
							extStyle 	= 0;
							posX 		= CW_USEDEFAULT;
							posY 		= CW_USEDEFAULT;
						}

						/* Calculate size including borders */
						rect.left 	= 0;
						rect.top	= 0;
						rect.right 	= pt.x;
						rect.bottom = pt.y;
						AdjustWindowRectEx(&rect, style, false, extStyle);

						if (!ndata->hWnd){

							/* Create the actual window */
							ndata->hWnd = CreateWindowExA(extStyle, clsName, "Plugin", style, posX, posY, rect.right - rect.left, rect.bottom - rect.top, 0, 0, 0, 0);
							if (ndata->hWnd){
								hwndToInstance.insert( std::pair<HWND, NPP>(ndata->hWnd, instance) );

								if (ndata->embeddedMode)
									makeWindowEmbedded(instance, ndata->hWnd);

								ShowWindow(ndata->hWnd, SW_SHOW);
								UpdateWindow(ndata->hWnd);

								/* Only do this once to prevent leaking DCs */
								if (ndata->windowlessMode){
									ndata->window.window 		= GetDC(ndata->hWnd);
									ndata->window.type 			= NPWindowTypeDrawable;
								}else{
									ndata->window.window 		= ndata->hWnd;
									ndata->window.type 			= NPWindowTypeWindow;
								}

							}else
								DBG_ERROR("failed to create window!");

						}else
							SetWindowPos(ndata->hWnd, 0, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOACTIVATE | SWP_NOMOVE);

					}else{ /* linux windowless mode */

						if (!ndata->hDC){
							ndata->hDC = CreateDCA("DISPLAY", NULL, NULL, NULL);
							ndata->lastDrawableDC = 0;
						}

						ndata->window.window 	= ndata->hDC;
						ndata->window.type   	= NPWindowTypeDrawable;

						if (!ndata->hDC)
							DBG_ERROR("failed to create DC!");

					}

					/* send data to plugin */
					if (ndata->hWnd || ndata->hDC){
						ndata->window.x 				= 0;
						ndata->window.y 				= 0;
						ndata->window.width 			= pt.x;
						ndata->window.height 			= pt.y;
						ndata->window.clipRect.top 		= 0;
						ndata->window.clipRect.left 	= 0;
						ndata->window.clipRect.right 	= pt.x;
						ndata->window.clipRect.bottom 	= pt.y;

						pluginFuncs.setwindow(instance, &ndata->window);

						/* force redrawing */
						NPN_InvalidateRect(instance, NULL);
					}

				}else{
					DBG_ERROR("unable to allocate window because of missing ndata.");
				}

				DBG_TRACE("FUNCTION_NPP_SET_WINDOW -> void");
				returnCommand();
			}
			break;

		case FUNCTION_NPP_NEW_STREAM:
			{
				NPP instance 					= readHandleInstance(stack);
				std::shared_ptr<char> type 		= readStringAsMemory(stack);
				NPStream *stream 				= readHandleStream(stack, HMGR_SHOULD_NOT_EXIST);
				NPBool seekable					= (NPBool) readInt32(stack);
				DBG_TRACE("FUNCTION_NPP_NEW_STREAM( instance=%p, type='%s', stream=%p, seekable=%d )", instance, type.get(), stream, seekable);

				uint16_t stype = NP_NORMAL; /* Fix for silverlight.... */
				NPError result = pluginFuncs.newstream(instance, type.get(), stream, seekable, &stype);

				/* Return result */
				if (result == NPERR_NO_ERROR){
					writeInt32(stype);
				}else{ /* Handle is now invalid because of this error */
					handleManager_removeByPtr(HMGR_TYPE_NPStream, stream);
				}

				writeInt32(result);

				DBG_TRACE("FUNCTION_NPP_NEW_STREAM -> result=%d", result);
				returnCommand();
			}
			break;

		case FUNCTION_NPP_DESTROY_STREAM:
			{
				NPP instance 		= readHandleInstance(stack);
				NPStream* stream 	= readHandleStream(stack, HMGR_SHOULD_EXIST);
				NPReason reason 	= (NPReason)readInt32(stack);
				DBG_TRACE("FUNCTION_NPP_DESTROY_STREAM( instance=%p, stream=%p, reason=%d )", instance, stream, reason);

				NPError result = pluginFuncs.destroystream(instance, stream, reason);

				/* Free data */
				if (stream){

					/* Let the handlemanager remove this one */
					handleManager_removeByPtr(HMGR_TYPE_NPStream, stream);

					if (stream->url) 		free((char*)stream->url);
					if (stream->headers) 	free((char*)stream->headers);
					free(stream);
				}

				writeInt32(result);

				DBG_TRACE("FUNCTION_NPP_DESTROY_STREAM -> result=%d", result);
				returnCommand();
			}
			break;

		case FUNCTION_NPP_WRITE_READY:
			{
				NPP instance 		= readHandleInstance(stack);
				NPStream* stream 	= readHandleStream(stack, HMGR_SHOULD_EXIST);

				DBG_TRACE("FUNCTION_NPP_WRITE_READY( instance=%p, stream=%p )", instance, stream);

				int32_t result = pluginFuncs.writeready(instance, stream);
				writeInt32(result);

				DBG_TRACE("FUNCTION_NPP_WRITE_READY -> result=%d", result);
				returnCommand();
			}
			break;

		case FUNCTION_NPP_WRITE:
			{
				NPP instance 		= readHandleInstance(stack);
				NPStream* stream 	= readHandleStream(stack, HMGR_SHOULD_EXIST);
				int32_t offset 		= readInt32(stack);
				size_t length;
				std::shared_ptr<char> data = readMemory(stack, length);
				DBG_TRACE("FUNCTION_NPP_WRITE( instance=%p, stream=%p, offset=%d, length=%d, data=%p )", instance, stream, offset, length, data.get());

				int32_t result = pluginFuncs.write(instance, stream, offset, length, data.get());
				writeInt32(result);

				DBG_TRACE("FUNCTION_NPP_WRITE -> result=%d", result);
				returnCommand();
			}
			break;

		case FUNCTION_NPP_URL_NOTIFY:
			{
				NPP instance 				= readHandleInstance(stack);
				std::shared_ptr<char> url 	= readStringAsMemory(stack);
				NPReason reason 			= (NPReason) readInt32(stack);
				void *notifyData 			= readHandleNotify(stack, HMGR_SHOULD_EXIST);
				DBG_TRACE("FUNCTION_NPP_URL_NOTIFY( instance=%p, url='%s', reason=%d, notifyData=%p )", instance, url.get(), reason, notifyData);

				pluginFuncs.urlnotify(instance, url.get(), reason, notifyData);

				DBG_TRACE("FUNCTION_NPP_URL_NOTIFY -> void");
				returnCommand();
			}
			break;

		case FUNCTION_NPP_STREAM_AS_FILE:
			{
				NPP instance 				= readHandleInstance(stack);
				NPStream* stream 			= readHandleStream(stack, HMGR_SHOULD_EXIST);
				std::string fname_lin       = readString(stack);
				DBG_TRACE("FUNCTION_NPP_STREAM_AS_FILE( instance=%p, stream=%p, fname='%s' )", instance, stream, fname_lin.c_str());

				std::string fname 			= convertToWindowsPath(fname_lin);
				if (fname != "" && checkIsFile(fname)){
					DBG_TRACE("windows filename: '%s'.", fname.c_str());

					pluginFuncs.asfile(instance, stream, fname.c_str());

				}else{
					DBG_ERROR("unable to access linux stream '%s' as file.", fname_lin.c_str());
				}

				DBG_TRACE("FUNCTION_NPP_STREAM_AS_FILE -> void");
				returnCommand();
			}
			break;

		case NP_SHUTDOWN:
			{
				DBG_TRACE("NP_SHUTDOWN()");

				/* TODO: Implement deinitialization! We dont call Shutdown, as otherwise we would have to call Initialize again! */

				DBG_TRACE("NP_SHUTDOWN -> void");
				returnCommand();
			}
			break;

		default:
			DBG_ABORT("specified function %d not found!", functionid);
			break;

	}
}