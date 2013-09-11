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
	DBG_TRACE("( npobj=0x%p )", npobj);
	NOTIMPLEMENTED();
}

bool NPHasMethodFunction(NPObject *npobj, NPIdentifier name){
	DBG_TRACE("( npobj=0x%p, name=0x%p )", npobj, name);
	NOTIMPLEMENTED();
	return false;
}

bool NPInvokeFunction(NPObject *npobj, NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result){
	DBG_TRACE("( npobj=0x%p, name=0x%p, args=0x%p, argCount=%d, result=0x%p )", npobj, name, args, argCount, result);
	NOTIMPLEMENTED();
	return false;
}

bool NPInvokeDefaultFunction(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result){
	DBG_TRACE("( npobj=0x%p, args=0x%p, argCount=%d, result=0x%p )", npobj, args, argCount, result);
	NOTIMPLEMENTED();
	return false;
}

bool NPHasPropertyFunction(NPObject *npobj, NPIdentifier name){
	DBG_TRACE("( npobj=0x%p, name=0x%p )", npobj, name);
	NOTIMPLEMENTED();
	return false;
}

bool NPGetPropertyFunction(NPObject *npobj, NPIdentifier name, NPVariant *result){
	DBG_TRACE("( npobj=0x%p, name=0x%p, result=0x%p )", npobj, name, result);
	NOTIMPLEMENTED();
	return false;
}

bool NPSetPropertyFunction(NPObject *npobj, NPIdentifier name, const NPVariant *value){
	DBG_TRACE("( npobj=0x%p, name=0x%p, result=0x%p )", npobj, name, value);
	NOTIMPLEMENTED();
	return false;
}

bool NPRemovePropertyFunction(NPObject *npobj, NPIdentifier name){
	DBG_TRACE("( npobj=0x%p, name=0x%p )", npobj, name);
	NOTIMPLEMENTED();
	return false;
}

bool NPEnumerationFunction(NPObject *npobj, NPIdentifier **value, uint32_t *count){
	DBG_TRACE("( npobj=0x%p, value=0x%p, count=0x%p )", npobj, value, count);
	NOTIMPLEMENTED();
	return false;
}

bool NPConstructFunction(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result){
	DBG_TRACE("( npobj=0x%p, args=0x%p, argCount=%d, result=0x%p )", npobj, args, argCount, result);
	NOTIMPLEMENTED();
	return false;
}

NPObject * NPAllocateFunction(NPP instance, NPClass *aClass){
	DBG_TRACE("( instance=0x%p, aClass=0x%p )", instance, aClass);
	NOTIMPLEMENTED();
	return NULL;
}

void NPDeallocateFunction(NPObject *npobj){
	DBG_TRACE("( npobj=%p )", npobj);
	NOTIMPLEMENTED();
}

NPClass myClass = {
	NP_CLASS_STRUCT_VERSION,
	NULL, // NPAllocateFunction,
	NULL, // NPDeallocateFunction,
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
