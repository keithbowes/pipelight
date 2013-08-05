#pragma once
#include <memory>
#include <string>
#include <vector>


enum FunctionIDs{

	// ------- Special -------

	// Tells the windows side that the given object should be freed
	OBJECT_KILL = 1,
	OBJECT_IS_CUSTOM,

	// Used to request stream details
	HANDLE_MANAGER_REQUEST_STREAM_INFO,

	// Processes windoe events
	PROCESS_WINDOW_EVENTS,


	// ------- Plugin -------
	FUNCTION_GET_VERSION,
	FUNCTION_GET_MIMETYPE,
	FUNCTION_GET_NAME,
	FUNCTION_GET_DESCRIPTION,


	FUNCTION_NP_INVOKE,
	FUNCTION_NP_HAS_PROPERTY_FUNCTION,
	FUNCTION_NP_HAS_METHOD_FUNCTION,
	FUNCTION_NP_GET_PROPERTY_FUNCTION,
	FUNCTION_NP_SET_PROPERTY_FUNCTION,
	FUNCTION_NP_INVALIDATE_FUNCTION,

	FUNCTION_NPP_NEW,
	FUNCTION_NPP_DESTROY,
	FUNCTION_NPP_GETVALUE_BOOL,
	FUNCTION_NPP_GETVALUE_OBJECT,
	FUNCTION_NPP_SET_WINDOW,
	FUNCTION_NPP_NEW_STREAM,
	FUNCTION_NPP_DESTROY_STREAM,
	FUNCTION_NPP_WRITE_READY,
	FUNCTION_NPP_WRITE,
	FUNCTION_NPP_URL_NOTIFY,
	FUNCTION_NPN_USERAGENT,

	// ------- Browser -------
	FUNCTION_NPN_CREATE_OBJECT,

	FUNCTION_NPN_GETVALUE_BOOL,
	FUNCTION_NPN_GETVALUE_OBJECT,

	FUNCTION_NPP_GET_STRINGIDENTIFIER,
	FUNCTION_NPN_GET_PROPERTY,
	FUNCTION_NPN_RELEASEOBJECT,
	FUNCTION_NPN_RETAINOBJECT,
	FUNCTION_NPN_EVALUATE,
	FUNCTION_NPN_INVOKE,

	FUNCTION_NPN_GET_URL_NOTIFY,
	FUNCTION_NPN_POST_URL_NOTIFY,
	FUNCTION_NPN_WRITE,
	FUNCTION_NPN_DESTROY_STREAM,

	FUNCTION_NPN_STATUS,

	FUNCTION_NPN_UTF8_FROM_IDENTIFIER,
	FUNCTION_NPN_IDENTIFIER_IS_STRING,
	FUNCTION_NPN_INT_FROM_IDENTIFIER,

};

enum{
	BLOCKCMD_CALL_DIRECT = 0,
	BLOCKCMD_RETURN,

	BLOCKCMD_PUSH_INT32,
	BLOCKCMD_PUSH_INT64,
	BLOCKCMD_PUSH_DOUBLE,
	BLOCKCMD_PUSH_STRING,
	BLOCKCMD_PUSH_MEMORY
};

struct ParameterInfo{

	public:
		char command;
		std::shared_ptr<char> data;
		size_t length;

		ParameterInfo(char command, char* newdata, size_t length);
		~ParameterInfo();

};

typedef std::vector<ParameterInfo> Stack;

void transmitData(const char* data, size_t length);
void writeCommand(char command, const char* data = NULL, size_t length = 0);
void callFunction(int32_t function);
void returnCommand();

void writeInt32(int32_t value);
int32_t readInt32(Stack &stack);

void writeInt64(int64_t value);
uint64_t readInt64(Stack &stack);

void writeDouble(double value);
double readDouble(Stack &stack);

void writeString(std::string str);
void writeString(const char *str);
void writeString(const char *str, size_t length);
std::string readString(Stack &stack);
std::shared_ptr<char> readStringAsMemory(Stack &stack, size_t &resultLength);
std::shared_ptr<char> readStringAsMemory(Stack &stack);
char* readStringMalloc(Stack &stack, size_t &resultLength);
char* readStringMalloc(Stack &stack);
#ifndef __WIN32__
char* readStringBrowserAlloc(Stack &stack, size_t &resultLength);
char* readStringBrowserAlloc(Stack &stack);
#endif

void writeMemory(const char *memory, size_t length);
std::shared_ptr<char> readMemory(Stack &stack, size_t &resultLength);
std::shared_ptr<char> readMemory(Stack &stack);
char* readMemoryMalloc(Stack &stack, size_t &resultLength);
char* readMemoryMalloc(Stack &stack);
#ifndef __WIN32__
char* readMemoryBrowserAlloc(Stack &stack, size_t &resultLength);
char* readMemoryBrowserAlloc(Stack &stack);
#endif

void readCommands(Stack &stack, bool allowReturn = true);

int32_t readResultInt32();
int64_t readResultInt64();
std::string readResultString();
void waitReturn();

void debugEnterFunction( std::string name );
void debugNotImplemented( std::string name );
