#include "../npapi-headers/npapi.h"
#include "../npapi-headers/npruntime.h"

#include "../communication/communication.h"

/*
	NP Class
	These function *should* never be called from a plugin.
	The plugin has to use the browser API instead, so we just
	need stubs to detect a violation of the API.
*/

void NPInvalidateFunction(NPObject *npobj){
	NotImplemented();
}

bool NPHasMethodFunction(NPObject *npobj, NPIdentifier name){
	NotImplemented();
	return false;
}

bool NPInvokeFunction(NPObject *npobj, NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result){
	NotImplemented();
	return false;
}

bool NPInvokeDefaultFunction(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result){
	NotImplemented();
	return false;
}

bool NPHasPropertyFunction(NPObject *npobj, NPIdentifier name){
	NotImplemented();
	return false;
}

bool NPGetPropertyFunction(NPObject *npobj, NPIdentifier name, NPVariant *result){
	NotImplemented();
	return false;
}

bool NPSetPropertyFunction(NPObject *npobj, NPIdentifier name, const NPVariant *value){
	NotImplemented();
	return false;
}

bool NPRemovePropertyFunction(NPObject *npobj, NPIdentifier name){
	NotImplemented();
	return false;
}

bool NPEnumerationFunction(NPObject *npobj, NPIdentifier **value, uint32_t *count){
	NotImplemented();
	return false;
}

bool NPConstructFunction(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result){
	NotImplemented();
	return false;
}

NPObject * NPAllocateFunction(NPP npp, NPClass *aClass){
	NotImplemented();
	return NULL;
}

void NPDeallocateFunction(NPObject *npobj){
	NotImplemented();
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
