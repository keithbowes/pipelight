#pragma once
#include <memory>
#include <string>
#include <vector>


enum FunctionIDs{

	// ------- Plugin -------
	FUNCTION_GET_VERSION = 1,
	FUNCTION_GET_MIMETYPE,
	FUNCTION_GET_NAME,
	FUNCTION_GET_DESCRIPTION,

	FUNCTION_NPP_NEW,

	FUNCTION_NP_INVOKE,
	FUNCTION_NP_HAS_PROPERTY_FUNCTION,
	FUNCTION_NP_HAS_METHOD_FUNCTION,
	FUNCTION_NP_GET_PROPERTY_FUNCTION,

	FUNCTION_NPP_GETVALUE_BOOL,
	FUNCTION_NPP_GETVALUE_OBJECT,
	
	FUNCTION_SET_WINDOW_INFO,
	FUNCTION_NPP_NEW_STREAM,
	FUNCTION_NPP_DESTROY_STREAM,
	FUNCTION_NPP_WRITE_READY,
	FUNCTION_NPP_WRITE,
	FUNCTION_NPP_URL_NOTIFY,

	// ------- Browser -------
	FUNCTION_NPN_CREATE_OBJECT,
	
	FUNCTION_NPN_GET_WINDOWNPOBJECT,
	FUNCTION_NPN_GET_PRIVATEMODE,
	FUNCTION_NPN_GET_PLUGINELEMENTNPOBJECT,

	FUNCTION_NPP_GET_STRINGIDENTIFIER,
	FUNCTION_NPN_GET_PROPERTY,
	FUNCTION_NPN_RELEASEOBJECT,
	FUNCTION_NPN_RETAINOBJECT,
	FUNCTION_NPN_Evaluate,
	FUNCTION_NPN_INVOKE,

	FUNCTION_NPN_GET_URL_NOTIFY,

	FUNCTION_NPN_STATUS,

	FUNCTION_NPN_UTF8_FROM_IDENTIFIER,
	FUNCTION_NPN_IDENTIFIER_IS_STRING,
	FUNCTION_NPN_INT_FROM_IDENTIFIER,

	// ------- Both -------
	HANDLE_MANAGER_DELETE,
	HANDLE_MANAGER_REQUEST_STREAM_INFO,
	OBJECT_KILL,
	//STREAM_KILL,

	// ------- IDLE -------
	NOP,
	PROCESS_WINDOW_EVENTS
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
void writeInt64(int64_t value);
void writeDouble(double value);
void writeString(std::string str);
void writeString(const char *str);
void writeMemory(const char *memory, size_t length);

#ifndef __WIN32__
bool checkReadCommands();
#endif

void readCommands(Stack &stack, bool noReturn = false, bool returnWhenStackEmpty = false);

int32_t readInt32(Stack &stack);
uint64_t readInt64(Stack &stack);
std::shared_ptr<char> readBinaryData(Stack &stack, size_t &resultLength);
std::shared_ptr<char> readStringAsBinaryData(Stack &stack);
char* readStringAsMalloc(Stack &stack);
std::string readString(Stack &stack);

int32_t readResultInt32();
int64_t readResultInt64();
double readDouble(Stack &stack);
std::string readResultString();

void waitReturn();

