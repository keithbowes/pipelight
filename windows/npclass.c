#include "pluginloader.h"

/*
	NP Class
	These function *should* never be called from a plugin.
	The plugin must use the browser API instead, so we just
	need stubs to detect a violation of the api.
*/

void NPInvalidateFunction(NPObject *npobj){
	output << ">>>>> NPClass STUB: NPInvalidateFunction" << std::endl;
}

bool NPHasMethodFunction(NPObject *npobj, NPIdentifier name){
	output << ">>>>> NPClass STUB: NPHasMethodFunction" << std::endl;
	return false;
}

bool NPInvokeFunction(NPObject *npobj, NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result){
	output << ">>>>> NPClass STUB: NPInvokeFunction" << std::endl;
	return false;
}

bool NPInvokeDefaultFunction(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result){
	output << ">>>>> NPClass STUB: NPInvokeDefaultFunction" << std::endl;
	return false;
}

bool NPHasPropertyFunction(NPObject *npobj, NPIdentifier name){
	output << ">>>>> NPClass STUB: NPHasPropertyFunction" << std::endl;
	return false;
}

bool NPGetPropertyFunction(NPObject *npobj, NPIdentifier name, NPVariant *result){
	output << ">>>>> NPClass STUB: NPGetPropertyFunction" << std::endl;
	return false;
}

bool NPSetPropertyFunction(NPObject *npobj, NPIdentifier name, const NPVariant *value){
	output << ">>>>> NPClass STUB: NPSetPropertyFunction" << std::endl;
	return false;
}

bool NPRemovePropertyFunction(NPObject *npobj, NPIdentifier name){
	output << ">>>>> NPClass STUB: NPRemovePropertyFunction" << std::endl;
	return false;
}

bool NPEnumerationFunction(NPObject *npobj, NPIdentifier **value, uint32_t *count){
	output << ">>>>> NPClass STUB: NPEnumerationFunction" << std::endl;
	return false;
}

bool NPConstructFunction(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result){
	output << ">>>>> NPClass STUB: NPConstructFunction" << std::endl;
	return false;
}

NPObject * NPAllocateFunction(NPP npp, NPClass *aClass){
	output << ">>>>> NPClass STUB: NPAllocateFunction" << std::endl;
	return 0;
}

void NPDeallocateFunction(NPObject *npobj){
	output << ">>>>> NPClass STUB: NPDeallocateFunction" << std::endl;
}

NPClass myClass = {
	NP_CLASS_STRUCT_VERSION,
	NULL,//NPAllocateFunction,
	NULL,//NPDeallocateFunction,
	NPInvalidateFunction,
	NPHasMethodFunction,
	NPInvokeFunction,
	NPInvokeDefaultFunction, 
	NPHasPropertyFunction,
	NPGetPropertyFunction,
	NPSetPropertyFunction,
	NPRemovePropertyFunction,
	NPEnumerationFunction,
	NPConstructFunction	
};
