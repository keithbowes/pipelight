#include <cstring>								// for memset, ...
#include <cstdlib>								// for malloc, ...
#include <map>									// for std::map
#include <stdexcept>							// for std::runtime_error
#include <iomanip>								// std::setprecision

#include "../communication/communication.h"
#include "handlemanager.h"

#ifdef DEBUG_LOG_HANDLES
	#ifdef __WINE__
		#undef min
		#undef max
	#endif
	#include <iostream>							// for std::cerr
#endif

#ifdef __WIN32__
	extern NPClass myClass;						// required for implementation of createNPObject
#else
	#include "../npapi-headers/npfunctions.h"	// for sBrowserFuncs->{memfree, retain, releasevariant}
	extern NPNetscapeFuncs *sBrowserFuncs;
#endif

extern HandleManager handlemanager;				// global handlemanager object

// TODO: Improve this method - allow using low handles again
uint64_t HandleManager::getFreeID(){
	if(!handlesID.empty()){
		//The last elment has the biggest ID
		uint64_t freeHandle = handlesID.rbegin()->first + 1;

		if(freeHandle == 0){
			throw std::runtime_error("Too much handles?");
		}

		return freeHandle;

	}else{
		return 1;
	}

}

#ifdef __WIN32__
NPObject* createNPObject(uint64_t id, NPClass *aclass = NULL, NPP instance = 0){
	NPObject* obj 		= NULL;
	bool customObject  	= (aclass != NULL);

	if(!aclass) aclass = &myClass;

	if(aclass->allocate){
		obj = aclass->allocate(instance, aclass);
	}else{
		obj = (NPObject*)malloc(sizeof(NPObject));
	}

	if(!obj) throw std::runtime_error("Could not create object!");			

	obj->_class 		= aclass;

	// If its a custom created object then we can get the deallocate event and don't have to do manually refcounting.
	// Otherwise its just a proxy object and can be destroyed when there is no pointer anymore in the Windows area
	if(customObject){
		DBG_TRACE("created custom object %p with aclass %p.", obj, aclass);
		obj->referenceCount = REFCOUNT_UNDEFINED;

	}else{
		DBG_TRACE("created proxy object %p.", obj);
		obj->referenceCount	= 0; // Will be incremented via readHandleObjInc
	}

	return obj;
}

NPP_t* createNPPInstance(uint64_t id){
	NPP_t* instance = (NPP_t*)malloc(sizeof(NPP_t));

	if(instance)
		memset(instance, 0, sizeof(NPP_t));

	return instance;
}

NPStream * createNPStream(uint64_t id){
	NPStream *stream = (NPStream*)malloc(sizeof(NPStream));

	if(!stream) throw std::runtime_error("Could not create stream!");

	// We cannot use writeHandle, as the handle manager hasn't finished adding this yet.
	writeInt64(id);
	writeInt32(TYPE_NPStream);
	callFunction(LIN_HANDLE_MANAGER_REQUEST_STREAM_INFO);

	std::vector<ParameterInfo> 	stack;
	readCommands(stack);
	
	// Initialize memory
	stream->pdata 			= NULL;
	stream->ndata 			= NULL;
	stream->url				= readStringMalloc(stack);
	stream->end 			= readInt32(stack);
	stream->lastmodified 	= readInt32(stack);
	stream->notifyData		= readHandleNotify(stack);
	stream->headers 		= readStringMalloc(stack);

	return stream;
}

#else

NotifyDataRefCount* createNotifyDataRefCount(uint64_t id){
	NotifyDataRefCount* notifyData = (NotifyDataRefCount*)malloc(sizeof(NotifyDataRefCount));
	if(!notifyData) throw std::runtime_error("Could not create notify-data wrapper!");

	notifyData->referenceCount = 0;

	return notifyData;
}

#endif

// Used for incoming handle translation(id -> real)
// aclass and instance  are used for some cases when a new object is generated
uint64_t HandleManager::translateFrom(uint64_t id, HandleType type, NPP instance, NPClass *aclass, HandleExists shouldExist){
	std::map<uint64_t, Handle>::iterator it;

	if(!id){
		if(type == TYPE_NotifyData){
			return 0;

		}else{
			throw std::runtime_error("Trying to translate the reserved null ID");
		}
	}

	it = handlesID.find(id);
	if(it != handlesID.end()){

		// When an aclass is given, this is an error, as we expected a new object
		if(aclass || shouldExist == HANDLE_SHOULD_NOT_EXIST){		
			throw std::runtime_error("Expected a new handle, but I already got this one");

		}else if(it->second.type != type){
			throw std::runtime_error("Wrong handle type when translating ID to real object");
		}

		return it->second.real;
	}

	// Should the ID already exist
	if(shouldExist == HANDLE_SHOULD_EXIST){
		throw std::runtime_error("Got ID which should exist, but it doesnt!");
	}

	//Create handle
	Handle handle;
	handle.id 	= id;
	handle.type = type;

	#ifdef __WIN32__
		if(type == TYPE_NPObject){
			handle.real = (uint64_t) createNPObject(id, aclass, instance);
		}else if(type == TYPE_NPIdentifier){
			handle.real = id;
		}else if(type == TYPE_NPPInstance){
			handle.real = (uint64_t) createNPPInstance(id);
		}else if(type == TYPE_NPStream){
			handle.real = (uint64_t) createNPStream(id);
		}else{
			throw std::runtime_error("Error in handle manager: Cannot create remote object of this type");
		}
	#else
		if(type == TYPE_NotifyData){
			handle.real = (uint64_t) createNotifyDataRefCount(id);
		}else{
			throw std::runtime_error("Error in handle manager: Cannot create local object of this type");
		}
	#endif

	handlesID[id] 														= handle;
	handlesReal[std::pair<HandleType, uint64_t>(type, handle.real)] 	= handle;
	return handle.real;
}

uint64_t HandleManager::translateTo(uint64_t real, HandleType type, HandleExists shouldExist){
	std::map<std::pair<HandleType, uint64_t>, Handle>::iterator it;

	// Except for TYPE_NotifyData we dont allow nullpointers here for obvious reasons
	if(!real){
		if(type == TYPE_NotifyData){
			return 0;

		}else{
			throw std::runtime_error("Trying to translate a null-handle");
		}
	}

	it = handlesReal.find(std::pair<HandleType, uint64_t>(type, real));
	if(it != handlesReal.end()){

		if(shouldExist == HANDLE_SHOULD_NOT_EXIST){
			throw std::runtime_error("Expected a new handle, but I already got this one");
		}

		return it->second.id;
	}

	if(shouldExist == HANDLE_SHOULD_EXIST){
		throw std::runtime_error("Got real handle which should exist, but it doesnt!");
	}

	#ifdef __WIN32__
		if(type != TYPE_NotifyData){
			throw std::runtime_error("Error in handle manager: Cannot create remote object of this type");
		}
	#else
		if(type == TYPE_NotifyData){
			throw std::runtime_error("Error in handle manager: Cannot create local object of this type");
		}
	#endif

	Handle handle;
	handle.id 			= getFreeID();
	handle.real 		= real;
	handle.type 		= type;

	handlesID[handle.id] 	= handle;
	handlesReal[std::pair<HandleType, uint64_t>(type, real)] 		= handle;

	return handle.id;
}

void HandleManager::removeHandleByID(uint64_t id){
	std::map<uint64_t, Handle>::iterator it;

	it = handlesID.find(id);
	if(it == handlesID.end()) throw std::runtime_error("Trying to remove handle by nonexistend ID");

	handlesReal.erase(std::pair<HandleType, uint64_t>(it->second.type, it->second.real));
	handlesID.erase(it);
}

void HandleManager::removeHandleByReal(uint64_t real, HandleType type){
	std::map<std::pair<HandleType, uint64_t>, Handle>::iterator it;

	it = handlesReal.find(std::pair<HandleType, uint64_t>(type, real));
	if(it == handlesReal.end()) throw std::runtime_error("Trying to remove handle by nonexistend real object");

	handlesID.erase(it->second.id);
	handlesReal.erase(it);
}

bool HandleManager::existsHandleByReal(uint64_t real, HandleType type){
	std::map<std::pair<HandleType, uint64_t>, Handle>::iterator it;

	it = handlesReal.find(std::pair<HandleType, uint64_t>(type, real));
	if(it == handlesReal.end()) return false;

	return true;
}

NPP_t* HandleManager::findInstance(){
	std::map<uint64_t, Handle>::iterator it;

	for(it = handlesID.begin(); it != handlesID.end(); it++){
		if(it->second.type == TYPE_NPPInstance){
			return (NPP_t*)it->second.real;
		}
	}

	return NULL;
}

uint64_t HandleManager::handleCount(){
	uint64_t count = handlesID.size();

	if(count != handlesReal.size()){
		throw std::runtime_error("Number if local handles is not unique due to destroyed handle manager structures");
	}

	return count;
}


void HandleManager::clear(){

	handlesID.clear();
	handlesReal.clear();

}

void writeHandle(uint64_t real, HandleType type, HandleExists shouldExist){
	writeInt64(handlemanager.translateTo(real, type, shouldExist));
	writeInt32(type);
}

void writeHandleObj(NPObject *obj, HandleExists shouldExist, bool deleteFromRemoteHandleManager){
	#ifndef __WIN32__
		if(deleteFromRemoteHandleManager) throw std::runtime_error("writeHandleObj called with deleteFromRemoteHandleManager=true, but not allowed on Linux.");
	#endif

	writeInt32(deleteFromRemoteHandleManager);
	writeHandle((uint64_t)obj, TYPE_NPObject, shouldExist);
}

void writeHandleInstance(NPP instance, HandleExists shouldExist){
	writeHandle((uint64_t)instance, TYPE_NPPInstance, shouldExist);
}

void writeHandleIdentifier(NPIdentifier name, HandleExists shouldExist){
	writeHandle((uint64_t)name, TYPE_NPIdentifier, shouldExist);
}

void writeHandleStream(NPStream* stream, HandleExists shouldExist){
	writeHandle((uint64_t)stream, TYPE_NPStream, shouldExist);
}

void writeHandleNotify(void* notifyData, HandleExists shouldExist){
	writeHandle((uint64_t)notifyData, TYPE_NotifyData, shouldExist);
}


uint64_t readHandle(Stack &stack, int32_t &type, NPP instance, NPClass *aclass, HandleExists shouldExist){
	type = readInt32(stack);
	return handlemanager.translateFrom(readInt64(stack), (HandleType)type, instance, aclass, shouldExist);
}

#ifndef __WIN32__

NPObject * readHandleObj(Stack &stack, NPP instance, NPClass *aclass, HandleExists shouldExist){
	int32_t type;
	NPObject *obj = (NPObject *)readHandle(stack, type, instance, aclass, shouldExist);
	
	if (type != TYPE_NPObject)
		throw std::runtime_error("Wrong handle type, expected object");

	bool deleteFromRemoteHandleManager     = (bool)readInt32(stack);
	if(deleteFromRemoteHandleManager){
		handlemanager.removeHandleByReal((uint64_t)obj, TYPE_NPObject);
	}

	return obj;
}

#endif

NPIdentifier readHandleIdentifier(Stack &stack, HandleExists shouldExist){
	int32_t type;
	NPIdentifier identifier = (NPIdentifier)readHandle(stack, type, 0, NULL, shouldExist);
	
	if (type != TYPE_NPIdentifier)
		throw std::runtime_error("Wrong handle type, expected identifier");

	return identifier;
}

NPP readHandleInstance(Stack &stack, HandleExists shouldExist){
	int32_t type;
	NPP instance = (NPP)readHandle(stack, type, 0, NULL, shouldExist);
	
	if (type != TYPE_NPPInstance)
		throw std::runtime_error("Wrong handle type, expected instance");

	return instance;
}

NPStream* readHandleStream(Stack &stack, HandleExists shouldExist){
	int32_t type;
	NPStream* stream = (NPStream*)readHandle(stack, type, 0, NULL, shouldExist);
	
	if (type != TYPE_NPStream)
		throw std::runtime_error("Wrong handle type, expected stream");

	return stream;
}

void* readHandleNotify(Stack &stack, HandleExists shouldExist){
	int32_t type;
	void* notifyData = (void*)readHandle(stack, type, 0, NULL, shouldExist);
	
	if (type != TYPE_NotifyData)
		throw std::runtime_error("Wrong handle type, expected notify-data");

	return notifyData;
}


#ifdef __WIN32__

NPObject * readHandleObjIncRef(Stack &stack, NPP instance, NPClass *aclass, HandleExists shouldExist){
	int32_t type;
	NPObject *obj = (NPObject *)readHandle(stack, type, instance, aclass, shouldExist);
	
	if (type != TYPE_NPObject)
		throw std::runtime_error("Wrong handle type, expected object");

	readInt32(stack); // deleteFromRemoteHandleManager not possible on Windows

	if(obj->referenceCount != REFCOUNT_UNDEFINED)
		obj->referenceCount++;

	return obj;
}

void writeHandleObjDecRef(NPObject *obj, HandleExists shouldExist){
	writeHandleObj(obj, shouldExist, (obj->referenceCount == 1));
	objectDecRef(obj, false);
}


void objectDecRef(NPObject *obj, bool deleteFromRemoteHandleManager){

	// Is the refcount already zero?
	if(obj->referenceCount == 0){
		throw std::runtime_error("Reference count is zero when calling objectDecRef!");
	}

	// Decrement refcounter
	if(obj->referenceCount != REFCOUNT_UNDEFINED)
		obj->referenceCount--;

	// Remove the object locally
	if(obj->referenceCount == 0){
		DBG_TRACE("removing object %p from handle manager.", obj);

		if( obj->_class->deallocate){
			throw std::runtime_error("Proxy object has a deallocate method set?");
		}

		if(deleteFromRemoteHandleManager){
			writeHandleObj(obj, HANDLE_SHOULD_EXIST);
			callFunction(LIN_HANDLE_MANAGER_FREE_OBJECT);
			waitReturn();
		}

		free((char*)obj);

		// Remove it in the handle manager
		handlemanager.removeHandleByReal((uint64_t)obj, TYPE_NPObject);

	}
}

void objectKill(NPObject *obj){

	if(obj->referenceCount != REFCOUNT_UNDEFINED){
		throw std::runtime_error("objectKill for wrong object type");
	}

	// Set the refcount to zero to ensure that Silverlight doesn't call releaseObject!
	obj->referenceCount = 0;

	// Remove it in the handle manager
	handlemanager.removeHandleByReal((uint64_t)obj, TYPE_NPObject);

	// Remove the object locally
	if(obj->_class->deallocate){
		obj->_class->deallocate(obj);
	}else{
		free((char*)obj);
	}

}


void writeVariantReleaseDecRef(NPVariant &variant){
	writeVariantConst(variant, (variant.type == NPVariantType_Object && variant.value.objectValue->referenceCount == 1));

	if( variant.type == NPVariantType_String){
		if (variant.value.stringValue.UTF8Characters)
			free((char*)variant.value.stringValue.UTF8Characters);

	}else if(variant.type == NPVariantType_Object){
		NPObject* obj = variant.value.objectValue;
		objectDecRef(obj, false);

	}
	
	variant.type 				= NPVariantType_Void;
	variant.value.objectValue 	= NULL;
}

void writeVariantArrayReleaseDecRef(NPVariant *variant, int count){
	for(int i = count - 1; i >= 0; i--){
		writeVariantReleaseDecRef(variant[i]);
	}
}

#else

void writeVariantRelease(NPVariant &variant){
	writeVariantConst(variant);

	// We want to keep the contained object until it is freed on the other side
	if(variant.type == NPVariantType_Object){
		sBrowserFuncs->retainobject(variant.value.objectValue);
	}

	// But the variant itself is not required anymore.
	sBrowserFuncs->releasevariantvalue(&variant);
}

void writeVariantArrayRelease(NPVariant *variant, int count){
	for(int i = count - 1; i >= 0; i--){
		writeVariantRelease(variant[i]);
	}
}

#endif

void writeVariantConst(const NPVariant &variant, bool deleteFromRemoteHandleManager){
	#ifndef __WIN32__
		if(deleteFromRemoteHandleManager) throw std::runtime_error("writeHandleObj called with deleteFromRemoteHandleManager=true, but not allowed on Linux.");
	#endif

	switch(variant.type){
		
		case NPVariantType_Null:
			break;

		case NPVariantType_Void:
			break;

		case NPVariantType_Bool:
			writeInt32(variant.value.boolValue);
			break;

		case NPVariantType_Int32:
			writeInt32(variant.value.intValue);
			break;	

		case NPVariantType_Double:
			writeDouble(variant.value.doubleValue);
			break;		

		case NPVariantType_String:
			writeString((char*)variant.value.stringValue.UTF8Characters, variant.value.stringValue.UTF8Length);
			break;

		case NPVariantType_Object:
			writeHandleObj(variant.value.objectValue, HANDLE_CAN_EXIST, deleteFromRemoteHandleManager);
			break;

		default:
			throw std::runtime_error("Unsupported variant type");

	}

	writeInt32(variant.type);
}

void writeVariantArrayConst(const NPVariant *variant, int count){
	for(int i = count - 1; i >= 0; i--){
		writeVariantConst(variant[i]);
	}
}

#ifdef __WIN32__

void readVariantIncRef(Stack &stack, NPVariant &variant){
	int32_t type = readInt32(stack);
	variant.type = (NPVariantType)type;

	size_t stringLength;

	switch(variant.type){
		
		case NPVariantType_Null:
			variant.value.objectValue = NULL;
			break;

		case NPVariantType_Void:
			variant.value.objectValue = NULL;
			break;

		case NPVariantType_Bool:
			variant.value.boolValue 	= (bool)readInt32(stack);
			break;

		case NPVariantType_Int32:
			variant.value.intValue  	= readInt32(stack);
			break;	

		case NPVariantType_Double:
			variant.value.doubleValue  	= readDouble(stack);
			break;		

		case NPVariantType_String:
			variant.value.stringValue.UTF8Characters 	= readStringMalloc(stack, stringLength);
			variant.value.stringValue.UTF8Length 		= stringLength;
			break;

		case NPVariantType_Object:
			variant.value.objectValue 	= readHandleObjIncRef(stack);
			break;

		default:
			throw std::runtime_error("Unsupported variant type");

	}
}

std::vector<NPVariant> readVariantArrayIncRef(Stack &stack, int count){

	NPVariant variant;
	std::vector<NPVariant> result;

	for(int i = 0; i < count; i++){
		readVariantIncRef(stack, variant);
		result.push_back(variant);
	}

	return result;
}

void freeVariantDecRef(NPVariant &variant){
	if( variant.type == NPVariantType_String){
		if (variant.value.stringValue.UTF8Characters)
			free((char*)variant.value.stringValue.UTF8Characters);

	}else if(variant.type == NPVariantType_Object){
		NPObject* obj = variant.value.objectValue;
		objectDecRef(obj);

	}
	
	variant.type 				= NPVariantType_Void;
	variant.value.objectValue 	= NULL;
}

void freeVariantArrayDecRef(std::vector<NPVariant> args){
	for(NPVariant &variant :  args){
		freeVariantDecRef(variant);
	}
}

#else

void readVariant(Stack &stack, NPVariant &variant){
	int32_t type = readInt32(stack);
	variant.type = (NPVariantType)type;

	size_t stringLength;

	switch(variant.type){
		
		case NPVariantType_Null:
			variant.value.objectValue = NULL;
			break;

		case NPVariantType_Void:
			variant.value.objectValue = NULL;
			break;

		case NPVariantType_Bool:
			variant.value.boolValue 	= (bool)readInt32(stack);
			break;

		case NPVariantType_Int32:
			variant.value.intValue  	= readInt32(stack);
			break;	

		case NPVariantType_Double:
			variant.value.doubleValue  	= readDouble(stack);
			break;		

		case NPVariantType_String:
			variant.value.stringValue.UTF8Characters 	= readStringBrowserAlloc(stack, stringLength);
			variant.value.stringValue.UTF8Length 		= stringLength;
			break;

		case NPVariantType_Object:
			variant.value.objectValue 	= readHandleObj(stack);
			break;

		default:
			throw std::runtime_error("Unsupported variant type");

	}
}

std::vector<NPVariant> readVariantArray(Stack &stack, int count){
	NPVariant variant;
	std::vector<NPVariant> result;

	for(int i = 0; i < count; i++){
		readVariant(stack, variant);
		result.push_back(variant);
	}

	return result;
}


void freeVariant(NPVariant &variant){
	if (variant.type == NPVariantType_String){
		if( variant.value.stringValue.UTF8Characters )
			sBrowserFuncs->memfree((char*)variant.value.stringValue.UTF8Characters);

	} // Objects dont have to be freed

	variant.type 				= NPVariantType_Void;
	variant.value.objectValue 	= NULL;
}

void freeVariantArray(std::vector<NPVariant> args){
	for(NPVariant &variant :  args){
		freeVariant(variant);
	}
}

#endif



void writeNPString(NPString *string){
	if(!string)
		throw std::runtime_error("Invalid String pointer!");

	writeString((char*)string->UTF8Characters, string->UTF8Length);
}

void readNPString(Stack &stack, NPString &string){
	size_t stringLength;
	#ifdef __WIN32__
		string.UTF8Characters = readStringMalloc(stack, stringLength);
	#else
		string.UTF8Characters = readStringBrowserAlloc(stack, stringLength);
	#endif
	string.UTF8Length = stringLength;
}

void freeNPString(NPString &string){
	if(string.UTF8Characters){	
		#ifdef __WIN32__
			free((char*)string.UTF8Characters);
		#else
			sBrowserFuncs->memfree((char*)string.UTF8Characters);
		#endif
	}

	string.UTF8Characters 	= NULL;
	string.UTF8Length		= 0;
}

void writeStringArray(char* str[], int count){

	for(int i = count - 1; i >= 0; i--){
		writeString(str[i]);
	}

}

std::vector<char*> readStringArray(Stack &stack, int count){
	std::vector<char*> result;

	for(int i = 0; i < count; i++){
		result.push_back( readStringMalloc(stack) );
	}

	return result;
}

void freeStringArray(std::vector<char*> str){
	for(char* &ptr: str){
		free(ptr);
	}

	//while(!str.empty()){
	//	free( str.back() ); // Free pointer
	//	str.pop_back();
	//}
}


void writeIdentifierArray(NPIdentifier* identifiers, int count){
	for(int i = count - 1; i >= 0; i--){
		writeHandleIdentifier(identifiers[i]);
	}
}


std::vector<NPIdentifier> readIdentifierArray(Stack &stack, int count){
	std::vector<NPIdentifier> result;

	for(int i = 0; i < count; i++){
		result.push_back( readHandleIdentifier(stack) );
	}

	return result;
}




void writeNPBool(NPBool value){
	writeInt32(value);
}

NPBool readNPBool(Stack &stack){
	return (NPBool)readInt32(stack);
}