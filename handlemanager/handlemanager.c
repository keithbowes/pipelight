#include <cstring>
#include <cstdlib>
#include "handlemanager.h"
#include "../communication/communication.h"

extern NPClass myClass;
extern HandleManager handlemanager;

#ifndef __WIN32__
extern NPNetscapeFuncs *sBrowserFuncs;
#endif

#include <fstream>
extern std::ofstream output;

uint64_t HandleManager::getFreeID(){
	if(handlesID.size() > 0){
		//The last elment has the biggest ID
		return handlesID.rbegin()->first + 1;
	}else{
		return 1;
	}

}

NPObject* createNPObject(uint64_t id, NPClass *aclass = NULL, NPP instance = 0){

	NPObject* obj = NULL;

	if(!aclass) aclass = &myClass;

	if(aclass != &myClass) output << "Created object without fake class" << std::endl;

	if(aclass->allocate){
		obj = aclass->allocate(instance, aclass);
	}else{
		obj = (NPObject*)malloc(sizeof(NPObject));
	}

	if(!obj) throw std::runtime_error("Could not create object!");			


	obj->_class 		= aclass; //(NPClass*)0xdeadbeef;
	obj->referenceCount	= 0; //0xDEADBEEF; // TODO: Is this useful?

	//if(aclass == &myClass) obj->_class = (NPClass*)0xDEADBEEF;

	if(aclass != &myClass) output << "Version number: " << aclass->structVersion << " (my version:" <<  NP_CLASS_STRUCT_VERSION << ")" << std::endl;

	output << "Handle manager added object " << (void*)obj << std::endl;

	return obj;
}


NPP_t* createNPPInstance(uint64_t id){
	NPP_t* instance = (NPP_t*)malloc(sizeof(NPP_t));

	if(instance)
		memset(instance, 0, sizeof(NPP_t));

	return instance;
}


#ifdef __WIN32__
NPStream * createNPStream(uint64_t id){
	NPStream *stream = (NPStream*)malloc(sizeof(NPStream));

	if(!stream) throw std::runtime_error("Could not create stream!");

	// In order to simulate a browser we have to query the original fields of this stream

	// We cannot use writeHandle, as the handle manager hasn't finished adding this yet.
	writeInt64(id);
	writeInt32(TYPE_NPStream);
	callFunction(HANDLE_MANAGER_REQUEST_STREAM_INFO);

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
#endif


// Used for incoming handle translation(id -> real)
// aclass and instance  are used for some cases when a new object is generated
uint64_t HandleManager::translateFrom(uint64_t id, HandleType type, NPP instance, NPClass *aclass, bool shouldExist){
	std::map<uint64_t, Handle>::iterator it;

	it = handlesID.find(id);
	if(it != handlesID.end()){

		// Ensure that aClass or instance is given
		if(instance || aclass) throw std::runtime_error("Expected a new handle, but I already got this one");

		return it->second.real;
	}

	if(shouldExist) throw std::runtime_error("Got ID which sould exist, but it doesnt!");

	//Create handle
	Handle handle;
	handle.id 			= id;
	handle.type 		= type;
	handle.selfCreated 	= true;

	switch(type){

		case TYPE_NPObject:

			#ifndef __WIN32__
				throw std::runtime_error("Got unknown object! This should never happen!");
			#endif

			handle.real = (uint64_t) createNPObject(id, aclass, instance);
			break;

		case TYPE_NPIdentifier:
			// These are just some identifiers for strings we can simply use our internal id for them
			handle.real = id;
			break;

		case TYPE_NPPInstance:
			handle.real = (uint64_t) createNPPInstance(id);
			break;

		case TYPE_NPStream:
			#ifdef __WIN32__
				handle.real = (uint64_t) createNPStream(id);
			#else
				throw std::runtime_error("Error in handle manager: Cannot create remote NPStream");
			#endif

			break;

		case TYPE_NotifyData:
			#ifdef __WIN32__
				handle.real = 0; 	// If something is new for the plugin side, there is no notifyData
			#else
				handle.real = id; 	// But on the other side we have to set notifyData!
			#endif
				
			break;

		default:
			throw std::runtime_error("Unknown handle type");
			break;
	}	

	handlesID[id] 			= handle;
	handlesReal[std::pair<HandleType, uint64_t>(type, handle.real)] 	= handle;
	return handle.real;
}

uint64_t HandleManager::translateTo(uint64_t real, HandleType type, bool shouldExist){
	std::map<std::pair<HandleType, uint64_t>, Handle>::iterator it;

	// Except for TYPE_NotifyData we dont allow nullpointers here for obvious reasons
	if(!real && type != TYPE_NotifyData){
		throw std::runtime_error("trying to translate a null-handle");
	}

	it = handlesReal.find(std::pair<HandleType, uint64_t>(type, real));
	if(it != handlesReal.end()){
		return it->second.id;
	}

	if(shouldExist) throw std::runtime_error("Got real handle which sould exist, but it doesnt!");

	Handle handle;
	handle.id 			= getFreeID();
	handle.real 		= real;
	handle.type 		= type;
	handle.selfCreated 	= false;

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


	output << "Removed from handle manager: ID=" << id << std::endl;
}

void HandleManager::removeHandleByReal(uint64_t real, HandleType type){
	std::map<std::pair<HandleType, uint64_t>, Handle>::iterator it;

	it = handlesReal.find(std::pair<HandleType, uint64_t>(type, real));
	if(it == handlesReal.end()) throw std::runtime_error("Trying to remove handle by nonexistend real object");

	handlesID.erase(it->second.id);
	handlesReal.erase(it);

	output << "Removed from handle manager: REAL=" << (void*)real << std::endl;
}


void writeHandle(uint64_t real, HandleType type, bool shouldExist){
	writeInt64(handlemanager.translateTo(real, type, shouldExist));
	writeInt32(type);
}

void writeHandle(NPP instance, bool shouldExist){
	writeHandle((uint64_t)instance, TYPE_NPPInstance, shouldExist);
}

void writeHandle(NPObject *obj, bool shouldExist){
	writeHandle((uint64_t)obj, TYPE_NPObject, shouldExist);
}

void writeHandle(NPIdentifier name, bool shouldExist){
	writeHandle((uint64_t)name, TYPE_NPIdentifier, shouldExist);
}

void writeHandle(NPStream* stream, bool shouldExist){
	writeHandle((uint64_t)stream, TYPE_NPStream, shouldExist);
}

void writeHandleNotify(void* notifyData, bool shouldExist){
	writeHandle((uint64_t)notifyData, TYPE_NotifyData, shouldExist);
}

uint64_t readHandle(Stack &stack, int32_t &type, NPP instance, NPClass *aclass, bool shouldExist){
	type = readInt32(stack);
	return handlemanager.translateFrom(readInt64(stack), (HandleType)type, instance, aclass, shouldExist);
}

NPObject * readHandleObj(Stack &stack, NPP instance, NPClass *aclass, bool shouldExist){
	int32_t type;
	NPObject *obj = (NPObject *)readHandle(stack, type, instance, aclass, shouldExist);
	
	if (type != TYPE_NPObject)
		throw std::runtime_error("Wrong handle type, expected object");

	// Check if this is required everywhere
	//#ifdef __WIN32__
	//	obj->referenceCount++;
	//#endif

	return obj;
}

NPIdentifier readHandleIdentifier(Stack &stack, bool shouldExist){
	int32_t type;
	NPIdentifier identifier = (NPIdentifier)readHandle(stack, type, 0, NULL, shouldExist);
	
	if (type != TYPE_NPIdentifier)
		throw std::runtime_error("Wrong handle type, expected identifier");

	return identifier;
}

NPP readHandleInstance(Stack &stack, bool shouldExist){
	int32_t type;
	NPP instance = (NPP)readHandle(stack, type, 0, NULL, shouldExist);
	
	if (type != TYPE_NPPInstance)
		throw std::runtime_error("Wrong handle type, expected instance");

	return instance;
}

NPStream* readHandleStream(Stack &stack, bool shouldExist){
	int32_t type;
	NPStream* stream = (NPStream*)readHandle(stack, type, 0, NULL, shouldExist);
	
	if (type != TYPE_NPStream)
		throw std::runtime_error("Wrong handle type, expected stream");

	return stream;
}

void* readHandleNotify(Stack &stack, bool shouldExist){
	int32_t type;
	void* notifyData = (void*)readHandle(stack, type, 0, NULL, shouldExist);
	
	if (type != TYPE_NotifyData)
		throw std::runtime_error("Wrong handle type, expected notify-data");

	return notifyData;
}


void writeVariantRelease(NPVariant &variant){
	writeVariantConst(variant);

	#ifdef __WIN32__

		// The variant has already incremented the refcounter by one... in case of an object we dont have to do anything

		if( variant.type == NPVariantType_String){
			if (variant.value.stringValue.UTF8Characters)
				free((char*)variant.value.stringValue.UTF8Characters);
		}

		variant.type = NPVariantType_Null;

		/*
		if(variant.type == NPVariantType_Object){
			NPN_RetainObject(variant.value.objectValue);			
		}

		NPN_ReleaseVariantValue(&variant);*/

	#else
		if(variant.type == NPVariantType_Object){
			sBrowserFuncs->retainobject(variant.value.objectValue);
		}

		sBrowserFuncs->releasevariantvalue(&variant);
	#endif
}

void writeVariantArrayRelease(NPVariant *variant, int count){
	for(int i = count - 1; i >= 0; i--){
		writeVariantRelease(variant[i]);
	}
}

void writeVariantConst(const NPVariant &variant){
	switch(variant.type){
		
		case NPVariantType_Null:
			output << "WriteVariant: Null" << std::endl;
			break;

		case NPVariantType_Void:
			output << "WriteVariant: Void" << std::endl;
			break;

		case NPVariantType_Bool:
			writeInt32(variant.value.boolValue );
			output << "WriteVariant: Bool(" << variant.value.boolValue << ")" << std::endl;
			break;

		case NPVariantType_Int32:
			writeInt32(variant.value.intValue);
			output << "WriteVariant: Int32(" << variant.value.intValue << ")" << std::endl;
			break;	

		case NPVariantType_Double:
			writeDouble(variant.value.doubleValue);
			output << "WriteVariant: Double(" << variant.value.doubleValue << ")" << std::endl;
			break;		

		case NPVariantType_String:
			writeString((char*)variant.value.stringValue.UTF8Characters, variant.value.stringValue.UTF8Length);
			output << "WriteVariant: String('" << variant.value.stringValue.UTF8Characters << "')" << std::endl;
			break;

		case NPVariantType_Object:
			writeHandle(variant.value.objectValue);
			output << "WriteVariant: Object(" <<  (void*)variant.value.objectValue << ")" << std::endl;
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

void readVariant(Stack &stack, NPVariant &variant){
	int32_t type = readInt32(stack);
	variant.type = (NPVariantType)type;

	size_t stringLength;

	switch(variant.type){
		
		case NPVariantType_Null:
			output << "ReadVariant: Null" << std::endl;
			break;

		case NPVariantType_Void:
			output << "ReadVariant: Void" << std::endl;
			break;

		case NPVariantType_Bool:
			variant.value.boolValue 	= (bool)readInt32(stack);
			output << "ReadVariant: Bool(" << variant.value.boolValue << ")" << std::endl;
			break;

		case NPVariantType_Int32:
			variant.value.intValue  	= readInt32(stack);
			output << "ReadVariant: Int32(" << variant.value.intValue << ")" << std::endl;
			break;	

		case NPVariantType_Double:
			variant.value.doubleValue  	= readDouble(stack);
			output << "ReadVariant: Double(" << variant.value.doubleValue << ")" << std::endl;
			break;		

		case NPVariantType_String:
			#ifdef __WIN32__
				variant.value.stringValue.UTF8Characters = readStringMalloc(stack, stringLength);
			#else
				variant.value.stringValue.UTF8Characters = readStringBrowserAlloc(stack, stringLength);
			#endif
			variant.value.stringValue.UTF8Length = stringLength;
			output << "ReadVariant: String('" << variant.value.stringValue.UTF8Characters << "')" << std::endl;
			break;


		case NPVariantType_Object:
			variant.value.objectValue 	= readHandleObj(stack);
			output << "ReadVariant: Object(" <<  (void*)variant.value.objectValue << ")" << std::endl;
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

#ifdef __WIN32__
NPObject * readHandleObjIncRef(Stack &stack, NPP instance, NPClass *aclass, bool shouldExist){
	int32_t type;
	NPObject *obj = (NPObject *)readHandle(stack, type, instance, aclass, shouldExist);
	
	if (type != TYPE_NPObject)
		throw std::runtime_error("Wrong handle type, expected object");

	// Check if this is required everywhere
	obj->referenceCount++;

	return obj;
}

void readVariantIncRef(Stack &stack, NPVariant &variant){
	int32_t type = readInt32(stack);
	variant.type = (NPVariantType)type;

	size_t stringLength;

	switch(variant.type){
		
		case NPVariantType_Null:
			output << "ReadVariant: Null" << std::endl;
			break;

		case NPVariantType_Void:
			output << "ReadVariant: Void" << std::endl;
			break;

		case NPVariantType_Bool:
			variant.value.boolValue 	= (bool)readInt32(stack);
			output << "ReadVariant: Bool(" << variant.value.boolValue << ")" << std::endl;
			break;

		case NPVariantType_Int32:
			variant.value.intValue  	= readInt32(stack);
			output << "ReadVariant: Int32(" << variant.value.intValue << ")" << std::endl;
			break;	

		case NPVariantType_Double:
			variant.value.doubleValue  	= readDouble(stack);
			output << "ReadVariant: Double(" << variant.value.doubleValue << ")" << std::endl;
			break;		

		case NPVariantType_String:
			#ifdef __WIN32__
				variant.value.stringValue.UTF8Characters = readStringMalloc(stack, stringLength);
			#else
				variant.value.stringValue.UTF8Characters = readStringBrowserAlloc(stack, stringLength);
			#endif
			variant.value.stringValue.UTF8Length = stringLength;
			output << "ReadVariant: String('" << variant.value.stringValue.UTF8Characters << "')" << std::endl;
			break;


		case NPVariantType_Object:
			variant.value.objectValue 	= readHandleObjIncRef(stack);
			output << "ReadVariant: Object(" <<  (void*)variant.value.objectValue << ")" << std::endl;
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

#endif



void freeVariant(NPVariant &variant){
	if (variant.type == NPVariantType_String){

		#ifdef __WIN32__
			free((char*)variant.value.stringValue.UTF8Characters);
		#else
			sBrowserFuncs->memfree((char*)variant.value.stringValue.UTF8Characters); // o.O ?
		#endif

	} else if (variant.type == NPVariantType_Object){
		// Placeholder?
		// On the windows side only strings should be freed!
		// Same is true on linux!

	}

	variant.type = NPVariantType_Null;
}

void freeVariantArray(std::vector<NPVariant> args){
	for(NPVariant &variant :  args){
		freeVariant(variant);
	}
}

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

		if( str[i] ){
			output << "writeString: " << str[i] << std::endl << std::flush;
		}else{
			output << "writeString: NULLPTR" <<  std::endl << std::flush;
		}

	}

}

std::vector<char*> readStringArray(Stack &stack, int count){
	std::vector<char*> result;

	for(int i = 0; i < count; i++){
		result.push_back( readStringMalloc(stack) );

		if( result.back() ){
			output << "got string: " << result.back() <<  std::endl<< std::flush;
		}else{
			output << "got string: NULLPTR" <<  std::endl << std::flush;
		}
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


void writeNPBool(NPBool value){
	writeInt32(value);
}

NPBool readNPBool(Stack &stack){
	return (NPBool)readInt32(stack);
}