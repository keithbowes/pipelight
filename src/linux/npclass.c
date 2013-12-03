/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is fds-team.de code.
 *
 * The Initial Developer of the Original Code is
 * Michael Müller <michael@fds-team.de>
 * Portions created by the Initial Developer are Copyright (C) 2013
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michael Müller <michael@fds-team.de>
 *   Sebastian Lackner <sebastian@fds-team.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <string.h> 							// for memcpy

#include "../common/common.h"

void NPInvalidateFunction(NPObject *npobj){
	DBG_TRACE("( npobj=%p )", npobj);

	writeHandleObj(npobj);
	callFunction(FUNCTION_NP_INVALIDATE);
	readResultVoid();

	DBG_TRACE(" -> void");
}

bool NPHasMethodFunction(NPObject *npobj, NPIdentifier name){
	DBG_TRACE("( npobj=%p, name=%p )", npobj, name);

	writeHandleIdentifier(name);
	writeHandleObj(npobj);
	callFunction(FUNCTION_NP_HAS_METHOD);

	bool resultBool = (bool)readResultInt32();
	DBG_TRACE(" -> result=%d", resultBool);
	return resultBool;
}

bool NPInvokeFunction(NPObject *npobj, NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result){
	DBG_TRACE("( npobj=%p, name=%p, args[]=%p, argCount=%d, result=%p )", npobj, name, args, argCount, result);

	/* warning: parameter order swapped! */
	writeVariantArrayConst(args, argCount);
	writeInt32(argCount);
	writeHandleIdentifier(name);
	writeHandleObj(npobj);
	callFunction(FUNCTION_NP_INVOKE);

	Stack stack;
	readCommands(stack);

	bool resultBool = (bool)readInt32(stack);

	if (resultBool){
		readVariant(stack, *result); /* refcount already incremented by invoke() */
	}else{
		result->type 				= NPVariantType_Void;
		result->value.objectValue 	= NULL;
	}

	DBG_TRACE(" -> ( result=%d, ... )", resultBool);
	return resultBool;
}

bool NPInvokeDefaultFunction(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result){
	DBG_TRACE("( npobj=%p, args=%p, argCount=%d, result=%p )", npobj, args, argCount, result);

	writeVariantArrayConst(args, argCount);
	writeInt32(argCount);
	writeHandleObj(npobj);
	callFunction(FUNCTION_NP_INVOKE_DEFAULT);

	Stack stack;
	readCommands(stack);

	bool resultBool = (bool)readInt32(stack);

	if (resultBool){
		readVariant(stack, *result); /* refcount already incremented by invoke() */
	}else{
		result->type 				= NPVariantType_Void;
		result->value.objectValue 	= NULL;
	}	

	DBG_TRACE(" -> ( result=%d, ... )", resultBool);
	return resultBool;
}

bool NPHasPropertyFunction(NPObject *npobj, NPIdentifier name){
	DBG_TRACE("( npobj=%p, name=%p )", npobj, name);

	writeHandleIdentifier(name);
	writeHandleObj(npobj);
	callFunction(FUNCTION_NP_HAS_PROPERTY);

	bool resultBool = (bool)readResultInt32();
	DBG_TRACE(" -> ( result=%d, ... )", resultBool);
	return resultBool;
}

bool NPGetPropertyFunction(NPObject *npobj, NPIdentifier name, NPVariant *result){
	DBG_TRACE("( npobj=%p, name=%p, result=%p )", npobj, name, result);

	writeHandleIdentifier(name);
	writeHandleObj(npobj);
	callFunction(FUNCTION_NP_GET_PROPERTY);

	Stack stack;
	readCommands(stack);

	bool resultBool = readInt32(stack); /* refcount already incremented by getProperty() */

	if (resultBool){
		readVariant(stack, *result);
	}else{
		result->type 				= NPVariantType_Void;
		result->value.objectValue 	= NULL;
	}	

	DBG_TRACE(" -> ( result=%d, ... )", resultBool);
	return resultBool;
}

bool NPSetPropertyFunction(NPObject *npobj, NPIdentifier name, const NPVariant *value){
	DBG_TRACE("( npobj=%p, name=%p, value=%p )", npobj, name, value);

	writeVariantConst(*value);
	writeHandleIdentifier(name);
	writeHandleObj(npobj);
	callFunction(FUNCTION_NP_SET_PROPERTY);

	bool resultBool = (bool)readResultInt32();
	DBG_TRACE(" -> ( result=%d, ... )", resultBool);
	return resultBool;
}

bool NPRemovePropertyFunction(NPObject *npobj, NPIdentifier name){
	DBG_TRACE("( npobj=%p, name=%p )", npobj, name);

	writeHandleIdentifier(name);
	writeHandleObj(npobj);
	callFunction(FUNCTION_NP_REMOVE_PROPERTY);

	bool resultBool = (bool)readResultInt32();
	DBG_TRACE(" -> ( result=%d, ... )", resultBool);
	return resultBool;
}

bool NPEnumerationFunction(NPObject *npobj, NPIdentifier **value, uint32_t *count){
	DBG_TRACE("( npobj=%p, value=%p, count=%p )", npobj, value, count);

	writeHandleObj(npobj);
	callFunction(FUNCTION_NP_ENUMERATE);

	Stack stack;
	readCommands(stack);

	bool result = (bool)readInt32(stack);
	if (result){

		uint32_t identifierCount = readInt32(stack);
		if (identifierCount == 0){
			*value = NULL;
			*count = 0;

		}else{
			std::vector<NPIdentifier> identifiers 	= readIdentifierArray(stack, identifierCount);
			NPIdentifier* identifierTable 			= (NPIdentifier*)sBrowserFuncs->memalloc(identifierCount * sizeof(NPIdentifier));

			if (identifierTable){
				memcpy(identifierTable, identifiers.data(), sizeof(NPIdentifier) * identifierCount);

				*value = identifierTable;
				*count = identifierCount;

			}else{
				result = false;

			}
		}
	}

	DBG_TRACE(" -> ( result=%d, ... )", result);
	return result;
}

bool NPConstructFunction(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result){
	DBG_TRACE("( npobj=%p, args=%p, argCount=%d, result=%p )", npobj, args, argCount, result);
	NOTIMPLEMENTED();
	DBG_TRACE(" -> result=0");
	return false;
}

NPObject * NPAllocateFunction(NPP instance, NPClass *aClass){
	DBG_TRACE("( instance=%p, aClass=%p )", instance, aClass);

	NPObject* obj = (NPObject*)malloc(sizeof(NPObject));
	if (obj){
		obj->_class = aClass;
	}

	DBG_TRACE(" -> obj=%p", obj);
	return obj;
}

void NPDeallocateFunction(NPObject *npobj){
	DBG_TRACE("( npobj=%p )", npobj);

	if (npobj){
		if (handleManager_existsByPtr(HMGR_TYPE_NPObject, npobj)){
			DBG_TRACE("seems to be a user created handle, calling WIN_HANDLE_MANAGER_FREE_OBJECT( obj=%p ).", npobj);

			/* kill the object on the other side */
			writeHandleObj(npobj);
			callFunction(WIN_HANDLE_MANAGER_FREE_OBJECT);
			readResultVoid();

			/* remove it in the handle manager */
			handleManager_removeByPtr(HMGR_TYPE_NPObject, npobj);
		}

		/* remove the object locally */
		free(npobj);
	}

	DBG_TRACE(" -> void");
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