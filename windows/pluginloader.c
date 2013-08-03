#include <iostream>
#include <cstdlib>
#include <stdexcept>
#include <memory>
#include <string>
#include <fstream>
#include <io.h>
#include "pluginloader.h"

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
			PostQuitMessage(WM_QUIT);
			break;
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

std::string createLinuxCompatibleMimeType(){

	std::string result = "";

	int start 	= 0;
	int i 		= 0;

	while (i < np_MimeType.length()){

		while (i < np_MimeType.length() && np_MimeType[i] != '|'){
			i++;
		}

		if (i - start > 0){

			if (start != 0)
				result += ";";

			result += np_MimeType.substr(start, i-start);
			result += ":" + np_FileExtents;
			result += ":" + np_FileOpenName;
		}

		i++;
		start = i;
	}


	return result;

}

bool InitDLL(){
	
	SetDllDirectory(DLL_PATH);

	HMODULE dll = LoadLibrary(DLL_NAME);

	if(dll){

		int requiredBytes = GetFileVersionInfoSize(DLL_NAME, NULL);

		if(requiredBytes){
			
			std::unique_ptr<void, void (*)(void *)> data(malloc(requiredBytes), freeDataPointer);
			if(data){

				if (GetFileVersionInfo(DLL_NAME, 0, requiredBytes, data.get())){

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

						if(NP_GetEntryPoints(&pluginFuncs) == NPERR_NO_ERROR){
						
							if (NP_Initialize(&browserFuncs) == NPERR_NO_ERROR){
								return true;

							}
						}

					}else{
						output << "Could not load Entry Points from DLL" << std::endl;
					}
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
	}

	return false;

}


int main(){
	
	
	output << "---------" << std::endl;

	if (InitDLL()){

		/* Great TIP:
			Create a copy of Stdin & Stdout!
			Some plugins will take it away otherwise...
		*/

		int stdoutF	= _dup(1);
		pipeOutF 	= _fdopen(stdoutF, 	"wb");

		int stdinF	= _dup(0);
		pipeInF 	= _fdopen(stdinF, 	"rb");
		
		// Disable buffering
		setbuf(pipeInF, NULL);

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
			output << "Init sucessfull!" << std::endl;

			Stack stack;
			readCommands(stack, true);		
		}else{
			output << "Failed to register class" << std::endl;
		}
	}

	return 2;
	
}

void callNPP_New(Stack &stack){

	std::shared_ptr<char> mimeType	= readStringAsBinaryData(stack);

	output << "Init: " << std::endl;

	NPP instance		= readHandleInstance(stack);
	uint16_t mode 		= readInt32(stack);
	int16_t argc 		= readInt32(stack);

	instance->ndata = NULL;

	//Fix code here
	if(mode != NP_EMBED){
		mode = NP_EMBED;
	}

	charArray argn 		= readCharStringArray(stack, argc);
	charArray argv 		= readCharStringArray(stack, argc);

	NPSavedData saved;
	size_t length;

	std::shared_ptr<char> data = readBinaryData(stack, length);

	saved.buf = data.get();
	saved.len = length;

	NPError result = pluginFuncs.newp(mimeType.get(), instance, mode, argc, argn.charPointers.data(), argv.charPointers.data(), &saved);

	output << "Init end " << std::endl;
	output << "result = " << result << std::endl;

	writeInt32(result);
	returnCommand();
}

void freeVariant(NPVariant &variant){
	if (variant.type == NPVariantType_String){
		free((char*)variant.value.stringValue.UTF8Characters);
	}
}

void freeVariant(std::vector<NPVariant> args){
	for(NPVariant &variant :  args){
		freeVariant(variant);
	}
}


void callNP_Invoke(Stack &stack){

	output << "callNP_INVOKE" << std::endl << std::flush;

	NPObject 	*npobj 	= readHandleObj(stack);
	NPIdentifier name 	= readHandleIdentifier(stack);
	uint32_t argCount 	= readInt32(stack);

	std::vector<NPVariant> args = readVariantArray(stack, argCount);
	NPVariant resultVariant;

	bool result = npobj->_class->invoke(npobj, name, args.data(), argCount, &resultVariant);

	freeVariant(args);

	if(result){
		writeVariant(resultVariant);
	}

	writeInt32(result);
	returnCommand();
	
}

void callNPP_GetValue_Bool(Stack &stack){

	NPP instance 			= readHandleInstance(stack);
	NPPVariable variable 	= (NPPVariable)readInt32(stack);

	PRBool result;

	NPError returnvalue = pluginFuncs.getvalue(instance, variable, &result);

	writeInt32(result);
	writeInt32(returnvalue);
	returnCommand();
}

void callNPP_GetValue_Object(Stack &stack){

	NPP instance 			= readHandleInstance(stack);
	NPPVariable variable 	= (NPPVariable)readInt32(stack);

	NPObject *result;

	NPError returnvalue = pluginFuncs.getvalue(instance, variable, &result);

	writeHandle(result);
	writeInt32(returnvalue);
	returnCommand();
}


void setWindowInfo(Stack &stack){

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

	}else{
		output << "Could not create window" << std::endl;
	}

	returnCommand();
}

void callNPP_NewStream(Stack &stack){
	
	output << "callNPP_NewStream" << std::endl;

	NPP instance 			= readHandleInstance(stack);
	std::string mimeType 	= readString(stack);
	NPStream *stream 		= readHandleStream(stack);
	NPBool seekable			= (NPBool) readInt32(stack); 

	uint16_t stype = NP_NORMAL; //Fix for silverlight....
	output << "New Stream with Type " << mimeType << " an URL: " << stream->url << std::endl;
	//output << "Overriding Mimetype to application/x-silverlight-app" << std::endl;
	NPError result = pluginFuncs.newstream(instance, (char*)mimeType.c_str(), stream, seekable, &stype);
	//NPError result = pluginFuncs.newstream(instance, "application/x-silverlight-app", stream, seekable, &stype);

	writeInt32(stype);
	writeInt32(result);

	returnCommand();

	output << "callNPP_NewStream ready with result " << result << " and type " << stype << std::endl;

}

void callNPP_DestroyStream(Stack &stack){
	
	output << "callNPP_DestroyStream" << std::endl;

	NPP instance 		= readHandleInstance(stack);
	NPStream* stream 	= readHandleStream(stack);
	NPReason reason 	= (NPReason)readInt32(stack);

	NPError result = pluginFuncs.destroystream(instance, stream, reason);

	handlemanager.removeHandleByReal((uint64_t)stream);

	// Free Data
	if(stream->url) free((char*)stream->url);
	if(stream->headers) free((char*)stream->headers);

	delete stream;

	writeInt32(result);
	returnCommand();

}


void callNPP_WriteReady(Stack &stack){
	
	output << "callNPP_WriteReady" << std::endl;

	NPP instance 		= readHandleInstance(stack);
	NPStream* stream 	= readHandleStream(stack);

	int32_t result = pluginFuncs.writeready(instance, stream);
	writeInt32(result);	
	returnCommand();

}

void callNPP_Write(Stack &stack){
	
	output << "callNPP_Write" << std::endl;

	NPP instance 		= readHandleInstance(stack);
	NPStream* stream 	= readHandleStream(stack);
	int32_t offset 		= readInt32(stack);

	size_t len;
	std::shared_ptr<char> data = readBinaryData(stack, len);

	int32_t result = pluginFuncs.write(instance, stream, offset, len, data.get());

	output << "callNPP_Write Result: " << result << std::endl;

	writeInt32(result);	
	returnCommand();

}

void callNPP_URLNotify(Stack &stack){

	NPP instance 		= readHandleInstance(stack);
	std::string URL 	= readString(stack);
	NPReason reason 	= (NPReason) readInt32(stack);
	void *notifyData 	= readHandleNotify(stack);

	output << "NotifyData: " << notifyData << std::endl;

	pluginFuncs.urlnotify(instance, URL.c_str(), reason, notifyData);

	returnCommand();
}

void callNP_HasPropertyFunction(Stack &stack){

	output << "callNP_HasPropertyFunction" << std::endl;

	NPObject *obj 		= readHandleObj(stack);
	NPIdentifier name 	= readHandleIdentifier(stack);	

	bool result = obj->_class->hasProperty(obj, name);

	writeInt32(result);
	returnCommand();
}

void callNP_HasMethodFunction(Stack &stack){

	NPObject *obj 		= readHandleObj(stack);
	NPIdentifier name 	= readHandleIdentifier(stack);	

	bool result = obj->_class->hasMethod(obj, name);
	writeInt32(result);
	returnCommand();
}

void callNP_GetPropertyFunction(Stack &stack){

	NPObject *obj 		= readHandleObj(stack);
	NPIdentifier name 	= readHandleIdentifier(stack);	

	NPVariant resultVariant;

	bool result = obj->_class->getProperty(obj, name, &resultVariant);

	if (result){
		writeVariant(resultVariant);
	}

	writeInt32(result);
	returnCommand();	
}


void dispatcher(int functionid, Stack &stack){

	NPObject 	*obj;
	MSG msg;

	//output << "dispatching function " << functionid << std::endl;

	switch (functionid){
		
		case FUNCTION_GET_VERSION:
			writeString(np_FileDescription);
			returnCommand();
			break;

		case FUNCTION_GET_MIMETYPE:
			output << "Get Mimetype: " << createLinuxCompatibleMimeType().c_str() << std::endl;
			writeString(createLinuxCompatibleMimeType());
			returnCommand();
			break;

		case FUNCTION_GET_NAME:
			writeString(np_ProductName);
			returnCommand();
			break;

		case FUNCTION_GET_DESCRIPTION:
			output << "Request Description: " << np_FileDescription << std::endl;
			writeString(np_FileDescription);
			returnCommand();
			break;	

		case FUNCTION_NPP_NEW:
			callNPP_New(stack);
			break;

		case FUNCTION_NP_INVOKE:
			callNP_Invoke(stack);
			break;

		case OBJECT_KILL:
			
			obj = readHandleObj(stack);

			output << "Killed " << (void*) obj << std::endl;
			
			handlemanager.removeHandleByReal((uint64_t)obj);

			if(obj->_class->deallocate){
				obj->_class->deallocate(obj);
			}else{
				delete obj;
			}
			break;

		case FUNCTION_NPP_GETVALUE_BOOL:
			callNPP_GetValue_Bool(stack);
			break;

		case FUNCTION_NPP_GETVALUE_OBJECT:
			callNPP_GetValue_Object(stack);
			break;		

		case FUNCTION_SET_WINDOW_INFO:
			setWindowInfo(stack);
			break;

		case FUNCTION_NPP_NEW_STREAM:
			callNPP_NewStream(stack);
			break;

		case FUNCTION_NPP_DESTROY_STREAM:
			callNPP_DestroyStream(stack);
			break;

		case FUNCTION_NPP_WRITE_READY:
			callNPP_WriteReady(stack);
			break;

		case FUNCTION_NPP_WRITE:
			callNPP_Write(stack);
			break;

		case FUNCTION_NPP_URL_NOTIFY:
			callNPP_URLNotify(stack);
			break;

		case FUNCTION_NP_HAS_PROPERTY_FUNCTION:
			callNP_HasPropertyFunction(stack);
			break;		

		case FUNCTION_NP_HAS_METHOD_FUNCTION:
			callNP_HasMethodFunction(stack);
			break;		

		case FUNCTION_NP_GET_PROPERTY_FUNCTION:
			callNP_GetPropertyFunction(stack);
			break;

		case NOP:
			output << "NOP!" << std::endl;
			returnCommand();
			break;

		case PROCESS_WINDOW_EVENTS:
			// Process window events
			while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)){
				//output << "handling window events" << std::endl;
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			returnCommand();
			break;

		default:
			throw std::runtime_error("WTF? Which Function?");
			break;
	}
	
	//output << "Function returned" << std::endl;
}