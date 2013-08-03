#include "basicplugin.h"

char strMimeType[1024] 			= {0};
char strPluginversion[100]		= {0};
char strPluginName[256] 		= {0};
char strPluginDescription[256]	= {0};

void pokeString(std::string str, char *dest, unsigned int maxLength){

	unsigned int length = std::min((unsigned int)str.length(), maxLength-1);
	if (length > 0){
		memcpy(dest, str.c_str(), length);
	}
}


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

NP_EXPORT(char*)
NP_GetPluginVersion()
{
	callFunction(FUNCTION_GET_VERSION);
	std::string result = readResultString();
	pokeString(result, strPluginversion, sizeof(strPluginversion));

	return strPluginversion;
}

NP_EXPORT(const char*)
NP_GetMIMEDescription()
{
	callFunction(FUNCTION_GET_MIMETYPE);
	std::string result = readResultString();
	pokeString(result, strMimeType, sizeof(strMimeType));

	return strMimeType;
}

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
			break;

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

	//output << "timer tid" << syscall(SYS_gettid) << std::endl;

	/*if(checkReadCommands()){
		std::vector<ParameterInfo> stack;
		readCommands(stack, false, true); // return when stack empty
	}*/

	//output << "timer ready" << std::endl;

	callFunction(PROCESS_WINDOW_EVENTS);
	waitReturn();
}

NPError
NPP_New(NPMIMEType pluginType, NPP instance, uint16_t mode, int16_t argc, char* argn[], char* argv[], NPSavedData* saved) {

	output << "NPP_New" << std::endl;

	sBrowserFuncs->scheduletimer(instance, 50, true, timerFunc);

	//output << "other tid" << syscall(SYS_gettid) << std::endl;

	if (saved){
		writeMemory((char*)saved->buf, saved->len);
	}else{
		writeMemory(NULL, 0);
	}

	writeCharStringArray(argv, argc);
	writeCharStringArray(argn, argc);

	writeInt32(argc);
	writeInt32(mode);

	writeHandle(instance);
	writeString(pluginType);	

	callFunction(FUNCTION_NPP_NEW);

	return readResultInt32();
}

NPError
NPP_Destroy(NPP instance, NPSavedData** save) {
	output << ">>>>> STUB: NPP_Destroy" << std::endl;
	return NPERR_NO_ERROR;
}

NPError
NPP_SetWindow(NPP instance, NPWindow* window) {

	output << "NPP_SetWindow" << std::endl;

	//TODO: translate to Screen coordinates

	writeInt32(window->height);
	writeInt32(window->width);
	writeInt32(window->y);
	writeInt32(window->x);
	writeHandle(instance);

	callFunction(FUNCTION_SET_WINDOW_INFO);
	waitReturn();

	output << "NPP_SetWindow returned" << std::endl;

	return NPERR_NO_ERROR;
}

NPError
NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16_t* stype) {
	output << "NPP_NewStream with URL: " << stream->url << std::endl;

	writeInt32(seekable);
	writeHandle(stream);
	
	if(!type) 
		throw std::runtime_error("Mimetype: NULL?");

	writeString(type);
	writeHandle(instance);

	callFunction(FUNCTION_NPP_NEW_STREAM);

	Stack stack;
	readCommands(stack);	

	NPError result 	= readInt32(stack);
	*stype 			= (uint16_t)readInt32(stack);

	output << "NPP_NewStream finished with result " << result << " and type " << *stype << std::endl;

	return result;
}

NPError
NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason) {

	output << "NPP_DestroyStream" << std::endl;
	
	writeInt32(reason);
	writeHandle(stream);
	writeHandle(instance);
	callFunction(FUNCTION_NPP_DESTROY_STREAM);

	NPError result = readResultInt32();

	handlemanager.removeHandleByReal((uint64_t)stream);

	return result;
}

int32_t
NPP_WriteReady(NPP instance, NPStream* stream) {

	output << "NPP_WriteReady" << std::endl;

	writeHandle(stream);
	writeHandle(instance);	

	callFunction(FUNCTION_NPP_WRITE_READY);
	
	int32_t result = readResultInt32();

	output << "NPP_WriteReady - Maximum Length" << result << std::endl;

	return result;
}

int32_t
NPP_Write(NPP instance, NPStream* stream, int32_t offset, int32_t len, void* buffer) {

	output << "NPP_Write Length: " << len << std::endl;

	writeMemory((char*)buffer, len);
	//writeInt32(len); Our protocol does send this information automaticly
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
			*((PRBool *)value) = (PRBool)readInt32(stack);
			output << "XEmbed support: " << *((PRBool *)value) << " return error: " << result << std::endl;
			break;

		case NPPVpluginScriptableNPObject:

			output << "NPP_GetValue: NPPVpluginScriptableNPObject" << std::endl;
			
			writeInt32(variable);
			writeHandle(instance);
			callFunction(FUNCTION_NPP_GETVALUE_OBJECT);

			readCommands(stack);

			result 					= readInt32(stack);
			*((NPObject**)value) 	= readHandleObj(stack);
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