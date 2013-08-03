#include <stdio.h>
#include <stdlib.h>
#include <stdexcept>
#include <vector>
#include <iostream>
#include <sstream>
#include "communication.h"
#include <cstring>

#ifdef __WIN32__
#include <winsock2.h>
#include <windows.h>
#include <io.h>
extern HANDLE hStdIn;


#endif

#ifndef __WIN32__
#include <sys/ioctl.h>
#endif

#include <fstream>

extern void dispatcher(int functionid, Stack &stack);

extern FILE * pipeOutF;
extern FILE * pipeInF;
extern std::ofstream output;

void freeMemory(char *memory){
	if(memory){
		free(memory);
	}
}

ParameterInfo::ParameterInfo(char command, char* newdata, size_t length) : data(newdata, freeMemory){
	this->command = command;
	this->length  = length;
}

ParameterInfo::~ParameterInfo(){

}

std::string convertInt(int number){

	std::stringstream ss;
	ss << number;
	return ss.str();
}

void transmitData(const char* data, size_t length){

	size_t pos = 0;

	// Transmit data until everything is done
	while(pos < length){
		size_t numBytes = fwrite( data + pos, sizeof(char), length - pos, pipeOutF);
		if(numBytes <= 0) throw std::runtime_error("Unable to transmit data");
		pos += numBytes;
	}

	// Flush data!
	fflush(pipeOutF);

	//#ifdef __WIN32__
	//	FlushFileBuffers((HANDLE) _fileno(pipeOutF));
	//#else
		//if(fsync(fileno(pipeOutF))){
		//	output << "Failed to flush pipe" << std::endl;
		//}
	//#endif
}

void writeCommand(char command, const char* data, size_t length){
	if(length > 0xFFFFFF){
		throw std::runtime_error("Data for command too long");
	}

	int32_t blockInfo = (command << 24) | length;
	
	transmitData((char*)&blockInfo, sizeof(int32_t));
	
	//output << "writing command " << (int)command << " with length " << length << std::endl;

	if(length > 0 && data != NULL){
		transmitData(data, length);
	}
}

void callFunction(int32_t function){
	writeCommand(BLOCKCMD_CALL_DIRECT, (char*)&function, sizeof(int32_t));
}

void returnCommand(){
	writeCommand(BLOCKCMD_RETURN);
}

void writeInt32(int32_t value){
	writeCommand(BLOCKCMD_PUSH_INT32, (char*)&value, sizeof(int32_t));
}

void writeInt64(int64_t value){
	writeCommand(BLOCKCMD_PUSH_INT64, (char*)&value, sizeof(int64_t));
}

void writeDouble(double value){
	writeCommand(BLOCKCMD_PUSH_DOUBLE, (char*)&value, sizeof(double));
}

void writeString(std::string str){
	writeCommand(BLOCKCMD_PUSH_STRING, str.c_str(), str.length()+1);
}

void writeString(const char *str){

	if(str){
		size_t length = strlen(str);
		writeCommand(BLOCKCMD_PUSH_STRING, str, length+1);
	}else{
		writeCommand(BLOCKCMD_PUSH_STRING, NULL, 0);
	}

}

void writeMemory(const char *memory, size_t length){
	writeCommand(BLOCKCMD_PUSH_MEMORY, memory, length);
}

#ifndef __WIN32__
bool checkReadCommands(){
	int totalAvail = 0; // TODO: Is this correct?
	int res = ioctl(fileno(pipeInF), FIONREAD, &totalAvail);
	return (!res && (totalAvail != 0));
}
#endif

void readCommands(Stack &stack, bool noReturn, bool returnWhenStackEmpty){

	while(true){

		int32_t blockInfo 	= 0;
		size_t  pos    		= 0;

		//output << "waiting for more commands" << std::endl;

		while(pos < sizeof(int32_t)){

			// Handle events
			/*#ifdef __WIN32__
				if(noReturn){
					u_long totalAvail = 0;

					// If there is nothing available
					while(true){
						int res = ioctlsocket(_get_osfhandle(_fileno(pipeInF)), FIONREAD, &totalAvail);
						if(res || (totalAvail != 0)){
							//output << "res = " << res << ", totalAvail = " << totalAvail << std::endl;
							break;
						}
						//output << "PeekNamedPipe returned" << res << ", totalAvail" << totalAvail << std::endl;

						// Process window events
						MSG msg;
						while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)){
							//output << "handling window events" << std::endl;
							TranslateMessage(&msg);
							DispatchMessage(&msg);
						}

						// Sleep a bit
						Sleep(10);
					}
				}
				//output << "data available" << std::endl;
			#endif*/

			size_t numBytes = fread( (char*)&blockInfo + pos, sizeof(char), sizeof(int32_t) - pos, pipeInF);
			if( numBytes <= 0 ) throw std::runtime_error("Unable to receive data");
			pos += numBytes;
		}

		// Extract infos
		char    	blockCommand 	= blockInfo >> 24;
		uint32_t 	blockLength     = blockInfo & 0xFFFFFF;
		char*   	blockData  		= NULL;

		//output << "got command " << (int)blockCommand << ", now waiting for data" << std::endl;

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

		//output << "got command " << (int)blockCommand << " with data length " << blockLength << std::endl;

		if( blockCommand == BLOCKCMD_CALL_DIRECT ){
			if(blockLength != sizeof(uint32_t)) throw std::runtime_error("Wrong number of arguments for BLOCKCMD_CALL_DIRECT");
			int32_t function = *((int32_t*)blockData);
			if(blockData) free(blockData);

			// Here the dispatcher routine - depending on the command number call the specific function
			// Remove the number of elements from the stack required for the function

			if(function != 0){
				dispatcher(function, stack);
			}
			
		}else if( blockCommand == BLOCKCMD_RETURN ){
			if(blockData) free(blockData);
			if(!noReturn) return;

		}else{
			stack.emplace_back(blockCommand, blockData, blockLength);
		}

		// Return if the stack is empty now!
		if(returnWhenStackEmpty && stack.size() == 0) return;

	}
}

int32_t readInt32(Stack &stack){

	// get last element in stack
	std::vector<ParameterInfo>::reverse_iterator rit = stack.rbegin();
	if(rit == stack.rend())	throw std::runtime_error("No return value found");

	uint32_t *data = (uint32_t*)rit->data.get();

	// check for correct type
	if( rit->command != BLOCKCMD_PUSH_INT32 || rit->length != sizeof(int32_t) || data == NULL ){
		throw std::runtime_error("Wrong return value");
	}

	int32_t result = *data;
	stack.pop_back();

	return result;
}

uint64_t readInt64(Stack &stack){

	// get last element in stack
	std::vector<ParameterInfo>::reverse_iterator rit = stack.rbegin();
	if(rit == stack.rend())	throw std::runtime_error("No return value found");

	uint64_t *data = (uint64_t*)rit->data.get();

	// check for correct type
	if( rit->command != BLOCKCMD_PUSH_INT64 || rit->length != sizeof(uint64_t) || data == NULL ){
		throw std::runtime_error("Wrong return value");
	}

	uint64_t result = *data;
	stack.pop_back();

	return result;
}

double readDouble(Stack &stack){

	// get last element in stack
	std::vector<ParameterInfo>::reverse_iterator rit = stack.rbegin();
	if(rit == stack.rend())	throw std::runtime_error("No return value found");

	double *data = (double*)rit->data.get();

	// check for correct type
	if( rit->command != BLOCKCMD_PUSH_DOUBLE || rit->length != sizeof(double) || data == NULL ){
		throw std::runtime_error("Wrong return value");
	}

	double result = *data;
	stack.pop_back();

	return result;
}


std::shared_ptr<char> readBinaryData(Stack &stack, size_t &resultLength){
	
	// get last element in stack
	std::vector<ParameterInfo>::reverse_iterator rit = stack.rbegin();
	if(rit == stack.rend())	throw std::runtime_error("No return value found");

	// check for correct type
	if( rit->command != BLOCKCMD_PUSH_MEMORY ){
		throw std::runtime_error("Wrong return value");
	}

	std::shared_ptr<char> result = rit->data;

	if( rit->length == 0 || !result ){
		resultLength = 0;
	}else{
		resultLength = rit->length;
	}

	stack.pop_back();

	return result;
}

std::shared_ptr<char> readStringAsBinaryData(Stack &stack){
	
	// get last element in stack
	std::vector<ParameterInfo>::reverse_iterator rit = stack.rbegin();
	if(rit == stack.rend())	throw std::runtime_error("No return value found");

	// check for correct type
	if( rit->command != BLOCKCMD_PUSH_STRING ){
		throw std::runtime_error("Wrong return value");
	}

	std::shared_ptr<char> result = rit->data;

	if( rit->length != 0 && result){
		if (result.get()[rit->length-1] != 0){
			throw std::runtime_error("String not nullterimanted");
		}
	}

	stack.pop_back();

	return result;
}

char* readStringAsMalloc(Stack &stack){

	// get last element in stack
	std::vector<ParameterInfo>::reverse_iterator rit = stack.rbegin();
	if(rit == stack.rend())	throw std::runtime_error("No return value found");

	// check for correct type
	if( rit->command != BLOCKCMD_PUSH_STRING ){
		throw std::runtime_error("Wrong return value");
	}

	char *data = rit->data.get();

	if( rit->length != 0 && data){
		if (data[rit->length-1] != 0){
			throw std::runtime_error("String not nullterimanted");
		}
	}

	if(!rit->length || !data){
		stack.pop_back();
		return NULL;
	}

	char *result = (char*)malloc(rit->length);
	if(result){
		memcpy(result, data, rit->length);
	}

	stack.pop_back();

	return result;	
}

std::string readString(Stack &stack){
	
	// get last element in stack
	std::vector<ParameterInfo>::reverse_iterator rit = stack.rbegin();
	if(rit == stack.rend())	throw std::runtime_error("No return value found");

	char *data = rit->data.get();

	// check for correct type
	if( rit->command != BLOCKCMD_PUSH_STRING ){
		throw std::runtime_error("Wrong return value");
	}

	std::string result;

	size_t size = rit->length;

	if( rit->length != 0 && data != NULL ){
		while( size > 0 && data[size-1] == 0) size--;
		result = std::string(data, size);
	}

	stack.pop_back();

	return result;
}

int32_t readResultInt32(){
	std::vector<ParameterInfo> stack;
	readCommands(stack);
	return readInt32(stack);
}

int64_t readResultInt64(){
	std::vector<ParameterInfo> stack;
	readCommands(stack);
	return readInt64(stack);
}

std::string readResultString(){
	std::vector<ParameterInfo> stack;
	readCommands(stack);
	return readString(stack);
}

void waitReturn(){
	std::vector<ParameterInfo> stack;
	readCommands(stack);
}