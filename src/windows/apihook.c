#include <windows.h>                            // for PVOID and other types
#include <iostream>                             // for std::cerr
#include <vector>                               // for std::vector

void* patchDLLExport(PVOID ModuleBase, const char* functionName, void* newFunctionPtr)
{
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
		if(i >= max_name || ord >= max_func) {
			break;
		}

		if(strcmp( (PCHAR) ModuleBase + names[i], functionName ) == 0 ){
			std::cerr << "[PIPELIGHT] Replaced API function " << functionName << std::endl;

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
	if(numTimers){

		for( unsigned int i = 0; i < numTimers; i++){
			// Access by index to avoid problems with new timers
			TimerEntry *it = &timerEntries[i];

			if(it->hWnd){
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

		for(std::vector<TimerEntry>::iterator it = timerEntries.begin(); it != timerEntries.end();){
			if(it->hWnd == 0){
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
	if(hWnd && nIDEvent && uElapse == 0){
		EnterCriticalSection(&timerCS);

		std::vector<TimerEntry>::iterator it;
		
		for(it = timerEntries.begin(); it != timerEntries.end(); it++){
			if(it->hWnd == hWnd && it->IDEvent == nIDEvent){
				it->lpTimerFunc = lpTimerFunc;
				break;
			}
		}

		if( it == timerEntries.end() ){
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

	if(hWnd && uIDEvent){
		bool found = false;

		EnterCriticalSection(&timerCS);

		for(std::vector<TimerEntry>::iterator it = timerEntries.begin(); it != timerEntries.end(); it++){
			if(it->hWnd == hWnd && it->IDEvent == uIDEvent){
				it->hWnd        = (HWND)0;
				found           = true;
				break;
			}
		}

		LeaveCriticalSection(&timerCS);

		// If it was successful, then return true
		if(found) return 1;
	}

	return originalKillTimer(hWnd, uIDEvent);
}

bool installTimerHook(){

	HMODULE user32 = LoadLibrary("user32.dll");

	if(!user32)
		return false;

	InitializeCriticalSection(&timerCS);

	if(!originalSetTimer)
		originalSetTimer    = (SetTimerPtr)patchDLLExport(user32,   "SetTimer", (void*)&mySetTimer);
	
	if(!originalKillTimer)
		originalKillTimer   = (KillTimerPtr)patchDLLExport(user32,  "KillTimer", (void*)&myKillTimer);

	return (originalSetTimer && originalKillTimer);
}