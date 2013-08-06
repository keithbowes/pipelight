#include "pluginloader.h"

/*
	NP Class
	These function *should* never be called from a plugin.
	The plugin has to use the browser API instead, so we just
	need stubs to detect a violation of the API.
*/

void NPInvalidateFunction(NPObject *npobj){
	debugNotImplemented("NPCLASS:NPInvalidateFunction");
}

bool NPHasMethodFunction(NPObject *npobj, NPIdentifier name){
	debugNotImplemented("NPCLASS:NPHasMethodFunction");
	return false;
}

bool NPInvokeFunction(NPObject *npobj, NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result){
	debugNotImplemented("NPCLASS:NPInvokeFunction");
	return false;
}

bool NPInvokeDefaultFunction(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result){
	debugNotImplemented("NPCLASS:NPInvokeDefaultFunction");
	return false;
}

bool NPHasPropertyFunction(NPObject *npobj, NPIdentifier name){
	debugNotImplemented("NPCLASS:NPHasPropertyFunction");
	return false;
}

bool NPGetPropertyFunction(NPObject *npobj, NPIdentifier name, NPVariant *result){
	debugNotImplemented("NPCLASS:NPGetPropertyFunction");
	return false;
}

bool NPSetPropertyFunction(NPObject *npobj, NPIdentifier name, const NPVariant *value){
	debugNotImplemented("NPCLASS:NPSetPropertyFunction");
	return false;
}

bool NPRemovePropertyFunction(NPObject *npobj, NPIdentifier name){
	debugNotImplemented("NPCLASS:NPRemovePropertyFunction");
	return false;
}

bool NPEnumerationFunction(NPObject *npobj, NPIdentifier **value, uint32_t *count){
	debugNotImplemented("NPCLASS:NPEnumerationFunction");
	return false;
}

bool NPConstructFunction(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result){
	debugNotImplemented("NPCLASS:NPConstructFunction");
	return false;
}

NPObject * NPAllocateFunction(NPP npp, NPClass *aClass){
	debugNotImplemented("NPCLASS:NPAllocateFunction");
	return NULL;
}

void NPDeallocateFunction(NPObject *npobj){
	debugNotImplemented("NPCLASS:NPDeallocateFunction");
}

NPClass myClass = {
	NP_CLASS_STRUCT_VERSION,
	NULL,
	NULL,
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
