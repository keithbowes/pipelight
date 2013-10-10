#include <cstdlib>								// for malloc, ...
#include <string>								// for std::string
#include <vector>								// for std::vector
#include <algorithm>							// for std::transform
#include <stdio.h>								// for _fdopen
#include <float.h>								// _controlfp_s

#ifdef __WINE__
	#include <unistd.h>							// for dup
#else
	#include <io.h>								// for _dup
#endif

#include "../common/common.h"
#include "pluginloader.h"
#include "apihook.h"

#include <windows.h>
#include <objbase.h>							// for CoInitializeEx

/* BEGIN GLOBAL VARIABLES */

LPCTSTR ClsName = "VirtualBrowser";

std::map<HWND, NPP> hwndToInstance;

bool isWindowlessMode	= false;
bool isEmbeddedMode		= false;
bool usermodeTimer      = false;

char strUserAgent[1024] = {0};

std::string np_MimeType;
std::string np_FileExtents;
std::string np_FileOpenName;
std::string np_ProductName;
std::string np_FileDescription;
std::string np_Language;

NPPluginFuncs pluginFuncs = {sizeof(pluginFuncs), (NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR};

/* END GLOBAL VARIABLES */

LRESULT CALLBACK wndProcedure(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam){

	// Only messages with a hwnd can be relevant in windowlessmode mode
	if (hWnd){

		// Find the specific instance
		std::map<HWND, NPP>::iterator it = hwndToInstance.find(hWnd);
		if (it != hwndToInstance.end()){
			NPP instance = it->second;

			// Get netscape data
			NetscapeData* ndata = (NetscapeData*)instance->ndata;
			if (ndata && ndata->window){

				// Handle events in windowless mode
				if (ndata->windowlessMode && ndata->window){
					NPWindow* window = ndata->window;

					// Paint event
					if (Msg == WM_PAINT){

						RECT rect;
						PAINTSTRUCT paint;
						HDC hDC;

						if (GetClientRect(hWnd, &rect)) {
							
							hDC = BeginPaint(hWnd, &paint);
							if (hDC != NULL){

								// Save the previous DC (or allocate a new one)
								HDC previousDC;
								if (window->type == NPWindowTypeDrawable){
									previousDC = (HDC)window->window;
								}else{
									previousDC = GetDC(hWnd);
								}

								window->window 				= hDC;
								window->x 					= 0;
								window->y 					= 0;
								window->width 				= rect.right;
								window->height 				= rect.bottom;
								window->clipRect.top 		= 0;
								window->clipRect.left 		= 0;
								window->clipRect.right 		= rect.right;
								window->clipRect.bottom 	= rect.bottom;
								window->type 				= NPWindowTypeDrawable;
								pluginFuncs.setwindow(instance, window);

								NPRect nRect;
								nRect.top 		= paint.rcPaint.top;
								nRect.left 		= paint.rcPaint.left;
								nRect.bottom 	= paint.rcPaint.bottom;
								nRect.right 	= paint.rcPaint.right;

								NPEvent event;
								event.event 	= Msg;
								event.wParam 	= (uintptr_t)hDC;
								event.lParam 	= (uintptr_t)&nRect;
								pluginFuncs.event(instance, &event);

								EndPaint(hWnd, &paint);

								// Restore the previous DC
								window->window = previousDC;
								pluginFuncs.setwindow(instance, window);

							}

							return 0;
						}

					// All other events
					}else{

						// Workaround for Silverlight - the events are not correctly handled if
						// window->window is nonzero
						// Set it to zero before calling the event handler in this case

						HDC previousDC = NULL;

						if (window->type == NPWindowTypeDrawable &&
							((Msg >= WM_KEYFIRST && Msg <= WM_KEYLAST) || (Msg >= WM_MOUSEFIRST && Msg <= WM_MOUSELAST )) ){

							previousDC = (HDC)window->window;
							window->window = NULL;
						}

						NPEvent event;
						event.event 	= Msg;
						event.wParam 	= wParam;
						event.lParam 	= lParam;
						int16_t result = pluginFuncs.event(instance, &event);

						if (previousDC)
							window->window = previousDC;

						if (result == kNPEventHandled) return 0;

					}

				}

			}
		}
	}

	// Otherwise we only have to handle several regular events
	if (Msg == WM_DESTROY){
		return 0;

	}else if (Msg == WM_CLOSE){
		return 0;

	}else if (Msg == WM_SIZE){
		InvalidateRect(hWnd, NULL, false);
		return 0;

	}else{
		return DefWindowProc(hWnd, Msg, wParam, lParam);
	}
}

void freeSharedPtrMemory(void *memory){
	if (memory)
		free(memory);
}

std::vector<std::string> splitMimeType(std::string input){
	std::vector<std::string> result;

	int start 			= 0;
	unsigned int i 		= 0;

	while (i < input.length()){
		while (i < input.length() && input[i] != '|'){
			i++;
		}

		if (i - start > 0){
			result.push_back(input.substr(start, i-start));
		}

		i++;
		start = i;
	}

	return result;
}

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

bool initDLL(std::string dllPath, std::string dllName){

	// Thanks Microsoft - I searched a whole day to find this bug!
	//CoInitialize(NULL);
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	if (!SetDllDirectory(dllPath.c_str())){
		DBG_ERROR("failed to set DLL directory.");
	}

	HMODULE dll = LoadLibrary(dllName.c_str());
	if (!dll){
		DBG_ERROR("could not load library '%s' (last error = %lu).", dllName.c_str(), (unsigned long)GetLastError());
		return false;
	}

	int requiredBytes = GetFileVersionInfoSize(dllName.c_str(), NULL);
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

	if (!GetFileVersionInfo(dllName.c_str(), 0, requiredBytes, data.get())){
		DBG_ERROR("failed to get file version.");
		FreeLibrary(dll);
		return false;
	}

	char *info = NULL;
	UINT size = 0; 

	if (VerQueryValue(data.get(), "\\StringFileInfo\\040904E4\\MIMEType", (void**)&info, &size)){
		while( size > 0 && info[size-1] == 0) size--;
		np_MimeType = std::string(info, size);
	}

	if (VerQueryValue(data.get(), "\\StringFileInfo\\040904E4\\FileExtents", (void**)&info, &size)){
		while( size > 0 && info[size-1] == 0) size--;
		np_FileExtents = std::string(info, size);
	}

	if (VerQueryValue(data.get(), "\\StringFileInfo\\040904E4\\FileOpenName", (void**)&info, &size)){
		while( size > 0 && info[size-1] == 0) size--;
		np_FileOpenName = std::string(info, size);
	}

	if (VerQueryValue(data.get(), "\\StringFileInfo\\040904E4\\ProductName", (void**)&info, &size)){
		while( size > 0 && info[size-1] == 0) size--;
		np_ProductName = std::string(info, size);
	}

	if (VerQueryValue(data.get(), "\\StringFileInfo\\040904E4\\FileDescription", (void**)&info, &size)){
		while( size > 0 && info[size-1] == 0) size--;
		np_FileDescription = std::string(info, size);
	}
	
	if (VerQueryValue(data.get(), "\\StringFileInfo\\040904E4\\Language", (void**)&info, &size)){
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


std::string readPathFromRegistry(HKEY hKey, std::string regKey){

	std::string fullKey = "Software\\MozillaPlugins\\" + regKey + "\\";

	DWORD type;
	DWORD length;

	// Check if the value exists and get required size
	if (RegGetValue(hKey, fullKey.c_str(), "Path", RRF_RT_ANY, &type, NULL, &length) != ERROR_SUCCESS)
		return "";

	// Check if the value is a string and the length is > 0
	if (type != REG_SZ || !length)
		return "";

	char *path = (char*)malloc(length);
	if (!path)
		return "";

	if (RegGetValue(hKey, fullKey.c_str(), "Path", RRF_RT_REG_SZ, NULL, path, &length) != ERROR_SUCCESS){
		free(path);
		return "";
	}

	std::string result(path);
	free(path);

	return result;
}

int main(int argc, char *argv[]){

	// When compiling with wineg++ the _controlfp_s isn't available
	// We should find a workaround for this (asm implementation) as soon as wineg++ support works properly
	#ifndef __WINE__
		unsigned int control_word;
		_controlfp_s(&control_word, _CW_DEFAULT, MCW_PC);
	#endif

	std::string dllPath;
	std::string dllName;
	std::string regKey;

	for (int i = 1; i < argc; i++){
		std::string arg = std::string(argv[i]);
		std::transform(arg.begin(), arg.end(), arg.begin(), ::tolower);

		if       (arg == "--pluginname"){
			if (i + 1 >= argc) break;
			setMultiPluginName(argv[++i]);

		}else if (arg == "--windowless"){
			isWindowlessMode 	= true;

		}else if (arg == "--embed"){
			isEmbeddedMode 		= true;

		}else if (arg == "--usermodetimer"){
			usermodeTimer 		= true;

		}else if (arg == "--dllpath"){
			if (i + 1 >= argc) break;
			dllPath = std::string(argv[++i]);

		}else if (arg == "--dllname"){
			if (i + 1 >= argc) break;
			dllName = std::string(argv[++i]);

		}else if (arg == "--regkey"){
			if (i + 1 >= argc) break;
			regKey  = std::string(argv[++i]);

		}
	}

	if (regKey == "" && (dllPath == "" || dllName == "")){
		DBG_ERROR("you must at least specify --regKey or --dllPath and --dllName.");
		return 1;
	}

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

	DBG_INFO("windowless mode is %s.", (isWindowlessMode ? "on" : "off"));
	DBG_INFO("embedded mode   is %s.", (isEmbeddedMode ? "on" : "off"));
	DBG_INFO("usermode Timer  is %s.", (usermodeTimer ? "on" : "off"));

	DBG_ASSERT(initCommIO(), "unable to initialize communication channel.");

	//Redirect STDOUT to STDERR
	SetStdHandle(STD_OUTPUT_HANDLE, GetStdHandle(STD_ERROR_HANDLE));

	// Create the application window
	WNDCLASSEX WndClsEx;
	WndClsEx.cbSize        = sizeof(WNDCLASSEX);
	WndClsEx.style         = CS_HREDRAW | CS_VREDRAW;
	WndClsEx.lpfnWndProc   = &wndProcedure;
	WndClsEx.cbClsExtra    = 0;
	WndClsEx.cbWndExtra    = 0;
	WndClsEx.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
	WndClsEx.hCursor       = LoadCursor(NULL, IDC_ARROW);
	WndClsEx.hbrBackground = NULL; //(HBRUSH)GetStockObject(LTGRAY_BRUSH);
	WndClsEx.lpszMenuName  = NULL;
	WndClsEx.lpszClassName = ClsName;
	WndClsEx.hInstance     = GetModuleHandle(NULL);
	WndClsEx.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);

	ATOM classAtom = RegisterClassEx(&WndClsEx);
	if (!classAtom){
		DBG_ERROR("failed to register class.");
		return 1;
	}

	// Install hooks
	if (usermodeTimer) installTimerHook();

	// Load the DLL
	if (!initDLL(dllPath, dllName)){
		DBG_ERROR("failed to initialize DLL.");
		return 1;
	}

	DBG_INFO("init successful!");

	Stack stack;
	readCommands(stack, false);	

	return 0;
}


void dispatcher(int functionid, Stack &stack){
	switch (functionid){

		case INIT_OKAY:
			{
				DBG_TRACE("INIT_OKAY()");
				DBG_TRACE("INIT_OKAY -> void");
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

		case WIN_HANDLE_MANAGER_OBJECT_IS_CUSTOM:
			{
				NPObject 	*obj = readHandleObjIncRef(stack, NULL, 0, HMGR_SHOULD_EXIST);
				DBG_TRACE("WIN_HANDLE_MANAGER_OBJECT_IS_CUSTOM( obj=%p )", obj);

				writeInt32( (obj->referenceCount == REFCOUNT_UNDEFINED) );

				DBG_TRACE("WIN_HANDLE_MANAGER_OBJECT_IS_CUSTOM -> bool=%d", (obj->referenceCount == REFCOUNT_UNDEFINED));
				objectDecRef(obj); // not really required, but looks better ;-)
				returnCommand();
			}
			break;

		case PROCESS_WINDOW_EVENTS:
			{
				uint64_t remoteHandleCount = readInt64(stack);
				DBG_ASSERT(remoteHandleCount == handleManager_count(), "remote handle count doesn't match local one.");

				// Process window events
				MSG msg;
				DBG_TRACE("PROCESS_WINDOW_EVENTS()");

				DWORD abortTime = GetTickCount() + 80;
				while (GetTickCount() < abortTime){
					if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)){
						TranslateMessage(&msg);
						DispatchMessage(&msg);

					}else if (usermodeTimer && handleTimerEvents()){
						// dummy

					}else{
						break;
					}
				}

				DBG_TRACE("PROCESS_WINDOW_EVENTS -> void");
				returnCommand();
			}
			break;

		case SHOW_UPDATE_WINDOW:
			{
				NPP instance = readHandleInstance(stack);
				DBG_TRACE("SHOW_UPDATE_WINDOW( instance=%p )", instance);

				// Only used when isEmbeddedMode is set
				if (isEmbeddedMode){
					NetscapeData* ndata = (NetscapeData*)instance->ndata;
					if (ndata){
						if (ndata->hWnd){
							ShowWindow(ndata->hWnd, SW_SHOW);
							UpdateWindow(ndata->hWnd);
						}
					}
				}

				DBG_TRACE("SHOW_UPDATE_WINDOW -> void");
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

				// The objects refcount has been incremented by invoke
				// Return the variant without modifying the objects refcount
				if (result)
					writeVariantReleaseDecRef(resultVariant);

				// This frees ONLY all the strings!
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

				// The objects refcount has been incremented by invoke
				// Return the variant without modifying the objects refcount
				if (result)
					writeVariantReleaseDecRef(resultVariant);

				// This frees ONLY all the strings!
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

					// Free the memory for the table
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

				// Note: The plugin is responsible for freeing the saved memory when not required anymore!
				if (saved_data){
					saved.buf 	= saved_data;
					saved.len 	= saved_length;
					savedPtr 	= &saved;
				}

				DBG_TRACE("FUNCTION_NPP_NEW( mimeType='%s', instance=%p, mode=%d, argc=%d, argn=%p, argv=%p, saved=%p )", \
						mimeType.get(), instance, mode, argc, argn.data(), argv.data(), savedPtr);

				// Most plugins only support windowlessMode in combination with NP_EMBED
				if (isWindowlessMode)
					mode = NP_EMBED;

				// Set privata data before calling the plugin, it might already use some commands
				NetscapeData* ndata = (NetscapeData*)malloc(sizeof(NetscapeData));
				if (ndata){
					instance->ndata = ndata;

					ndata->windowlessMode 	= isWindowlessMode;
					ndata->hWnd 			= NULL;
					ndata->window 			= NULL;

				}else{
					instance->ndata = NULL;
					DBG_ERROR("unable to allocate memory for private data.");
				}

				NPError result = pluginFuncs.newp(mimeType.get(), instance, mode, argc, argn.data(), argv.data(), savedPtr);

				// TODO: Do we have to deallocate the privata data or does NPP_DESTROY get called?

				// Free the arrays before returning
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

				// Destroy the pointers
				NetscapeData* ndata = (NetscapeData*)instance->ndata;
				if (ndata){

					// ReleaseDC and free memory for window info
					if (ndata->window){
						if (ndata->window->type == NPWindowTypeDrawable && ndata->hWnd){
							ReleaseDC(ndata->hWnd, (HDC)ndata->window->window);
						}
						free(ndata->window);
					}

					// Destroy the window itself
					if (ndata->hWnd){
						hwndToInstance.erase(ndata->hWnd);
						DestroyWindow(ndata->hWnd);
					}

					// Free this structure
					free(ndata);
					instance->ndata = NULL;
				}

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

				if (false){ // not used at the moment
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
				NPP instance 	= readHandleInstance(stack);
				int32_t x 		= readInt32(stack);
				int32_t y 		= readInt32(stack);
				int32_t width 	= readInt32(stack);
				int32_t height 	= readInt32(stack);
				DBG_TRACE("FUNCTION_NPP_SET_WINDOW( instance=%p, x=%d, y=%d, width=%d, height=%d )", instance, x, y, width, height);

				// Only used in XEMBED mode
				int32_t windowIDX11 = 0;

				NetscapeData* ndata = (NetscapeData*)instance->ndata;
				if (ndata){

					// Note: It breaks input event handling when calling
					// SetWindowPos(ndata->hWnd, HWND_TOP, 0, 0, width, height, SWP_NOMOVE | SWP_SHOWWINDOW);
					// here ... although we don't call it the window seems to resize properly

					if (!ndata->hWnd){
						RECT rect;
						rect.left 	= 0;
						rect.top	= 0;
						rect.right 	= width;
						rect.bottom = height;

						DWORD style;
						DWORD extStyle;
						int posX;
						int posY;

						if (isEmbeddedMode){
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

						AdjustWindowRectEx(&rect, style, false, extStyle);

						ndata->hWnd = CreateWindowEx(extStyle, ClsName, "Plugin", style, posX, posY, rect.right - rect.left, rect.bottom - rect.top, 0, 0, 0, 0);
						if (ndata->hWnd){
							hwndToInstance.insert( std::pair<HWND, NPP>(ndata->hWnd, instance) );

							if (isEmbeddedMode){
								windowIDX11 = (int32_t) GetPropA(ndata->hWnd, "__wine_x11_whole_window");
								// Its better not to show the window until its at the final position

							}else{

								ShowWindow(ndata->hWnd, SW_SHOW);
								UpdateWindow (ndata->hWnd);
							}

						}
					}

					if (ndata->hWnd){
						NPWindow* window = ndata->window;

						// Allocate new window structure
						if (!window){
							window 			= (NPWindow*)malloc(sizeof(NPWindow));

							if (window){
								ndata->window 	= window;
								
								// Only do this once to prevent leaking DCs
								if (ndata->windowlessMode){
									window->window 			= GetDC(ndata->hWnd);
									window->type 			= NPWindowTypeDrawable;
								}else{
									window->window 			= ndata->hWnd;
									window->type 			= NPWindowTypeWindow;
								}
							}

						}

						if (window){
							window->x 				= 0; //x;
							window->y 				= 0; //y;
							window->width 			= width;
							window->height 			= height; 
							window->clipRect.top 	= 0;
							window->clipRect.left 	= 0;
							window->clipRect.right 	= width;
							window->clipRect.bottom = height;

							pluginFuncs.setwindow(instance, window);
						}
						
					}else{
						DBG_ERROR("failed to create window!");
					}

				}else{
					DBG_ERROR("unable to allocate window because of missing ndata.");
				}

				writeInt32(windowIDX11);

				DBG_TRACE("FUNCTION_NPP_SET_WINDOW -> windowIDX11=%d", windowIDX11);
				returnCommand();

				// These parameters currently are not required
				UNREFERENCED_PARAMETER(x);
				UNREFERENCED_PARAMETER(y);
			}
			break;

		case FUNCTION_NPP_NEW_STREAM:
			{
				NPP instance 					= readHandleInstance(stack);
				std::shared_ptr<char> type 		= readStringAsMemory(stack);
				NPStream *stream 				= readHandleStream(stack, HMGR_SHOULD_NOT_EXIST);
				NPBool seekable					= (NPBool) readInt32(stack); 
				DBG_TRACE("FUNCTION_NPP_NEW_STREAM( instance=%p, type='%s', stream=%p, seekable=%d )", instance, type.get(), stream, seekable);

				uint16_t stype = NP_NORMAL; // Fix for silverlight....
				NPError result = pluginFuncs.newstream(instance, type.get(), stream, seekable, &stype);
				
				// Return result
				if (result == NPERR_NO_ERROR){
					writeInt32(stype);
				}else{ // Handle is now invalid because of this error
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

				// Free data
				if (stream){

					// Let the handlemanager remove this one
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

		case NP_SHUTDOWN:
			{
				DBG_TRACE("NP_SHUTDOWN()");

				// TODO: Implement deinitialization! We dont call Shutdown, as otherwise we would have to call Initialize again!

				DBG_TRACE("NP_SHUTDOWN -> void");
				returnCommand();
			}
			break;

		default:
			DBG_ABORT("specified function not found!");
			break;

	}
}