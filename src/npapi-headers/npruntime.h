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
 * The Original Code is fds-team.de code.
 *
 * The Initial Developer of the Original Code is
 * Michael Müller <michael@fds-team.de>
 * Portions created by the Initial Developer are Copyright (C) 2014
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michael Müller <michael@fds-team.de>
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

#ifndef _NP_RUNTIME_H_
#define _NP_RUNTIME_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "nptypes.h"

typedef void *NPIdentifier;
typedef char NPUTF8;
typedef struct _NPClass NPClass;

typedef struct _NPString
{
	const NPUTF8 *UTF8Characters;
	uint32_t UTF8Length;
} NPString;

typedef struct _NPObject
{
	NPClass *_class;
	uint32_t referenceCount;
} NPObject;

typedef enum
{
	NPVariantType_Void,
	NPVariantType_Null,
	NPVariantType_Bool,
	NPVariantType_Int32,
	NPVariantType_Double,
	NPVariantType_String,
	NPVariantType_Object
} NPVariantType;

typedef struct _NPVariant
{
	NPVariantType type;
	union
	{
		bool		boolValue;
		int32_t		intValue;
		DOUBLE		doubleValue;
		NPString	stringValue;
		NPObject	*objectValue;
	} value;
} NPVariant;

#define NP_CLASS_STRUCT_VERSION			3
#define NP_CLASS_STRUCT_VERSION_ENUM	2
#define NP_CLASS_STRUCT_VERSION_CTOR	3

struct _NPClass
{
	uint32_t structVersion;
	NPObject *(*allocate)(NPP instance, NPClass *aClass);
	void (*deallocate)(NPObject *obj);
	void (*invalidate)(NPObject *obj);
	bool (*hasMethod)(NPObject *obj, NPIdentifier ident);
	bool (*invoke)(NPObject *obj, NPIdentifier ident, const NPVariant *args, uint32_t count, NPVariant *ret);
	bool (*invokeDefault)(NPObject *obj, const NPVariant *args, uint32_t count, NPVariant *ret);
	bool (*hasProperty)(NPObject *obj, NPIdentifier ident);
	bool (*getProperty)(NPObject *obj, NPIdentifier ident, NPVariant *ret);
	bool (*setProperty)(NPObject *obj, NPIdentifier ident, const NPVariant *val);
	bool (*removeProperty)(NPObject *obj, NPIdentifier ident);
	bool (*enumerate)(NPObject *obj, NPIdentifier **val, uint32_t *count);
	bool (*construct)(NPObject *obj, const NPVariant *args, uint32_t count, NPVariant *ret);
};

#ifdef __cplusplus
}
#endif

#endif
