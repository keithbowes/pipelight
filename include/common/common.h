#ifndef Common_h_
#define Common_h_

#include <memory>								/* for std::shared_ptr */
#include <string>								/* for std::string */
#include <vector>								/* for std::vector */
#include <stdio.h>								/* for fprintf */
#include <string.h>								/* for strlen */
#include <stdlib.h>								/* for getenv, exit, ... */
#include <stdint.h>								// for (u)intptr_t

#if defined(PLUGINLOADER) && !defined(__WINE__)
	#include <io.h>
#endif

#if defined(__WIN32__)
	#define XP_WIN 1
#elif defined(__linux__) || defined (__FreeBSD__)
	#define XP_UNIX 1
	#define MOZ_X11 1
#elif defined(__APPLE__)
	#define XP_UNIX 1
	#define XP_MACOSX 1
#else
	#error "Could not detect your operating system!"
#endif

#include "../npapi-headers/nptypes.h"
#include "../npapi-headers/npapi.h"
#include "../npapi-headers/npfunctions.h"
#include "../npapi-headers/npruntime.h"

#if defined(__WINE__)
	#include <sys/stat.h>						/* for stat */

#elif defined(PLUGINLOADER)
	#include <windows.h>						/* for GetFileAttributes */

#else
	#include <sys/stat.h>						/* for stat */
	#include <pthread.h>
	#include <semaphore.h>

	extern NPNetscapeFuncs *sBrowserFuncs;
#endif

/* init */
#ifdef PLUGINLOADER

#define INIT_EARLY  /* nothing */

#else

#define INIT_EARLY	__attribute__((init_priority(101)))
#define CONSTRUCTOR __attribute__((constructor(102)))
#define DESTRUCTOR	__attribute__((destructor))

#endif

/* debug */
#ifdef PLUGINLOADER
	#define PIPELIGHT_DEBUG_MSG "PIPELIGHT:WIN"
#else
	#define PIPELIGHT_DEBUG_MSG "PIPELIGHT:LIN"
#endif

/* #define PIPELIGHT_DEBUG */

#ifdef PLUGINLOADER
	NPIdentifier NP_LOADDS NPN_GetIntIdentifier(int32_t intid);
	NPIdentifier NP_LOADDS NPN_GetStringIdentifier(const NPUTF8* name);
#endif

class Context;
extern Context *ctx;
extern char strMultiPluginName[64];

#ifdef PLUGINLOADER
	extern DWORD mainThreadID;
#endif

#ifdef PIPELIGHT_DEBUG

	#if !defined(PIPELIGHT_DBGSYNC)

		#define DBG_TRACE(fmt, ...) \
			do{ fprintf(stderr, "[" PIPELIGHT_DEBUG_MSG ":%s] %s:%d:%s(): " fmt "\n", strMultiPluginName, __FILE__, __LINE__, __func__, ##__VA_ARGS__); }while(0)

	#elif PIPELIGHT_DBGSYNC == 1

		#define DBG_TRACE(fmt, ...) \
			do{ \
				char __buffer[4096]; \
				int  __res = snprintf(__buffer, sizeof(__buffer), "[" PIPELIGHT_DEBUG_MSG ":%s] %s:%d:%s(): " fmt "\n", strMultiPluginName, __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
				if (__res >= 0 && __res <= (signed)sizeof(__buffer)){ \
					if (__res == (signed)sizeof(__buffer)){ \
						__buffer[ sizeof(__buffer) - 2 ] = '$'; \
						__buffer[ sizeof(__buffer) - 1 ] = '\n'; \
					} \
					fwrite(__buffer, sizeof(char), __res, stderr); \
				} \
			}while(0)

	#endif

	#define DBG_INFO \
		DBG_TRACE

	#define DBG_WARN \
		DBG_TRACE

	#define DBG_ERROR \
		DBG_TRACE

#else

	#define DBG_TRACE(fmt, ...) \
		do{ }while(0)

	#define DBG_INFO(fmt, ...) \
		do{ fprintf(stderr, "[" PIPELIGHT_DEBUG_MSG ":%s] " fmt "\n", strMultiPluginName, ##__VA_ARGS__); }while(0)

	#define DBG_WARN \
		DBG_INFO

	#define DBG_ERROR(fmt, ...) \
		do{ fprintf(stderr, "[" PIPELIGHT_DEBUG_MSG ":%s] %s:%d:%s(): " fmt "\n", strMultiPluginName, __FILE__, __LINE__, __func__, ##__VA_ARGS__); }while(0)

#endif

#define DBG_ABORT(fmt, ...) \
	do{ \
		DBG_ERROR(fmt, ##__VA_ARGS__); \
		exit(1); \
	}while(0)

#define DBG_ASSERT(res, fmt, ...) \
	do{ if (!(res)) DBG_ABORT(fmt, ##__VA_ARGS__); }while(0)

#define NOTIMPLEMENTED(fmt, ...) \
	DBG_ERROR("STUB! " fmt, ##__VA_ARGS__)

#ifdef PLUGINLOADER
	#if defined(PIPELIGHT_DEBUG) && !defined(__WINE__)

		#define DBG_CHECKTHREAD() \
			DBG_ASSERT( GetCurrentThreadId() == mainThreadID, "NPAPI command called from wrong thread!" )

	#else

		#define DBG_CHECKTHREAD() \
			do{ }while(0)

	#endif
#endif

/* common.c */

typedef enum { PR_FALSE = 0, PR_TRUE = 1 } PRBool;

#define REFCOUNT_CUSTOM			0x80000000
#define REFCOUNT_MASK			0x7fffffff

#define HMGR_HANDLE				uint32_t
#define writeHandleId			writeInt32
#define readHandleId			readInt32

enum IDENT_TYPE{
	IDENT_TYPE_Integer = 0,
	IDENT_TYPE_String
};

#ifdef PLUGINLOADER

struct NPIdentifierDescription{
	IDENT_TYPE  type;
	union {
		char    *name;
		int32_t intid;
	} value;
};

#else

struct NotifyDataRefCount{
	uint32_t referenceCount;
};

#endif

enum HMGR_TYPE{
	HMGR_TYPE_NPObject		= 0,
	HMGR_TYPE_NPPInstance	= 2,
	HMGR_TYPE_NPStream,
	HMGR_TYPE_NotifyData,
	HMGR_NUMTYPES
};

enum HMGR_EXISTS{
	HMGR_SHOULD_NOT_EXIST	= -1,
	HMGR_CAN_EXIST			= 0,
	HMGR_SHOULD_EXIST		= 1
};

class ParameterInfo{
	public:
		char command;
		std::shared_ptr<char> data;
		size_t length;

		ParameterInfo(char command, char *newdata, size_t length);
		~ParameterInfo();
};

typedef std::vector<ParameterInfo> Stack;

/* increase this whenever you do changes in the protocol stack */
#define PIPELIGHT_PROTOCOL_VERSION 0x10000013

enum{
	/* ------- Special ------- */

	/* Check if Init was okay */
	INIT_OKAY = 1,

	/* Handlemanager on the linux side */
	LIN_HANDLE_MANAGER_REQUEST_STREAM_INFO,
	LIN_HANDLE_MANAGER_FREE_OBJECT_ASYNC,

	/* Additional commands on the linux side */
	CHANGE_EMBEDDED_MODE,

	/* Additional commands on the windows side */
	SILVERLIGHT_IS_GRAPHIC_DRIVER_SUPPORTED,

	/* Handlemanager on the windows side */
	WIN_HANDLE_MANAGER_FREE_NOTIFY_DATA_ASYNC,
	WIN_HANDLE_MANAGER_FREE_OBJECT,

	/* Additional commands on the windows side */
	PROCESS_WINDOW_EVENTS,
	WINDOWLESS_EVENT_PAINT,
	WINDOWLESS_EVENT_MOUSEMOVE,
	WINDOWLESS_EVENT_MOUSEBUTTON,
	WINDOWLESS_EVENT_KEYBOARD,

	/* ------- Plugin ------- */
	FUNCTION_GET_PLUGIN_INFO,

	FUNCTION_NP_INVOKE,
	FUNCTION_NP_INVOKE_DEFAULT,
	FUNCTION_NP_HAS_PROPERTY,
	FUNCTION_NP_HAS_METHOD,
	FUNCTION_NP_GET_PROPERTY,
	FUNCTION_NP_SET_PROPERTY,
	FUNCTION_NP_REMOVE_PROPERTY,
	FUNCTION_NP_ENUMERATE,
	FUNCTION_NP_INVALIDATE,

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
	FUNCTION_NPP_STREAM_AS_FILE,

	NP_SHUTDOWN,

	/* ------- Browser ------- */
	FUNCTION_NPN_CREATE_OBJECT,

	FUNCTION_NPN_GETVALUE_BOOL,
	FUNCTION_NPN_GETVALUE_OBJECT,
	FUNCTION_NPN_GETVALUE_STRING,

	FUNCTION_NPN_RELEASEOBJECT,
	FUNCTION_NPN_RELEASEOBJECT_ASYNC,
	FUNCTION_NPN_RETAINOBJECT,
	FUNCTION_NPN_RETAINOBJECT_ASYNC,
	FUNCTION_NPN_EVALUATE,

	FUNCTION_NPN_INVOKE,
	FUNCTION_NPN_INVOKE_DEFAULT,
	FUNCTION_NPN_HAS_PROPERTY,
	FUNCTION_NPN_HAS_METHOD,
	FUNCTION_NPN_GET_PROPERTY,
	FUNCTION_NPN_SET_PROPERTY,
	FUNCTION_NPN_REMOVE_PROPERTY,
	FUNCTION_NPN_ENUMERATE,

	FUNCTION_NPN_SET_EXCEPTION,
	FUNCTION_NPN_SET_EXCEPTION_ASYNC,

	FUNCTION_NPN_GET_URL_NOTIFY,
	FUNCTION_NPN_POST_URL_NOTIFY,
	FUNCTION_NPN_GET_URL,
	FUNCTION_NPN_POST_URL,
	FUNCTION_NPN_REQUEST_READ,
	FUNCTION_NPN_WRITE,
	FUNCTION_NPN_NEW_STREAM,
	FUNCTION_NPN_DESTROY_STREAM,

	FUNCTION_NPN_STATUS,
	FUNCTION_NPN_STATUS_ASYNC,
	FUNCTION_NPN_USERAGENT,

	FUNCTION_NPN_PUSH_POPUPS_ENABLED_STATE,
	FUNCTION_NPN_PUSH_POPUPS_ENABLED_STATE_ASYNC,
	FUNCTION_NPN_POP_POPUPS_ENABLED_STATE,
	FUNCTION_NPN_POP_POPUPS_ENABLED_STATE_ASYNC

};

enum{
	BLOCKCMD_CALL_DIRECT = 0,
	BLOCKCMD_RETURN,

	BLOCKCMD_PUSH_INT32,
	BLOCKCMD_PUSH_INT64,
	BLOCKCMD_PUSH_DOUBLE,
	BLOCKCMD_PUSH_STRING,
	BLOCKCMD_PUSH_MEMORY,

	BLOCKCMD_PUSH_POINT,
	BLOCKCMD_PUSH_RECT
};

/* last command which requires flushing stdout */
#define BLOCKCMD_FLUSH_REQUIRED BLOCKCMD_RETURN

enum{
	INVALIDATE_NOTHING = 0,
	INVALIDATE_EVERYTHING,
	INVALIDATE_RECT,
};

#ifndef PLUGINLOADER
	struct POINT{
		uint32_t x;
		uint32_t y;
	};

	struct RECT{
		uint32_t left;
		uint32_t top;
		uint32_t right;
		uint32_t bottom;
	};
#endif

struct RECT2{
	int32_t x;
	int32_t y;
	uint32_t width;
	uint32_t height;
};

#define MAKEDWORD(low, high) (((uint32_t)(low) & 0xFFFF) | (((uint32_t)(high) & 0xFFFF) << 16))

#define RECT_AddOffset(rect, cx, cy) \
	do{ \
		(rect).left   += (cx); \
		(rect).top    += (cy); \
		(rect).right  += (cx); \
		(rect).bottom += (cy); \
	}while(0)

#define POINT_AddOffset(pt, cx, cy) \
	do{ \
		(pt).x += (cx); \
		(pt).y += (cy); \
	}while(0)

extern void setMultiPluginName(const std::string str);
extern void setMultiPluginName(const char *str);

extern int32_t readInt32(Stack &stack);
extern int64_t readInt64(Stack &stack);
extern double readDouble(Stack &stack);
extern std::string readString(Stack &stack);
extern std::shared_ptr<char> readStringAsMemory(Stack &stack, size_t &resultLength);
extern std::shared_ptr<char> readStringAsMemory(Stack &stack);
extern char* readStringMalloc(Stack &stack, size_t &resultLength);
extern char* readStringMalloc(Stack &stack);

#ifndef PLUGINLOADER
extern char* readStringBrowserAlloc(Stack &stack, size_t &resultLength);
extern char* readStringBrowserAlloc(Stack &stack);
#endif

extern std::shared_ptr<char> readMemory(Stack &stack, size_t &resultLength);
extern std::shared_ptr<char> readMemory(Stack &stack);
extern char* readMemoryMalloc(Stack &stack, size_t &resultLength);
extern char* readMemoryMalloc(Stack &stack);

#ifndef PLUGINLOADER
extern char* readMemoryBrowserAlloc(Stack &stack, size_t &resultLength);
extern char* readMemoryBrowserAlloc(Stack &stack);
#endif

extern void readPOINT(Stack &stack, POINT &pt);
extern void readRECT(Stack &stack, RECT &rect);
extern void readRECT2(Stack &stack, RECT2 &rect);
extern void readNPRect(Stack &stack, NPRect &rect);

extern HMGR_HANDLE handleManager_getFreeID(HMGR_TYPE type);
extern void* handleManager_idToPtr(HMGR_TYPE type, HMGR_HANDLE id, void *arg0, void *arg1, HMGR_EXISTS exists);
extern HMGR_HANDLE handleManager_ptrToId(HMGR_TYPE type, void* ptr, HMGR_EXISTS exists);
extern void handleManager_removeByPtr(HMGR_TYPE type, void *ptr);
extern bool handleManager_existsByPtr(HMGR_TYPE type, void *ptr);
extern NPP handleManager_findInstance();
extern size_t handleManager_count();
extern void handleManager_clear();
#ifdef PLUGINLOADER
extern NPIdentifier handleManager_lookupIdentifier(IDENT_TYPE type, void *value);
extern void handleManager_updateIdentifier(NPIdentifier identifier);
#endif

#ifdef PLUGINLOADER
extern void objectDecRef(NPObject *obj, bool deleteFromRemoteHandleManager = true);
extern void objectKill(NPObject *obj);
extern void freeVariantDecRef(NPVariant &variant, bool deleteFromRemoteHandleManager = true);
extern void readVariantIncRef(Stack &stack, NPVariant &variant);
#endif

#ifndef PLUGINLOADER
extern void readVariant(Stack &stack, NPVariant &variant);
extern void freeVariant(NPVariant &variant);
#endif


class Context
{
	public:
		FILE *commPipeOut;
		FILE *commPipeIn;

	#ifndef PLUGINLOADER
		char strMimeType[2048];
		char strPluginVersion[100];
		char strPluginName[256];
		char strPluginDescription[1024];

		uint32_t	eventTimerID;
		NPP			eventTimerInstance;
		pthread_t	eventThread;

		sem_t		eventThreadSemRequestAsyncCall;
		sem_t		eventThreadSemScheduledAsyncCall;
	#endif

		inline Context()
		{
			commPipeOut = NULL;
			commPipeIn  = NULL;

		#ifndef PLUGINLOADER
			strcpy(strMimeType, "");
			strcpy(strPluginVersion, "");
			strcpy(strPluginName, "");
			strcpy(strPluginDescription, "");

			eventTimerID = 0;
			eventTimerInstance = NULL;
			eventThread = 0;

			sem_init(&ctx->eventThreadSemRequestAsyncCall, 0, 0);
			sem_init(&ctx->eventThreadSemScheduledAsyncCall, 0, 0);
		#endif
		}

	#ifndef PLUGINLOADER
		void savePluginInformation();
		bool loadPluginInformation();
	#endif

		bool initCommPipes(int out, int in);
		bool initCommIO();

		bool transmitData(const char *data, size_t length);
		void receiveData(char *data, size_t length);
		bool receiveCommand(char *data, size_t length, int abortTimeout);

		bool writeCommand(uint8_t command, const char* data = NULL, size_t length = 0);
		bool __writeString(const char* data, size_t length);
		bool readCommands(Stack &stack, bool allowReturn = true, int abortTimeout = 0);

		void writeVariantReleaseDecRef(NPVariant &variant);
		void writeVariantConst(const NPVariant &variant, bool deleteFromRemoteHandleManager = false);

		void dispatcher(int function, Stack &stack);

		/* Reads an int32 */
		inline int32_t readResultInt32(){
			Stack stack;
			readCommands(stack);
			return readInt32(stack);
		}

		/* Reads an int64 */
		inline int64_t readResultInt64(){
			Stack stack;
			readCommands(stack);
			return readInt64(stack);
		}

		/* Reads a string */
		inline std::string readResultString(){
			Stack stack;
			readCommands(stack);
			return readString(stack);
		}

		/* Waits until the function returns */
		inline void readResultVoid(){
			Stack stack;
			readCommands(stack);
		}

		/* Call a function */
		inline void callFunction(uint32_t function){
			DBG_ASSERT(writeCommand(BLOCKCMD_CALL_DIRECT, (char*)&function, sizeof(uint32_t)),
				"Unable to send BLOCKCMD_CALL_DIRECT.");
		}

		/* Return from a function */
		inline void returnCommand(){
			DBG_ASSERT(writeCommand(BLOCKCMD_RETURN),
				"Unable to send BLOCKCMD_RETURN.");
		}

		/* Writes an int32 */
		inline void writeInt32(int32_t value){
			DBG_ASSERT(writeCommand(BLOCKCMD_PUSH_INT32, (char*)&value, sizeof(int32_t)),
				"Unable to send BLOCKCMD_PUSH_INT32.");
		}

		/* Writes an int64 */
		inline void writeInt64(int64_t value){
			DBG_ASSERT(writeCommand(BLOCKCMD_PUSH_INT64, (char*)&value, sizeof(int64_t)),
				"Unable to send BLOCKCMD_PUSH_INT64.");
		}

		/* Writes a double */
		inline void writeDouble(double value){
			DBG_ASSERT(writeCommand(BLOCKCMD_PUSH_DOUBLE, (char*)&value, sizeof(double)),
				"Unable to send BLOCKCMD_PUSH_DOUBLE.");
		}

		/* Writes a C++-string */
		inline void writeString(const std::string str){
			DBG_ASSERT(writeCommand(BLOCKCMD_PUSH_STRING, str.c_str(), str.length()+1),
				"Unable to send BLOCKCMD_PUSH_STRING.");
		}

		/* Writes a char* string */
		inline void writeString(const char *str){
			DBG_ASSERT(writeCommand(BLOCKCMD_PUSH_STRING, str, str ? (strlen(str)+1) : 0 ),
				"Unable to send BLOCKCMD_PUSH_STRING.");
		}

		/* Writes a string with a specific length */
		inline void writeString(const char *str, size_t length){
			DBG_ASSERT(__writeString(str, length),
				"Unable to send BLOCKCMD_PUSH_STRING.");
		}

		/* Writes a memory block (also works for NULL ptr) */
		inline void writeMemory(const char *memory, size_t length){
			DBG_ASSERT(writeCommand(BLOCKCMD_PUSH_MEMORY, memory, length),
				"Unable to send BLOCKCMD_PUSH_MEMORY.");
		}

		/* Writes a POINT */
		inline void writePOINT(const POINT &pt){
			DBG_ASSERT(writeCommand(BLOCKCMD_PUSH_POINT, (char*)&pt, sizeof(pt)),
				"Unable to send BLOCKCMD_PUSH_POINT.");
		}

		inline void writePointXY(int32_t x, int32_t y){
			POINT tmp;
			tmp.x = x;
			tmp.y = y;
			writePOINT(tmp);
		}

		/* Writes a RECT */
		inline void writeRECT(const RECT &rect){
			DBG_ASSERT(writeCommand(BLOCKCMD_PUSH_RECT, (char*)&rect, sizeof(rect)),
				"Unable to send BLOCKCMD_PUSH_RECT.");
		}

		inline void writeRECT2(const RECT2 &rect){
			RECT tmp;
			tmp.left   = rect.x;
			tmp.top    = rect.y;
			tmp.right  = rect.x + rect.width;
			tmp.bottom = rect.y + rect.height;
			writeRECT(tmp);
		}

		inline void writeNPRect(const NPRect &rect){
			RECT tmp;
			tmp.left	= rect.left;
			tmp.top		= rect.top;
			tmp.right	= rect.right;
			tmp.bottom	= rect.bottom;
			writeRECT(tmp);
		}

		inline void writeRectXYWH(int32_t x, int32_t y, uint32_t width, uint32_t height){
			RECT tmp;
			tmp.left	= x;
			tmp.top		= y;
			tmp.right	= x + width;
			tmp.bottom	= y + height;
			writeRECT(tmp);
		}

		/* Writes a handle */
		inline void writeHandle(HMGR_TYPE type, void *ptr, HMGR_EXISTS exists = HMGR_CAN_EXIST){
			writeHandleId(handleManager_ptrToId(type, ptr, exists));
			writeInt32(type);
		}

		inline void writeHandleObj(NPObject *obj, HMGR_EXISTS exists = HMGR_CAN_EXIST, bool deleteFromRemoteHandleManager = false){
			#ifndef PLUGINLOADER
				DBG_ASSERT(!deleteFromRemoteHandleManager, "deleteFromRemoteHandleManager set on Linux side.");
			#endif

			writeInt32(deleteFromRemoteHandleManager);
			writeHandle(HMGR_TYPE_NPObject, (void *)obj, exists);
		}

		inline void writeHandleIdentifier(NPIdentifier name, HMGR_EXISTS exists = HMGR_CAN_EXIST){
		#ifdef PLUGINLOADER
			NPIdentifierDescription *ident = (NPIdentifierDescription *)name;
			DBG_ASSERT(ident != NULL, "got NULL identifier.");

			switch(ident->type){
				case IDENT_TYPE_Integer:
					writeInt32(ident->value.intid);
					break;

				case IDENT_TYPE_String:
					writeString(ident->value.name);
					break;

				default:
					DBG_ABORT("unsupported identifier type.");
					break;
			}

			writeInt32(ident->type);

		#else
			if (sBrowserFuncs->identifierisstring(name)){
				NPUTF8 *str = sBrowserFuncs->utf8fromidentifier(name);
				writeString((char *)str);
				if (str) sBrowserFuncs->memfree(str);
				writeInt32(IDENT_TYPE_String);

			}else{
				writeInt32(sBrowserFuncs->intfromidentifier(name));
				writeInt32(IDENT_TYPE_Integer);
			}
		#endif
		}

		inline void writeHandleInstance(NPP instance, HMGR_EXISTS exists = HMGR_CAN_EXIST){
			writeHandle(HMGR_TYPE_NPPInstance, (void*)instance, exists);
		}

		inline void writeHandleStream(NPStream *stream, HMGR_EXISTS exists = HMGR_CAN_EXIST){
			writeHandle(HMGR_TYPE_NPStream, (void*)stream, exists);
		}

		inline void writeHandleNotify(void* notifyData, HMGR_EXISTS exists = HMGR_CAN_EXIST){
			writeHandle(HMGR_TYPE_NotifyData, notifyData, exists);
		}


	#ifdef PLUGINLOADER
		inline void writeHandleObjDecRef(NPObject *obj, HMGR_EXISTS exists = HMGR_CAN_EXIST){
			writeHandleObj(obj, exists, (obj->referenceCount == 1));
			objectDecRef(obj, false);
		}

		inline void writeVariantArrayReleaseDecRef(NPVariant *variant, size_t count){
			for (int i = count - 1; i >= 0; i--)
				writeVariantReleaseDecRef(variant[i]);
		}
	#endif

	#ifndef PLUGINLOADER
		inline void writeVariantRelease(NPVariant &variant){
			writeVariantConst(variant);

			if (variant.type == NPVariantType_Object){
				if (variant.value.objectValue)
					sBrowserFuncs->retainobject(variant.value.objectValue);
			}

			sBrowserFuncs->releasevariantvalue(&variant);
		}

		inline void writeVariantArrayRelease(NPVariant *variant, size_t count){
			for (int i = count - 1; i >= 0; i--)
				writeVariantRelease(variant[i]);
		}
	#endif

		inline void writeVariantArrayConst(const NPVariant *variant, size_t count){
			for (int i = count - 1; i >= 0; i--)
				writeVariantConst(variant[i]);
		}

		inline void writeNPString(NPString *string){
			DBG_ASSERT(string, "invalid string pointer.");
			writeString((char*)string->UTF8Characters, string->UTF8Length);
		}

		inline void writeStringArray(char* str[], int count){
			for (int i = count - 1; i >= 0; i--)
				writeString(str[i]);
		}

		inline void writeIdentifierArray(NPIdentifier *identifiers, int count){
			for (int i = count - 1; i >= 0; i--)
				writeHandleIdentifier(identifiers[i]);
		}

};

/* inline functions */

/* Reads a handle */
inline void* __readHandle(HMGR_TYPE type, Stack &stack, void *arg0 = NULL, void *arg1 = NULL, HMGR_EXISTS exists = HMGR_CAN_EXIST){
	DBG_ASSERT(readInt32(stack) == type, "wrong handle type, expected %d.", type);
	return handleManager_idToPtr(type, readHandleId(stack), arg0, arg1, exists);
}

#ifndef PLUGINLOADER

inline NPObject* readHandleObj(Stack &stack, HMGR_EXISTS exists = HMGR_CAN_EXIST){
	NPObject *obj = (NPObject*)__readHandle(HMGR_TYPE_NPObject, stack, NULL, NULL, exists);
	bool deleteFromRemoteHandleManager = (bool)readInt32(stack);
	if (deleteFromRemoteHandleManager)
		handleManager_removeByPtr(HMGR_TYPE_NPObject, (void*)obj);
	return obj;
}

#endif

inline NPIdentifier readHandleIdentifier(Stack &stack, HMGR_EXISTS exists = HMGR_CAN_EXIST){
	NPIdentifier identifier;
	int32_t type = readInt32(stack);

	switch(type){
		case IDENT_TYPE_Integer:
		#ifdef PLUGINLOADER
			identifier = NPN_GetIntIdentifier(readInt32(stack));
		#else
			identifier = sBrowserFuncs->getintidentifier(readInt32(stack));
		#endif
			break;

		case IDENT_TYPE_String:
			{
				std::shared_ptr<char> utf8name = readStringAsMemory(stack);
			#ifdef PLUGINLOADER
				identifier = NPN_GetStringIdentifier(utf8name.get());
			#else
				identifier = sBrowserFuncs->getstringidentifier(utf8name.get());
			#endif
			}
			break;

		default:
			DBG_ABORT("unsupported identifier type.");
			break;
	}

	return identifier;
}

#ifdef PLUGINLOADER
#define readHandleIdentifierCreate(stack) readHandleIdentifier(stack, HMGR_CAN_EXIST)
#endif

inline NPP readHandleInstance(Stack &stack, HMGR_EXISTS exists = HMGR_CAN_EXIST){
	return (NPP)__readHandle(HMGR_TYPE_NPPInstance, stack, NULL, NULL, exists);
}

inline NPStream* readHandleStream(Stack &stack, HMGR_EXISTS exists = HMGR_CAN_EXIST){
	return (NPStream *)__readHandle(HMGR_TYPE_NPStream, stack, NULL, NULL, exists);
}

inline void* readHandleNotify(Stack &stack, HMGR_EXISTS exists = HMGR_CAN_EXIST){
	return __readHandle(HMGR_TYPE_NotifyData, stack, NULL, NULL, exists);
}

#ifdef PLUGINLOADER

inline NPObject* readHandleObjIncRef(Stack &stack, HMGR_EXISTS exists = HMGR_CAN_EXIST){
	NPObject *obj = (NPObject *)__readHandle(HMGR_TYPE_NPObject, stack, NULL, NULL, exists);
	readInt32(stack); /* deleteFromRemoteHandleManager */
	obj->referenceCount++;
	return obj;
}

inline NPObject* readHandleObjIncRefCreate(Stack &stack, NPP instance = NULL, NPClass *cls = NULL){
	NPObject* obj = (NPObject *)__readHandle(HMGR_TYPE_NPObject, stack, instance, cls, HMGR_SHOULD_NOT_EXIST);
	readInt32(stack); /* deleteFromRemoteHandleManager */
	obj->referenceCount++;
	return obj;
}

inline std::vector<NPVariant> readVariantArrayIncRef(Stack &stack, int count){
	NPVariant variant;
	std::vector<NPVariant> result;

	for (int i = 0; i < count; i++){
		readVariantIncRef(stack, variant);
		result.push_back(variant);
	}

	return result;
}

inline void freeVariantArrayDecRef(std::vector<NPVariant> args){
	for (std::vector<NPVariant>::iterator it = args.begin(); it != args.end(); it++)
		freeVariantDecRef(*it);
}

#endif

#ifndef PLUGINLOADER

inline std::vector<NPVariant> readVariantArray(Stack &stack, int count){
	NPVariant variant;
	std::vector<NPVariant> result;

	for (int i = 0; i < count; i++){
		readVariant(stack, variant);
		result.push_back(variant);
	}

	return result;
}

inline void freeVariantArray(std::vector<NPVariant> args){
	for (std::vector<NPVariant>::iterator it = args.begin(); it != args.end(); it++)
		freeVariant(*it);
}

#endif

inline std::vector<char *> readStringArray(Stack &stack, int count){
	std::vector<char *> result;

	for (int i = 0; i < count; i++)
		result.push_back(readStringMalloc(stack));

	return result;
}

inline void freeStringArray(std::vector<char *> args){
	for (std::vector<char*>::iterator it = args.begin(); it != args.end(); it++)
		free(*it);
}

inline std::vector<NPIdentifier> readIdentifierArray(Stack &stack, int count){
	std::vector<NPIdentifier> result;

	for (int i = 0; i < count; i++)
		result.push_back(readHandleIdentifier(stack));

	return result;
}

/*
inline void writeNPBool(NPBool value){
	writeInt32(value);
}

inline NPBool readNPBool(Stack &stack){
	return (NPBool)readInt32(stack);
}
*/

#ifndef PLUGINLOADER

inline bool pluginInitOkay(){
	uint32_t function = INIT_OKAY;
	Stack stack;

	if (!ctx->writeCommand(BLOCKCMD_CALL_DIRECT, (char *)&function, sizeof(uint32_t)))
		return false;

	if (!ctx->readCommands(stack, true, 60000))
		return false;

	/* ensure that we're using the correct protocol version */
	if (readInt32(stack) != PIPELIGHT_PROTOCOL_VERSION){
		DBG_ERROR("incompatible version of pluginloader.exe");
		return false;
	}

	return true;
}

#endif

/* external */

extern NPClass proxy_class;

/* misc */

#if defined(__WINE__)

inline bool checkIsFile(const std::string path){
	struct stat fileInfo;
	return (stat(path.c_str(), &fileInfo) == 0 && S_ISREG(fileInfo.st_mode));
}

#elif defined(__WIN32__)

inline bool checkIsFile(const std::string path){
	DWORD attrib = GetFileAttributesA(path.c_str());
	return (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY));
}

#else

inline bool checkIsFile(const std::string path){
	struct stat fileInfo;
	return (stat(path.c_str(), &fileInfo) == 0 && S_ISREG(fileInfo.st_mode));
}

#endif

#ifndef PLUGINLOADER

inline bool checkIfExists(const std::string path){
	struct stat fileInfo;
	return (stat(path.c_str(), &fileInfo) == 0);
}

inline std::string getEnvironmentString(const std::string variable){
	char *str = getenv(variable.c_str());
	return str ? std::string(str) : "";
}

inline long int getEnvironmentInteger(const std::string variable, long int defaultInt = 0) {
	long int res;
	char *endp, *str;

	if ( !(str = getenv(variable.c_str())) )
		return defaultInt;

	res = strtol(str, &endp, 10);

	// Not a valid string
	if (endp == str || *endp != 0)
		return defaultInt;

	return res;
}

#endif

inline std::string trim(std::string str){
	size_t pos;

	pos = str.find_first_not_of(" \f\n\r\t\v");
	if (pos != std::string::npos)
		str = str.substr(pos, std::string::npos);

	pos = str.find_last_not_of(" \f\n\r\t\v");
	if (pos != std::string::npos)
		str = str.substr(0, pos+1);

	return str;
}

inline void __pokeString(char *dest, const char *str, size_t maxLength){
	if (maxLength > 0){
		size_t length = strlen(str);

		if (length > maxLength - 1)
			length = maxLength - 1;

		memcpy(dest, str, length);
		dest[length] = 0;
	}
}

inline void __pokeString(char *dest, const std::string str, size_t maxLength){
	__pokeString(dest, str.c_str(), maxLength);
}

#define pokeString(dest, str) \
	__pokeString((dest), (str), sizeof(dest))

#define c_alphanumchar(c) \
	( ((c) >= 'A' && (c) <= 'Z') || ((c) >= 'a' && (c) <= 'z') || ((c) >= '0' && (c) <= '9') || (c) == '_' )

/* locale independent tolower function */
inline int c_tolower(int c){
	if (c >= 'A' && c <= 'Z')
		c += ('a' - 'A');
	return c;
}

/* locale independent toupper function */
inline int c_toupper(int c){
	if (c >= 'a' && c <= 'z')
		c += ('A' - 'a');
	return c;
}


#endif // Common_h_
