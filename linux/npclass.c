#include "basicplugin.h"

void NPInvalidateFunction(NPObject *npobj){
	output << ">>>>> STUB: NPInvalidateFunction" << std::endl;
}

// Verified, everything okay
bool NPHasMethodFunction(NPObject *npobj, NPIdentifier name){
	output << "NPHasMethodFunction" << std::endl;

	writeHandle(name);
	writeHandle(npobj);
	callFunction(FUNCTION_NP_HAS_METHOD_FUNCTION);
	return (bool)readResultInt32();

}

// Verified, everything okay
bool NPInvokeFunction(NPObject *npobj, NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result){

	output << "NPInvokeFunction (myClass)" << std::endl;

	// Warning: parameter order swapped!
	writeVariantArrayConst(args, argCount);
	writeInt32(argCount);
	writeHandle(name);
	writeHandle(npobj);
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

	output << "NP_Invoke Result: " << resultBool << std::endl;

	return resultBool;
}

bool NPInvokeDefaultFunction(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result){
	output << ">>>>> STUB: NPInvokeDefaultFunction" << std::endl;
	return false;
}

// Verified, everything okay
bool NPHasPropertyFunction(NPObject *npobj, NPIdentifier name){

	output << "NPHasPropertyFunction" << std::endl;

	writeHandle(name);
	writeHandle(npobj);
	callFunction(FUNCTION_NP_HAS_PROPERTY_FUNCTION);
	return (bool)readResultInt32();
	
}

// Verified, everything okay
bool NPGetPropertyFunction(NPObject *npobj, NPIdentifier name, NPVariant *result){

	output << "NPGetPropertyFunction" << std::endl;

	writeHandle(name);
	writeHandle(npobj);
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
	output << ">>>>> STUB: NPSetPropertyFunction" << std::endl;
	return false;
}

bool NPRemovePropertyFunction(NPObject *npobj, NPIdentifier name){
	output << ">>>>> STUB: NPRemovePropertyFunction" << std::endl;
	return false;
}

bool NPEnumerationFunction(NPObject *npobj, NPIdentifier **value, uint32_t *count){
	output << ">>>>> STUB: NPEnumerationFunction" << std::endl;
	return false;
}

bool NPConstructFunction(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result){
	output << ">>>>> STUB: NPConstructFunction" << std::endl;
	return false;
}

// Verified, everything okay
NPObject * NPAllocateFunction(NPP npp, NPClass *aClass){

	NPObject* obj = (NPObject*)malloc(sizeof(NPObject));
	if(obj){
		obj->_class = aClass;
	}

	output << "Browser called Allocate Object " << (void*)obj << std::endl;

	return obj;
}

// Verified, everything okay
void NPDeallocateFunction(NPObject *npobj){
	output << "Browser called Deallocate Object " << (void*)npobj << std::endl;

	if(npobj){

		// Kill the object on the other side
		/*writeHandle(npobj);
		callFunction(OBJECT_KILL);
		waitReturn();*/

		// Remove the object locally
		free(npobj);

		// Remove it in the handle manager
		//handlemanager.removeHandleByReal((uint64_t)npobj, TYPE_NPObject);
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