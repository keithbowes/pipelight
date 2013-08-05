#include <map>
#include <cstdint>
#include <stdexcept>

#include "../npapi-headers/npapi.h"
#include "../npapi-headers/npruntime.h"
#include "../npapi-headers/npfunctions.h"
#include "../communication/communication.h"

typedef enum { PR_FALSE = 0, PR_TRUE = 1 } PRBool;

#define REFCOUNT_UNDEFINED 0xffffffff

enum HandleType{  
	TYPE_NPObject = 0,
	TYPE_NPIdentifier,
	TYPE_NPPInstance,
	TYPE_NPStream,
	TYPE_NotifyData,

	TYPE_MaxTypes
};

struct Handle{
	uint64_t 	id;
	uint64_t 	real;
	HandleType	type;
	bool		selfCreated;		
};

class HandleManager{

	private:
		uint64_t getFreeID();
		std::map<uint64_t, Handle> handlesID;
		std::map<std::pair<HandleType, uint64_t>, Handle> handlesReal;

	public:
		uint64_t translateFrom(uint64_t id, HandleType type, NPP instance = NULL, NPClass *aclass = 0, bool shouldExist = false);
		uint64_t translateTo(uint64_t real, HandleType type, bool shouldExist = false);

		void removeHandleByID(uint64_t id);
		void removeHandleByReal(uint64_t real, HandleType type);

		// Checks if the given handletype exists
		bool existsHandleByReal(uint64_t real, HandleType type);
};

void writeHandle(uint64_t real, HandleType type, bool shouldExist = false);
uint64_t		readHandle(Stack &stack, int32_t &type, NPP instance = NULL, NPClass *aclass = 0, bool shouldExist = false);

void writeHandleObj(NPObject *obj, bool shouldExist = false, bool deleteFromHandleManager = false);
void writeHandleInstance(NPP instance, bool shouldExist = false);
void writeHandleIdentifier(NPIdentifier name, bool shouldExist = false);
void writeHandleStream(NPStream* stream, bool shouldExist = false);
void writeHandleNotify(void* notifyData, bool shouldExist = false);

#ifndef __WIN32__
NPObject * 		readHandleObj(Stack &stack, NPP instance = NULL, NPClass *aclass = 0, bool shouldExist = false);
#endif
NPIdentifier 	readHandleIdentifier(Stack &stack, bool shouldExist = false);
NPP 			readHandleInstance(Stack &stack, bool shouldExist = false);
NPStream* 		readHandleStream(Stack &stack, bool shouldExist = false);
void* 			readHandleNotify(Stack &stack, bool shouldExist = false);

#ifdef __WIN32__
NPObject * 		readHandleObjIncRef(Stack &stack, NPP instance = NULL, NPClass *aclass = 0, bool shouldExist = false);
void writeHandleObjDecRef(NPObject *obj, bool shouldExist = false);
void objectDecRef(NPObject *obj);

void objectKill(NPObject *obj);

void writeVariantReleaseDecRef(NPVariant &variant);
void writeVariantArrayReleaseDecRef(NPVariant *variant, int count);

#else
void writeVariantRelease(NPVariant &variant);
void writeVariantArrayRelease(NPVariant *variant, int count);
#endif

void writeVariantConst(const NPVariant &variant);
void writeVariantArrayConst(const NPVariant *variant, int count);

#ifdef __WIN32__
void readVariantIncRef(Stack &stack, NPVariant &variant);
std::vector<NPVariant> readVariantArrayIncRef(Stack &stack, int count);

void freeVariantDecRef(NPVariant &variant);
void freeVariantArrayDecRef(std::vector<NPVariant> args);

#else
void readVariant(Stack &stack, NPVariant &variant);
std::vector<NPVariant> readVariantArray(Stack &stack, int count);

void freeVariant(NPVariant &variant);
void freeVariantArray(std::vector<NPVariant> args);
#endif

void writeNPString(NPString *string);
void readNPString(Stack &stack, NPString &string);
void freeNPString(NPString &string);

void writeStringArray(char* str[], int count);
std::vector<char*> readStringArray(Stack &stack, int count);
void freeStringArray(std::vector<char*> str);

void writeNPBool(NPBool value);
NPBool readNPBool(Stack &stack);
