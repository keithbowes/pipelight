#include <iostream>								// for std::cerr
#include <cstdlib>								// for malloc, ...
#include <stdexcept>							// for std::runtime_error
#include <memory>								// for std::shared_ptr
#include <string>								// for std::string
//#include <fstream>
#include <vector>								// for std::vector
#include <algorithm>							// for std::transform
#include <stdio.h>								// for _fdopen
#include <io.h>									// for _dup
#include <objbase.h>							// for CoInitializeEx

#include "pluginloader.h"

/* BEGIN GLOBAL VARIABLES */

// Pipes to communicate with the linux process
FILE * pipeOutF = stdout;
FILE * pipeInF 	= stdin;

// Windows Classname for CreateWindowEx
LPCTSTR ClsName = "VirtualBrowser";

// hWnd -> Instance
std::map<HWND, NPP> hwndToInstance;

// Global plugin configuration (only required fields)
bool isWindowlessMode	= false;
bool isEmbeddedMode		= false;

// Used by NPN_UserAgent
char strUserAgent[1024] = {0};

std::string np_MimeType;
std::string np_FileExtents;
std::string np_FileOpenName;
std::string np_ProductName;
std::string np_FileDescription;
std::string np_Language;

// Handlemanager
HandleManager handlemanager;

// The plugin itself
NPPluginFuncs pluginFuncs = {sizeof(pluginFuncs), NP_VERSION_MINOR};

/* END GLOBAL VARIABLES */

LRESULT CALLBACK WndProcedure(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{

	// Handle events in windowless mode
	if(isWindowlessMode && hWnd){

		// Get instance
		std::map<HWND, NPP>::iterator it = hwndToInstance.find(hWnd);
		if(it != hwndToInstance.end()){
			NPP instance = it->second;

			// Get netscape data
			NetscapeData* ndata = (NetscapeData*)instance->ndata;
			if(ndata && ndata->window){
				NPWindow* window = ndata->window;

				// Paint event
				if( Msg == WM_PAINT ){

					RECT rect;
					PAINTSTRUCT paint;
					HDC hDC;

					if( GetClientRect(hWnd, &rect) ) {
						
						hDC = BeginPaint(hWnd, &paint);
						if(hDC != NULL){

							// Save the previous DC (or allocate a new one)
							HDC previousDC;
							if(window->type == NPWindowTypeDrawable){
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

					// Workaround for Contextmenu in embedded mode
					if(Msg == WM_RBUTTONDOWN && isEmbeddedMode){
						int32_t windowIDX11 = (int32_t) GetPropA(hWnd, "__wine_x11_whole_window");
						if(windowIDX11){
							Stack stack;

							writeInt32(windowIDX11);
							callFunction(GET_WINDOW_RECT);

							readCommands(stack);

							if( (bool)readInt32(stack) ){
								int32_t x 			= readInt32(stack);
								int32_t y 			= readInt32(stack);
								/*int32_t width 		= readInt32(stack);
								int32_t height 		= readInt32(stack);*/

								SetWindowPos(ndata->hWnd, HWND_TOP, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
							}
						}
					}


					// Workaround for Silverlight - the events are not correctly handled if
					// window->window is nonzero
					// Set it to zero before calling the event handler in this case

					HDC previousDC = NULL;

					if(	window->type == NPWindowTypeDrawable &&
						((Msg >= WM_KEYFIRST && Msg <= WM_KEYLAST) ||
						 (Msg >= WM_MOUSEFIRST && Msg <= WM_MOUSELAST )) ){

						previousDC = (HDC)window->window;
						window->window = NULL;
					}

					NPEvent event;
					event.event 	= Msg;
					event.wParam 	= wParam;
					event.lParam 	= lParam;
					int16_t result = pluginFuncs.event(instance, &event);

					if(previousDC){
						window->window = previousDC;
					}

					if(result == kNPEventHandled) return 0;

				}
			}
		}
	}

	// Otherwise we only have to handle several regular events
	if(Msg == WM_DESTROY){
		return 0;

	}else if(Msg == WM_CLOSE){
		return 0;

	}else if(Msg == WM_ERASEBKGND){
    	return 0; // TODO: Correct return value?

	}else if( Msg == WM_SIZE ){
		InvalidateRect(hWnd, NULL, false);
		return 0;

	}else{
		return DefWindowProc(hWnd, Msg, wParam, lParam);
	}
}

void freeSharedPtrMemory(void *memory){
	if(memory){
		free(memory);
	}
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

	for(unsigned int i = 0; i < mimeTypes.size(); i++){

		if(i != 0) 
			result += ";";

		result += mimeTypes[i];
		
		result += ":";	
		if(i < fileExtensions.size())
			result += fileExtensions[i];

		result += ":";
		if(i < extDescription.size())
			result += extDescription[i];
	}

	return result;

}

bool InitDLL(std::string dllPath, std::string dllName){

	// Thanks Microsoft - I searched a whole day to find this bug!
	//CoInitialize(NULL);
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	if(!SetDllDirectory(dllPath.c_str())){
		std::cerr << "[PIPELIGHT] Failed to set DLL directory" << std::endl;
	}

	HMODULE dll = LoadLibrary(dllName.c_str());

	if(dll){

		int requiredBytes = GetFileVersionInfoSize(dllName.c_str(), NULL);

		if(requiredBytes){

			std::unique_ptr<void, void (*)(void *)> data(malloc(requiredBytes), freeSharedPtrMemory);
			if(data){

				if (GetFileVersionInfo(dllName.c_str(), 0, requiredBytes, data.get())){

					char *info = NULL;
					UINT size = 0; 

					if(VerQueryValue(data.get(), "\\StringFileInfo\\040904E4\\MIMEType", (void**)&info, &size)){
						while( size > 0 && info[size-1] == 0) size--;
						np_MimeType = std::string(info, size);
					}

					if(VerQueryValue(data.get(), "\\StringFileInfo\\040904E4\\FileExtents", (void**)&info, &size)){
						while( size > 0 && info[size-1] == 0) size--;
						np_FileExtents = std::string(info, size);
					}

					if(VerQueryValue(data.get(), "\\StringFileInfo\\040904E4\\FileOpenName", (void**)&info, &size)){
						while( size > 0 && info[size-1] == 0) size--;
						np_FileOpenName = std::string(info, size);
					}

					if(VerQueryValue(data.get(), "\\StringFileInfo\\040904E4\\ProductName", (void**)&info, &size)){
						while( size > 0 && info[size-1] == 0) size--;
						np_ProductName = std::string(info, size);
					}

					if(VerQueryValue(data.get(), "\\StringFileInfo\\040904E4\\FileDescription", (void**)&info, &size)){
						while( size > 0 && info[size-1] == 0) size--;
						np_FileDescription = std::string(info, size);
					}
					
					if(VerQueryValue(data.get(), "\\StringFileInfo\\040904E4\\Language", (void**)&info, &size)){
						while( size > 0 && info[size-1] == 0) size--;
						np_Language = std::string(info, size);
					}

					/*
					std::cerr << "[PIPELIGHT] mimeType: " << np_MimeType << std::endl;
					std::cerr << "[PIPELIGHT] FileExtents: " << np_FileExtents << std::endl;
					std::cerr << "[PIPELIGHT] FileOpenName" << np_FileOpenName << std::endl;
					std::cerr << "[PIPELIGHT] ProductName" << np_ProductName << std::endl;
					std::cerr << "[PIPELIGHT] FileDescription" << np_FileDescription << std::endl;
					std::cerr << "[PIPELIGHT] Language:" << np_Language << std::endl;
					*/

					NP_GetEntryPointsFunc 	NP_GetEntryPoints 	= (NP_GetEntryPointsFunc) 	GetProcAddress(dll, "NP_GetEntryPoints");
					NP_InitializeFunc 		NP_Initialize 		= (NP_InitializeFunc) 		GetProcAddress(dll, "NP_Initialize");

					if(NP_GetEntryPoints && NP_Initialize){
						if (NP_Initialize(&browserFuncs) == NPERR_NO_ERROR){

							if(NP_GetEntryPoints(&pluginFuncs) == NPERR_NO_ERROR){
								return true;

							}else{
								std::cerr << "[PIPELIGHT] Failed to get entry points for plugin functions" << std::endl;
							}
						}else{
							std::cerr << "[PIPELIGHT] Failed to initialize" << std::endl;
						}
					}else{
						std::cerr << "[PIPELIGHT] Could not load Entry Points from DLL" << std::endl;
					}

				}else{
					std::cerr << "[PIPELIGHT] Failed to get File Version" << std::endl;
				}
			}else{
				std::cerr << "[PIPELIGHT] Failed to allocate Memory" << std::endl;
			}
		}else{
			std::cerr << "[PIPELIGHT] Could not load version information" << std::endl;
		}

		FreeLibrary(dll);

	}else{
		std::cerr << "[PIPELIGHT] Last error: " << GetLastError() << std::endl;
		std::cerr << "[PIPELIGHT] Could not load library" << std::endl;
	}

	return false;

}


int main(int argc, char *argv[]){

	if(argc < 3)
		throw std::runtime_error("Not enough arguments supplied");

	std::string dllPath 		= std::string(argv[1]);
	std::string dllName 		= std::string(argv[2]);

	for(int i = 3; i < argc; i++){

		std::string arg = std::string(argv[i]);
		std::transform(arg.begin(), arg.end(), arg.begin(), ::tolower);

		if(arg == "--windowless"){
			isWindowlessMode 	= true;
		}else if(arg == "--embed"){
			isEmbeddedMode 		= true;
		}

	}

	if(isWindowlessMode){
		std::cerr << "[PIPELIGHT] Using WINDOWLESS mode" << std::endl;
	}else{
		std::cerr << "[PIPELIGHT] Using WINDOW mode" << std::endl;
	}

	if(isEmbeddedMode){
		std::cerr << "[PIPELIGHT] Using EMBED mode" << std::endl;
	}else{
		std::cerr << "[PIPELIGHT] Using external window" << std::endl;
	}

	// Copy stdin and stdout
	int stdoutF	= _dup(1);
	pipeOutF 	= _fdopen(stdoutF, 	"wb");

	int stdinF	= _dup(0);
	pipeInF 	= _fdopen(stdinF, 	"rb");
	
	// Disable buffering not necessary here
	//setbuf(pipeInF, NULL);

	//Redirect STDOUT to STDERR
	SetStdHandle(STD_OUTPUT_HANDLE, GetStdHandle(STD_ERROR_HANDLE));

	// Create the application window
	WNDCLASSEX WndClsEx;
	WndClsEx.cbSize        = sizeof(WNDCLASSEX);
	WndClsEx.style         = CS_HREDRAW | CS_VREDRAW;
	WndClsEx.lpfnWndProc   = &WndProcedure;
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
	if(classAtom){

		if (InitDLL(dllPath, dllName)){
			std::cerr << "[PIPELIGHT] Init sucessfull!" << std::endl;

			Stack stack;
			readCommands(stack, false);	

		}else{
			std::cerr << "[PIPELIGHT] Failed to initialize DLL" << std::endl;
		}


	}else{
		std::cerr << "[PIPELIGHT] Failed to register class" << std::endl;
	}


	return 1;
	
}


void dispatcher(int functionid, Stack &stack){
	switch (functionid){

		case INIT_OKAY:
			{
				returnCommand();
			}
			break;
		
		case OBJECT_KILL:
			{
				NPObject 	*obj = readHandleObjIncRef(stack, NULL, 0, HANDLE_SHOULD_EXIST);

				#ifdef DEBUG_LOG_HANDLES
					std::cerr << "[PIPELIGHT:WINDOWS] OBJECT_KILL(" << (void*)obj << ")" << std::endl;
				#endif

				objectKill(obj);
				returnCommand();
			}
			break;

		case OBJECT_IS_CUSTOM:
			{
				NPObject 	*obj = readHandleObjIncRef(stack, NULL, 0, HANDLE_SHOULD_EXIST);

				#ifdef DEBUG_LOG_HANDLES
					std::cerr << "[PIPELIGHT:WINDOWS] OBJECT_IS_CUSTOM(" << (void*)obj << ")" << std::endl;
				#endif

				writeInt32( (obj->referenceCount == REFCOUNT_UNDEFINED) );
				returnCommand();
			}
			break;

		// HANDLE_MANAGER_REQUEST_STREAM_INFO not implemented

		case HANDLE_MANAGER_FREE_NOTIFY_DATA:
			{
				void *notifyData 			= readHandleNotify(stack, HANDLE_SHOULD_EXIST);

				handlemanager.removeHandleByReal((uint64_t)notifyData, TYPE_NotifyData);

				returnCommand();
			}
			break;

		case PROCESS_WINDOW_EVENTS:
			{
				// Process window events
				MSG msg;

				DWORD abortTime = GetTickCount() + 100;
				while (GetTickCount() < abortTime && PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) ){
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
				returnCommand();
			}
			break;

		case FUNCTION_GET_VERSION:
			{
				writeString(np_FileDescription);
				returnCommand();
			}
			break;

		case FUNCTION_GET_MIMETYPE:
			{
				writeString(createLinuxCompatibleMimeType());
				returnCommand();
			}
			break;

		case FUNCTION_GET_NAME:
			{
				writeString(np_ProductName);
				returnCommand();
			}
			break;

		case FUNCTION_GET_DESCRIPTION:
			{
				writeString(np_FileDescription);
				returnCommand();
			}
			break;	


		case FUNCTION_NP_INVOKE:
			{
				NPObject 	*npobj 			= readHandleObjIncRef(stack);
				NPIdentifier name 			= readHandleIdentifier(stack);
				uint32_t argCount 			= readInt32(stack);
				std::vector<NPVariant> args = readVariantArrayIncRef(stack, argCount);

				NPVariant resultVariant;
				resultVariant.type = NPVariantType_Null;

				bool result = npobj->_class->invoke(npobj, name, args.data(), argCount, &resultVariant);

				// The objects refcount has been incremented by invoke
				// Return the variant without modifying the objects refcount
				if(result){
					writeVariantReleaseDecRef(resultVariant);
				}

				// This frees ONLY all the strings!
				freeVariantArrayDecRef(args);
				objectDecRef(npobj);

				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NP_INVOKE_DEFAULT: // UNTESTED!
			{
				NPObject 	*npobj 			= readHandleObjIncRef(stack);
				uint32_t argCount 			= readInt32(stack);
				std::vector<NPVariant> args = readVariantArrayIncRef(stack, argCount);

				NPVariant resultVariant;
				resultVariant.type = NPVariantType_Null;

				bool result = npobj->_class->invokeDefault(npobj, args.data(), argCount, &resultVariant);

				// The objects refcount has been incremented by invoke
				// Return the variant without modifying the objects refcount
				if(result){
					writeVariantReleaseDecRef(resultVariant);
				}

				// This frees ONLY all the strings!
				freeVariantArrayDecRef(args);
				objectDecRef(npobj);

				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NP_HAS_PROPERTY:
			{
				NPObject *obj 		= readHandleObjIncRef(stack);
				NPIdentifier name 	= readHandleIdentifier(stack);	

				bool result = obj->_class->hasProperty(obj, name);

				objectDecRef(obj);

				writeInt32(result);
				returnCommand();
			}
			break;		

		case FUNCTION_NP_HAS_METHOD:
			{
				NPObject *obj 		= readHandleObjIncRef(stack);
				NPIdentifier name 	= readHandleIdentifier(stack);	

				bool result = obj->_class->hasMethod(obj, name);

				objectDecRef(obj);

				writeInt32(result);
				returnCommand();
			}
			break;		

		case FUNCTION_NP_GET_PROPERTY:
			{
				NPObject *obj 		= readHandleObjIncRef(stack);
				NPIdentifier name 	= readHandleIdentifier(stack);	

				NPVariant resultVariant;
				resultVariant.type = NPVariantType_Null;

				bool result = obj->_class->getProperty(obj, name, &resultVariant);

				if (result){
					writeVariantReleaseDecRef(resultVariant);
				}

				objectDecRef(obj);

				writeInt32(result);
				returnCommand();	
			}
			break;

		case FUNCTION_NP_SET_PROPERTY:
			{
				NPObject 		*obj 		= readHandleObjIncRef(stack);
				NPIdentifier 	name 		= readHandleIdentifier(stack);	
				NPVariant 		variant;

				readVariantIncRef(stack, variant);

				bool result = obj->_class->setProperty(obj, name, &variant);

				freeVariantDecRef(variant);
				objectDecRef(obj);

				writeInt32(result);
				returnCommand();	
			}
			break;

		case FUNCTION_NP_REMOVE_PROPERTY: // UNTESTED!
			{
				NPObject 		*obj 		= readHandleObjIncRef(stack);
				NPIdentifier 	name 		= readHandleIdentifier(stack);	

				bool result = obj->_class->removeProperty(obj, name);

				objectDecRef(obj);

				writeInt32(result);
				returnCommand();	
			}
			break;

		case FUNCTION_NP_ENUMERATE: // UNTESTED!
			{
				NPObject 		*obj 		= readHandleObjIncRef(stack);

				NPIdentifier*   identifierTable  = NULL;
				uint32_t 		identifierCount  = 0;

				bool result = obj->_class->enumerate(obj, &identifierTable, &identifierCount);

				if(result){
					writeIdentifierArray(identifierTable, identifierCount);

					// Free the memory for the table
					if(identifierTable)
						free(identifierTable);
				}

				objectDecRef(obj);

				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NP_INVALIDATE:
			{
				NPObject *obj = readHandleObjIncRef(stack);

				obj->_class->invalidate(obj);

				objectDecRef(obj);

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
				if(saved_data){
					saved.buf 	= saved_data;
					saved.len 	= saved_length;
					savedPtr 	= &saved;
				}

				// Most plugins only support windowlessMode in combination with NP_EMBED
				if(isWindowlessMode)	mode = NP_EMBED;

				NPError result = pluginFuncs.newp(mimeType.get(), instance, mode, argc, argn.data(), argv.data(), savedPtr);

				// Allocate structure!
				if(result == NPERR_NO_ERROR){
					NetscapeData* ndata = (NetscapeData*)malloc(sizeof(NetscapeData));
					if(ndata){
						instance->ndata = ndata;

						ndata->hWnd 	= NULL;
						ndata->window 	= NULL;

					}else{
						instance->ndata = NULL;
						std::cerr << "Unable to allocate memory for private data" << std::endl;
					}
				}

				// Free the arrays before returning
				freeStringArray(argn);
				freeStringArray(argv);

				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NPP_DESTROY:
			{
				NPSavedData* saved 	= NULL;
				NPP instance 		= readHandleInstance(stack);
				
				NPError result 		= pluginFuncs.destroy(instance, &saved);

				// Destroy the pointers
				NetscapeData* ndata = (NetscapeData*)instance->ndata;
				if(ndata){

					// ReleaseDC and free memory for window info
					if(ndata->window){
						if(ndata->window->type == NPWindowTypeDrawable && ndata->hWnd){
							ReleaseDC(ndata->hWnd, (HDC)ndata->window->window);
						}
						free(ndata->window);
					}

					// Destroy the window itself
					if(ndata->hWnd){
						hwndToInstance.erase(ndata->hWnd);
						DestroyWindow(ndata->hWnd);
					}

					// Free this structure
					free(ndata);
					instance->ndata = NULL;
				}

				free(instance);

				handlemanager.removeHandleByReal((uint64_t)instance, TYPE_NPPInstance);

				if(result == NPERR_NO_ERROR){
					if(saved){
						writeMemory((char*)saved->buf, saved->len);

						if(saved->buf) free((char*)saved->buf);
						free(saved);

					}else{
						writeMemory(NULL, 0);
					}
				}

				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NPP_GETVALUE_BOOL:
			{
				NPP instance 			= readHandleInstance(stack);
				NPPVariable variable 	= (NPPVariable)readInt32(stack);

				PRBool boolValue;

				NPError result = pluginFuncs.getvalue(instance, variable, &boolValue);

				if(result == NPERR_NO_ERROR)
					writeInt32(boolValue);

				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NPP_GETVALUE_OBJECT:
			{
				NPP instance 			= readHandleInstance(stack);
				NPPVariable variable 	= (NPPVariable)readInt32(stack);

				NPObject *objectValue;

				NPError result = pluginFuncs.getvalue(instance, variable, &objectValue);

				// Note: The refcounter of objectValue will be incremented when returing an objectValue!
				// http://stackoverflow.com/questions/1955073/when-to-release-object-in-npapi-plugin
				// The incrementing has to be done using NPN_RetainObject, so this will be already redirected
				// to the linux side ...

				if(result == NPERR_NO_ERROR)
					writeHandleObjDecRef(objectValue);

				writeInt32(result);
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

				// Only used in XEMBED mode
				int32_t windowIDX11 = 0;

				NetscapeData* ndata = (NetscapeData*)instance->ndata;
				if(ndata){

					if(! ndata->hWnd){

						RECT rect;
						rect.left 	= 0;
						rect.top	= 0;
						rect.right 	= width;
						rect.bottom = height;

						DWORD style;
						DWORD extStyle;
						int posX;
						int posY;

						if(isEmbeddedMode){
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
						if(ndata->hWnd){

							if(isEmbeddedMode){
								windowIDX11 = (int32_t) GetPropA(ndata->hWnd, "__wine_x11_whole_window");
							}

							ShowWindow(ndata->hWnd, SW_SHOW);
							UpdateWindow (ndata->hWnd);

							hwndToInstance.insert( std::pair<HWND, NPP>(ndata->hWnd, instance) );
						}
					}

					if(ndata->hWnd){
						NPWindow* window = ndata->window;

						// Allocate new window structure
						if(!window){
							window 			= (NPWindow*)malloc(sizeof(NPWindow));
							ndata->window 	= window;
							// Only do this once to prevent leaking DCs
							if(isWindowlessMode){
								window->window 			= GetDC(ndata->hWnd);
								window->type 			= NPWindowTypeDrawable;
							}else{
								window->window 			= ndata->hWnd;
								window->type 			= NPWindowTypeWindow;
							}

						}

						if(window){
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
						std::cerr << "[PIPELIGHT] Failed to create window!" << std::endl;
					}

				}else{
					std::cerr << "[PIPELIGHT] Unable to allocate window because of missing ndata" << std::endl;
				}

				writeInt32(windowIDX11);
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
				NPStream *stream 				= readHandleStream(stack, HANDLE_SHOULD_NOT_EXIST);
				NPBool seekable					= (NPBool) readInt32(stack); 

				uint16_t stype = NP_NORMAL; // Fix for silverlight....
				NPError result = pluginFuncs.newstream(instance, type.get(), stream, seekable, &stype);
				
				// Return result
				if(result == NPERR_NO_ERROR){
					writeInt32(stype);
				}else{ // Handle is now invalid because of this error
					handlemanager.removeHandleByReal((uint64_t)stream, TYPE_NPStream);
				}
				
				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NPP_DESTROY_STREAM:
			{
				NPP instance 		= readHandleInstance(stack);
				NPStream* stream 	= readHandleStream(stack, HANDLE_SHOULD_EXIST);
				NPReason reason 	= (NPReason)readInt32(stack);

				NPError result = pluginFuncs.destroystream(instance, stream, reason);

				// Free Data
				if(stream){
					if(stream->url) 	free((char*)stream->url);
					if(stream->headers) free((char*)stream->headers);
					free(stream);
				}

				// Let the handlemanager remove this one
				handlemanager.removeHandleByReal((uint64_t)stream, TYPE_NPStream);

				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NPP_WRITE_READY:
			{
				NPP instance 		= readHandleInstance(stack);
				NPStream* stream 	= readHandleStream(stack, HANDLE_SHOULD_EXIST);

				int32_t result = pluginFuncs.writeready(instance, stream);

				writeInt32(result);	
				returnCommand();
			}
			break;

		case FUNCTION_NPP_WRITE:
			{
				NPP instance 		= readHandleInstance(stack);
				NPStream* stream 	= readHandleStream(stack, HANDLE_SHOULD_EXIST);
				int32_t offset 		= readInt32(stack);

				size_t length;
				std::shared_ptr<char> data = readMemory(stack, length);

				int32_t result = pluginFuncs.write(instance, stream, offset, length, data.get());

				writeInt32(result);	
				returnCommand();
			}
			break;

		case FUNCTION_NPP_URL_NOTIFY:
			{
				NPP instance 				= readHandleInstance(stack);
				std::shared_ptr<char> URL 	= readStringAsMemory(stack);
				NPReason reason 			= (NPReason) readInt32(stack);
				void *notifyData 			= readHandleNotify(stack, HANDLE_SHOULD_EXIST);

				pluginFuncs.urlnotify(instance, URL.get(), reason, notifyData);

				returnCommand();
			}
			break;

		case NP_SHUTDOWN:
			{
				// TODO: Implement deinitialization! We dont call Shutdown, as otherwise we would have to call Initialize again!
				returnCommand();
			}
			break;


		default:
			throw std::runtime_error("Specified function not found!");
			break;

	}
}