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

#ifndef ConfigLoader_h_
#define ConfigLoader_h_

#include <strings.h>							// for strcasecmp
#include <string>								// for std::string
#include <vector>								// for std::vector
#include <map>									// for std::map

struct stringInsensitiveCompare { 
	bool operator() (const std::string& a, const std::string& b) const{
		return strcasecmp(a.c_str(), b.c_str()) < 0;
	}
};

struct MimeInfo{
	std::string mimeType;
	std::string extension;
	std::string description;
	std::string originalMime;
};

struct PluginConfig{
	std::string		configPath;
	std::string		pluginName;
	bool 			diagnosticMode;
	
	std::string 	sandboxPath;

	std::string 	winePath;
	std::string		wineArch;
	std::string 	winePrefix;
	std::string		wineDLLOverrides;

	std::string 	dllPath;
	std::string 	dllName;
	std::string 	regKey;
	std::string 	pluginLoaderPath;
	std::string 	gccRuntimeDLLs;

	bool 			windowlessMode;
	bool			embed;
	std::string 	fakeVersion;
	std::map<std::string, std::string, stringInsensitiveCompare> overwriteArgs;
	std::vector<MimeInfo> fakeMIMEtypes;

	std::string		dependencyInstaller;
	std::vector<std::string> dependencies;
	bool 			quietInstallation;

	bool 			eventAsyncCall;
	bool			operaDetection;
	std::string 	executeJavascript;

	std::string		silverlightGraphicDriverCheck;

	bool 			experimental_unityHacks;
	bool            experimental_windowClassHook;
	bool 			experimental_renderTopLevelWindow;
};

extern bool loadConfig(PluginConfig &config);

#endif // ConfigLoader_h_
