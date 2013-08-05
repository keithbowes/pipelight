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




void dispatcher(int functionid, Stack &stack){
	switch(functionid){
		
		// OBJECT_KILL not implemented

		case HANDLE_MANAGER_REQUEST_STREAM_INFO:
			{
				NPStream* stream = readHandleStream(stack); // shouldExist not necessary, Linux checks always

				writeString(stream->headers);
				writeHandleNotify(stream->notifyData);
				writeInt32(stream->lastmodified);
				writeInt32(stream->end);
				writeString(stream->url);
				returnCommand();
			}
			break;

		// PROCESS_WINDOW_EVENTS not implemented

		// Plugin specific commands (_GET_, _NP_ and _NPP_) not implemented

		case FUNCTION_NPN_CREATE_OBJECT: // Verified, everything okay
			{
				NPObject* obj = sBrowserFuncs->createobject(readHandleInstance(stack), &myClass);

				writeHandleObj(obj); // refcounter is hopefully 1
				returnCommand();
			}
			break;


		case FUNCTION_NPN_GETVALUE_BOOL: // Verified, everything okay
			{
				NPP instance 			= readHandleInstance(stack);
				NPNVariable variable 	= (NPNVariable)readInt32(stack);

				NPBool resultBool;
				NPError result = sBrowserFuncs->getvalue(instance, variable, &resultBool);

				if(result == NPERR_NO_ERROR)
					writeInt32(resultBool);

				writeInt32(result);
				returnCommand();
			}
			break;


		case FUNCTION_NPN_GETVALUE_OBJECT: // Verified, everything okay
			{
				NPP instance 			= readHandleInstance(stack);
				NPNVariable variable 	= (NPNVariable)readInt32(stack);

				NPObject* obj = NULL;
				NPError result = sBrowserFuncs->getvalue(instance, variable, &obj);

				if(result == NPERR_NO_ERROR)
					writeHandleObj(obj); // Refcount was already incremented by getValue

				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_RELEASEOBJECT: // Verified, everything okay
			{
				NPObject* obj 		= readHandleObj(stack);
				//bool killObject 	= readInt32(stack);

				// TODO: Comment this out in the final version - acessing the referenceCount variable directly is not a very nice way ;-)
				/*if(obj->referenceCount == 1 && !killObject){

					writeHandleObj(obj);
					callFunction(OBJECT_IS_CUSTOM);

					if( !(bool)readResultInt32() ){
						throw std::runtime_error("Forgot to set killObject?");
					}
					
				} */

				sBrowserFuncs->releaseobject(obj);

				/*if(killObject){
					handlemanager.removeHandleByReal((uint64_t)obj, TYPE_NPObject);
				}*/

				returnCommand();
			}
			break;

		case FUNCTION_NPN_RETAINOBJECT: // Verified, everything okay
			{
				NPObject* obj = readHandleObj(stack);

				sBrowserFuncs->retainobject(obj);

				returnCommand();
			}
			break;


		case FUNCTION_NPN_EVALUATE: // Verified, everything okay
			{
				NPString script;

				NPP instance 		= readHandleInstance(stack);
				NPObject* obj 		= readHandleObj(stack);	
				readNPString(stack, script);

				// Reset variant type
				NPVariant resultVariant;
				resultVariant.type = NPVariantType_Null;

				bool result = sBrowserFuncs->evaluate(instance, obj, &script, &resultVariant);	
				
				// Free the string
				freeNPString(script);

				if(result)
					writeVariantRelease(resultVariant);

				writeInt32( result );
				returnCommand();
			}
			break;
	
		case FUNCTION_NPN_INVOKE:
			{
				NPP instance 					= readHandleInstance(stack);
				NPObject* obj 					= readHandleObj(stack);
				NPIdentifier identifier			= readHandleIdentifier(stack);
				int32_t argCount				= readInt32(stack);
				std::vector<NPVariant> args 	= readVariantArray(stack, argCount);
				// refcount is not incremented here!

				NPVariant resultVariant;
				resultVariant.type = NPVariantType_Null;
				
				bool result = sBrowserFuncs->invoke(instance, obj, identifier, args.data(), argCount, &resultVariant);

				// Free the variant array
				freeVariantArray(args);

				if(result)
					writeVariantRelease(resultVariant);

				writeInt32( result );
				returnCommand();	

			}
			break;

		case FUNCTION_NPN_INVOKE_DEFAULT: // UNTESTED!
			{
				NPP instance 					= readHandleInstance(stack);
				NPObject* obj 					= readHandleObj(stack);
				int32_t argCount				= readInt32(stack);
				std::vector<NPVariant> args 	= readVariantArray(stack, argCount);
				// refcount is not incremented here!

				NPVariant resultVariant;
				resultVariant.type = NPVariantType_Null;
				
				bool result = sBrowserFuncs->invokeDefault(instance, obj, args.data(), argCount, &resultVariant);

				// Free the variant array
				freeVariantArray(args);

				if(result)
					writeVariantRelease(resultVariant);

				writeInt32( result );
				returnCommand();	

			}
			break;

		case FUNCTION_NPN_HAS_PROPERTY: // UNTESTED!
			{
				NPP instance 					= readHandleInstance(stack);
				NPObject* obj 					= readHandleObj(stack);
				NPIdentifier identifier			= readHandleIdentifier(stack);

				bool result = sBrowserFuncs->hasproperty(instance, obj, identifier);

				writeInt32(result);
				returnCommand();	
			}
			break;

		case FUNCTION_NPN_HAS_METHOD: // UNTESTED!
			{
				NPP instance 					= readHandleInstance(stack);
				NPObject* obj 					= readHandleObj(stack);
				NPIdentifier identifier			= readHandleIdentifier(stack);

				bool result = sBrowserFuncs->hasmethod(instance, obj, identifier);

				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_GET_PROPERTY: // Verified, everything okay
			{
				NPP instance 				= readHandleInstance(stack);
				NPObject*  obj 				= readHandleObj(stack);
				NPIdentifier propertyName	= readHandleIdentifier(stack);

				NPVariant resultVariant;
				resultVariant.type = NPVariantType_Null;

				bool result = sBrowserFuncs->getproperty(instance, obj, propertyName, &resultVariant);

				if(result)
					writeVariantRelease(resultVariant); // free variant (except contained objects)

				writeInt32( result );
				returnCommand();
			}
			break;

		case FUNCTION_NPN_SET_PROPERTY: // UNTESTED!
			{
				NPP instance 					= readHandleInstance(stack);
				NPObject* obj 					= readHandleObj(stack);
				NPIdentifier identifier			= readHandleIdentifier(stack);

				NPVariant value;
				readVariant(stack, value);

				bool result = sBrowserFuncs->setproperty(instance, obj, identifier, &value);

				freeVariant(value);

				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_REMOVE_PROPERTY: // UNTESTED!
			{
				NPP instance 					= readHandleInstance(stack);
				NPObject* obj 					= readHandleObj(stack);
				NPIdentifier identifier			= readHandleIdentifier(stack);

				bool result = sBrowserFuncs->removeproperty(instance, obj, identifier);

				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_ENUMERATE: // UNTESTED!
			{
				NPP instance 					= readHandleInstance(stack);
				NPObject 		*obj 			= readHandleObj(stack);

				NPIdentifier*   identifierTable  = NULL;
				uint32_t 		identifierCount  = 0;

				bool result = sBrowserFuncs->enumerate(instance, obj, &identifierTable, &identifierCount);

				if(result){
					writeIdentifierArray(identifierTable, identifierCount);

					// Free the memory for the table
					if(identifierTable)
						sBrowserFuncs->memfree(identifierTable);
				}

				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_SET_EXCEPTION: // UNTESTED!
			{
				NPObject* obj 					= readHandleObj(stack);
				std::shared_ptr<char> message 	= readStringAsMemory(stack);

				sBrowserFuncs->setexception(obj, message.get());

				returnCommand();
			}
			break;

		case FUNCTION_NPN_GET_URL_NOTIFY:
			{
				NPP instance 					= readHandleInstance(stack);
				std::shared_ptr<char> url 		= readStringAsMemory(stack);
				std::shared_ptr<char> target 	= readStringAsMemory(stack);
				void* notifyData 				= readHandleNotify(stack);

				NPError result = sBrowserFuncs->geturlnotify(instance, url.get(), target.get(), notifyData);

				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_POST_URL_NOTIFY:
			{
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
			break;

		case FUNCTION_NPN_GET_URL:
			{
				NPP instance 					= readHandleInstance(stack);
				std::shared_ptr<char> url 		= readStringAsMemory(stack);
				std::shared_ptr<char> target 	= readStringAsMemory(stack);

				NPError result = sBrowserFuncs->geturl(instance, url.get(), target.get());

				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_POST_URL:
			{
				NPP instance 					= readHandleInstance(stack);
				std::shared_ptr<char> url 		= readStringAsMemory(stack);
				std::shared_ptr<char> target 	= readStringAsMemory(stack);

				size_t len;
				std::shared_ptr<char> buffer	= readMemory(stack, len);
				bool file 						= (bool)readInt32(stack);

				NPError result = sBrowserFuncs->posturl(instance, url.get(), target.get(), len, buffer.get(), file);

				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_REQUEST_READ: // UNTESTED!
			{
				NPStream *stream 				= readHandleStream(stack);
				uint32_t rangeCount				= readInt32(stack);

				// TODO: Verify that this is correct!
				std::vector<NPByteRange> rangeVector;

				for(int i = 0; i < rangeCount; i++){
					NPByteRange range;
					range.offset = readInt32(stack);
					range.length = readInt32(stack);
					range.next   = NULL;
					rangeVector.push_back(range);
				}

				// The last element is the latest one, we have to create the links between them...
				std::vector<NPByteRange>::reverse_iterator lastObject = rangeVector.rend();
				NPByteRange* rangeList = NULL;

				for(std::vector<NPByteRange>::reverse_iterator it = rangeVector.rbegin(); it != rangeVector.rend(); it++){
					if(lastObject != rangeVector.rend()){
						lastObject->next = &(*it);
					}else{
						rangeList        = &(*it);
					}
					lastObject = it;
				}

				NPError result = sBrowserFuncs->requestread(stream, rangeList);

				// As soon as the vector is deallocated everything else is gone, too

				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_WRITE:
			{
				size_t len;

				NPP instance 					= readHandleInstance(stack);
				NPStream *stream 				= readHandleStream(stack);
				std::shared_ptr<char> buffer	= readMemory(stack, len);	

				int32_t result = sBrowserFuncs->write(instance, stream, len, buffer.get());
				
				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_NEW_STREAM:
			{
				NPP instance 					= readHandleInstance(stack);
				std::shared_ptr<char> type 		= readStringAsMemory(stack);
				std::shared_ptr<char> target 	= readStringAsMemory(stack);

				NPStream* stream = NULL;
				NPError result = sBrowserFuncs->newstream(instance, type.get(), target.get(), &stream);

				if(result == NPERR_NO_ERROR)
					writeHandleStream(stream);

				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_DESTROY_STREAM:
			{
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
			break;		

		case FUNCTION_NPN_STATUS:
			{		
				NPP instance 					= readHandleInstance(stack);
				std::shared_ptr<char> message	= readStringAsMemory(stack);

				sBrowserFuncs->status(instance, message.get());
				returnCommand();
			}
			break;

		case FUNCTION_NPN_USERAGENT: // Verified, everything okay
			{
				writeString( sBrowserFuncs->uagent(readHandleInstance(stack)) );
				returnCommand();
			}
			break;

		case FUNCTION_NPN_IDENTIFIER_IS_STRING:
			{
				NPIdentifier identifier = readHandleIdentifier(stack);
				bool result = sBrowserFuncs->identifierisstring(identifier);

				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_UTF8_FROM_IDENTIFIER:
			{
				NPIdentifier identifier	= readHandleIdentifier(stack);
				NPUTF8 *str = sBrowserFuncs->utf8fromidentifier(identifier);

				writeString((char*) str);

				// Free the string
				if(str)
					sBrowserFuncs->memfree(str);

				returnCommand();
			}
			break;


		case FUNCTION_NPN_INT_FROM_IDENTIFIER:
			{
				NPIdentifier identifier = readHandleIdentifier(stack);
				int32_t result = sBrowserFuncs->intfromidentifier(identifier);
				
				writeInt32(result);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_GET_STRINGIDENTIFIER: // Verified, everything okay
			{
				std::shared_ptr<char> utf8name 	= readStringAsMemory(stack);
				NPIdentifier identifier 		= sBrowserFuncs->getstringidentifier((NPUTF8*) utf8name.get());

				writeHandleIdentifier(identifier);
				returnCommand();
			}
			break;

		case FUNCTION_NPN_GET_INTIDENTIFIER:
			{
				int32_t intid 					= readInt32(stack);
				NPIdentifier identifier 		= sBrowserFuncs->getintidentifier(intid);

				writeHandleIdentifier(identifier);
				returnCommand();
			}
			break;


		default:
			throw std::runtime_error("Specified function not found!");
			break;
	}
}