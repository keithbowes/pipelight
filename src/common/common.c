#include <stdio.h>								// for fread, fwrite
#include <stdlib.h>								// for malloc, ...
#include <cstring>								// for memcpy, ...
#include <map>									// for std::map

#include "common.h"

#ifndef __WIN32__
	#include <sys/time.h>						// for select etc.
	#include <sys/types.h>
	#include <unistd.h>
#endif

FILE *commPipeOut	= NULL;
FILE *commPipeIn	= NULL;

char strMultiPluginName[64] = "unknown";

/* global mappings */
static inline std::map<HMGR_HANDLE, void*>& __idToPtr(int type){
	static std::map<HMGR_HANDLE, void*> idToPtr[HMGR_NUMTYPES];
	DBG_ASSERT(type >= 0 && type < HMGR_NUMTYPES, "invalid handle type.");
	return idToPtr[type];
}

static inline std::map<void*, HMGR_HANDLE>& __ptrToId(int type){
	static std::map<void*, HMGR_HANDLE> ptrToId[HMGR_NUMTYPES];
	DBG_ASSERT(type >= 0 && type < HMGR_NUMTYPES, "invalid handle type.");
	return ptrToId[type];
}

/* freeSharedPtrMemory */
void freeSharedPtrMemory(char *memory){
	if (memory)
		free(memory);
}

ParameterInfo::ParameterInfo(char command, char* newdata, size_t length) : data(newdata, freeSharedPtrMemory){
	this->command = command;
	this->length  = length;
}

ParameterInfo::~ParameterInfo(){
	/* do nothing */
}

/*
	Initializes the communication pipes
*/
bool initCommPipes(int out, int in){
	if (commPipeOut)
		fclose(commPipeOut);

	if (commPipeIn)
		fclose(commPipeIn);

	#if defined(__WINE__) || !defined(__WIN32__)
		commPipeOut = fdopen(out, "wb");
		commPipeIn	= fdopen(in,  "rb");
	#else
		commPipeOut = _fdopen(out, "wb");
		commPipeIn	= _fdopen(in,  "rb");
	#endif

	if (!commPipeOut || !commPipeIn){

		if (!commPipeOut) 	fclose(commPipeOut);
		if (!commPipeIn)	fclose(commPipeIn);

		commPipeOut = NULL;
		commPipeIn 	= NULL;
		return false;
	}

	#ifndef __WIN32__
		/* Disable buffering for input pipe (to allow waiting for a pipe) */
		setbuf(commPipeIn, NULL);
	#endif

	return true;
}

bool initCommIO(){
	#if defined(__WINE__) || !defined(__WIN32__)
		return initCommPipes(dup(1), dup(0));
	#else
		return initCommPipes(_dup(1), _dup(0));
	#endif
}

/*
	Initializes the plugin name
*/
void setMultiPluginName(const std::string str){
	pokeString(strMultiPluginName, str, sizeof(strMultiPluginName));
}

void setMultiPluginName(const char* str){
	pokeString(strMultiPluginName, str, sizeof(strMultiPluginName));
}

/*
	Transmits the buffer and returns
*/
bool transmitData(const char* data, size_t length){
	size_t pos, numBytes;

	if (!commPipeOut)
		return false;

	/* transmit the whole buffer */
	for (pos = 0; pos < length; pos += numBytes){
		numBytes = fwrite( data + pos, sizeof(char), length - pos, commPipeOut);
		if (numBytes == 0)
			return false;
	}

	return true;
}

/*
	Writes a command to the pipe
*/
bool writeCommand(char command, const char* data, size_t length){
	uint32_t blockInfo;

	/* no data given -> length = 0 */
	if (!data)
		length = 0;

	if (length > 0xFFFFFF || !commPipeOut)
		return false;

	/* transmit block info */
	blockInfo = (command << 24) | length;
	if (!transmitData((char*)&blockInfo, sizeof(uint32_t)))
		return false;

	/* transmit argument (if any) */
	if (data != NULL && length > 0){
		if (!transmitData(data, length))
			return false;
	}

	/* flush data! */
	fflush(commPipeOut);
	return true;
}

/*
	(Internal) writes a string
*/
bool __writeString(const char* data, size_t length){
	uint32_t blockInfo;
	char eos;

	if (!commPipeOut)
		return false;

	if (!data)
		return writeCommand(BLOCKCMD_PUSH_STRING);

	if (length > 0xFFFFFF - 1)
		return false;

	/* transmit block info */
	blockInfo = (BLOCKCMD_PUSH_STRING << 24) | (length + 1);
	if (!transmitData((char*)&blockInfo, sizeof(uint32_t)))
		return false;

	/* transmit string */
	if (length > 0){
		if (!transmitData(data, length))
			return false;
	}

	/* transmit eos */
	eos = 0;
	if (!transmitData(&eos, 1))
		return false;
		
	/* flush data! */
	fflush(commPipeOut);
	return true;
}

/*
	Read commands from a pipe
*/
bool readCommands(Stack &stack, bool allowReturn, int abortTimeout){
	uint32_t 	blockInfo;
	size_t 		pos, numBytes;

	char		blockCommand;
	uint32_t	blockLength;
	char		*blockData;

	uint32_t 	function;


	#ifdef __WIN32__
		DBG_ASSERT(abortTimeout == 0, "readCommand called with abortTimeout, but not allowed on Windows.");
	#endif

	if (!commPipeIn)
		return false;

	while (1){
		/* DBG_TRACE("waiting for next command."); */

		for (pos = 0; pos < sizeof(uint32_t); pos += numBytes){

			#ifndef __WIN32__
				/*
					Note: We only check the timeout when waiting for the initial command, not for the embedded data
					This relies on the assumption that the data is always transmitted correctly as this happens
					immediately afterwards
				*/

				if (abortTimeout){
					fd_set rfds;
					struct timeval tv;
					int res;

					FD_ZERO(&rfds);
					FD_SET(fileno(commPipeIn), &rfds);

					tv.tv_sec 	=  abortTimeout / 1000;
					tv.tv_usec 	= (abortTimeout % 1000) * 1000;
					res 		= select(fileno(commPipeIn) + 1, &rfds, NULL, NULL, &tv);

					if (res <= 0){
						DBG_ERROR("unable to receive data within the specified timeout.");
						return false;
					}

				}
			#endif

			numBytes = fread( (char*)&blockInfo + pos, sizeof(char), sizeof(uint32_t) - pos, commPipeIn);
			if (numBytes == 0){

				/* broken pipe */
				if (abortTimeout){
					DBG_ERROR("unable to receive data.");
					return false;
				}

				#ifdef __WIN32__
					if (!handleManager_findInstance()) exit(0);
				#endif
				DBG_ABORT("unable to receive data.");
			}

		}

		blockCommand 	= (blockInfo >> 24) & 0xFF;
		blockLength		= blockInfo & 0xFFFFFF;
		blockData		= NULL;

		/* read arguments */
		if (blockLength > 0){

			blockData = (char*)malloc(blockLength);
			DBG_ASSERT(blockData != NULL, "failed to allocate memory.");

			for (pos = 0; pos < blockLength; pos += numBytes){
				numBytes = fread( blockData + pos, sizeof(char), blockLength - pos, commPipeIn);
				if (numBytes == 0){
					free(blockData);
					DBG_ABORT("unable to receive data.");
				}
			}

		}

		/* DBG_TRACE("received command %d with length %d.", blockCommand, blockLength); */

		/* call command */
		if (blockCommand == BLOCKCMD_CALL_DIRECT){
			DBG_ASSERT(blockData && blockLength == sizeof(uint32_t), "wrong number of arguments for BLOCKCMD_CALL_DIRECT.");

			/* get functionID */
			function = *((uint32_t*)blockData);
			free(blockData);

			/* call dispatcher */
			DBG_ASSERT(function != 0, "function zero for BLOCKCMD_CALL_DIRECT not allowed.");
			dispatcher(function, stack);

		/* return command */
		}else if (blockCommand == BLOCKCMD_RETURN){
			if (blockData) free(blockData);

			DBG_ASSERT(allowReturn, "BLOCKCMD_RETURN not allowed here.");

			break;

		/* push to stack */
		}else{
			stack.emplace_back(blockCommand, blockData, blockLength);
		}

	}

	return true;
}

/* Read an int32 */
int32_t readInt32(Stack &stack){
	Stack::reverse_iterator rit = stack.rbegin();
	int32_t *data, result;

	DBG_ASSERT(rit != stack.rend(), "no return value found.");
	data = (int32_t*)rit->data.get();

	DBG_ASSERT(rit->command == BLOCKCMD_PUSH_INT32 && data && rit->length == sizeof(int32_t), \
		"wrong return value, expected int32.");
	result = *data;

	stack.pop_back();
	return result;
}

/* Read an int64 */
int64_t readInt64(Stack &stack){
	Stack::reverse_iterator rit = stack.rbegin();
	int64_t *data, result;

	DBG_ASSERT(rit != stack.rend(), "no return value found.");
	data = (int64_t*)rit->data.get();

	DBG_ASSERT(rit->command == BLOCKCMD_PUSH_INT64 && data && rit->length == sizeof(int64_t), \
		"wrong return value, expected int64.");
	result = *data;

	stack.pop_back();
	return result;
}

/* Read a double */
double readDouble(Stack &stack){
	Stack::reverse_iterator rit = stack.rbegin();
	double *data, result;

	DBG_ASSERT(rit != stack.rend(), "no return value found.");
	data = (double*)rit->data.get();

	DBG_ASSERT(rit->command == BLOCKCMD_PUSH_DOUBLE && data && rit->length == sizeof(double), \
		"wrong return value, expected double.");
	result = *data;

	stack.pop_back();
	return result;
}

/* Read a string (NULL-ptr strings will be returned as empty string) */
std::string readString(Stack &stack){
	Stack::reverse_iterator rit = stack.rbegin();
	char *data;
	std::string result = "";

	DBG_ASSERT(rit != stack.rend(), "no return value found.");
	DBG_ASSERT(rit->command == BLOCKCMD_PUSH_STRING, "wrong return value, expected string.");

	data = rit->data.get();
	if (data && rit->length > 0){
		DBG_ASSERT(data[rit->length-1] == 0, "string not nullterminated!");
		result = std::string(data, rit->length);
	}

	stack.pop_back();
	return result;
}

/* Read a string as a shared pointer (resultLength is the length without NULL-char) */
std::shared_ptr<char> readStringAsMemory(Stack &stack, size_t &resultLength){
	Stack::reverse_iterator rit = stack.rbegin();
	std::shared_ptr<char> result;

	DBG_ASSERT(rit != stack.rend(), "no return value found.");
	DBG_ASSERT(rit->command == BLOCKCMD_PUSH_STRING, "wrong return value, expected string.");

	result 			= rit->data;
	resultLength 	= 0;

	if (result && rit->length > 0){
		DBG_ASSERT(result.get()[rit->length-1] == 0, "string not nullterminated!");
		resultLength = rit->length - 1;
	}

	stack.pop_back();
	return result;
}

/* Read a string as a shared pointer */
std::shared_ptr<char> readStringAsMemory(Stack &stack){
	size_t resultLength;
	return readStringAsMemory(stack, resultLength);
}

/* Reads a memory-block as a char pointer */
char* readStringMalloc(Stack &stack, size_t &resultLength){
	Stack::reverse_iterator rit = stack.rbegin();
	char *data, *result;

	DBG_ASSERT(rit != stack.rend(), "no return value found.");
	DBG_ASSERT(rit->command == BLOCKCMD_PUSH_STRING, "wrong return value, expected string.");

	data 			= rit->data.get();
	result 			= NULL;
	resultLength 	= 0;

	if (data && rit->length > 0){
		DBG_ASSERT(data[rit->length-1] == 0, "string not nullterminated!");

		result = (char*)malloc(rit->length);
		if (result){
			memcpy(result, data, rit->length);
			resultLength = rit->length - 1;
		}
	}

	stack.pop_back();
	return result;
}

/* Reads a memory-block as a char pointer */
char* readStringMalloc(Stack &stack){
	size_t resultLength;
	return readStringMalloc(stack, resultLength);
}

#ifndef __WIN32__

char* readStringBrowserAlloc(Stack &stack, size_t &resultLength){
	Stack::reverse_iterator rit = stack.rbegin();
	char *data, *result;

	DBG_ASSERT(rit != stack.rend(), "no return value found.");
	DBG_ASSERT(rit->command == BLOCKCMD_PUSH_STRING, "wrong return value, expected string.");

	data 			= rit->data.get();
	result 			= NULL;
	resultLength 	= 0;

	if (data && rit->length > 0){
		DBG_ASSERT(data[rit->length-1] == 0, "string not nullterminated!");

		result = (char*)sBrowserFuncs->memalloc(rit->length);
		if (result){
			memcpy(result, data, rit->length);
			resultLength = rit->length - 1;
		}
	}

	stack.pop_back();
	return result;
}

char* readStringBrowserAlloc(Stack &stack){
	size_t resultLength;
	return readStringBrowserAlloc(stack, resultLength);
}

#endif

/* Reads a memory-block as a shared pointer */
std::shared_ptr<char> readMemory(Stack &stack, size_t &resultLength){
	Stack::reverse_iterator rit = stack.rbegin();
	std::shared_ptr<char> result;

	DBG_ASSERT(rit != stack.rend(), "no return value found.");
	DBG_ASSERT(rit->command == BLOCKCMD_PUSH_MEMORY, "wrong return value, expected memory.");

	result 			= rit->data;
	resultLength 	= 0;

	if (result && rit->length > 0)
		resultLength = rit->length;

	stack.pop_back();
	return result;
}

/* Reads a memory-block as a shared pointer */
std::shared_ptr<char> readMemory(Stack &stack){
	size_t resultLength;
	return readMemory(stack, resultLength);
}


/* Reads a memory-block as a char pointer */
char* readMemoryMalloc(Stack &stack, size_t &resultLength){
	Stack::reverse_iterator rit = stack.rbegin();
	char *data, *result;

	DBG_ASSERT(rit != stack.rend(), "no return value found.");
	DBG_ASSERT(rit->command == BLOCKCMD_PUSH_MEMORY, "wrong return value, expected memory.");

	data 			= rit->data.get();
	result 			= NULL;
	resultLength 	= 0;

	if (data && rit->length > 0){
		result = (char*)malloc(rit->length);
		if (result){
			memcpy(result, data, rit->length);
			resultLength = rit->length;
		}
	}

	stack.pop_back();
	return result;
}

/* Reads a memory-block as a char pointer */
char* readMemoryMalloc(Stack &stack){
	size_t resultLength;
	return readMemoryMalloc(stack, resultLength);
}

#ifndef __WIN32__

char* readMemoryBrowserAlloc(Stack &stack, size_t &resultLength){
	Stack::reverse_iterator rit = stack.rbegin();
	char *data, *result;

	DBG_ASSERT(rit != stack.rend(), "no return value found.");
	DBG_ASSERT(rit->command == BLOCKCMD_PUSH_MEMORY, "wrong return value, expected memory.");

	data 			= rit->data.get();
	result 			= NULL;
	resultLength 	= 0;

	if (data && rit->length > 0){
		result = (char*)sBrowserFuncs->memalloc(rit->length);
		if (result){
			memcpy(result, data, rit->length);
			resultLength = rit->length;
		}
	}

	stack.pop_back();
	return result;
}

char* readMemoryBrowserAlloc(Stack &stack){
	size_t resultLength;
	return readMemoryBrowserAlloc(stack, resultLength);
}

#endif

#ifdef __WIN32__

NPObject* createNPObject(HMGR_HANDLE id, NPP instance = NULL, NPClass *cls = NULL){
	bool customObject  	= (cls != NULL);
	NPObject* obj;

	/* use proxy class if nothing specified */
	if (!cls)
		cls = &myClass;

	if (cls->allocate){
		obj = cls->allocate(instance, cls);
	}else{
		obj = (NPObject*)malloc(sizeof(NPObject));
	}

	DBG_ASSERT(obj != NULL, "could not create object.");
	obj->_class = cls;

	/*
		If its a custom created object then we can get the deallocate event and don't have to do manually refcounting.
		Otherwise its just a proxy object and can be destroyed when there is no pointer anymore in the Windows area.
	*/
	if (customObject){
		DBG_TRACE("created custom object %p with class %p.", obj, cls);
		obj->referenceCount = REFCOUNT_UNDEFINED;

	}else{
		DBG_TRACE("created proxy object %p.", obj);
		obj->referenceCount	= 0;
	}

	return obj;
}

NPP createNPPInstance(HMGR_HANDLE id){
	NPP instance = (NPP_t*)malloc(sizeof(NPP_t));

	DBG_ASSERT(instance != NULL, "could not create instance.");
	memset(instance, 0, sizeof(NPP_t));

	return instance;
}

NPStream* createNPStream(HMGR_HANDLE id){
	NPStream *stream = (NPStream*)malloc(sizeof(NPStream));
	Stack stack;

	DBG_ASSERT(stream != NULL, "could not create stream.");

	/* we cannot use writeHandle, as the handle manager hasn't finished adding this yet. */
	writeHandleId(id);
	writeInt32(HMGR_TYPE_NPStream);
	callFunction(LIN_HANDLE_MANAGER_REQUEST_STREAM_INFO);
	readCommands(stack);
	
	/* initialize memory */
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

NotifyDataRefCount* createNotifyData(HMGR_HANDLE id){
	NotifyDataRefCount* notifyData = (NotifyDataRefCount*)malloc(sizeof(NotifyDataRefCount));

	DBG_ASSERT(notifyData != NULL, "could not create notifyData.");
	notifyData->referenceCount = 0;

	return notifyData;
}

#endif

/*
	Allocate a unused ID for a specific type
*/
HMGR_HANDLE handleManager_getFreeID(HMGR_TYPE type){
	std::map<HMGR_HANDLE, void*> &idToPtr = __idToPtr(type);
	HMGR_HANDLE id;

	if (idToPtr.empty())
		return 1;

	id = idToPtr.rbegin()->first + 1;

	if (!id){
		while (idToPtr.find(++id) != idToPtr.end()){ /* empty */ }
	}

	return id;
}

/*
	Convert ID to ptr
*/
void* handleManager_idToPtr(HMGR_TYPE type, HMGR_HANDLE id, NPP instance, NPClass *cls, HMGR_EXISTS exists){
	std::map<HMGR_HANDLE, void*> &idToPtr = __idToPtr(type);
	std::map<HMGR_HANDLE, void*>::iterator it;
	void* ptr;

	/* handle null id */
	if (!id){
		DBG_ASSERT(type == HMGR_TYPE_NotifyData, "trying to translate reserved null ID.");
		return NULL;
	}

	/* translate id -> ptr */
	it = idToPtr.find(id);
	if (it != idToPtr.end()){
		DBG_ASSERT(!cls && exists != HMGR_SHOULD_NOT_EXIST, "expected new handle, but I already got this one.");
		return it->second;
	}

	DBG_ASSERT(exists != HMGR_SHOULD_EXIST, "got non-existent ID.");

	#ifdef __WIN32__
		if (type == HMGR_TYPE_NPObject){
			ptr = createNPObject(id, instance, cls);
		}else if (type == HMGR_TYPE_NPIdentifier){
			ptr = (void*)id;
		}else if (type == HMGR_TYPE_NPPInstance){
			ptr = createNPPInstance(id);
		}else if (type == HMGR_TYPE_NPStream){
			ptr = createNPStream(id);
		}else{
			DBG_ABORT("cannot create remote object of type %d.", type);
		}

	#else
		if (type == HMGR_TYPE_NotifyData){
			ptr = createNotifyData(id);
		}else{
			DBG_ABORT("cannot create local object of type %d.", type);
		}

	#endif

	std::map<void*, HMGR_HANDLE> &ptrToId = __ptrToId(type);

	idToPtr[id] 	= ptr;
	ptrToId[ptr]	= id;
	return ptr;
}

/*
	Convert ptr to ID
*/
HMGR_HANDLE handleManager_ptrToId(HMGR_TYPE type, void* ptr, HMGR_EXISTS exists){
	std::map<void*, HMGR_HANDLE> &ptrToId = __ptrToId(type);
	std::map<void*, HMGR_HANDLE>::iterator it;
	HMGR_HANDLE id;

	if(!ptr){
		DBG_ASSERT(type == HMGR_TYPE_NotifyData, "trying to translate a null pointer.");
		return 0;
	}

	it = ptrToId.find(ptr);
	if (it != ptrToId.end()){
		DBG_ASSERT(exists != HMGR_SHOULD_NOT_EXIST, "expected new handle, but I already got this one.");
		return it->second;
	}

	DBG_ASSERT(exists != HMGR_SHOULD_EXIST, "got non-existent pointer.");

	#ifdef __WIN32__
		DBG_ASSERT(type == HMGR_TYPE_NotifyData, "cannot create remote object of type %d.", type);
	#else
		DBG_ASSERT(type != HMGR_TYPE_NotifyData, "cannot create local object of type %d.", type);
	#endif

	id = handleManager_getFreeID(type);
	DBG_ASSERT(id != 0, "unable to find free id.");

	std::map<HMGR_HANDLE, void*> &idToPtr = __idToPtr(type);

	idToPtr[id] 	= ptr;
	ptrToId[ptr]	= id;
	return id;
}

/*
	Delete by ptr
*/
void handleManager_removeByPtr(HMGR_TYPE type, void* ptr){
	std::map<HMGR_HANDLE, void*> &idToPtr = __idToPtr(type);
	std::map<void*, HMGR_HANDLE> &ptrToId = __ptrToId(type);
	std::map<void*, HMGR_HANDLE>::iterator it;

	it = ptrToId.find(ptr);
	DBG_ASSERT(it != ptrToId.end(), "trying to remove handle by nonexistent pointer.");

	idToPtr.erase(it->second);
	ptrToId.erase(it);
}

/*
	Check if handle exists
*/
bool handleManager_existsByPtr(HMGR_TYPE type, void* ptr){
	std::map<void*, HMGR_HANDLE> &ptrToId = __ptrToId(type);
	std::map<void*, HMGR_HANDLE>::iterator it;

	it = ptrToId.find(ptr);
	return (it != ptrToId.end());
}

/*
	Find any instance
*/
NPP handleManager_findInstance(){
	std::map<HMGR_HANDLE, void*> &idToPtr = __idToPtr(HMGR_TYPE_NPPInstance);

	if (idToPtr.empty())
		return NULL;

	return (NPP)idToPtr.rbegin()->second;
}

/*
	Count
*/
size_t handleManager_count(){
	size_t count = 0, tmp;
	int type;

	for (type = 0; type < HMGR_NUMTYPES; type++){
		std::map<HMGR_HANDLE, void*> &idToPtr = __idToPtr(type);
		std::map<void*, HMGR_HANDLE> &ptrToId = __ptrToId(type);

		tmp = idToPtr.size();
		DBG_ASSERT(tmp == ptrToId.size(), "number of handles idToPtr and ptrToId  doesn't match.");
		count += tmp;
	}

	return count;
}

/*
	Clear
*/
void handleManager_clear(){
	int type;

	for (type = 0; type < HMGR_NUMTYPES; type++){
		std::map<HMGR_HANDLE, void*> &idToPtr = __idToPtr(type);
		std::map<void*, HMGR_HANDLE> &ptrToId = __ptrToId(type);

		idToPtr.clear();
		ptrToId.clear();
	}
}

#ifdef __WIN32__

/* objectDecRef */
void objectDecRef(NPObject *obj, bool deleteFromRemoteHandleManager){
	DBG_ASSERT(obj->referenceCount != 0, "reference count is zero.");

	if (obj->referenceCount != REFCOUNT_UNDEFINED)
		obj->referenceCount--;

	if (obj->referenceCount == 0){
		DBG_TRACE("removing object %p from handle manager.", obj);
		DBG_ASSERT(!obj->_class->deallocate, "proxy object has a deallocate method set.");

		if (deleteFromRemoteHandleManager){
			writeHandleObj(obj, HMGR_SHOULD_EXIST);
			callFunction(LIN_HANDLE_MANAGER_FREE_OBJECT);
			readResultVoid();
		}

		handleManager_removeByPtr(HMGR_TYPE_NPObject, (void*)obj);

		free(obj);
	}
}

/* objectKill */
void objectKill(NPObject *obj){
	DBG_ASSERT(obj->referenceCount == REFCOUNT_UNDEFINED, "reference count is not REFCOUNT_UNDEFINED.");

	obj->referenceCount = 0;

	handleManager_removeByPtr(HMGR_TYPE_NPObject, (void*)obj);

	if (obj->_class->deallocate){
		obj->_class->deallocate(obj);
	}else{
		free(obj);
	}
}

/* freeVariantDecRef */
void freeVariantDecRef(NPVariant &variant, bool deleteFromRemoteHandleManager){
	if (variant.type == NPVariantType_String){
		if (variant.value.stringValue.UTF8Characters)
			free((char*)variant.value.stringValue.UTF8Characters);

	}else if (variant.type == NPVariantType_Object){
		if (variant.value.objectValue)
			objectDecRef(variant.value.objectValue, deleteFromRemoteHandleManager);

	}
	
	variant.type 				= NPVariantType_Void;
	variant.value.objectValue 	= NULL;
}

/* writeVariantReleaseDecRef */
void writeVariantReleaseDecRef(NPVariant &variant){
	bool deleteFromRemoteHandleManager = false;
	NPObject* obj = NULL;

	if (variant.type == NPVariantType_Object){
		obj = variant.value.objectValue;
		deleteFromRemoteHandleManager = (obj && obj->referenceCount == 1);
	}

	writeVariantConst(variant, deleteFromRemoteHandleManager);
	freeVariantDecRef(variant, false);
}

/* readVariantIncRef */
void readVariantIncRef(Stack &stack, NPVariant &variant){
	int32_t type = readInt32(stack);
	size_t stringLength;

	variant.type = (NPVariantType)type;

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
			DBG_ABORT("unsupported variant type.");
			break;
	}
}

#endif

#ifndef __WIN32__

/* readVariant */
void readVariant(Stack &stack, NPVariant &variant){
	int32_t type = readInt32(stack);
	size_t stringLength;

	variant.type = (NPVariantType)type;

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
			DBG_ABORT("unsupported variant type.");
			break;
	}
}

/* freeVariant */
void freeVariant(NPVariant &variant){
	if (variant.type == NPVariantType_String){
		if (variant.value.stringValue.UTF8Characters)
			free((char*)variant.value.stringValue.UTF8Characters);
	}
	
	/* (dont free objects here) */

	variant.type 				= NPVariantType_Void;
	variant.value.objectValue 	= NULL;
}

#endif

/* writeVariantConst */
void writeVariantConst(const NPVariant &variant, bool deleteFromRemoteHandleManager){
	#ifndef __WIN32__
		DBG_ASSERT(!deleteFromRemoteHandleManager, "deleteFromRemoteHandleManager set on Linux side.");
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
			writeHandleObj(variant.value.objectValue, HMGR_CAN_EXIST, deleteFromRemoteHandleManager);
			break;

		default:
			DBG_ABORT("unsupported variant type.");
			break;
	}

	writeInt32(variant.type);
}
