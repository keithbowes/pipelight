#include <stdio.h>								// for fread, fwrite
#include <stdlib.h>								// for malloc, ...
#include <stdexcept>							// for std::runtime_error
#include <iostream>								// for std::cerr
#include <cstring>								// for memcpy, ...
#include <memory>								// for shared_ptr
#include <string>								// for std::string
#include <vector>								// for std::vector<ParameterInfo>

#include "communication.h"

#ifdef __WIN32__
	#include "../handlemanager/handlemanager.h"	// for handlemanager.findInstance
	extern HandleManager handlemanager;

#else
	#include "../npapi-headers/npfunctions.h"	// for sBrowserFuncs->memalloc
	extern NPNetscapeFuncs *sBrowserFuncs;
#endif

extern void dispatcher(int functionid, Stack &stack);
extern FILE * pipeOutF;
extern FILE * pipeInF;

// Function to free shared_ptr memory - prevent null pointer exceptions
void freeSharedPtrMemory(char *memory){
	if(memory){
		free(memory);
	}
}

ParameterInfo::ParameterInfo(char command, char* newdata, size_t length) : data(newdata, freeSharedPtrMemory){
	this->command = command;
	this->length  = length;
}

ParameterInfo::~ParameterInfo(){
	// Do nothing
}


// Transmits the given number of bytes
void transmitData(const char* data, size_t length){
	size_t pos = 0;

	// Transmit data until everything is done
	while(pos < length){
		size_t numBytes = fwrite( data + pos, sizeof(char), length - pos, pipeOutF);
		if(numBytes <= 0) throw std::runtime_error("Unable to transmit data");
		pos += numBytes;
	}

}

// Writes a command to the pipe
void writeCommand(char command, const char* data, size_t length){
	if(length > 0xFFFFFF){
		throw std::runtime_error("Data for command too long");
	}

	int32_t blockInfo = (command << 24) | length;

	// Transmit command info	
	transmitData((char*)&blockInfo, sizeof(int32_t));

	// Transmit argument (if any)
	if(length > 0 && data != NULL){
		transmitData(data, length);
	}

	// Flush data!
	fflush(pipeOutF);
}

// Call a function
void callFunction(int32_t function){
	writeCommand(BLOCKCMD_CALL_DIRECT, (char*)&function, sizeof(int32_t));
}

// Return from a function
void returnCommand(){
	writeCommand(BLOCKCMD_RETURN);
}

// Writes an int32
void writeInt32(int32_t value){
	writeCommand(BLOCKCMD_PUSH_INT32, (char*)&value, sizeof(int32_t));
}

// Read an int32
int32_t readInt32(Stack &stack){

	// get last element in stack
	std::vector<ParameterInfo>::reverse_iterator rit = stack.rbegin();
	if(rit == stack.rend())	throw std::runtime_error("No return value found");

	uint32_t *data = (uint32_t*)rit->data.get();

	// check for correct type
	if( rit->command != BLOCKCMD_PUSH_INT32 || rit->length != sizeof(int32_t) || data == NULL ){
		throw std::runtime_error("Wrong return value, expected int32");
	}

	int32_t result = *data;
	stack.pop_back();

	return result;
}

// Writes an int64
void writeInt64(int64_t value){
	writeCommand(BLOCKCMD_PUSH_INT64, (char*)&value, sizeof(int64_t));
}

// Read an int64
uint64_t readInt64(Stack &stack){

	// get last element in stack
	std::vector<ParameterInfo>::reverse_iterator rit = stack.rbegin();
	if(rit == stack.rend())	throw std::runtime_error("No return value found");

	uint64_t *data = (uint64_t*)rit->data.get();

	// check for correct type
	if( rit->command != BLOCKCMD_PUSH_INT64 || rit->length != sizeof(uint64_t) || data == NULL ){
		throw std::runtime_error("Wrong return value, expected int64");
	}

	uint64_t result = *data;
	stack.pop_back();

	return result;
}

// Writes an double
void writeDouble(double value){
	writeCommand(BLOCKCMD_PUSH_DOUBLE, (char*)&value, sizeof(double));
}

// Read double
double readDouble(Stack &stack){

	// get last element in stack
	std::vector<ParameterInfo>::reverse_iterator rit = stack.rbegin();
	if(rit == stack.rend())	throw std::runtime_error("No return value found");

	double *data = (double*)rit->data.get();

	// check for correct type
	if( rit->command != BLOCKCMD_PUSH_DOUBLE || rit->length != sizeof(double) || data == NULL ){
		throw std::runtime_error("Wrong return value, expected double");
	}

	double result = *data;
	stack.pop_back();

	return result;
}


// Writes a C++-String (including terminating zero)
void writeString(std::string str){
	writeCommand(BLOCKCMD_PUSH_STRING, str.c_str(), str.length()+1);
}

// Writes a string based on a char* (including the terminating zero)
void writeString(const char *str){
	if(str){
		writeCommand(BLOCKCMD_PUSH_STRING, str, strlen(str)+1);

	}else{
		writeCommand(BLOCKCMD_PUSH_STRING, NULL, 0);

	}
}

// Writes a string and appends a final nullptr
void writeString(const char *str, size_t length){
	if(str){
		//writeCommand(BLOCKCMD_PUSH_STRING, str, length);

		if(length > 0xFFFFFF - 1){
			throw std::runtime_error("Data for command too long");
		}

		int32_t blockInfo = (BLOCKCMD_PUSH_STRING << 24) | (length + 1);

		// Transmit command info	
		transmitData((char*)&blockInfo, sizeof(int32_t));

		// Transmit string (first part)
		if(length > 0) transmitData(str, length);

		// Transmit end of string
		char eos = 0;
		transmitData(&eos, sizeof(char));
		
		// Flush data!
		fflush(pipeOutF);

	}else{
		writeCommand(BLOCKCMD_PUSH_STRING, NULL, 0);
	}
}

// Read string as std::string
// WARNING: Nullpointer strings will be returned as empty-strings! This is not always correct!
std::string readString(Stack &stack){
	
	// get last element in stack
	std::vector<ParameterInfo>::reverse_iterator rit = stack.rbegin();
	if(rit == stack.rend())	throw std::runtime_error("No return value found");

	char *data = rit->data.get();

	// check for correct type
	if( rit->command != BLOCKCMD_PUSH_STRING ){
		throw std::runtime_error("Wrong return value, expected string");
	}

	std::string result = "";

	if( rit->length > 0 && data ){

		// Ensure string is nullterminated
		if( data[rit->length-1] != 0 ) throw std::runtime_error("String not nullterminated!");

		result = std::string(data, rit->length);
	}

	stack.pop_back();

	return result;
}

// Reads a memory-block as a shared pointer
// ResultLength is the number of bytes WITHOUT the trailing nullbyte
std::shared_ptr<char> readStringAsMemory(Stack &stack, size_t &resultLength){
	
	// get last element in stack
	std::vector<ParameterInfo>::reverse_iterator rit = stack.rbegin();
	if(rit == stack.rend())	throw std::runtime_error("No return value found");

	// check for correct type
	if( rit->command != BLOCKCMD_PUSH_STRING ){
		throw std::runtime_error("Wrong return value, expected string");
	}

	std::shared_ptr<char> result = rit->data;
	resultLength = 0;

	if( rit->length > 0 && result ){

		// Ensure string is nullterminated
		if( result.get()[rit->length-1] != 0 ) throw std::runtime_error("String not nullterminated!");

		resultLength = rit->length - 1;
	}

	stack.pop_back();

	return result;
}

std::shared_ptr<char> readStringAsMemory(Stack &stack){
	size_t resultLength;
	return readStringAsMemory(stack, resultLength);
}

// Reads a memory-block as a char pointer
// YOU ARE RESPONSIBLE FOR FREEING THIS PTR!
char* readStringMalloc(Stack &stack, size_t &resultLength){

	// get last element in stack
	std::vector<ParameterInfo>::reverse_iterator rit = stack.rbegin();
	if(rit == stack.rend())	throw std::runtime_error("No return value found");

	// check for correct type
	if( rit->command != BLOCKCMD_PUSH_STRING ){
		throw std::runtime_error("Wrong return value, expected string");
	}

	char *data 		= rit->data.get();
	char *result 	= NULL;
	resultLength = 0;

	if(rit->length > 0 && data){

		// Ensure string is nullterminated
		if( data[rit->length-1] != 0 ) throw std::runtime_error("String not nullterminated!");

		result = (char*)malloc(rit->length);
		if(result){
			memcpy(result, data, rit->length);
			resultLength = rit->length - 1;
		}
	}

	stack.pop_back();

	return result;	
}

// YOU ARE RESPONSIBLE FOR FREEING THIS PTR!
char* readStringMalloc(Stack &stack){
	size_t resultLength;
	return readStringMalloc(stack, resultLength);
}

#ifndef __WIN32__

char* readStringBrowserAlloc(Stack &stack, size_t &resultLength){

	// get last element in stack
	std::vector<ParameterInfo>::reverse_iterator rit = stack.rbegin();
	if(rit == stack.rend())	throw std::runtime_error("No return value found");

	// check for correct type
	if( rit->command != BLOCKCMD_PUSH_STRING ){
		throw std::runtime_error("Wrong return value, expected string");
	}

	char *data 		= rit->data.get();
	char *result 	= NULL;
	resultLength = 0;

	if(rit->length > 0 && data){

		// Ensure string is nullterminated
		if( data[rit->length-1] != 0 ) throw std::runtime_error("String not nullterminated!");

		result = (char*)sBrowserFuncs->memalloc(rit->length);
		if(result){
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

// Writes a memory block (also works for nullptr)
void writeMemory(const char *memory, size_t length){
	writeCommand(BLOCKCMD_PUSH_MEMORY, memory, length);
}

// Reads a memory-block as a shared pointer
std::shared_ptr<char> readMemory(Stack &stack, size_t &resultLength){
	
	// get last element in stack
	std::vector<ParameterInfo>::reverse_iterator rit = stack.rbegin();
	if(rit == stack.rend())	throw std::runtime_error("No return value found");

	// check for correct type
	if( rit->command != BLOCKCMD_PUSH_MEMORY ){
		throw std::runtime_error("Wrong return value, expected memory");
	}

	std::shared_ptr<char> result = rit->data;
	resultLength = 0;

	if( rit->length > 0 && result ){
		resultLength = rit->length;
	}

	stack.pop_back();

	return result;
}

std::shared_ptr<char> readMemory(Stack &stack){
	size_t resultLength;
	return readMemory(stack, resultLength);
}

// Reads a memory-block as a char pointer
// YOU ARE RESPONSIBLE FOR FREEING THIS PTR!
char* readMemoryMalloc(Stack &stack, size_t &resultLength){

	// get last element in stack
	std::vector<ParameterInfo>::reverse_iterator rit = stack.rbegin();
	if(rit == stack.rend())	throw std::runtime_error("No return value found");

	// check for correct type
	if( rit->command != BLOCKCMD_PUSH_MEMORY ){
		throw std::runtime_error("Wrong return value, expected memory");
	}

	char *data 		= rit->data.get();
	char *result 	= NULL;
	resultLength = 0;

	if(rit->length > 0 && data){
		result = (char*)malloc(rit->length);
		if(result){
			memcpy(result, data, rit->length);
			resultLength = rit->length;
		}
	}

	stack.pop_back();

	return result;	
}

// YOU ARE RESPONSIBLE FOR FREEING THIS PTR!
char* readMemoryMalloc(Stack &stack){
	size_t resultLength;
	return readMemoryMalloc(stack, resultLength);
}

#ifndef __WIN32__

// YOU ARE RESPONSIBLE FOR FREEING THIS PTR!
char* readMemoryBrowserAlloc(Stack &stack, size_t &resultLength){

	// get last element in stack
	std::vector<ParameterInfo>::reverse_iterator rit = stack.rbegin();
	if(rit == stack.rend())	throw std::runtime_error("No return value found");

	// check for correct type
	if( rit->command != BLOCKCMD_PUSH_MEMORY ){
		throw std::runtime_error("Wrong return value, expected memory");
	}

	char *data 		= rit->data.get();
	char *result 	= NULL;
	resultLength = 0;

	if(rit->length > 0 && data){
		result = (char*)sBrowserFuncs->memalloc(rit->length);
		if(result){
			memcpy(result, data, rit->length);
			resultLength = rit->length;
		}
	}

	stack.pop_back();

	return result;	
}

// YOU ARE RESPONSIBLE FOR FREEING THIS PTR!
char* readMemoryBrowserAlloc(Stack &stack){
	size_t resultLength;
	return readMemoryBrowserAlloc(stack, resultLength);
}

#endif


void readCommands(Stack &stack, bool allowReturn){

	while(true){
		int32_t blockInfo 	= 0;
		size_t  pos    		= 0;

		// Wait for initial command
		while(pos < sizeof(int32_t)){
			size_t numBytes = fread( (char*)&blockInfo + pos, sizeof(char), sizeof(int32_t) - pos, pipeInF);
			if( numBytes <= 0 ){
				#ifdef __WIN32__
					// If we don't have any running instances of our plugin
					// a broken pipes simply means the shutdown of the browser
					// plugin and doesn't need to be an error
					if(!handlemanager.findInstance()){
						exit(0);
					}else{
						throw std::runtime_error("Unable to receive data");
					}
				#else
					throw std::runtime_error("Unable to receive data");
				#endif
			}

			pos += numBytes;
		}

		// Extract infos
		char    	blockCommand 	= blockInfo >> 24;
		uint32_t 	blockLength     = blockInfo & 0xFFFFFF;
		char*   	blockData  		= NULL;

		// Read arguments
		if(blockLength > 0){
			blockData = (char*)malloc(blockLength);
			if( blockData == NULL ) throw std::runtime_error("Not enough memory");

			pos = 0;

			while(pos < blockLength){
				size_t numBytes = fread( blockData + pos, sizeof(char), blockLength - pos, pipeInF);
				if( numBytes <= 0) throw std::runtime_error("Unable to receive data");
				pos += numBytes;
			}

		}

		// Call command
		if( blockCommand == BLOCKCMD_CALL_DIRECT ){
			if(blockLength != sizeof(uint32_t)) throw std::runtime_error("Wrong number of arguments for BLOCKCMD_CALL_DIRECT");
			int32_t function = *((int32_t*)blockData);
			if(blockData) free(blockData);

			if(function == 0){
				throw std::runtime_error("FunctionID 0 for BLOCKCMD_CALL_DIRECT not allowed");
			}

			// Here the dispatcher routine - depending on the command number call the specific function
			// Remove the number of elements from the stack required for the function
			dispatcher(function, stack);
			
		// Return command
		}else if( blockCommand == BLOCKCMD_RETURN ){
			if(blockData) free(blockData);
			
			if(!allowReturn){
				throw std::runtime_error("BLOCKCMD_RETURN not allowed here");
			}

			break;

		// Other commands - push to stack
		}else{
			stack.emplace_back(blockCommand, blockData, blockLength);

		}

	}

}


int32_t readResultInt32(){
	Stack stack;
	readCommands(stack);
	return readInt32(stack);
}

int64_t readResultInt64(){
	Stack stack;
	readCommands(stack);
	return readInt64(stack);
}

std::string readResultString(){
	Stack stack;
	readCommands(stack);
	return readString(stack);
}

void waitReturn(){
	Stack stack;
	readCommands(stack);
}

// Debug stuff

void debugEnterFunction( std::string name ){
	#ifdef __WIN32__
		std::cerr << "[PIPELIGHT:WINDOWS] " << name << " entered" << std::endl;

	#else
		std::cerr << "[PIPELIGHT:LINUX] " << name << " entered" << std::endl;

	#endif
}

void debugNotImplemented( std::string name ){
	#ifdef __WIN32__
		std::cerr << "[PIPELIGHT:WINDOWS] " << name << " not implemented!" << std::endl;

	#else
		std::cerr << "[PIPELIGHT:LINUX] " << name << " not implemented!" << std::endl;

	#endif
}
