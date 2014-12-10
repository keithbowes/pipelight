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

#include <stdio.h>
#include <stdbool.h>
#include <windows.h>
#include <aclapi.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <GL/gl.h>

#ifdef MINGW32_FALLBACK
	typedef LONG (* WINAPI RegGetValueAPtr)(HKEY hkey, LPCSTR lpSubKey, LPCSTR lpValue, DWORD dwFlags, LPDWORD pdwType, PVOID pvData, LPDWORD pcbData);
	typedef BOOL (* WINAPI CreateWellKnownSidPtr)(int WellKnownSidType, PSID DomainSid, PSID pSid, DWORD *cbSid);
	RegGetValueAPtr RegGetValueA = NULL;
	CreateWellKnownSidPtr CreateWellKnownSid = NULL;
	#define RRF_RT_ANY   0x0000FFFF
	#define RRF_RT_REG_SZ 0x00000002
	#define SECURITY_MAX_SID_SIZE 0x44
	#define WinBuiltinAdministratorsSid 26
	HMODULE module_advapi32;
#endif

const char *badOpenGLVendors[] =
{
	"VMware, Inc."
};

const char *badOpenGLRenderer[] =
{
	"Software Rasterizer",
	"Mesa GLX Indirect",
	"llvmpipe"
};

struct fontsToCheck
{
	const char *name;
	bool found;
};

struct fontsToCheck fonts[] =
{
	{ "Arial", false },
	{ "Verdana", false },
};

static inline uint16_t bswap16( uint16_t a )
{
	return (a<<8) | (a>>8);
}

static inline uint32_t bswap32( uint32_t a )
{
	return ((uint32_t)bswap16(a)<<16) | bswap16(a>>16);
}

struct TTF_TableDirectory
{
	int32_t version;
	USHORT numTables;
	USHORT searchRange;
	USHORT entrySelector;
	USHORT rangeShift;
} __attribute__((packed));

struct TTF_DirectoryEntry
{
	ULONG tag;
	ULONG checksum;
	ULONG offset;
	ULONG length;
} __attribute__((packed));

struct TTF_NameTable
{
	USHORT selector;
	USHORT count;
	USHORT offset;
} __attribute__((packed));

struct TTF_NameRecord
{
	USHORT platformID;
	USHORT platformEncoding;
	USHORT language;
	USHORT name;
	USHORT length;
	USHORT offset;
} __attribute__((packed));


char clsName[] = "Systemcheck";

LRESULT CALLBACK wndProcedure(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	return DefWindowProcA(hWnd, Msg, wParam, lParam);
}

bool registerClass()
{
	/* Create the application window */
	WNDCLASSEXA WndClsEx;
	WndClsEx.cbSize        = sizeof(WndClsEx);
	WndClsEx.style         = CS_HREDRAW | CS_VREDRAW;
	WndClsEx.lpfnWndProc   = &wndProcedure;
	WndClsEx.cbClsExtra    = 0;
	WndClsEx.cbWndExtra    = 0;
	WndClsEx.hIcon         = LoadIconA(NULL, (LPCSTR)IDI_APPLICATION);
	WndClsEx.hCursor       = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
	WndClsEx.hbrBackground = NULL; /* (HBRUSH)GetStockObject(LTGRAY_BRUSH); */
	WndClsEx.lpszMenuName  = NULL;
	WndClsEx.lpszClassName = clsName;
	WndClsEx.hInstance     = GetModuleHandleA(NULL);
	WndClsEx.hIconSm       = LoadIconA(NULL, (LPCSTR)IDI_APPLICATION);

	ATOM classAtom = RegisterClassExA(&WndClsEx);
	if (!classAtom)
		return false;

	return true;
}

bool checkOpenGL()
{
	HWND hWnd = 0;
	HDC hDC = 0;
	HGLRC context = NULL;
	bool result = false;
	int pixelformat;
	const char* renderer = NULL;
	const char* vendor = NULL;
	const char* extensions = NULL;
	unsigned int i;
	bool badOpenGL = false;
	bool directRendering = false;

	PIXELFORMATDESCRIPTOR pfd =
	{
		sizeof(PIXELFORMATDESCRIPTOR),
		1,
		PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL,
		PFD_TYPE_RGBA,
		32,
		0, 0, 0, 0, 0, 0,
		0,
		0,
		0,
		0, 0, 0, 0,
		0,
		0,
		0,
		PFD_MAIN_PLANE,
		0,
		0, 0, 0
	};

	hWnd = CreateWindowExA(0, clsName, "OpenGL Test", WS_TILEDWINDOW, 0, 0, 100, 100, 0, 0, 0, 0);
	if (!hWnd)
		return false;

	hDC = GetDC(hWnd);
	if (!hDC)
		goto error;

	pixelformat = ChoosePixelFormat(hDC, &pfd);
	if (!pixelformat)
		goto error;

	if (!SetPixelFormat(hDC, pixelformat, &pfd))
		goto error;

	context = wglCreateContext(hDC);
	if (!context)
		goto error;

	if (!wglMakeCurrent(hDC, context))
		goto error;

	vendor		= (const char *)glGetString(GL_VENDOR);
	renderer	= (const char *)glGetString(GL_RENDERER);
	extensions	= (const char *)glGetString(GL_EXTENSIONS);

	if (extensions && strstr(extensions, "WINE_EXT_direct_rendering"))
		directRendering = true;

	printf("OpenGL Vendor: %s\n", vendor);
	printf("OpenGL Renderer: %s\n", renderer);
	printf("OpenGL Direct Rendering: %s\n",
	directRendering ? "True" : "False (or old/wrong wine version)");

	if (!vendor || !renderer)
		goto error;

	for (i = 0; i < sizeof(badOpenGLVendors) / sizeof(badOpenGLVendors[0]); i++)
	{
		if (strstr(vendor, badOpenGLVendors[i]))
		{
			printf("ERROR: found bad OpenGL Vendor: %s\n", badOpenGLVendors[i]);
			badOpenGL = true;
			break;
		}
	}

	for (i = 0; i < sizeof(badOpenGLRenderer) / sizeof(badOpenGLRenderer[0]); i++)
	{
		if (strstr(renderer, badOpenGLRenderer[i]))
		{
			printf("ERROR: found bad OpenGL Renderer: %s\n", badOpenGLRenderer[i]);
			badOpenGL = true;
			break;
		}
	}

	if (!badOpenGL && directRendering)
		result = true;

error:
	if (context) wglDeleteContext(context);
	if (hDC) ReleaseDC(hWnd, hDC);
	DestroyWindow(hWnd);
	return result;
}

#define READ_SAFE(file, ptr, length) \
	if (fread((ptr), 1, (length), (file)) != (length)) \
		goto error

bool checkFontFile(const char *pattern, const char *name, const char *path)
{
	FILE *file = NULL;
	bool result = false;
	struct TTF_TableDirectory directory;
	struct TTF_DirectoryEntry entry;
	struct TTF_NameTable nameTable;
	struct TTF_NameRecord nameEntry;
	ULONG i, j, k, l;
	ULONG offset;
	ULONG nameOffset;
	char fontFamily[256];
	ULONG fontFamilyLength;

	file = fopen(path, "rb");
	if (!file)
		return false;

	READ_SAFE(file, &directory, sizeof(directory));

	for (i = 0; i < bswap16(directory.numTables); i++)
	{
		READ_SAFE(file, &entry, sizeof(entry));

		if (memcmp(&entry.tag, "name", 4) != 0)
			continue;

		offset = bswap32(entry.offset);
		if (fseek(file, offset, SEEK_SET) != 0)
			goto error;

		READ_SAFE(file, &nameTable, sizeof(nameTable));
		for (j = 0; j < bswap16(nameTable.count); j++)
		{
			READ_SAFE(file, &nameEntry, sizeof(nameEntry));
			if (bswap16(nameEntry.name) != 1)
				continue;

			fontFamilyLength = bswap16(nameEntry.length);
			if (fontFamilyLength > sizeof(fontFamily) - 1)
				fontFamilyLength = sizeof(fontFamily) - 1;

			nameOffset = offset + bswap16(nameTable.offset) + bswap16(nameEntry.offset);
			if (fseek(file, nameOffset, SEEK_SET) != 0)
				goto error;

			READ_SAFE(file, fontFamily, fontFamilyLength);
			fontFamily[fontFamilyLength] = 0;

			/* check for MS encoding */
			if (bswap16(nameEntry.platformEncoding) == 3)
			{
				/* convert big endian wide char strings into ascii */
				for (l = 0, k = 1; k < fontFamilyLength; k +=2)
					fontFamily[l++] = fontFamily[k];
				fontFamily[l] = 0;
			}

			if (strcmp(pattern, fontFamily) == 0)
				result = true;

			break;
		}
		break;
	}

error:
	fclose(file);
	return result;
}

bool checkFonts()
{
	LPCTSTR path = "Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts";
	HKEY hKey = 0;
	bool result = false;
	DWORD index = 0;
	char fontName[256];
	char fontPath[256];
	DWORD lengthName = sizeof(fontName);
	DWORD lengthPath;
	unsigned int i;

	/* reset found flag */
	for (i = 0; i < sizeof(fonts) / sizeof(fonts[0]); i++)
		fonts[i].found = false;

	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
		return false;

	while (RegEnumValue(hKey, index, fontName, &lengthName, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
	{
		lengthName = sizeof(fontName);
		index++;

		for (i = 0; i < sizeof(fonts) / sizeof(fonts[0]); i++)
		{
			if (strstr(fontName, fonts[i].name))
			{
				lengthPath = sizeof(fontPath);
				if (RegGetValueA(hKey, NULL, fontName, RRF_RT_REG_SZ, NULL, fontPath, &lengthPath) != ERROR_SUCCESS)
					continue;

				if (checkFontFile(fonts[i].name, fontName, fontPath))
				{
					printf("Found %s in %s\n", fonts[i].name, fontPath);
					fonts[i].found = true;
				}
				break;
			}
		}
	}

	result = true;
	for (i = 0; i < sizeof(fonts) / sizeof(fonts[0]); i++)
	{
		if (!fonts[i].found)
		{
			printf("Missing %s\n", fonts[i].name);
			result = false;
		}
	}

	RegCloseKey(hKey);
	return result;
}

bool checkACLs()
{
	char sidStorage[SECURITY_MAX_SID_SIZE];
	char daclStorage[100];
	PSID sid = (PSID) &sidStorage;
	DWORD sidSize = sizeof(sidStorage);
	PACL dacl = (PACL) &daclStorage;
	PACL daclResult;
	DWORD daclSize = sizeof(daclStorage);
	SECURITY_DESCRIPTOR secDescriptor;
	SECURITY_ATTRIBUTES secAttrs;
	PSECURITY_DESCRIPTOR secDescriptorResult;
	HANDLE file;
	char *testFile = (char*)"C:\\acl-test.txt";
	bool result = false;

	if (!CreateWellKnownSid(WinBuiltinAdministratorsSid, NULL, sid, &sidSize))
		return false;

	if (!InitializeSecurityDescriptor(&secDescriptor, SECURITY_DESCRIPTOR_REVISION))
		return false;

	if(!InitializeAcl(dacl, daclSize, ACL_REVISION))
		return false;

	if (!AddAccessAllowedAceEx(dacl, ACL_REVISION, OBJECT_INHERIT_ACE|CONTAINER_INHERIT_ACE, GENERIC_ALL, sid))
		return false;

	if (!SetSecurityDescriptorDacl(&secDescriptor, TRUE, dacl, FALSE))
		return false;

	secAttrs.nLength = sizeof(secAttrs);
	secAttrs.lpSecurityDescriptor = &secDescriptor;
	secAttrs.bInheritHandle = false;

	if (GetFileAttributes(testFile) != INVALID_FILE_ATTRIBUTES)
	{
		if (!DeleteFile(testFile))
		{
			printf("Failed to delete old test file!\n");
			return false;
		}
	}

	file = CreateFileA(testFile, GENERIC_WRITE, 0, &secAttrs, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return false;

	CloseHandle(file);

	if (GetNamedSecurityInfo(testFile, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, &daclResult, NULL, &secDescriptorResult) == ERROR_SUCCESS)
	{
		ACL_SIZE_INFORMATION aclSize;
		unsigned int i;

		if (GetAclInformation(daclResult, &aclSize, sizeof(aclSize), AclSizeInformation))
		{
			for (i = 0; i < aclSize.AceCount; i++)
			{
				ACE_HEADER *pAceHeader;
				ACCESS_ALLOWED_ACE *pAceAllow;

				if (!GetAce(daclResult, i, (VOID**)&pAceHeader))
					continue;

				if (pAceHeader->AceType != ACCESS_ALLOWED_ACE_TYPE)
					continue;

				pAceAllow = (ACCESS_ALLOWED_ACE *)pAceHeader;
				if (EqualSid(&pAceAllow->SidStart, sid))
				{
					result = true;
					break;
				}
			}
		}

		LocalFree(secDescriptorResult);
	}

	DeleteFile(testFile);
	return result;
}


int main()
{
	bool test, ret = 0;

#ifdef MINGW32_FALLBACK
	module_advapi32 = LoadLibraryA("advapi32.dll");
	assert(module_advapi32);

	RegGetValueA = (RegGetValueAPtr)GetProcAddress(module_advapi32, "RegGetValueA");
	CreateWellKnownSid = (CreateWellKnownSidPtr)GetProcAddress(module_advapi32, "CreateWellKnownSid");
	assert(RegGetValueA && CreateWellKnownSid);
#endif

	if (!registerClass()) ret = 1;

	printf("Checking OpenGL ...\n");
	test = checkOpenGL();
	if (!test) ret = 1;
	printf("OpenGL: %s\n", test ? "PASSED" : "FAILURE");
	printf("\n");

	printf("Checking fonts ...\n");
	test = checkFonts();
	if (!test) ret = 1;
	printf("Fonts: %s\n", test ? "PASSED" : "FAILURE");
	printf("\n");

	printf("Checking ACLs / XATTR ...\n");
	test = checkACLs();
	if (!test) ret = 1;
	printf("ACLs: %s\n", test ? "PASSED" : "FAILURE");

	exit(ret);
}