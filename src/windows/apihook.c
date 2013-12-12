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

#include <vector>								// for std::vector
#include "../common/common.h"
#include "pluginloader.h"

#include <windows.h>							// for PVOID and other types
#include <string.h>								// for memset

void* patchDLLExport(PVOID ModuleBase, const char* functionName, void* newFunctionPtr){
	/*
		Based on the following source code:
		http://alter.org.ua/docs/nt_kernel/procaddr/#RtlImageDirectoryEntryToData
	*/

	PIMAGE_DOS_HEADER dos              =    (PIMAGE_DOS_HEADER) ModuleBase;
	PIMAGE_NT_HEADERS nt               =    (PIMAGE_NT_HEADERS)((ULONG) ModuleBase + dos->e_lfanew);

	PIMAGE_DATA_DIRECTORY expdir       =   (PIMAGE_DATA_DIRECTORY)(nt->OptionalHeader.DataDirectory + IMAGE_DIRECTORY_ENTRY_EXPORT);
	ULONG                 addr         =   expdir->VirtualAddress;
	PIMAGE_EXPORT_DIRECTORY exports    =   (PIMAGE_EXPORT_DIRECTORY)((ULONG) ModuleBase + addr);

	PULONG functions =  (PULONG)((ULONG) ModuleBase + exports->AddressOfFunctions);
	PSHORT ordinals  =  (PSHORT)((ULONG) ModuleBase + exports->AddressOfNameOrdinals);
	PULONG names     =  (PULONG)((ULONG) ModuleBase + exports->AddressOfNames);
	ULONG  max_name  =  exports->NumberOfNames;
	ULONG  max_func  =  exports->NumberOfFunctions;

	ULONG i;
	DWORD oldProtect;

	for (i = 0; i < max_name; i++)
	{
		ULONG ord = ordinals[i];
		if (i >= max_name || ord >= max_func)
			break;

		if (strcmp( (PCHAR) ModuleBase + names[i], functionName ) == 0){
			if (!VirtualProtect(&functions[ord], sizeof(ULONG), PAGE_EXECUTE_READWRITE, &oldProtect))
				return NULL;

			DBG_INFO("replaced API function %s.", functionName);

			void* oldFunctionPtr = (PVOID)((PCHAR) ModuleBase + functions[ord]);
			functions[ord] = (ULONG)newFunctionPtr - (ULONG)ModuleBase;

			VirtualProtect(&functions[ord], sizeof(ULONG), oldProtect, &oldProtect);
			return oldFunctionPtr;
		}

	}

	return NULL;
};

/* -------- Popup menu hook --------*/

enum MenuAction{
	MENU_ACTION_NONE,
	MENU_ACTION_ABOUT_PIPELIGHT,
	MENU_ACTION_TOGGLE_EMBED,
	MENU_ACTION_TOGGLE_STAY_IN_FULLSCREEN
};

struct MenuEntry{
	UINT 		identifier;
	MenuAction 	action;

	MenuEntry(UINT identifier, MenuAction action){
		this->identifier = identifier;
		this->action 	 = action;
	}
};

#define MENUID_OFFSET 0x50495045 /* 'PIPE' */

std::vector<MenuEntry> menuAddEntries(HMENU hMenu, HWND hwnd){
	std::vector<MenuEntry> 	entries;
	MENUITEMINFOA			entryInfo;
	std::string 			temp;

	int count = GetMenuItemCount(hMenu);
	if(count == -1)
		return entries;

	memset(&entryInfo, 0, sizeof(entryInfo));
	entryInfo.cbSize	= sizeof(entryInfo);
	entryInfo.wID 		= MENUID_OFFSET;

	/* ------- Separator ------- */
	entryInfo.fMask		= MIIM_FTYPE | MIIM_ID;
	entryInfo.fType		= MFT_SEPARATOR;
	InsertMenuItemA(hMenu, count, true, &entryInfo);
	entries.emplace_back(entryInfo.wID, MENU_ACTION_NONE);
	count++; entryInfo.wID++;

	/* ------- About Pipelight ------- */
	entryInfo.fMask			= MIIM_FTYPE | MIIM_STRING | MIIM_ID;
	entryInfo.fType			= MFT_STRING;
	entryInfo.dwTypeData 	= (char*)"Pipelight\t" PIPELIGHT_VERSION;
	InsertMenuItemA(hMenu, count, true, &entryInfo);
	entries.emplace_back(entryInfo.wID, MENU_ACTION_ABOUT_PIPELIGHT);
	count++; entryInfo.wID++;

	/* ------- Wine version ------- */
	temp  = "Wine\t";
	temp += getWineVersion();
	entryInfo.fMask			= MIIM_FTYPE | MIIM_STRING | MIIM_ID | MIIM_STATE;
	entryInfo.fType			= MFT_STRING;
	entryInfo.fState		= MFS_DISABLED;
	entryInfo.dwTypeData 	= (char*)temp.c_str();
	InsertMenuItemA(hMenu, count, true, &entryInfo);
	entries.emplace_back(entryInfo.wID, MENU_ACTION_NONE);
	count++; entryInfo.wID++;

	/* ------- Sandbox status ------- */
	temp  = "Sandbox\t";
	temp += isSandboxed ? "enabled" : "disabled";
	entryInfo.fMask			= MIIM_FTYPE | MIIM_STRING | MIIM_ID | MIIM_STATE;
	entryInfo.fType			= MFT_STRING;
	entryInfo.fState		= MFS_DISABLED;
	entryInfo.dwTypeData 	= (char*)temp.c_str();
	InsertMenuItemA(hMenu, count, true, &entryInfo);
	entries.emplace_back(entryInfo.wID, MENU_ACTION_NONE);
	count++; entryInfo.wID++;

	/* ------- Separator ------- */
	entryInfo.fMask		= MIIM_FTYPE | MIIM_ID;
	entryInfo.fType		= MFT_SEPARATOR;
	InsertMenuItemA(hMenu, count, true, &entryInfo);
	entries.emplace_back(entryInfo.wID, MENU_ACTION_NONE);
	count++; entryInfo.wID++;

	/* ------- Embed into browser ------- */
	entryInfo.fMask			= MIIM_FTYPE | MIIM_STRING | MIIM_ID | MIIM_STATE;
	entryInfo.fType			= MFT_STRING;
	entryInfo.fState        = isEmbeddedMode ? MFS_CHECKED : 0;
	entryInfo.dwTypeData 	= (char*)"Embed into browser";
	InsertMenuItemA(hMenu, count, true, &entryInfo);
	entries.emplace_back(entryInfo.wID, MENU_ACTION_TOGGLE_EMBED);
	count++; entryInfo.wID++;

	/* ------- Stay in fullscreen ------- */
	if (windowClassHook){
		entryInfo.fMask			= MIIM_FTYPE | MIIM_STRING | MIIM_ID | MIIM_STATE;
		entryInfo.fType			= MFT_STRING;
		entryInfo.fState        = stayInFullscreen ? MFS_CHECKED : 0;
		entryInfo.dwTypeData 	= (char*)"Stay in fullscreen";
		InsertMenuItemA(hMenu, count, true, &entryInfo);
		entries.emplace_back(entryInfo.wID, MENU_ACTION_TOGGLE_STAY_IN_FULLSCREEN);
		count++; entryInfo.wID++;
	}

	return entries;

}

void menuRemoveEntries(HMENU hMenu, const std::vector<MenuEntry> &entries){
	for (std::vector<MenuEntry>::const_iterator it = entries.begin(); it != entries.end(); it++)
		RemoveMenu(hMenu, it->identifier, MF_BYCOMMAND);
}

bool menuHandler(NPP instance, UINT identifier, const std::vector<MenuEntry> &entries){
	for (std::vector<MenuEntry>::const_iterator it = entries.begin(); it != entries.end(); it++){
		if (it->identifier != identifier) continue;

		switch (it->action){

			case MENU_ACTION_TOGGLE_EMBED:
				changeEmbeddedMode(!isEmbeddedMode);
				break;

			case MENU_ACTION_ABOUT_PIPELIGHT:
				NPN_PushPopupsEnabledState(instance, PR_TRUE);
				NPN_GetURL(instance, "https://launchpad.net/pipelight", "_blank");
				NPN_PopPopupsEnabledState(instance);
				break;

			case MENU_ACTION_TOGGLE_STAY_IN_FULLSCREEN:
				stayInFullscreen = !stayInFullscreen;
				break;

			default:
				break;

		}
		return true;
	}

	return false;
}

typedef BOOL (* WINAPI TrackPopupMenuExPtr)(HMENU hMenu, UINT fuFlags, int x, int y, HWND hWnd, LPTPMPARAMS lptpm);
typedef BOOL (* WINAPI TrackPopupMenuPtr)(HMENU hMenu, UINT uFlags, int x, int y, int nReserved, HWND hWnd, const RECT *prcRect);

TrackPopupMenuExPtr originalTrackPopupMenuEx    = NULL;
TrackPopupMenuPtr 	originalTrackPopupMenu  	= NULL;

/*
	One disadvantage of our current implementation of the hook is that the
	return value is not completely correct since we are using TPM_RETURNCMD
	and we can not distinguish whether the user didn't not select anything
	or if there was an error. Both cases would return 0 when using
	TPM_RETURNCMD. So we always return true (assuming that there is no error)
	if the hook was called without TPM_RETURNCMD as flag and the return value
	of originalTrackPopupMenu(Ex) is 0.

	The return value of TrackPopupMenu(Ex) is really defined as BOOL although
	it can contain an ID, which may look wrong on the first sight.
*/

BOOL WINAPI myTrackPopupMenuEx(HMENU hMenu, UINT fuFlags, int x, int y, HWND hWnd, LPTPMPARAMS lptpm){

	/* Called from wrong thread -> redirect without intercepting the call */
	if (GetCurrentThreadId() != mainThreadID)
		return originalTrackPopupMenuEx(hMenu, fuFlags, x, y, hWnd, lptpm);

	/* Find the specific instance */
	std::map<HWND, NPP>::iterator it = hwndToInstance.find(hWnd);
	if (it == hwndToInstance.end())
		return originalTrackPopupMenuEx(hMenu, fuFlags, x, y, hWnd, lptpm);

	NPP instance = it->second;

	/* Don't send messages to windows, but return the identifier as return value */
	UINT newFlags = (fuFlags & ~TPM_NONOTIFY) | TPM_RETURNCMD;

	std::vector<MenuEntry> entries = menuAddEntries(hMenu, hWnd);
	BOOL identifier = originalTrackPopupMenuEx(hMenu, newFlags, x, y, hWnd, lptpm);
	menuRemoveEntries(hMenu, entries);

	if (!identifier || menuHandler(instance, identifier, entries))
		return (fuFlags & TPM_RETURNCMD) ? 0 : true;

	if (!(fuFlags & TPM_NONOTIFY))
		PostMessageA(hWnd, WM_COMMAND, identifier, 0);

	return (fuFlags & TPM_RETURNCMD) ? identifier : true;
}

BOOL WINAPI myTrackPopupMenu(HMENU hMenu, UINT uFlags, int x, int y, int nReserved, HWND hWnd, const RECT *prcRect){

	/* Called from wrong thread -> redirect without intercepting the call */
	if (GetCurrentThreadId() != mainThreadID)
		return originalTrackPopupMenu(hMenu, uFlags, x, y, nReserved, hWnd, prcRect);

	/* Find the specific instance */
	std::map<HWND, NPP>::iterator it;
	HWND instancehWnd = hWnd;

	while (instancehWnd && instancehWnd != GetDesktopWindow()){
		it = hwndToInstance.find(instancehWnd);
		if (it != hwndToInstance.end()) break;
		instancehWnd = GetParent(instancehWnd);
	}

	if (it == hwndToInstance.end())
		return originalTrackPopupMenu(hMenu, uFlags, x, y, nReserved, hWnd, prcRect);

	NPP instance = it->second;

	/* Don't send messages to windows, but return the identifier as return value */
	UINT newFlags = (uFlags & ~TPM_NONOTIFY) | TPM_RETURNCMD;

	std::vector<MenuEntry> entries = menuAddEntries(hMenu, hWnd);
	BOOL identifier = originalTrackPopupMenu(hMenu, newFlags, x, y, nReserved, hWnd, prcRect);
	menuRemoveEntries(hMenu, entries);

	if (!identifier || menuHandler(instance, identifier, entries))
		return (uFlags & TPM_RETURNCMD) ? identifier : true;

	if (!(uFlags & TPM_NONOTIFY))
		PostMessageA(hWnd, WM_COMMAND, identifier, 0);

	return (uFlags & TPM_RETURNCMD) ? identifier : true;
}

bool installPopupHook(){
	HMODULE user32 = LoadLibraryA("user32.dll");

	if (!user32)
		return false;

	if (!originalTrackPopupMenuEx)
		originalTrackPopupMenuEx    = (TrackPopupMenuExPtr)	patchDLLExport(user32, "TrackPopupMenuEx", (void*)&myTrackPopupMenuEx);

	if (!originalTrackPopupMenu)
		originalTrackPopupMenu   	= (TrackPopupMenuPtr)	patchDLLExport(user32, "TrackPopupMenu", (void*)&myTrackPopupMenu);

	return (originalTrackPopupMenuEx && originalTrackPopupMenu);
}

/* -------- CreateWindowEx hooks --------*/

typedef HWND (* WINAPI CreateWindowExAPtr)(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int x, int y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam);
typedef HWND (* WINAPI CreateWindowExWPtr)(DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName, DWORD dwStyle, int x, int y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam);

CreateWindowExAPtr originalCreateWindowExA = NULL;
CreateWindowExWPtr originalCreateWindowExW = NULL;

std::map<HWND, WNDPROC> prevWndProcMap;
CRITICAL_SECTION        prevWndProcCS;

LRESULT CALLBACK wndHookProcedureA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam){
	WNDPROC prevWndProc = NULL;

	EnterCriticalSection(&prevWndProcCS);

	std::map<HWND, WNDPROC>::iterator it = prevWndProcMap.find(hWnd);
	if (it != prevWndProcMap.end()){
		prevWndProc = it->second;
		if (Msg == WM_DESTROY){
			prevWndProcMap.erase(it);
			DBG_TRACE("fullscreen window %p has been destroyed.", hWnd);
		}
	}

	LeaveCriticalSection(&prevWndProcCS);

	if (!prevWndProc || (stayInFullscreen && Msg == WM_KILLFOCUS))
		return 0;

	return CallWindowProcA(prevWndProc, hWnd, Msg, wParam, lParam);
}

LRESULT CALLBACK wndHookProcedureW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam){
	WNDPROC prevWndProc = NULL;

	EnterCriticalSection(&prevWndProcCS);

	std::map<HWND, WNDPROC>::iterator it = prevWndProcMap.find(hWnd);
	if (it != prevWndProcMap.end()){
		prevWndProc = it->second;
		if (Msg == WM_DESTROY){
			prevWndProcMap.erase(it);
			DBG_TRACE("fullscreen window %p has been destroyed.", hWnd);
		}
	}

	LeaveCriticalSection(&prevWndProcCS);

	if (!prevWndProc || (stayInFullscreen && Msg == WM_KILLFOCUS))
		return 0;

	return CallWindowProcW(prevWndProc, hWnd, Msg, wParam, lParam);
}

bool hookFullscreenClass(HWND hWnd, std::string classname, bool unicode){

	if (classname != "AGFullScreenWinClass" && classname != "ShockwaveFlashFullScreen")
		return false;

	DBG_INFO("hooking fullscreen window with hWnd %p and classname '%s'.", hWnd, classname.c_str());

	/* create the actual hook */
	WNDPROC prevWndProc = (WNDPROC)SetWindowLongPtrA(hWnd, GWLP_WNDPROC, (LONG_PTR)(unicode ? &wndHookProcedureW : &wndHookProcedureA));

	EnterCriticalSection(&prevWndProcCS);
	prevWndProcMap[hWnd] = prevWndProc;
	LeaveCriticalSection(&prevWndProcCS);

	return true;
}

HWND WINAPI myCreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int x, int y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam){
	HWND hWnd = originalCreateWindowExA(dwExStyle, lpClassName, lpWindowName, dwStyle, x, y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);

	if (!IS_INTRESOURCE(lpClassName)){
		std::string classname(lpClassName);
		hookFullscreenClass(hWnd, classname, false);
	}

	return hWnd;
}

HWND WINAPI myCreateWindowExW(DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName, DWORD dwStyle, int x, int y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam){
	HWND hWnd = originalCreateWindowExW(dwExStyle, lpClassName, lpWindowName, dwStyle, x, y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);

	if (!IS_INTRESOURCE(lpClassName)){
		char name[256];
		WideCharToMultiByte(CP_ACP, 0, lpClassName, -1, name, sizeof(name), NULL, NULL);
		std::string classname(name);
		hookFullscreenClass(hWnd, classname, true);
	}

	return hWnd;
}

bool installWindowClassHook(){
	HMODULE user32 = LoadLibraryA("user32.dll");

	if(!user32)
		return false;

	if (!originalCreateWindowExA)
		originalCreateWindowExA     = (CreateWindowExAPtr)patchDLLExport(user32, "CreateWindowExA", (void*)&myCreateWindowExA);

	if (!originalCreateWindowExW)
		originalCreateWindowExW     = (CreateWindowExWPtr)patchDLLExport(user32, "CreateWindowExW", (void*)&myCreateWindowExW);

	InitializeCriticalSection(&prevWndProcCS);

	return (originalCreateWindowExA && originalCreateWindowExW);
}

/* -------- Unity hooks --------*/

typedef BOOL (* WINAPI SetNamedPipeHandleStatePtr)(HANDLE hNamedPipe, LPDWORD lpMode, LPDWORD lpMaxCollectionCount, LPDWORD lpCollectDataTimeout);
typedef HDESK (* WINAPI OpenInputDesktopPtr)(DWORD dwFlags, BOOL fInherit, ACCESS_MASK dwDesiredAccess);

SetNamedPipeHandleStatePtr originalSetNamedPipeHandleState = NULL;
OpenInputDesktopPtr originalOpenInputDesktop = NULL;

/*
	Unity uses named pipes in message mode during the update of the engine,
	but Wine doesn't support them yet and therefore the update will fail.
	Since Linux doesn't support similar types of pipes, we can not use a
	simple patch to implement this feature, instead we do something risky here
	and claim that the pipe is now in message mode. This only works if the
	size of the message is small and some Wine functions will behave wrong,
	but I tested it several times and Unity was always able to update.
*/
BOOL WINAPI mySetNamedPipeHandleState(HANDLE hNamedPipe, LPDWORD lpMode, LPDWORD lpMaxCollectionCount, LPDWORD lpCollectDataTimeout){
	originalSetNamedPipeHandleState(hNamedPipe, lpMode, lpMaxCollectionCount, lpCollectDataTimeout);
	return true;
}

/*
	OpenInputDesktop is not supported by Wine, but Unity will enter an endless
	loop in some games when we return 0. Since Wine ignores the parameter on
	all functions which use an input desktop we simply return a fake handle.
*/
HDESK WINAPI myOpenInputDesktop(DWORD dwFlags, BOOL fInherit, ACCESS_MASK dwDesiredAccess){
	HDESK res = originalOpenInputDesktop(dwFlags, fInherit, dwDesiredAccess);

	/*
		The Return value 0 will cause problems with some games, so we return
		the Desktop used by the current thread instead, since this function is
		implemented in Wine. The MSDN states that a handle returned by
		GetThreadDesktop() does not need to be closed with CloseDesktop() in
		contrary to OpenInputDesktop(), but Wine increases the refcounter on the
		handle when GetThreadDesktop() is called. This bug (?) allows us to
		treat the handle returned by GetThreadDesktop() as a new handle.
	*/
	if (!res)
		res = GetThreadDesktop(GetCurrentThreadId());

	return res;
}

bool installUnityHooks(){
	HMODULE user32 = LoadLibraryA("user32.dll");

	if(!user32)
		return false;

	if(!originalOpenInputDesktop)
		originalOpenInputDesktop    = (OpenInputDesktopPtr)patchDLLExport(user32,   "OpenInputDesktop", (void*)&myOpenInputDesktop);

	/*
		The SetNamedPipeHandleState hackfix is now done by a Wine patch since
		function is not directly called by the Unity plugin, but by the updater.
		Since this is an external process and therefore doesn't care about
		our hook, we either need to inject code into another process or use a
		Wine patch.
	*/

	/*
		HMODULE kernel32 = LoadLibrary("kernel32.dll");

		if(!kernel32)
			return false;

		if(!originalSetNamedPipeHandleState)
			originalSetNamedPipeHandleState    = (SetNamedPipeHandleStatePtr)patchDLLExport(kernel32,   "SetNamedPipeHandleState", (void*)&mySetNamedPipeHandleState);

		return (originalOpenInputDesktop && originalSetNamedPipeHandleState);
	*/

	return originalOpenInputDesktop;
}