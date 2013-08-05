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

// Verified, everything okay
void handle_NPN_Invoke(Stack &stack){

	output << "handle_NPN_Invoke" << std::endl;


	NPP instance 					= readHandleInstance(stack);
	NPObject* obj 					= readHandleObj(stack);
	NPIdentifier identifier			= readHandleIdentifier(stack);
	int32_t argCount				= readInt32(stack);
	std::vector<NPVariant> args 	= readVariantArray(stack, argCount);
	// refcount is not incremented here!

	NPVariant resultVariant;
	resultVariant.type = NPVariantType_Null;
	
	output << "browser side: before invoke" << std::endl;
	output << "argCount = " << argCount << std::endl;

	bool result = sBrowserFuncs->invoke(instance, obj, identifier, args.data(), argCount, &resultVariant);


	output << "browser side: after invoke" << std::endl;

	freeVariantArray(args);

	if(result)
		writeVariantRelease(resultVariant);

	writeInt32(result);
	returnCommand();	
}

// Verified, everything okay
void sendStreamInfo(Stack &stack){

	NPStream* stream = readHandleStream(stack);

	writeString(stream->headers);
	writeHandleNotify(stream->notifyData);
	writeInt32(stream->lastmodified);
	writeInt32(stream->end);
	writeString(stream->url);

	output << "Sending stream with url" << stream->url << std::endl;

	returnCommand();

}

// Verified, everything okay
void handle_NPN_GetURLNotify(Stack &stack){
	NPP instance 					= readHandleInstance(stack);
	std::shared_ptr<char> url 		= readStringAsMemory(stack);
	std::shared_ptr<char> target 	= readStringAsMemory(stack);
	void* notifyData 				= readHandleNotify(stack);

	NPError result = sBrowserFuncs->geturlnotify(instance, url.get(), target.get(), notifyData);

	writeInt32(result);
	returnCommand();
}

void handle_NPN_PostURLNotify(Stack &stack){

	NPP instance 					= readHandleInstance(stack);
	std::shared_ptr<char> url 		= readStringAsMemory(stack);
	std::shared_ptr<char> target 	= readStringAsMemory(stack);

	size_t len;
	std::shared_ptr<char> buffer	= readMemory(stack, len);
	bool file 						= (bool)readInt32(stack);
	void* notifyData 				= readHandleNotify(stack);

	NPError result = sBrowserFuncs->posturlnotify(instance, url.get(), target.get(), len, buffer.get(), file, notifyData);

	writeInt32(result);
	returnCommand();

}

// Verified, everything okay
void handle_NPN_Status(Stack &stack){

	NPP instance 					= readHandleInstance(stack);
	std::shared_ptr<char> message	= readStringAsMemory(stack);

	sBrowserFuncs->status(instance, message.get());
	returnCommand();

}

// Verified, everything okay
void handle_NPN_UTF8FromIdentifier(Stack &stack){

	output << "handle_NPN_UTF8FromIdentifier" << std::endl;

	NPIdentifier identifier	= readHandleIdentifier(stack);
	NPUTF8 *str = sBrowserFuncs->utf8fromidentifier(identifier);

	writeString((char*) str);

	// Free the string
	if(str)
		sBrowserFuncs->memfree(str);

	returnCommand();
}

// Verified, everything okay
void handle_NPN_IdentifierIsString(Stack &stack){

	output << "handle_NPN_IdentifierIsString" << std::endl;

	NPIdentifier identifier = readHandleIdentifier(stack);
	bool result = sBrowserFuncs->identifierisstring(identifier);

	writeInt32(result);
	returnCommand();

}

// Verified, everything okay
void handle_NPN_IntFromIdentifier(Stack &stack){

	NPIdentifier identifier = readHandleIdentifier(stack);
	int32_t result = sBrowserFuncs->intfromidentifier(identifier);
	
	writeInt32(result);
	returnCommand();

}


void handle_NPN_Write(Stack &stack){

	size_t len;

	NPP instance 					= readHandleInstance(stack);
	NPStream *stream 				= readHandleStream(stack);
	std::shared_ptr<char> buffer	= readMemory(stack, len);	

	int32_t result = sBrowserFuncs->write(instance, stream, len, buffer.get());
	
	writeInt32(result);
	returnCommand();

}

void handle_NPN_DestroyStream(Stack &stack){

	NPP instance 		= readHandleInstance(stack);
	NPStream *stream 	= readHandleStream(stack);
	NPReason reason 	= (NPReason) readInt32(stack);

	NPError result = sBrowserFuncs->destroystream(instance, stream, reason);
	
	// Let the handlemanager remove this one
	// TODO: Is this necessary?
	//handlemanager.removeHandleByReal((uint64_t)stream, TYPE_NPStream);

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

	NPBool resultBool;
	NPError error;

	int64_t id;
	int32_t type;

	bool killObject;

	//output << "dispatching function " << functionid << std::endl;


	switch(functionid){
		
		case FUNCTION_NPN_CREATE_OBJECT: // Verified, everything okay
			output << "FUNCTION_NPN_CREATE_OBJECT" << std::endl;

			obj = sBrowserFuncs->createobject(readHandleInstance(stack), &myClass);

			output << "FUNCTION_NPN_CREATE_OBJECT ready with obj " << (void*)obj << std::endl;

			writeHandle(obj); // refcounter is hopefully 1
			returnCommand();

			break;

		case FUNCTION_NPN_GET_WINDOWNPOBJECT: // Verified, everything okay
			error = sBrowserFuncs->getvalue(readHandleInstance(stack), NPNVWindowNPObject, &obj);

			output << "FUNCTION_NPN_GET_WINDOWNPOBJECT returned " << obj << " and error " << error << std::endl; 
			output << obj->referenceCount << std::endl;

			if(error == NPERR_NO_ERROR)
				writeHandle(obj); // Refcount was already incremented by getValue

			writeInt32(error);
			returnCommand();
			break;	
		
		case FUNCTION_NPN_GET_PRIVATEMODE: // Verified, everything okay
			error = sBrowserFuncs->getvalue(readHandleInstance(stack), NPNVprivateModeBool, &resultBool);

			if(error == NPERR_NO_ERROR)
				writeInt32(resultBool);

			writeInt32(error);
			returnCommand();
			break;	

		case FUNCTION_NPP_GET_STRINGIDENTIFIER: // Verified, everything okay

			utf8name 	= readStringAsMemory(stack);	
			identifier 	= sBrowserFuncs->getstringidentifier((NPUTF8*) utf8name.get());
			writeHandle(identifier);
			returnCommand();
			break;

		case FUNCTION_NPN_GET_PROPERTY: // Verified, everything okay

			instance 		= readHandleInstance(stack);
			obj 			= readHandleObj(stack);
			propertyName	= readHandleIdentifier(stack);

			// Reset variant type
			resultVariant.type = NPVariantType_Null;

			result = sBrowserFuncs->getproperty(instance, obj, propertyName, &resultVariant);

			output << "NPN_GET_PROPERTY Server result " << result << std::endl;

			if(result)
				writeVariantRelease(resultVariant); // free variant (except contained objects)

			writeInt32( result );
			returnCommand();
			break;

		case FUNCTION_NPN_RELEASEOBJECT: // Verified, everything okay
			obj 		= readHandleObj(stack);
			killObject 	= readInt32(stack);

			output << "RELEASE " << obj << std::endl;
			output << obj->referenceCount << std::endl;

			if(killObject){

				output << "Killed " << (void*) obj << std::endl;
			
				// Remove it in the handle manager
				handlemanager.removeHandleByReal((uint64_t)obj, TYPE_NPObject);

			}

			sBrowserFuncs->releaseobject(obj);

			returnCommand();
			break;

		case FUNCTION_NPN_RETAINOBJECT: // Verified, everything okay
			obj = readHandleObj(stack);


			output << "RETAIN " << obj << std::endl;
			output << obj->referenceCount << std::endl;

			sBrowserFuncs->retainobject(obj);

			output << "after: " << obj->referenceCount << std::endl;

			returnCommand();
			break;


		case FUNCTION_NPN_Evaluate: // Verified, everything okay
			output << "NPN_Evaluate Server" << std::endl;

			instance 	= readHandleInstance(stack);
			obj 		= readHandleObj(stack);	
			readNPString(stack, script);

			output << "--- SCRIPT ---" << std::endl;
			output << script.UTF8Characters << std::endl;
			output << "--- /SCRIPT ---" << std::endl;

			//output << std::string(script.UTF8Characters, script.UTF8Length) << std::endl;

			// Reset variant type
			resultVariant.type = NPVariantType_Null;

			result = sBrowserFuncs->evaluate(instance, obj, &script, &resultVariant);	
			
			output << "NPN_Evaluate Server result " << result << std::endl;

			// Free the string
			freeNPString(script);

			if(result)
				writeVariantRelease(resultVariant);

			writeInt32( result );
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

		case FUNCTION_NPN_POST_URL_NOTIFY:
			handle_NPN_PostURLNotify(stack);
			break;

		case FUNCTION_NPN_STATUS:
			handle_NPN_Status(stack);
			break;


		case FUNCTION_NPN_WRITE:
			handle_NPN_Write(stack);
			break;		

		case FUNCTION_NPN_DESTROY_STREAM:
			handle_NPN_DestroyStream(stack);
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


		case FUNCTION_NPN_GET_PLUGINELEMENTNPOBJECT: // Verified, everything okay
			error = sBrowserFuncs->getvalue(readHandleInstance(stack), NPNVPluginElementNPObject, &obj);

			if(error == NPERR_NO_ERROR)
				writeHandle(obj);
			
			writeInt32(error);
			returnCommand();
			break;

		case FUNCTION_NPN_USERAGENT: // Verified, everything okay
			writeString( sBrowserFuncs->uagent(readHandleInstance(stack)) );
			returnCommand();
			break;

		/*case OBJECT_KILL: // Verified, everything okay
			obj = readHandleObj(stack);

			output << "Killed " << (void*) obj << std::endl;
			
			// Remove it in the handle manager
			handlemanager.removeHandleByReal((uint64_t)obj, TYPE_NPObject);

			returnCommand();
			break;*/

		default:
			throw std::runtime_error("WTF? Which Function?");
			break;
	}

	//output << "Function returned" << std::endl;
}