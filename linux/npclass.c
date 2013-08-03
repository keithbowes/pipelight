#include "basicplugin.h"

void NPInvalidateFunction(NPObject *npobj){
	output << ">>>>> STUB: NPInvalidateFunction" << std::endl;
}

bool NPHasMethodFunction(NPObject *npobj, NPIdentifier name){
	output << "NPHasMethodFunction" << std::endl;

	writeHandle(name);
	writeHandle(npobj);
	callFunction(FUNCTION_NP_HAS_METHOD_FUNCTION);
	return (bool)readResultInt32();

}

bool NPInvokeFunction(NPObject *npobj, NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result){

	output << "NPInvokeFunction" << std::endl;

	// Warning: parameter order swapped!
	writeVariantArray(args, argCount);

	writeInt32(argCount);

	writeHandle(name);
	writeHandle(npobj);

	callFunction(FUNCTION_NP_INVOKE);

	std::vector<ParameterInfo> stack;
	readCommands(stack);

	uint32_t resultBool = readInt32(stack);
	if(resultBool){
		readVariant(stack, *result);
	}else{
		result->type = NPVariantType_Null;
	}	

	output << "NP_Invoke Result: " << resultBool << std::endl;

	return resultBool;
}

bool NPInvokeDefaultFunction(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result){
	output << ">>>>> STUB: NPInvokeDefaultFunction" << std::endl;
	return false;
}

bool NPHasPropertyFunction(NPObject *npobj, NPIdentifier name){

	output << "NPHasPropertyFunction" << std::endl;

	writeHandle(name);
	writeHandle(npobj);
	callFunction(FUNCTION_NP_HAS_PROPERTY_FUNCTION);
	
	output << "NPHasPropertyFunction redirected" << std::endl;


	bool result = (bool)readResultInt32();

	output << "NPHasPropertyFunction returned " << result << std::endl;

	return result;
	
}

bool NPGetPropertyFunction(NPObject *npobj, NPIdentifier name, NPVariant *result){

	output << "NPGetPropertyFunction" << std::endl;

	writeHandle(name);
	writeHandle(npobj);

	callFunction(FUNCTION_NP_GET_PROPERTY_FUNCTION);

	std::vector<ParameterInfo> stack;
	readCommands(stack);

	uint32_t resultBool = readInt32(stack);
	if(resultBool){
		readVariant(stack, *result);
	}else{
		result->type = NPVariantType_Null;
	}	

	return (bool)resultBool;
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

NPObject * NPAllocateFunction(NPP npp, NPClass *aClass){

	output << "Browser called Allocate Object" << std::endl;

	NPObject* obj = (NPObject*)malloc(sizeof(NPObject));
	if(obj){
		obj->_class = aClass;
	}
	return obj;
}

void NPDeallocateFunction(NPObject *npobj){
	output << "Browser called Deallocate Object" << std::endl;

	writeHandle(npobj);
	callFunction(OBJECT_KILL);
	waitReturn();

	handlemanager.removeHandleByReal((uint64_t)npobj);

	free(npobj);
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