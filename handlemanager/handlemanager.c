#include <cstring>
#include <cstdlib>
#include "handlemanager.h"

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

#ifdef __WIN32__

NPError NPN_GetURLNotify(NPP instance, const  char* url, const char* target, void* notifyData);
NPError NPN_PostURLNotify(NPP instance, const char* url, const char* target, uint32_t len, const char* buf, NPBool file, void* notifyData);


NPStream * getRemoteStream(uint64_t id, HandleType type){

	NPStream *stream = new NPStream;

	output << "getRemoteStream" << std::endl << std::flush;

	writeInt64(id);
	writeInt32(type);

	callFunction(HANDLE_MANAGER_REQUEST_STREAM_INFO);

	std::vector<ParameterInfo> 	stack;
	readCommands(stack);	
	
	std::string url = readString(stack);
	output << "Receiving Stream Info for " << url << std::endl;

	stream->url = (char*)malloc(url.length()+1);
	if(!stream->url)
		throw std::bad_alloc();
	
	// Copy trailing zero
	memcpy((char*)stream->url, url.c_str(), url.length()+1);

	stream->end 			= readInt32(stack);
	stream->lastmodified 	= readInt32(stack);
	stream->notifyData		= readHandleNotify(stack);

	size_t length;
	std::shared_ptr<char> headers = readBinaryData(stack, length);

	if(length && headers){

		stream->headers = (char*)malloc(length);
		if(!stream->headers)
			throw std::bad_alloc();
		
		// Copy trailing zero
		memcpy((char*)stream->headers, headers.get(), length);

	}else{
		stream->headers = NULL;

	}

	return stream;
}
#endif

uint64_t HandleManager::translateFrom(uint64_t id, HandleType type, bool forceRefInc, NPClass *aclass, NPP instance){

	std::map<uint64_t, Handle>::iterator it;

	it = handlesID.find(id);
	if(it != handlesID.end()){

		//output << "converted ID " << id << " to real object " << it->second.real << std::endl;

		if(it->second.type == TYPE_NPObject && forceRefInc){
			NPObject *obj = (NPObject *)it->second.real;
			obj->referenceCount++;

			//output << "manually incremeneted refcounter" << std::endl;

		}

		return it->second.real;
	}

	//Create Handle
	Handle handle;
	handle.id 			= id;
	handle.type 		= type;
	handle.selfCreated 	= true;

	NPObject *obj;
	
	output << "created remote object with id " << id << std::endl << std::flush;

	switch(type){

		case TYPE_NPObject:

			if(!aclass){
				aclass = &myClass;
			}else{
				output << "Do not use fake class!" << std::endl;
			} 

			if(aclass->allocate){
				obj = aclass->allocate(instance, aclass);
				output << "Created new object with non-fake class" << std::endl;
			}else{
				obj 				= new NPObject;
			}

			if(!obj)
				throw std::runtime_error("Could not create object!");			

			obj->_class 		= aclass; //(NPClass*)0xdeadbeef;
			obj->referenceCount	= 1;

			handle.real = (uint64_t)obj;

			break;

		case TYPE_NPIdentifier:
			// These are just some identifiers for strings
			// we can simply use our internal id for them
			handle.real = id;
			break;

		case TYPE_NPPInstance:
			handle.real = (uint64_t) new NPP_t;
			break;

		case TYPE_NPStream:
			#ifdef __WIN32__
				output << "Receiving Stream" << std::endl;
				handle.real = (uint64_t) getRemoteStream(id, type);
			#else
				throw std::runtime_error("Error in NPStream Handle Manager");
			#endif

			break;

		case TYPE_NotifyData:
			// These ist just for identifieng the right stream
			#ifdef __WIN32__
				handle.real = 0;
			#else
				handle.real = id;
			#endif
				
			break;

		default:
			throw std::runtime_error("Unknown handle type");
			break;
	}	

	handlesID[id] 				= handle;
	handlesReal[handle.real] 	= handle;

	//output << "allocated new remote object with ID " << id << " and real object " << handle.real << std::endl;

	return handle.real;
}

uint64_t HandleManager::translateTo(uint64_t real, HandleType type){

	std::map<uint64_t, Handle>::iterator it;

	if(!real && type != TYPE_NotifyData){
		throw std::runtime_error("trying to translate a null-handle");
	}

	it = handlesReal.find(real);
	if(it != handlesReal.end()){

		//output << "converted real object " << real << " to ID " << it->second.id << std::endl;

		return it->second.id;
	}

	Handle handle;
	handle.id 			= getFreeID();
	handle.real 		= real;
	handle.type 		= type;
	handle.selfCreated 	= false;

	handlesID[handle.id] 	= handle;
	handlesReal[real] 		= handle;

	//output << "allocated new local object " << handle.real << " with id " << handle.id << std::endl;

	return handle.id;
}

void HandleManager::removeHandleByID(uint64_t id){

	std::map<uint64_t, Handle>::iterator it;

	it = handlesID.find(id);
	if(it != handlesID.end()){

		output << "removed handle by id " << id << std::endl;

		handlesReal.erase(it->second.real);
		handlesID.erase(it);
	}

}

void HandleManager::removeHandleByReal(uint64_t real){

	std::map<uint64_t, Handle>::iterator it;

	it = handlesReal.find(real);
	if(it != handlesReal.end()){

		output << "removed handle by id " << real << std::endl;

		handlesID.erase(it->second.id);
		handlesReal.erase(it);
	}

}

/*
uint64_t HandleManager::manuallyAddHandle(uint64_t real, HandleType type){
	std::map<uint64_t, Handle>::iterator it;

	if(!real) throw std::runtime_error("trying to translate a null-handle");

	it = handlesReal.find(real);
	if(it != handlesReal.end()){

		//output << "converted real object " << real << " to ID " << it->second.id << std::endl;

		return it->second.id;
	}

	Handle handle;
	handle.id 			= getFreeID();
	handle.real 		= real;
	handle.type 		= type;
	handle.selfCreated 	= false;

	handlesID[handle.id] 	= handle;
	handlesReal[real] 		= handle;

	//output << "allocated new local object " << handle.real << " with id " << handle.id << std::endl;

	return handle.id;	
}
*/

void writeHandle(uint64_t real, HandleType type){

	writeInt64(handlemanager.translateTo(real, type));
	writeInt32(type);

}

void writeHandle(NPP instance){
	writeHandle((uint64_t)instance, TYPE_NPPInstance);
}

void writeHandle(NPObject *obj){
	writeHandle((uint64_t)obj, TYPE_NPObject);
}

void writeHandle(NPIdentifier name){
	writeHandle((uint64_t)name, TYPE_NPIdentifier);
}

void writeHandle(NPStream* stream){
	writeHandle((uint64_t)stream, TYPE_NPStream);
}

void writeHandleNotify(void* notifyData){
	writeHandle((uint64_t)notifyData, TYPE_NotifyData);
}

uint64_t readHandle(Stack &stack, int32_t &type, bool forceRefInc, NPClass *aclass, NPP instance){
	type = readInt32(stack);
	return handlemanager.translateFrom(readInt64(stack), (HandleType)type, forceRefInc, aclass, instance);
}

NPObject * readHandleObj(Stack &stack, bool forceRefInc, NPClass *aclass, NPP instance){
	
	int32_t type;
	NPObject *obj = (NPObject *)readHandle(stack, type, forceRefInc, aclass, instance);
	
	if (type != TYPE_NPObject)
		throw std::runtime_error("Wrong handle type!");

	return obj;
}

NPIdentifier readHandleIdentifier(Stack &stack){

	int32_t type;
	NPIdentifier identifier = (NPIdentifier)readHandle(stack, type);
	
	if (type != TYPE_NPIdentifier)
		throw std::runtime_error("Wrong handle type!");

	return identifier;
}

NPP readHandleInstance(Stack &stack){

	int32_t type;
	NPP instance = (NPP)readHandle(stack, type);
	
	if (type != TYPE_NPPInstance)
		throw std::runtime_error("Wrong handle type!");

	return instance;
}

NPStream* readHandleStream(Stack &stack){

	int32_t type;
	NPStream* stream = (NPStream*)readHandle(stack, type);
	
	if (type != TYPE_NPStream)
		throw std::runtime_error("Wrong handle type!");

	return stream;
}

void* readHandleNotify(Stack &stack){

	int32_t type;
	void* notifyData = (void*)readHandle(stack, type);
	
	if (type != TYPE_NotifyData)
		throw std::runtime_error("Wrong handle type!");

	return notifyData;
}


#ifndef __WIN32__
void writeVariant(const NPVariant &variant, bool releaseVariant){
#else
void writeVariant(const NPVariant &variant){
#endif

	switch(variant.type){
		
		case NPVariantType_Null:
			break;

		case NPVariantType_Void:
			break;

		case NPVariantType_Bool:
			writeInt32( (bool)variant.value.boolValue );
			break;

		case NPVariantType_Int32:
			writeInt32(variant.value.intValue);
			break;	

		case NPVariantType_Double:
			writeDouble(variant.value.doubleValue);
			break;		

		case NPVariantType_String:
			writeMemory((char*)variant.value.stringValue.UTF8Characters, variant.value.stringValue.UTF8Length);
			break;

		case NPVariantType_Object:
			#ifndef __WIN32__
				if (releaseVariant)
					sBrowserFuncs->retainobject(variant.value.objectValue);
			#endif
			writeHandle(variant.value.objectValue);
			break;

		default:
			throw std::runtime_error("Unsupported variant type");

	}

	writeInt32(variant.type);

	#ifndef __WIN32__
		if (releaseVariant)
			sBrowserFuncs->releasevariantvalue((NPVariant*)&variant);
	#endif
}

#ifndef __WIN32_
void writeVariantArray(const NPVariant *variant, int count, bool releaseVariant){
#else
void writeVariantArray(const NPVariant *variant, int count){
#endif

	for(int i = count - 1; i >= 0; i--){
		#ifndef __WIN32__
			writeVariant(variant[i], releaseVariant);	
		#else
			writeVariant(variant[i]);
		#endif
	}

}

void readVariant(Stack &stack, NPVariant &variant, bool forceRefInc){

	int32_t type = readInt32(stack);

	variant.type = (NPVariantType)type;
	std::shared_ptr<char> data;

	switch(variant.type){
		
		case NPVariantType_Null:
			break;

		case NPVariantType_Void:
			break;

		case NPVariantType_Bool:
			variant.value.boolValue 	= readInt32(stack);
			break;

		case NPVariantType_Int32:
			variant.value.intValue  	= readInt32(stack);
			break;	

		case NPVariantType_Double:
			variant.value.doubleValue  	= readDouble(stack);
			break;		

		case NPVariantType_String:
			size_t length;
			data = readBinaryData(stack, length);

			if (data && length){

				#ifdef __WIN32__
				char *ptr = (char*)malloc(length);
				#else
				char *ptr = (char*)sBrowserFuncs->memalloc(length);
				#endif
				if(!ptr) throw std::bad_alloc();

				memcpy(ptr, data.get(), length);
				variant.value.stringValue.UTF8Characters 	= ptr;
				variant.value.stringValue.UTF8Length		= length;

			}else{
				variant.value.stringValue.UTF8Characters 	= NULL;
				variant.value.stringValue.UTF8Length		= 0;
			}
			break;


		case NPVariantType_Object:
			variant.value.objectValue = readHandleObj(stack, forceRefInc);
			break;

		default:
			throw std::runtime_error("Unsupported variant type");

	}

}

std::vector<NPVariant> readVariantArray(Stack &stack, int count, bool forceRefInc){

	NPVariant variant;
	std::vector<NPVariant> result;

	for(int i = 0; i < count; i++){
		readVariant(stack, variant, forceRefInc);
		result.push_back(variant);
	}

	return result;
}

void writeNPString(NPString *string){
	
	if(!string)
		throw std::runtime_error("Invalid String pointer!");

	writeMemory((char*)string->UTF8Characters, string->UTF8Length);
}

void readNPString(Stack &stack, NPString &string){
	size_t length;
	std::shared_ptr<char> data = readBinaryData(stack, length);

	if( data && length ){
		#ifdef __WIN32__
		char *ptr = (char*)malloc(length);
		#else
		char *ptr = (char*)sBrowserFuncs->memalloc(length);
		#endif
		if(!ptr) throw std::bad_alloc();

		memcpy(ptr, data.get(), length);
		string.UTF8Characters 	= ptr;
		string.UTF8Length		= length;

	}else{
		string.UTF8Characters 	= NULL;
		string.UTF8Length		= 0;
	}

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

void writeCharStringArray(char* str[], int count){

	for(int i = count - 1; i >= 0; i--){
		writeString(str[i]);
	}

}

charArray readCharStringArray(Stack &stack, int count){
	charArray array;

	for(int i = 0; i < count; i++){
		
		std::shared_ptr<char> data = readStringAsBinaryData(stack);

		array.charPointers.push_back(data.get());
		array.sharedPointers.push_back(data);
	}

	return array;
}

void writeNPBool(NPBool value){
	writeInt32(value);
}

NPBool readNPBool(Stack &stack){
	return (NPBool)readInt32(stack);
}
