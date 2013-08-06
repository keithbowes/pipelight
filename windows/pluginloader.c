#include <iostream>
#include <cstdlib>
#include <stdexcept>
#include <memory>
#include <string>
#include <fstream>
#include <vector>
#include <io.h>
#include "pluginloader.h"

#include <objbase.h>

FILE * pipeOutF = stdout;
FILE * pipeInF 	= stdin;

LPCTSTR ClsName = "VirtualBrowser";

//Global Variables
HandleManager handlemanager;
std::ofstream output(PLUGIN_LOG, std::ios::out | std::ios::app);

LRESULT CALLBACK WndProcedure(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch(Msg)
	{

		case WM_DESTROY:
			//PostQuitMessage(WM_QUIT);
			return 0;

		default:
			return DefWindowProc(hWnd, Msg, wParam, lParam);

	}
	
	return 0;
}

void freeDataPointer(void *data){
	
	if (data){
		free(data);
		output << "called free" << std::endl;
	}

}

std::string np_MimeType;
std::string np_FileExtents;
std::string np_FileOpenName;
std::string np_ProductName;
std::string np_FileDescription;
std::string np_Language;

NPPluginFuncs pluginFuncs = {sizeof(pluginFuncs), NP_VERSION_MINOR};

std::vector<std::string> splitMimeType(std::string input){
	
	std::vector<std::string> result;

	int start 	= 0;
	int i 		= 0;

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

	for(int i = 0; i < mimeTypes.size(); i++){

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
	CoInitialize(NULL);
	//CoInitializeEx(NULL, COINIT_MULTITHREADED);

	if(!SetDllDirectory(dllPath.c_str())){
		output << "Failed to set DLL directory" << std::endl;
	}

	HMODULE dll = LoadLibrary(dllName.c_str());

	if(dll){

		int requiredBytes = GetFileVersionInfoSize(dllName.c_str(), NULL);

		if(requiredBytes){

			std::unique_ptr<void, void (*)(void *)> data(malloc(requiredBytes), freeDataPointer);
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
					output << "mimeType: " << np_MimeType << std::endl;
					output << "FileExtents: " << np_FileExtents << std::endl;
					output << "FileOpenName" << np_FileOpenName << std::endl;
					output << "ProductName" << np_ProductName << std::endl;
					output << "FileDescription" << np_FileDescription << std::endl;
					output << "Language:" << np_Language << std::endl;
					*/

					NP_GetEntryPointsFunc 	NP_GetEntryPoints 	= (NP_GetEntryPointsFunc) GetProcAddress(dll, "NP_GetEntryPoints");
					NP_InitializeFunc 		NP_Initialize 		= (NP_InitializeFunc) GetProcAddress(dll, "NP_Initialize");
					
					if(NP_GetEntryPoints && NP_Initialize){

						if (NP_Initialize(&browserFuncs) == NPERR_NO_ERROR){

							if(NP_GetEntryPoints(&pluginFuncs) == NPERR_NO_ERROR){
								output << "Adress NPP_New: " << (void*) pluginFuncs.newp << std::endl;
								return true;

							}else{
								output << "Failed to get entry points for plugin functions" << std::endl;
							}
						}else{
							output << "Failed to initialize" << std::endl;
						}
					}else{
						output << "Could not load Entry Points from DLL" << std::endl;
					}
				}else{
					output << "Failed to get File Version" << std::endl;
				}
			}else{
				output << "Failed to allocate Memory" << std::endl;		
			}
		}else{
			output << "Could not load version information" << std::endl;
		}
		FreeLibrary(dll);
	}else{
		output << "Could not load library :-(" << std::endl;
		output << "Last error: " << GetLastError() << std::endl;
	}

	return false;

}


int main(int argc, char *argv[]){
	
	output << "---------" << std::endl;

	if(argc < 3)
		throw std::runtime_error("Not enough arguments supplied");

	std::string dllPath = std::string(argv[1]);
	std::string dllName = std::string(argv[2]);

	/* Great TIP:
		Create a copy of Stdin & Stdout!
		Some plugins will take it away otherwise...
	*/

	int stdoutF	= _dup(1);
	pipeOutF 	= _fdopen(stdoutF, 	"wb");

	int stdinF	= _dup(0);
	pipeInF 	= _fdopen(stdinF, 	"rb");
	
	// Disable buffering not necessary here
	//setbuf(pipeInF, NULL);

	//Redirect STDOUT to STDERR
	SetStdHandle(STD_OUTPUT_HANDLE, GetStdHandle(STD_ERROR_HANDLE));

	WNDCLASSEX WndClsEx;

	// Create the application window
	WndClsEx.cbSize        = sizeof(WNDCLASSEX);
	WndClsEx.style         = CS_HREDRAW | CS_VREDRAW;
	WndClsEx.lpfnWndProc   = &WndProcedure;
	WndClsEx.cbClsExtra    = 0;
	WndClsEx.cbWndExtra    = 0;
	WndClsEx.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
	WndClsEx.hCursor       = LoadCursor(NULL, IDC_ARROW);
	WndClsEx.hbrBackground = (HBRUSH)GetStockObject(LTGRAY_BRUSH);
	WndClsEx.lpszMenuName  = NULL;
	WndClsEx.lpszClassName = ClsName;
	WndClsEx.hInstance     = GetModuleHandle(NULL);
	WndClsEx.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);

	ATOM classAtom = RegisterClassEx(&WndClsEx);

	if(classAtom){

		if (InitDLL(dllPath, dllName)){
			output << "Init sucessfull!" << std::endl;

			Stack stack;
			readCommands(stack, false);		
		}else{
			output << "Failed to initialize DLL" << std::endl;
		}


	}else{
		output << "Failed to register class" << std::endl;
	}


	return 1;
	
}

void dispatcher(int functionid, Stack &stack){
	switch (functionid){
		
		case OBJECT_KILL:
			{
				NPObject 	*obj = readHandleObjIncRef(stack, NULL, 0, true);

				objectKill(obj);
				returnCommand();
			}
			break;

		case OBJECT_IS_CUSTOM:
			{
				NPObject 	*obj = readHandleObjIncRef(stack, NULL, 0, true);

				writeInt32( (obj->referenceCount == REFCOUNT_UNDEFINED) );
				returnCommand();
			}
			break;

		// HANDLE_MANAGER_REQUEST_STREAM_INFO not implemented

		case PROCESS_WINDOW_EVENTS:
			{
				// Process window events
				MSG msg;
				while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)){
					//output << "handling window events" << std::endl;
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

		case FUNCTION_NP_HAS_PROPERTY_FUNCTION:
			{
				NPObject *obj 		= readHandleObjIncRef(stack);
				NPIdentifier name 	= readHandleIdentifier(stack);	

				bool result = obj->_class->hasProperty(obj, name);

				objectDecRef(obj);

				writeInt32(result);
				returnCommand();
			}
			break;		

		case FUNCTION_NP_HAS_METHOD_FUNCTION:
			{
				NPObject *obj 		= readHandleObjIncRef(stack);
				NPIdentifier name 	= readHandleIdentifier(stack);	

				bool result = obj->_class->hasMethod(obj, name);

				objectDecRef(obj);

				writeInt32(result);
				returnCommand();
			}
			break;		

		case FUNCTION_NP_GET_PROPERTY_FUNCTION:
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

		case FUNCTION_NP_SET_PROPERTY_FUNCTION:
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

		case FUNCTION_NP_INVALIDATE_FUNCTION:
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

				NPError result = pluginFuncs.newp(mimeType.get(), instance, mode, argc, argn.data(), argv.data(), savedPtr);

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

				// Destroy the window
				HWND hwnd = (HWND)instance->ndata;
				if(hwnd){
					DestroyWindow(hwnd);
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

				HWND hwnd = (HWND)instance->ndata;

				if(!hwnd){

					RECT rect;
					rect.left 	= 0;
					rect.top	= 0;
					rect.right 	= width;
					rect.bottom = height;

					AdjustWindowRect(&rect, WS_TILEDWINDOW, false);

					hwnd = CreateWindow(ClsName, "Plugintest", WS_TILEDWINDOW, x, y, rect.right - rect.left, rect.bottom - rect.top, 0, 0, 0, 0);

					ShowWindow(hwnd, SW_SHOW);
					UpdateWindow (hwnd);

					instance->ndata = (void*) hwnd;

					output << "Created Window" << std::endl;
				}

				//HWND hwnd = GetDesktopWindow();
				if (hwnd){
					
					NPWindow window;

					window.window 	= (void*)hwnd;
					window.x 		= x;
					window.y 		= y;
					window.width 	= width;
					window.height 	= height; 

					window.clipRect.top 	= 0;
					window.clipRect.left 	= 0;
					window.clipRect.right 	= width;
					window.clipRect.bottom 	= height;

					window.type = NPWindowTypeWindow;

					pluginFuncs.setwindow(instance, &window);

					// Show again!
					ShowWindow(hwnd, SW_SHOW);
					UpdateWindow (hwnd);

				}else{
					output << "Could not create window" << std::endl;
				}

				returnCommand();
			}
			break;

		case FUNCTION_NPP_NEW_STREAM:
			{
				NPP instance 					= readHandleInstance(stack);
				std::shared_ptr<char> type 		= readStringAsMemory(stack);
				NPStream *stream 				= readHandleStream(stack);
				NPBool seekable					= (NPBool) readInt32(stack); 

				uint16_t stype = NP_NORMAL; // Fix for silverlight....
				NPError result = pluginFuncs.newstream(instance, type.get(), stream, seekable, &stype);
				
				// Return result
				if(result == NPERR_NO_ERROR)
					writeInt32(stype);
				
				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NPP_DESTROY_STREAM:
			{
				NPP instance 		= readHandleInstance(stack);
				NPStream* stream 	= readHandleStream(stack);
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
				NPStream* stream 	= readHandleStream(stack);

				int32_t result = pluginFuncs.writeready(instance, stream);

				writeInt32(result);	
				returnCommand();
			}
			break;

		case FUNCTION_NPP_WRITE:
			{
				NPP instance 		= readHandleInstance(stack);
				NPStream* stream 	= readHandleStream(stack);
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
				void *notifyData 			= readHandleNotify(stack);

				pluginFuncs.urlnotify(instance, URL.get(), reason, notifyData);

				returnCommand();
			}
			break;


		default:
			throw std::runtime_error("Specified function not found!");
			break;

	}
}