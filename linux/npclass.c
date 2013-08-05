#include "basicplugin.h"

void NPInvalidateFunction(NPObject *npobj){
	debugEnterFunction("NPInvalidateFunction");

	writeHandleObj(npobj);
	callFunction(FUNCTION_NP_INVALIDATE_FUNCTION);
	waitReturn();
}

// Verified, everything okay
bool NPHasMethodFunction(NPObject *npobj, NPIdentifier name){
	debugEnterFunction("NPHasMethodFunction");

	writeHandleIdentifier(name);
	writeHandleObj(npobj);
	callFunction(FUNCTION_NP_HAS_METHOD_FUNCTION);

	return (bool)readResultInt32();

}

// Verified, everything okay
bool NPInvokeFunction(NPObject *npobj, NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result){
	debugEnterFunction("NPInvokeFunction");

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
		readVariant(stack, *result); // Dont increment refcount, this has already been done by invoke()
	}else{
		result->type = NPVariantType_Null;
	}	

	// The caller has to call NPN_ReleaseVariant if this should be freed

	return resultBool;
}

bool NPInvokeDefaultFunction(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result){
	debugNotImplemented("NPInvokeDefaultFunction");
	return false;
}

// Verified, everything okay
bool NPHasPropertyFunction(NPObject *npobj, NPIdentifier name){
	debugEnterFunction("NPHasPropertyFunction");

	writeHandleIdentifier(name);
	writeHandleObj(npobj);
	callFunction(FUNCTION_NP_HAS_PROPERTY_FUNCTION);

	return (bool)readResultInt32();
	
}

// Verified, everything okay
bool NPGetPropertyFunction(NPObject *npobj, NPIdentifier name, NPVariant *result){
	debugEnterFunction("NPGetPropertyFunction");

	writeHandleIdentifier(name);
	writeHandleObj(npobj);
	callFunction(FUNCTION_NP_GET_PROPERTY_FUNCTION);

	std::vector<ParameterInfo> stack;
	readCommands(stack);

	bool resultBool = readInt32(stack);

	if(resultBool){
		readVariant(stack, *result);
	}else{
		result->type = NPVariantType_Null;
	}	

	return resultBool;
}

bool NPSetPropertyFunction(NPObject *npobj, NPIdentifier name, const NPVariant *value){
	debugEnterFunction("NPSetPropertyFunction");

	writeVariantConst(*value);
	writeHandleIdentifier(name);
	writeHandleObj(npobj);
	callFunction(FUNCTION_NP_SET_PROPERTY_FUNCTION);

	return (bool)readResultInt32();

}

bool NPRemovePropertyFunction(NPObject *npobj, NPIdentifier name){
	debugNotImplemented("NPRemovePropertyFunction");
	return false;
}

bool NPEnumerationFunction(NPObject *npobj, NPIdentifier **value, uint32_t *count){
	debugNotImplemented("NPEnumerationFunction");
	return false;
}

bool NPConstructFunction(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result){
	debugNotImplemented("NPConstructFunction");
	return false;
}

// Verified, everything okay
NPObject * NPAllocateFunction(NPP npp, NPClass *aClass){
	debugEnterFunction("NPAllocateFunction");

	NPObject* obj = (NPObject*)malloc(sizeof(NPObject));
	if(obj){
		obj->_class = aClass; // Probably not required, just to be on the save side ;-)
	}

	return obj;
}

// Verified, everything okay
void NPDeallocateFunction(NPObject *npobj){
	debugEnterFunction("NPDeallocateFunction");

	if(npobj){
		bool exists = handlemanager.existsHandleByReal((uint64_t)npobj, TYPE_NPObject);

		if( exists ){
			// This has to be a user-created object which has to be freed via a KILL_OBJECT message

			// Kill the object on the other side
			writeHandleObj(npobj);
			callFunction(OBJECT_KILL);
			waitReturn();
		}

		// Remove the object locally
		free(npobj);

		// Remove it in the handle manager
		if( exists ){
			handlemanager.removeHandleByReal((uint64_t)npobj, TYPE_NPObject);
		}

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