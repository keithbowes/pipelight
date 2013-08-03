#include <map>
#include <cstdint>
#include <stdexcept>

#include "../npapi-headers/npapi.h"
#include "../npapi-headers/npruntime.h"
#include "../npapi-headers/npfunctions.h"
#include "../communication/communication.h"

typedef enum { PR_FALSE = 0, PR_TRUE = 1 } PRBool;

enum HandleType{  
	TYPE_NPObject,
	TYPE_NPIdentifier,
	TYPE_NPPInstance,
	TYPE_NPStream,
	TYPE_NotifyData
};

enum StreamNotify{
	NOTIFY_NULL,
	NOTIFY_GET,
	NOTIFY_POST
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
		std::map<uint64_t, Handle> handlesReal;

	public:
		uint64_t translateFrom(uint64_t id, HandleType type, bool forceRefInc = false, NPClass *aclass = 0, NPP instance = NULL);
		uint64_t translateTo(uint64_t real, HandleType type);

		void removeHandleByID(uint64_t id);
		void removeHandleByReal(uint64_t real);

		//uint64_t manuallyAddHandle(uint64_t real, HandleType type);
};

void writeHandle(uint64_t real, HandleType type);

void writeHandle(NPP instance);
void writeHandle(NPObject *obj);
void writeHandle(NPIdentifier name);
void writeHandle(NPStream* stream);
void writeHandleNotify(void* notifyData);

uint64_t		readHandle(Stack &stack, int32_t &type, bool forceRefInc = false, NPClass *aclass = 0, NPP instance = NULL);

NPObject * 		readHandleObj(Stack &stack, bool forceRefInc = false, NPClass *aclass = 0, NPP instance = NULL);
NPIdentifier 	readHandleIdentifier(Stack &stack);
NPP 			readHandleInstance(Stack &stack);
NPStream* 		readHandleStream(Stack &stack);
void* 			readHandleNotify(Stack &stack);

#ifndef __WIN32__
void writeVariant(const NPVariant &variant, bool releaseVariant = true);
#else
void writeVariant(const NPVariant &variant);
#endif

#ifndef __WIN32_
void writeVariantArray(const NPVariant *variant, int count, bool releaseVariant = true);
#else
void writeVariantArray(const NPVariant *variant, int count);
#endif

void readVariant(Stack &stack, NPVariant &variant, bool forceRefInc = false);

std::vector<NPVariant> readVariantArray(Stack &stack, int count, bool forceRefInc = false);

void writeNPString(NPString *string);
void readNPString(Stack &stack, NPString &string);

void freeNPString(NPString &string);

void writeCharStringArray(char* str[], int count);

struct charArray{
	std::vector<char*> 					charPointers;
	std::vector<std::shared_ptr<char> > sharedPointers;
};

charArray readCharStringArray(Stack &stack, int count);

void writeNPBool(NPBool value);
NPBool readNPBool(Stack &stack);
