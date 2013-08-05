#include "basicplugin.h"

char strMimeType[2048] 			= {0};
char strPluginversion[100]		= {0};
char strPluginName[256] 		= {0};
char strPluginDescription[1024]	= {0};

void pokeString(std::string str, char *dest, unsigned int maxLength){
	if(maxLength > 0){
		unsigned int length = std::min((unsigned int)str.length(), maxLength-1);

		// Always at least one byte to copy (nullbyte)
		memcpy(dest, str.c_str(), length);
		dest[length] = 0;
	}
}

// Verified, everything okay
NP_EXPORT(NPError)
NP_Initialize(NPNetscapeFuncs* bFuncs, NPPluginFuncs* pFuncs)
{

	output << "NP_Initialize" << std::endl;
	sBrowserFuncs = bFuncs;

	// Check the size of the provided structure based on the offset of the
	// last member we need.
	if (pFuncs->size < (offsetof(NPPluginFuncs, setvalue) + sizeof(void*)))
		return NPERR_INVALID_FUNCTABLE_ERROR;

	pFuncs->newp 			= NPP_New;
	pFuncs->destroy 		= NPP_Destroy;
	pFuncs->setwindow 		= NPP_SetWindow;
	pFuncs->newstream 		= NPP_NewStream;
	pFuncs->destroystream 	= NPP_DestroyStream;
	pFuncs->asfile 			= NPP_StreamAsFile;
	pFuncs->writeready 		= NPP_WriteReady;
	pFuncs->write 			= NPP_Write;
	pFuncs->print 			= NPP_Print;
	pFuncs->event 			= NPP_HandleEvent;
	pFuncs->urlnotify 		= NPP_URLNotify;
	pFuncs->getvalue 		= NPP_GetValue;
	pFuncs->setvalue 		= NPP_SetValue;

	return NPERR_NO_ERROR;
}

// Verified, everything okay
NP_EXPORT(char*)
NP_GetPluginVersion()
{
	callFunction(FUNCTION_GET_VERSION);

	std::string result = readResultString();
	pokeString(result, strPluginversion, sizeof(strPluginversion));

	return strPluginversion;
}

// Verified, everything okay
NP_EXPORT(const char*)
NP_GetMIMEDescription()
{
	callFunction(FUNCTION_GET_MIMETYPE);

	std::string result = readResultString();
	pokeString(result, strMimeType, sizeof(strMimeType));

	return strMimeType;
}

// Verified, everything okay
NP_EXPORT(NPError)
NP_GetValue(void* future, NPPVariable aVariable, void* aValue) {

	std::string result;

	switch (aVariable) {

		case NPPVpluginNameString:
			callFunction(FUNCTION_GET_NAME);

			result = readResultString();
			pokeString(result, strPluginName, sizeof(strPluginName));		

			*((char**)aValue) = strPluginName;
			break;

		case NPPVpluginDescriptionString:
			callFunction(FUNCTION_GET_DESCRIPTION);

			result = readResultString();
			pokeString(result, strPluginDescription, sizeof(strPluginDescription));		

			*((char**)aValue) = strPluginDescription;
			break;

		default:
			output << ">>>>> STUB: NP_GetValue" << std::endl;
			return NPERR_INVALID_PARAM;

	}

	return NPERR_NO_ERROR;
}


NP_EXPORT(NPError)
NP_Shutdown()
{
	output << ">>>>> STUB: NP_Shutdown" << std::endl;
	return NPERR_NO_ERROR;
}

void timerFunc(NPP instance, uint32_t timerID){
	callFunction(PROCESS_WINDOW_EVENTS);
	waitReturn();
}

// Verified, everything okay
NPError
NPP_New(NPMIMEType pluginType, NPP instance, uint16_t mode, int16_t argc, char* argn[], char* argv[], NPSavedData* saved) {

	output << "NPP_New" << std::endl;

	// TODO: SCHEDULE ONLY ONE TIMER?!
	sBrowserFuncs->scheduletimer(instance, 50, true, timerFunc);

	if(saved){
		writeMemory((char*)saved->buf, saved->len);
	}else{
		writeMemory(NULL, 0);
	}

	writeStringArray(argv, argc);
	writeStringArray(argn, argc);
	writeInt32(argc);
	writeInt32(mode);
	writeHandle(instance);
	writeString(pluginType);
	callFunction(FUNCTION_NPP_NEW);

	NPError result = readResultInt32();

	// The plugin is responsible for freeing *saved
	// The other side has its own copy of this memory
	if(saved){
		sBrowserFuncs->memfree(saved->buf);
		saved->buf = NULL;
		saved->len = 0;
	}

	//result = NPERR_NO_ERROR;

	output << "result is " << result << std::endl;

	return result;
}

NPError
NPP_Destroy(NPP instance, NPSavedData** save) {
	output << "NPP_Destroy" << std::endl;

	writeHandle(instance);
	callFunction(FUNCTION_NPP_DESTROY);

	Stack stack;
	readCommands(stack);	

	NPError result 	= readInt32(stack);

	// We write a nullpointer in case we dont want to return anything
	*save = NULL;

	if(result == NPERR_NO_ERROR){
		size_t save_length;
		char* save_data = readMemoryBrowserAlloc(stack, save_length);

		if(save_data){
			*save = (NPSavedData*) sBrowserFuncs->memalloc(sizeof(NPSavedData));
			if(*save){

				(*save)->buf = save_data;
				(*save)->len = save_length;

			}else{
				sBrowserFuncs->memfree(save_data);
			}
		}

	}

	handlemanager.removeHandleByReal((uint64_t)instance, TYPE_NPPInstance);

	return result;

}

// Verified, everything okay
NPError
NPP_SetWindow(NPP instance, NPWindow* window) {

	// TODO: translate to Screen coordinates
	// TODO: Use all parameters

	output << "NPP_SetWindow" << std::endl;
	output << "X11 Window: " << (uint64_t)window->window << std::endl;

	writeInt32(window->height);
	writeInt32(window->width);
	writeInt32(window->y);
	writeInt32(window->x);
	writeHandle(instance);
	callFunction(FUNCTION_SET_WINDOW_INFO);
	waitReturn();

	return NPERR_NO_ERROR;
}

// Verified, everything okay
NPError
NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16_t* stype) {
	output << "NPP_NewStream with URL: " << stream->url << std::endl;

	writeInt32(seekable);
	writeHandle(stream);
	writeString(type);
	writeHandle(instance);
	callFunction(FUNCTION_NPP_NEW_STREAM);

	Stack stack;
	readCommands(stack);	

	NPError result 	= readInt32(stack);

	if(result == NPERR_NO_ERROR)
		*stype 			= (uint16_t)readInt32(stack);

	output << "NPP_NewStream finished with result " << result << " and type " << *stype << std::endl;

	return result;
}

// Verified, everything okay
NPError
NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason) {

	output << "NPP_DestroyStream" << std::endl;
	
	writeInt32(reason);
	writeHandle(stream);
	writeHandle(instance);
	callFunction(FUNCTION_NPP_DESTROY_STREAM);

	NPError result = readResultInt32();

	// Remove the handle by the corresponding stream real object
	handlemanager.removeHandleByReal((uint64_t)stream, TYPE_NPStream);

	return result;
}

// Verified, everything okay
int32_t
NPP_WriteReady(NPP instance, NPStream* stream) {

	output << "NPP_WriteReady" << std::endl;

	writeHandle(stream);
	writeHandle(instance);	
	callFunction(FUNCTION_NPP_WRITE_READY);
	
	int32_t result = readResultInt32();

	output << "NPP_WriteReady - Maximum Length: " << result << std::endl;

	return result;
}

// Verified, everything okay
int32_t
NPP_Write(NPP instance, NPStream* stream, int32_t offset, int32_t len, void* buffer) {

	output << "NPP_Write Length: " << len << std::endl;

	writeMemory((char*)buffer, len);
	writeInt32(offset);
	writeHandle(stream);
	writeHandle(instance);
	callFunction(FUNCTION_NPP_WRITE);
	
	return readResultInt32();
}

void
NPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname) {
	output << ">>>>> STUB: NPP_StreamAsFile" << std::endl;
}

void
NPP_Print(NPP instance, NPPrint* platformPrint) {
	output << ">>>>> STUB: NPP_Print" << std::endl;
}

int16_t
NPP_HandleEvent(NPP instance, void* event) {
	output << ">>>>> STUB: NPP_HandleEvent" << std::endl;
	return 0;
}

// Verified, everything okay
void
NPP_URLNotify(NPP instance, const char* URL, NPReason reason, void* notifyData) {

	output << "NPP_URLNotify" << std::endl;

	writeHandleNotify(notifyData);
	writeInt32(reason);
	writeString(URL);
	writeHandle(instance);
	callFunction(FUNCTION_NPP_URL_NOTIFY);
	waitReturn();
}

// Verified, everything okay
NPError
NPP_GetValue(NPP instance, NPPVariable variable, void *value) {

	output << "NPP_GetValue: " << variable << std::endl;

	NPError result = NPERR_GENERIC_ERROR;
	std::vector<ParameterInfo> stack;

	switch(variable){

		case NPPVpluginNeedsXEmbed:
			output << "NPP_GetValue: NPPVpluginNeedsXEmbed" << std::endl;

			writeInt32(variable);
			writeHandle(instance);
			callFunction(FUNCTION_NPP_GETVALUE_BOOL);

			readCommands(stack);

			result = (NPError)readInt32(stack);

			if(result == NPERR_NO_ERROR)
				*((PRBool *)value) = (PRBool)readInt32(stack);
			
			// TODO: Remove this silverlight fix
			/*result = NPERR_NO_ERROR;
			*((PRBool *)value) = PR_TRUE;*/
			// END OF SILVERLIGHT FIX

			output << "XEmbed support: " << *((PRBool *)value) << " return error: " << result << std::endl;
			break;

		case NPPVpluginScriptableNPObject:

			output << "NPP_GetValue: NPPVpluginScriptableNPObject" << std::endl;
			
			writeInt32(variable);
			writeHandle(instance);
			callFunction(FUNCTION_NPP_GETVALUE_OBJECT);

			readCommands(stack);

			result 					= readInt32(stack);

			if(result == NPERR_NO_ERROR)
				*((NPObject**)value) 	= readHandleObj(stack);
			
			output << "NPPVpluginScriptableNPObject return error: " << result << std::endl;
			output << "TODO: Check if there was an RetainObject inbetween (should be there!)" << std::endl;
			break;


		default:
			output << "NPP_GetValue: unknown" << std::endl;
			break;
	}

	return result;
}

NPError
NPP_SetValue(NPP instance, NPNVariable variable, void *value) {
	output << ">>>>> STUB: NPP_SetValue" << std::endl;
	return NPERR_GENERIC_ERROR;
}