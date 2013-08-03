/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Josh Aas <josh@mozilla.com>
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

#include "basicplugin.h"

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <cstring>

#include <unistd.h>

static void attach() __attribute__((constructor));
static void dettach() __attribute__((destructor));

NPNetscapeFuncs* sBrowserFuncs = NULL;

std::ofstream output(BROWSER_LOG, std::ios::out | std::ios::app);

HandleManager handlemanager;

typedef struct InstanceData {
	NPP npp;
	NPWindow window;
} InstanceData;

int pipeOut[2] 	= {0, 0};
int pipeIn[2] 	= {0, 0};

#define PIPE_BROWSER_READ   pipeIn[0]
#define PIPE_PLUGIN_WRITE   pipeIn[1]
#define PIPE_BROWSER_WRITE  pipeOut[1]
#define PIPE_PLUGIN_READ    pipeOut[0]

FILE * pipeOutF = NULL;
FILE * pipeInF	= NULL;

pid_t pid = -1;

void attach(){
	
	output << "attached" << std::endl;

	if( pipe(pipeOut) == -1 ){
		output << "Could not create Pipe 1" << std::endl;
		return;
	}

	if( pipe(pipeIn) == -1 ){
		output << "Could not create Pipe 2" << std::endl;
		return;
	}

	pid_t pid = fork();
	if (pid == 0){
		
		close(PIPE_BROWSER_READ);
		close(PIPE_BROWSER_WRITE);

		dup2(PIPE_PLUGIN_READ,  0);
		dup2(PIPE_PLUGIN_WRITE, 1);	
		
		#ifdef WINE_PREFIX
		putenv(WINE_PREFIX);
		#endif
		execlp(WINE_PATH, "wine", PLUGIN_LOADER_PATH, NULL);			

	}else if (pid != -1){

		close(PIPE_PLUGIN_READ);
		close(PIPE_PLUGIN_WRITE);		

		pipeOutF 	= fdopen(PIPE_BROWSER_WRITE, 	"wb");
		pipeInF		= fdopen(PIPE_BROWSER_READ, 	"rb");		

	}else{
		output << "Error while fork" << std::endl;
	}

}

void dettach(){

}


void freeVariant(NPVariant &variant){
	if (variant.type == NPVariantType_String){
		sBrowserFuncs->memfree((char*)variant.value.stringValue.UTF8Characters);
		variant.value.stringValue.UTF8Characters 	= NULL;
		variant.value.stringValue.UTF8Length		= 0;
	}
}

void freeVariant(std::vector<NPVariant> args){
	for(NPVariant &variant :  args){
		freeVariant(variant);
	}
}


void handle_NPN_Invoke(Stack &stack){

	output << "handle_NPN_Invoke" << std::endl;


	NPP instance 					= readHandleInstance(stack);
	NPObject* obj 					= readHandleObj(stack);
	NPIdentifier identifier			= readHandleIdentifier(stack);
	int32_t argCount				= readInt32(stack);
	std::vector<NPVariant> args 	= readVariantArray(stack, argCount);
	NPVariant resultVariant;

	resultVariant.type = NPVariantType_Null;
	
	output << "browser side: before invoke" << std::endl;
	output << "argCount = " << argCount << std::endl;

	bool result = sBrowserFuncs->invoke(instance, obj, identifier, args.data(), argCount, &resultVariant);


	output << "browser side: after invoke" << std::endl;

	freeVariant(args);

	if(result)
		writeVariant(resultVariant);

	writeInt32(result);
	returnCommand();	
}

void sendStreamInfo(Stack &stack){

	NPStream* stream = readHandleStream(stack);

	// This value may be a NULL pointer,
	// so use writeMemory instead of writeString

	if (stream->headers){
		// Include trailing zero
		int length = strlen(stream->headers) + 1;
		writeMemory(stream->headers, length);
	}else{
		writeMemory(NULL, 0);
	}

	writeHandleNotify(stream->notifyData);

	writeInt32(stream->lastmodified);
	writeInt32(stream->end);
	writeString(stream->url);

	output << "Sending stream with url" << stream->url << std::endl;

	returnCommand();

}

void handle_NPN_GetURLNotify(Stack &stack){

	NPP instance 					= readHandleInstance(stack);
	std::shared_ptr<char> url 		= readStringAsBinaryData(stack);
	std::shared_ptr<char> target 	= readStringAsBinaryData(stack);
	void* notifyData 				= readHandleNotify(stack);

	NPError error = sBrowserFuncs->geturlnotify(instance, url.get(), target.get(), notifyData);
	writeInt32(error);
	returnCommand();

}

void handle_NPN_Status(Stack &stack){

	NPP instance 		= readHandleInstance(stack);
	std::string message	= readString(stack);
	sBrowserFuncs->status(instance, message.c_str());
	returnCommand();

}

void handle_NPN_UTF8FromIdentifier(Stack &stack){

	NPIdentifier identifier	= readHandleIdentifier(stack);
	NPUTF8 *str = sBrowserFuncs->utf8fromidentifier(identifier);
	writeString((char*) str);

	if(str)
		sBrowserFuncs->memfree(str);

	returnCommand();
}

void handle_NPN_IdentifierIsString(Stack &stack){

	output << "handle_NPN_IdentifierIsString" << std::endl;

	NPIdentifier identifier = readHandleIdentifier(stack);

	output << "handle_NPN_IdentifierIsString got args" << std::endl;

	bool result = sBrowserFuncs->identifierisstring(identifier);
	
	output << "handle_NPN_IdentifierIsString result: " << result << std::endl;

	writeInt32(result);
	returnCommand();

}

void handle_NPN_IntFromIdentifier(Stack &stack){

	NPIdentifier identifier = readHandleIdentifier(stack);
	int32_t result = sBrowserFuncs->intfromidentifier(identifier);
	
	writeInt32(result);
	returnCommand();

}

void dispatcher(int functionid, Stack &stack){

	NPP instance;
	NPObject* obj;
	NPIdentifier propertyName;
	NPVariant resultVariant;
	NPString script;
	std::shared_ptr<char> utf8name;
	NPIdentifier identifier;
	size_t resultLength;
	bool result;
	std::shared_ptr<char> data;

	uint64_t resultBool;
	NPError error;

	int64_t id;
	int32_t type;

	switch(functionid){
		
		case FUNCTION_NPN_CREATE_OBJECT:

			obj = sBrowserFuncs->createobject(readHandleInstance(stack), &myClass);
			writeHandle(obj);
			returnCommand();
			break;

		case FUNCTION_NPN_GET_WINDOWNPOBJECT:

			error = sBrowserFuncs->getvalue(readHandleInstance(stack), NPNVWindowNPObject, &obj);
			writeHandle(obj);
			writeInt32(error);
			returnCommand();
			break;	
			
		case FUNCTION_NPN_GET_PRIVATEMODE:

			error = sBrowserFuncs->getvalue(readHandleInstance(stack), NPNVprivateModeBool, &resultBool);
			writeInt32((bool)resultBool);
			writeInt32(error);
			returnCommand();
			break;	

		case FUNCTION_NPP_GET_STRINGIDENTIFIER:

			utf8name 	= readStringAsBinaryData(stack);	
			identifier 	= sBrowserFuncs->getstringidentifier((NPUTF8*) utf8name.get());
			writeHandle(identifier);
			returnCommand();
			break;

		case FUNCTION_NPN_GET_PROPERTY:

			instance 		= readHandleInstance(stack);
			obj 			= readHandleObj(stack);
			propertyName	= readHandleIdentifier(stack);

			result = sBrowserFuncs->getproperty(instance, obj, propertyName, &resultVariant);

			if(result)
				writeVariant(resultVariant);

			writeInt32( (bool)result );
			returnCommand();
			break;

		case FUNCTION_NPN_RELEASEOBJECT:
			sBrowserFuncs->releaseobject(readHandleObj(stack));
			returnCommand();
			break;

		case FUNCTION_NPN_RETAINOBJECT:
			sBrowserFuncs->retainobject(readHandleObj(stack));
			returnCommand();
			break;		


		case FUNCTION_NPN_Evaluate:
			instance 	= readHandleInstance(stack);
			obj 		= readHandleObj(stack);	

			readNPString(stack, script);

			output << "NPN_Evaluate Server" << std::endl;
			//output << std::string(script.UTF8Characters, script.UTF8Length) << std::endl;


			result = sBrowserFuncs->evaluate(instance, obj, &script, &resultVariant);	
			
			freeNPString(script);

			if(result)
				writeVariant(resultVariant);

			writeInt32( (bool)result );
			returnCommand();
			break;
	
		case FUNCTION_NPN_INVOKE:
			handle_NPN_Invoke(stack);
			break;

		/*case HANDLE_MANAGER_DELETE:
			type 	= readInt32(stack);
			id 		= readInt64(stack);

			handlemanager.removeHandleByID(id);

			returnCommand();
			break;*/

		case HANDLE_MANAGER_REQUEST_STREAM_INFO:
			sendStreamInfo(stack);
			break;

		case FUNCTION_NPN_GET_URL_NOTIFY:
			handle_NPN_GetURLNotify(stack);
			break;


		case FUNCTION_NPN_STATUS:
			handle_NPN_Status(stack);
			break;

		case FUNCTION_NPN_UTF8_FROM_IDENTIFIER:
			handle_NPN_UTF8FromIdentifier(stack);
			break;

		case FUNCTION_NPN_IDENTIFIER_IS_STRING:
			handle_NPN_IdentifierIsString(stack);
			break;

		case FUNCTION_NPN_INT_FROM_IDENTIFIER:
			handle_NPN_IntFromIdentifier(stack);
			break;		


		case FUNCTION_NPN_GET_PLUGINELEMENTNPOBJECT:
			error = sBrowserFuncs->getvalue(readHandleInstance(stack), NPNVPluginElementNPObject, &obj);
			writeHandle(obj);
			writeInt32(error);
			returnCommand();
			break;

		default:
			throw std::runtime_error("WTF? Which Function?");
			break;
	}

	//output << "Function returned" << std::endl;
}