#include <string.h> 							// for memcpy

#include "basicplugin.h"

#ifdef DEBUG_LOG_HANDLES
	#include <iostream>							// for std::cerr
#endif

void NPInvalidateFunction(NPObject *npobj){
	DBG_TRACE("NPInvalidateFunction( npobj=%p )", npobj);

	writeHandleObj(npobj);
	callFunction(FUNCTION_NP_INVALIDATE);
	waitReturn();
}

bool NPHasMethodFunction(NPObject *npobj, NPIdentifier name){
	DBG_TRACE("NPHasMethodFunction( npobj=%p, name=%p )", npobj, name);

	writeHandleIdentifier(name);
	writeHandleObj(npobj);
	callFunction(FUNCTION_NP_HAS_METHOD);

	return (bool)readResultInt32();

}

bool NPInvokeFunction(NPObject *npobj, NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result){
	DBG_TRACE("NPInvokeFunction( npobj=%p, name=%p, args[]=%p, argCount=%d, result=%p )", npobj, name, args, argCount, result);

	// Warning: parameter order swapped!
	writeVariantArrayConst(args, argCount);
	writeInt32(argCount);
	writeHandleIdentifier(name);
	writeHandleObj(npobj);
	callFunction(FUNCTION_NP_INVOKE);

	std::vector<ParameterInfo> stack;
	readCommands(stack);

	bool resultBool = (bool)readInt32(stack);

	if(resultBool){
		readVariant(stack, *result); // Refcount already incremented by invoke()
	}else{
		result->type 				= NPVariantType_Void;
		result->value.objectValue 	= NULL;
	}

	return resultBool;
}

bool NPInvokeDefaultFunction(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result){
	DBG_TRACE("NPInvokeDefaultFunction( npobj=%p, args=%p, argCount=%d, result=%p )", npobj, args, argCount, result);

	writeVariantArrayConst(args, argCount);
	writeInt32(argCount);
	writeHandleObj(npobj);
	callFunction(FUNCTION_NP_INVOKE_DEFAULT);

	std::vector<ParameterInfo> stack;
	readCommands(stack);

	bool resultBool = (bool)readInt32(stack);

	if(resultBool){
		readVariant(stack, *result); // Refcount already incremented by invoke()
	}else{
		result->type 				= NPVariantType_Void;
		result->value.objectValue 	= NULL;
	}	

	return resultBool;
}

bool NPHasPropertyFunction(NPObject *npobj, NPIdentifier name){
	DBG_TRACE("NPHasPropertyFunction( npobj=%p, name=%p )", npobj, name);

	writeHandleIdentifier(name);
	writeHandleObj(npobj);
	callFunction(FUNCTION_NP_HAS_PROPERTY);

	return (bool)readResultInt32();
}

bool NPGetPropertyFunction(NPObject *npobj, NPIdentifier name, NPVariant *result){
	DBG_TRACE("NPGetPropertyFunction( npobj=%p, name=%p, result=%p )", npobj, name, result);

	writeHandleIdentifier(name);
	writeHandleObj(npobj);
	callFunction(FUNCTION_NP_GET_PROPERTY);

	std::vector<ParameterInfo> stack;
	readCommands(stack);

	bool resultBool = readInt32(stack); // Refcount already incremented by getProperty()

	if(resultBool){
		readVariant(stack, *result);
	}else{
		result->type 				= NPVariantType_Void;
		result->value.objectValue 	= NULL;
	}	

	return resultBool;
}

bool NPSetPropertyFunction(NPObject *npobj, NPIdentifier name, const NPVariant *value){
	DBG_TRACE("NPSetPropertyFunction( npobj=%p, name=%p, value=%p )", npobj, name, value);

	writeVariantConst(*value);
	writeHandleIdentifier(name);
	writeHandleObj(npobj);
	callFunction(FUNCTION_NP_SET_PROPERTY);

	return (bool)readResultInt32();
}

bool NPRemovePropertyFunction(NPObject *npobj, NPIdentifier name){
	DBG_TRACE("NPRemovePropertyFunction( npobj=%p, name=%p )", npobj, name);

	writeHandleIdentifier(name);
	writeHandleObj(npobj);
	callFunction(FUNCTION_NP_REMOVE_PROPERTY);

	return (bool)readResultInt32();
}

bool NPEnumerationFunction(NPObject *npobj, NPIdentifier **value, uint32_t *count){
	DBG_TRACE("NPEnumerationFunction( npobj=%p, value=%p, count=%p )", npobj, value, count);

	writeHandleObj(npobj);
	callFunction(FUNCTION_NP_ENUMERATE);

	std::vector<ParameterInfo> stack;
	readCommands(stack);

	bool 	 result                         = (bool)readInt32(stack);
	if(!result){
		return false;
	}

	uint32_t identifierCount 				= readInt32(stack);
	if(identifierCount == 0){
		*value = NULL;
		*count = 0;
		return result;
	}

	std::vector<NPIdentifier> identifiers 	= readIdentifierArray(stack, identifierCount);

	NPIdentifier* identifierTable = (NPIdentifier*)sBrowserFuncs->memalloc(identifierCount * sizeof(NPIdentifier));
	if(!identifierTable){
		return false;
	}

	memcpy(identifierTable, identifiers.data(), sizeof(NPIdentifier) * identifierCount);

	*value = identifierTable;
	*count = identifierCount;
	return true;
}

bool NPConstructFunction(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result){
	DBG_TRACE("NPConstructFunction( npobj=%p, args=%p, argCount=%d, result=%p )", npobj, args, argCount, result);
	NOTIMPLEMENTED();
	return false;
}

NPObject * NPAllocateFunction(NPP npp, NPClass *aClass){
	DBG_TRACE("NPAllocateFunction( npp=%p, aClass=%p )", npp, aClass);

	NPObject* obj = (NPObject*)malloc(sizeof(NPObject));
	if(obj){
		obj->_class = aClass;
	}

	return obj;
}

void NPDeallocateFunction(NPObject *npobj){
	DBG_TRACE("NPDeallocateFunction( npp=%p )", npobj);

	if(npobj){
		bool exists = handlemanager.existsHandleByReal((uint64_t)npobj, TYPE_NPObject);

		if( exists ){
			DBG_TRACE("seems to be a user created handle, calling WIN_HANDLE_MANAGER_FREE_OBJECT(%p).", npobj);

			// Kill the object on the other side
			writeHandleObj(npobj);
			callFunction(WIN_HANDLE_MANAGER_FREE_OBJECT);
			waitReturn();

			// Remove it in the handle manager
			handlemanager.removeHandleByReal((uint64_t)npobj, TYPE_NPObject);
		}

		// Remove the object locally
		free(npobj);
	}
}

NPClass myClass = {
	NP_CLASS_STRUCT_VERSION,
	NPAllocateFunction,
	NPDeallocateFunction,
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