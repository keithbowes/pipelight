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
 * Portions created by the Initial Developer are Copyright (C) 2013
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michael Müller <michael@fds-team.de>
 *   Sebastian Lackner <sebastian@fds-team.de>
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

#define __WINESRC__

#include "common/common.h"

/*
	NP Class
	These function *should* never be called from a plugin.
	The plugin has to use the browser API instead, so we just
	need stubs to detect a violation of the API.
*/

static void NPInvalidateFunction(NPObject *npobj){
	DBG_TRACE("( npobj=%p )", npobj);
	NOTIMPLEMENTED();
	DBG_TRACE(" -> void");
}

static bool NPHasMethodFunction(NPObject *npobj, NPIdentifier name){
	DBG_TRACE("( npobj=%p, name=%p )", npobj, name);
	NOTIMPLEMENTED();
	DBG_TRACE(" -> result=0");
	return false;
}

static bool NPInvokeFunction(NPObject *npobj, NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result){
	DBG_TRACE("( npobj=%p, name=%p, args=%p, argCount=%d, result=%p )", npobj, name, args, argCount, result);
	NOTIMPLEMENTED();
	DBG_TRACE(" -> result=0");
	return false;
}

static bool NPInvokeDefaultFunction(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result){
	DBG_TRACE("( npobj=%p, args=%p, argCount=%d, result=%p )", npobj, args, argCount, result);
	NOTIMPLEMENTED();
	DBG_TRACE(" -> result=0");
	return false;
}

static bool NPHasPropertyFunction(NPObject *npobj, NPIdentifier name){
	DBG_TRACE("( npobj=%p, name=%p )", npobj, name);
	NOTIMPLEMENTED();
	DBG_TRACE(" -> result=0");
	return false;
}

static bool NPGetPropertyFunction(NPObject *npobj, NPIdentifier name, NPVariant *result){
	DBG_TRACE("( npobj=%p, name=%p, result=%p )", npobj, name, result);
	NOTIMPLEMENTED();
	DBG_TRACE(" -> result=0");
	return false;
}

static bool NPSetPropertyFunction(NPObject *npobj, NPIdentifier name, const NPVariant *value){
	DBG_TRACE("( npobj=%p, name=%p, result=%p )", npobj, name, value);
	NOTIMPLEMENTED();
	DBG_TRACE(" -> result=0");
	return false;
}

static bool NPRemovePropertyFunction(NPObject *npobj, NPIdentifier name){
	DBG_TRACE("( npobj=%p, name=%p )", npobj, name);
	NOTIMPLEMENTED();
	DBG_TRACE(" -> result=0");
	return false;
}

static bool NPEnumerationFunction(NPObject *npobj, NPIdentifier **value, uint32_t *count){
	DBG_TRACE("( npobj=%p, value=%p, count=%p )", npobj, value, count);
	NOTIMPLEMENTED();
	DBG_TRACE(" -> result=0");
	return false;
}

static bool NPConstructFunction(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result){
	DBG_TRACE("( npobj=%p, args=%p, argCount=%d, result=%p )", npobj, args, argCount, result);
	NOTIMPLEMENTED();
	DBG_TRACE(" -> result=0");
	return false;
}

NPClass myClass = {
	NP_CLASS_STRUCT_VERSION,
	NULL, /* NPAllocateFunction, */
	NULL, /* NPDeallocateFunction, */
	NPInvalidateFunction,
	NPHasMethodFunction,
	NPInvokeFunction,
	NPInvokeDefaultFunction,
	NPHasPropertyFunction,
	NPGetPropertyFunction,
	NPSetPropertyFunction,
	NPRemovePropertyFunction,
	NPEnumerationFunction,
	NPConstructFunction
};
