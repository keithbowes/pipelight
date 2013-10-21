#include <vector>								// for std::vector
#include "../common/common.h"

#include <windows.h>							// for PVOID and other types
#include <string.h>								// for memset

void* patchDLLExport(PVOID ModuleBase, const char* functionName, void* newFunctionPtr){
	// Based on the following source code:
	// http://alter.org.ua/docs/nt_kernel/procaddr/#RtlImageDirectoryEntryToData
	
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

	for (i = 0; i < max_name; i++)
	{
		ULONG ord = ordinals[i];
		if (i >= max_name || ord >= max_func)
			break;

		if (strcmp( (PCHAR) ModuleBase + names[i], functionName ) == 0){
			DBG_INFO("replaced API function %s.", functionName);

			void* oldFunctionPtr = (PVOID)((PCHAR) ModuleBase + functions[ord]);
			functions[ord] = (ULONG)newFunctionPtr - (ULONG)ModuleBase;
			return oldFunctionPtr;
		}

	}

	return NULL;
};

typedef UINT_PTR (* WINAPI SetTimerPtr)(HWND hWnd, UINT_PTR nIDEvent, UINT uElapse, TIMERPROC lpTimerFunc);
typedef BOOL (* WINAPI KillTimerPtr)(HWND hWnd, UINT_PTR uIDEvent);

SetTimerPtr originalSetTimer    = NULL;
KillTimerPtr originalKillTimer  = NULL;

struct TimerEntry{
	HWND        hWnd;
	UINT_PTR    IDEvent;
	TIMERPROC   lpTimerFunc;
};

std::vector<TimerEntry> timerEntries;
CRITICAL_SECTION        timerCS;

bool handleTimerEvents(){
	size_t numTimers;

	EnterCriticalSection(&timerCS);

	numTimers = timerEntries.size();
	if (numTimers){

		for (unsigned int i = 0; i < numTimers; i++){
			// Access by index to avoid problems with new timers
			TimerEntry *it = &timerEntries[i];

			if (it->hWnd){
				MSG msg;
				msg.hwnd    = it->hWnd;
				msg.message = WM_TIMER;
				msg.wParam  = it->IDEvent;
				msg.lParam  = (LPARAM)it->lpTimerFunc;
				msg.time    = GetTickCount();

				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

		for (std::vector<TimerEntry>::iterator it = timerEntries.begin(); it != timerEntries.end();){
			if (it->hWnd == 0){
				it = timerEntries.erase(it);
			}else{
				it++;
			}
		}

	}

	LeaveCriticalSection(&timerCS);

	return (numTimers != 0);
}

UINT_PTR WINAPI mySetTimer(HWND hWnd, UINT_PTR nIDEvent, UINT uElapse, TIMERPROC lpTimerFunc){

	// Callback immediately? 
	if (hWnd && nIDEvent && uElapse == 0){
		EnterCriticalSection(&timerCS);

		std::vector<TimerEntry>::iterator it;
		
		for (it = timerEntries.begin(); it != timerEntries.end(); it++){
			if (it->hWnd == hWnd && it->IDEvent == nIDEvent){
				it->lpTimerFunc = lpTimerFunc;
				break;
			}
		}

		if (it == timerEntries.end()){
			TimerEntry entry;
			entry.hWnd          = hWnd;
			entry.IDEvent       = nIDEvent;
			entry.lpTimerFunc   = lpTimerFunc;
			timerEntries.push_back(entry);
		}

		LeaveCriticalSection(&timerCS);

		return nIDEvent;
	}

	return originalSetTimer(hWnd, nIDEvent, uElapse, lpTimerFunc);
}

BOOL WINAPI myKillTimer(HWND hWnd, UINT_PTR uIDEvent){

	if (hWnd && uIDEvent){
		bool found = false;

		EnterCriticalSection(&timerCS);

		for (std::vector<TimerEntry>::iterator it = timerEntries.begin(); it != timerEntries.end(); it++){
			if (it->hWnd == hWnd && it->IDEvent == uIDEvent){
				it->hWnd        = (HWND)0;
				found           = true;
				break;
			}
		}

		LeaveCriticalSection(&timerCS);

		// If it was successful, then return true
		if (found) return 1;
	}

	return originalKillTimer(hWnd, uIDEvent);
}

bool installTimerHook(){
	HMODULE user32 = LoadLibrary("user32.dll");

	if (!user32)
		return false;

	InitializeCriticalSection(&timerCS);

	if (!originalSetTimer)
		originalSetTimer    = (SetTimerPtr)patchDLLExport(user32,   "SetTimer", (void*)&mySetTimer);
	
	if (!originalKillTimer)
		originalKillTimer   = (KillTimerPtr)patchDLLExport(user32,  "KillTimer", (void*)&myKillTimer);

	return (originalSetTimer && originalKillTimer);
}

/* -------- Menu Hook --------*/

enum MenuAction{
	MENU_SEPERATOR,
	MENU_ABOUT_PIPELIGHT
};

struct MenuEntry{
	UINT 		identifier;
	MenuAction 	action;
};

#define MENUID_OFFSET 0x50495045 // 'PIPE'

std::vector<MenuEntry> menuAddEntries(HMENU hMenu, HWND hwnd){
	std::vector<MenuEntry> 	entries;
	MenuEntry 				entry;
	MENUITEMINFO			entryInfo;

	int count = GetMenuItemCount(hMenu);
	if(count == -1)
		return entries;

	// ------- Seperator ------- //
	memset(&entryInfo, 0, sizeof(MENUITEMINFO));
	entryInfo.cbSize	= sizeof(MENUITEMINFO);
	entryInfo.fMask		= MIIM_FTYPE | MIIM_ID;
	entryInfo.fType		= MFT_SEPARATOR;
	entryInfo.wID 		= MENUID_OFFSET;

	InsertMenuItem(hMenu, count, true, &entryInfo);


	entry.identifier 	= entryInfo.wID;
	entry.action 		= MENU_SEPERATOR;
	entries.push_back(entry);

	count++;

	// ------- About Pipelight ------- //
	entryInfo.fMask			= MIIM_FTYPE | MIIM_STRING | MIIM_ID;
	entryInfo.fType			= MFT_STRING;
	entryInfo.wID 			= MENUID_OFFSET+1;
	entryInfo.dwTypeData 	= (char*)&"About Pipelight";

	InsertMenuItem(hMenu, count, true, &entryInfo);

	entry.identifier 	= entryInfo.wID;
	entry.action 		= MENU_ABOUT_PIPELIGHT;
	entries.push_back(entry);

	return entries;

}

void menuRemoveEntries(HMENU hMenu, std::vector<MenuEntry> entries){
	for (MenuEntry &entry : entries){
		RemoveMenu(hMenu, entry.identifier, MF_BYCOMMAND);
	}
}

bool menuHandle(UINT identifier, std::vector<MenuEntry> entries){
	for (MenuEntry &entry : entries){

		if(entry.identifier == identifier){

			switch (entry.action){

				case MENU_ABOUT_PIPELIGHT:
					MessageBox(NULL, "Pipelight by FDS-Team", "Pipelight", MB_OK);
					return true;

				default:
					return false;
			}
		}
	}
	return false;
}

typedef BOOL (* WINAPI TrackPopupMenuExPtr)(HMENU hMenu, UINT fuFlags, int x, int y, HWND hwnd, LPTPMPARAMS lptpm);
typedef BOOL (* WINAPI TrackPopupMenuPtr)(HMENU hMenu, UINT uFlags, int x, int y, int nReserved, HWND hWnd, const RECT *prcRect);

TrackPopupMenuExPtr originalTrackPopupMenuEx    = NULL;
TrackPopupMenuPtr 	originalTrackPopupMenu  	= NULL;

/*
	One disadvantage of our current implementation of the hook is that the
	return value is not completely correct since we are using TPM_RETURNCMD
	and we can not distinguish whether the user didn't not select anything
	or if there was an error. Both cases would return 0 when using
	TPM_RETURNCMD. So we always return true (assuming that there is no error)
	if the hook was call without TPM_RETURNCMD as flag and the return value of
	originalTrackPopupMenu(Ex) is 0.

	The return value of TrackPopupMenu(Ex) is really defined as BOOL although
	it can contain an ID, which may look wrong on the first sight.
*/

BOOL WINAPI myTrackPopupMenuEx(HMENU hMenu, UINT fuFlags, int x, int y, HWND hwnd, LPTPMPARAMS lptpm){

	// Don't send messages to windows, but return the identifier as return value
	UINT newFlags = (fuFlags & ~TPM_NONOTIFY) | TPM_RETURNCMD;

	std::vector<MenuEntry> entries = menuAddEntries(hMenu, hwnd);
	BOOL identifier = originalTrackPopupMenuEx(hMenu, newFlags, x, y, hwnd, lptpm);
	menuRemoveEntries(hMenu, entries);

	if(!identifier)
		return (fuFlags & TPM_RETURNCMD) ? identifier : true;

	if(menuHandle(identifier, entries))
		return (fuFlags & TPM_RETURNCMD) ? 0 : true;

	if(!(fuFlags & TPM_NONOTIFY))
		PostMessage(hwnd, WM_COMMAND, 0, identifier);

	return (fuFlags & TPM_RETURNCMD) ? identifier : true;
}

BOOL WINAPI myTrackPopupMenu(HMENU hMenu, UINT uFlags, int x, int y, int nReserved, HWND hWnd, const RECT *prcRect){

	// Don't send messages to windows, but return the identifier as return value
	UINT newFlags = (uFlags & ~TPM_NONOTIFY) | TPM_RETURNCMD;

	std::vector<MenuEntry> entries = menuAddEntries(hMenu, hWnd);
	BOOL identifier = originalTrackPopupMenu(hMenu, newFlags, x, y, nReserved, hWnd, prcRect);
	menuRemoveEntries(hMenu, entries);

	if(!identifier)
		return (uFlags & TPM_RETURNCMD) ? identifier : true;

	if(menuHandle(identifier, entries))
		return (uFlags & TPM_RETURNCMD) ? 0 : true;

	if(!(uFlags & TPM_NONOTIFY))
		PostMessage(hWnd, WM_COMMAND, 0, identifier);

	return (uFlags & TPM_RETURNCMD) ? identifier : true;
}

bool installPopupHook(){
	HMODULE user32 = LoadLibrary("user32.dll");

	if (!user32)
		return false;

	if (!originalTrackPopupMenuEx)
		originalTrackPopupMenuEx    = (TrackPopupMenuExPtr)	patchDLLExport(user32, "TrackPopupMenuEx", (void*)&myTrackPopupMenuEx);

	if (!originalTrackPopupMenu)
		originalTrackPopupMenu   	= (TrackPopupMenuPtr)	patchDLLExport(user32, "TrackPopupMenu", (void*)&myTrackPopupMenu);

	return (originalTrackPopupMenuEx && originalTrackPopupMenu);
}