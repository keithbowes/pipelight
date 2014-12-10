-----BEGIN PGP SIGNED MESSAGE-----
Hash: SHA256

#!/usr/bin/env bash

usage()
{
	echo ""
	echo "Usage: ./install-dependency DEPENDENCY1 [DEPENDENCY2 ...]"
	echo ""
	echo "Environment variables:"
	echo "  WINE                  path to the wine executable"
	echo "  WINEPREFIX            usually \$HOME/.wine-pipelight"
	echo "  WINEARCH              usually win32"
	echo "  QUIETINSTALLATION=1   don't show the original installation dialogs"
	echo ""
	echo "Package dependencies:"
	echo "	wine-silverlight4-installer"
	echo "	wine-silverlight5.0-installer"
	echo "	wine-silverlight5.1-installer"
	echo "	wine-flash-installer"
	echo "	wine-flash-debug-installer"
	echo "	wine-widevine-installer"
	echo "	wine-unity3d-installer"
	echo "	wine-x64-unity3d-installer"
	echo "	wine-adobereader-installer"
	echo "	wine-foxitpdf-installer"
	echo "	wine-shockwave-installer"
	echo "	wine-grandstream-installer"
	echo "	wine-hikvision-installer"
	echo "	wine-npactivex-installer"
	echo "	wine-roblox-installer"
	echo "	wine-vizzedrgr-installer"
	echo "	wine-viewright-caiway-installer"
	echo ""
	echo "Library dependencies:"
	echo "	wine-mpg2splt-installer"
	echo "	wine-wininet-installer"
	echo "	wine-mspatcha-installer"
	echo ""
}

PRG=$(basename "$0")

# > Marks a file in order to delete it at program termination
# arguments:
# $1	- File to delete
ATEXIT_RM_LIST=()
atexit_add_rm()
{
	ATEXIT_RM_LIST+=("$1")
}

atexit()
{
	local file
	for file in "${ATEXIT_RM_LIST[@]}"; do
		echo "Deleting temporary '$file'."
		rm "$file"
	done
}

mktemp_with_ext()
{
	file=$(mktemp --suffix=".$1" 2>/dev/null)
	if [ "$?" -eq 0 ]; then echo "$file"; return 0; fi
	file=$(mktemp 2>/dev/null) # old version of mktemp
	if [ "$?" -eq 0 ]; then echo "$file"; return 0; fi
	file=$(mktemp -t pipelight 2>/dev/null) # MacOS version of mktemp
	if [ "$?" -eq 0 ]; then echo "$file"; return 0; fi
	return 1
}

# > Checks if a dependency is already installed
# arguments:
# $1	- SHA256
# $DEP
is_installed()
{
	local SHA="$1"
	local ckfile="$WINEPREFIX/$DEP.installed"
	[ -f "$ckfile" ] && [ "$SHA" == "$(cat "$ckfile")" ]
	return $?
}

# > Marks a dependency as already installed
# arguments: same as is_installed
mark_installed()
{
	local SHA="$1"
	local ckfile="$WINEPREFIX/$DEP.installed"
	echo "$SHA" > "$ckfile"
}

# > Download a given dependency file
# arguments:
# $1	- URL
# $2	- SHA256
# $3	- Overwrite file extension
# returns:
# $DOWNLOADFILE
DOWNLOADFILE=""
download()
{
	local URL="$1";	local SHA="$2";	local EXT="$3"

	if [ -z "$EXT" ]; then
		EXT=$(echo "$URL" | sed 's/.*\.//')
	fi

	# Reuse files from the netflix-desktop package if available
	local dlfile="/var/lib/wine-browser-installer/$DEP.$EXT"
	if [ -f "$dlfile" ] && [ "$SHA" == "$(sha256sum "$dlfile" | cut -d' ' -f1)" ]; then
		DOWNLOADFILE="$dlfile"
		return 0
	fi

	# Reuse existing download
	local dlfile="/tmp/pipelight-$DEP.$EXT"
	if [ -f "$dlfile" ] && [ "$SHA" == "$(sha256sum "$dlfile" | cut -d' ' -f1)" ]; then
		DOWNLOADFILE="$dlfile"
		return 0
	fi

	local trycount=3
	local tmpfile=$(mktemp_with_ext "$EXT")
	[ -f "$tmpfile" ] || return 1
	local filesize=$(get_download_size "$URL")

	# Download to tmpfile
	while true; do
		if [ "$trycount" -le 0 ]; then
			rm "$tmpfile"
			echo "[$PRG] ERROR: Downloading of $DEP failed multiple times. Please check:" >&2
			echo "[$PRG]" >&2
			echo "[$PRG]        * that your internet connection is working properly" >&2
			echo "[$PRG]" >&2
			echo "[$PRG]        * and that the plugin database is up-to-date. To update it just run:" >&2
			echo "[$PRG]            sudo pipelight-plugin --update" >&2
			echo "[$PRG]" >&2
			echo "[$PRG]        If this doesn't help then most-likely the download URLs or checksums" >&2
			echo "[$PRG]        have changed. We recommend to open a bug-report in this case." >&2
			return 1
		fi

		download_file "$tmpfile" "$URL" 2>&1 | progressbar "Please wait, downloading ..." "Downloading $DEP ($filesize MiB)"
		if [ -f "$tmpfile" ] && [ "$SHA" == "$(sha256sum "$tmpfile" | cut -d' ' -f1)" ]; then
			break
		fi

		(( trycount-- ))
		sleep 2
	done

	# Move the downloaded file to the right path
	if mv "$tmpfile" "$dlfile"; then
		chmod 0644 "$dlfile"
		DOWNLOADFILE="$dlfile"
		return 0
	fi

	# Continue using the temp path
	atexit_add_rm "$tmpfile"
	DOWNLOADFILE="$tmpfile"
	return 0
}

# > Sets a registry key
# arguments:
# $1	- key
# $2	- path
register_mozilla_plugin()
{
	local KEY="$1"; local VAL="$2"

	local tmpfile=$(mktemp)
	[ -f "$tmpfile" ] || return 1

	local valfile=$("$WINE" winepath --windows "$VAL" | sed 's/\\/\\\\/g')

	(
		echo "REGEDIT4"
		echo ""
		echo "[HKEY_LOCAL_MACHINE\\Software\\MozillaPlugins\\$KEY]"
		echo "\"Path\"=\"$valfile\""
	) > "$tmpfile"

	"$WINE" regedit "$tmpfile"
	local res=$?

	# Cleanup
	rm "$tmpfile"

	# Successful
	return "$res"
}

# > Installer for Silverlight
# arguments:
# $1	- version
# $2	- short version
# $DOWNLOADFILE
install_silverlight()
{
	local VER="$1"
	local SHORTVER="$2"

	# Remove the registry keys for Silverlight since other versions can prevent this one from installing
	"$WINE" msiexec /uninstall {89F4137D-6C26-4A84-BDB8-2E5A4BB71E00};

	# Launch the installer
	if [ "$QUIETINSTALLATION" -eq 0 ]; then
		"$WINE" "$DOWNLOADFILE" /noupdate 2>&1
	else
		"$WINE" "$DOWNLOADFILE" /q /doNotRequireDRMPrompt /noupdate 2>&1 | progressbar "Please wait, installing ..." "Running $DEP"
	fi

	local programfiles="$WINEPREFIX/drive_c/Program Files"
	if [ ! -d "$programfiles/Microsoft Silverlight/$VER" ]; then
		echo "[$PRG] ERROR: Installer for $DEP did not run correctly or was aborted." >&2
		return 1
	fi

	# Move the installation to a version-specific folder that nothing will touch
	mkdir -p "$programfiles/Silverlight"
	mv "$programfiles/Microsoft Silverlight/$VER" "$programfiles/Silverlight/$VER"

	# Create a short symlink if SHORTVER is provided.
	if [ ! -z "$SHORTVER" ]; then
		local shortsymlink="$programfiles/Silverlight/$SHORTVER"
		if [ -L "$shortsymlink" ]; then
			rm "$shortsymlink"
		elif [ -e "$shortsymlink" ]; then
			echo "[$PRG] ERROR: Unable to overwrite $shortsymlink, please delete this file manually." >&2
			return 1
		fi
		ln -s "$VER" "$shortsymlink"
	fi

	# Remove the Silverlight menu shortcut
	rm -f "$WINEPREFIX/drive_c/users/$USER/Start Menu/Programs/Microsoft Silverlight/Microsoft Silverlight.lnk"

	# Workaround for users that are upgrading install-dependency before Pipelight release 0.2.6
	if [ "$VER" == "5.1.30214.0" ]; then
		if [ -d "$programfiles/Silverlight/5.1.20913.0" ]; then
			mv "$programfiles/Silverlight/5.1.20913.0" "$programfiles/Silverlight/5.1.20913.0.orig"
		fi
		ln -s "$programfiles/Silverlight/$VER" "$programfiles/Silverlight/5.1.20913.0"
	fi

	# Successful
	return 0
}

# > Extract cab library
# arguments:
# $1	- file to extract
# $DOWNLOADFILE
#
# optional arguments:
# --reg - run regsvr32.dll to register the dll
install_cabextract()
{
	local FILE="$1"; shift

	local system32="$WINEPREFIX/drive_c/windows/system32"
	cabextract -d "$system32" "$DOWNLOADFILE" -F "$FILE"
	if [ ! -f "$system32/$FILE" ]; then
		echo "[$PRG] ERROR: Failed to extract $FILE from cab file." >&2
		return 1
	fi

	# Process additional args
	while [ $# -gt 0 ] ; do
		local cmd=$1; shift
		case "$cmd" in
			--rename)
				if ! mv "$system32/$FILE" "$system32/$1"; then
					echo "[$PRG] ERROR: Unable to rename extracted file." >&2
					return 1
				fi
				FILE="$1"; shift
				;;
			--reg)
				"$WINE" regsvr32.exe "$FILE"
				;;
			*)
				echo "[$PRG] ERROR: Internal error, install_cabextract called with argument: $cmd" >&2
				return 1
				;;
		esac
	done

	# Successful
	return 0
}

# > Install wininet.dll
# arguments:
# $DOWNLOADFILE
install_wininet()
{
	if ! install_cabextract wininet.x86.5.0.3700.6713.dll --rename "wininet.dll"; then return 1; fi

	# Setup wine dlloverride and adjust some wininet.dll related settings
	local tmpfile=$(mktemp)
	[ -f "$tmpfile" ] || return 1

	(
		echo "REGEDIT4"
		echo ""
		echo "[HKEY_CURRENT_USER\\Software\\Wine\\DllOverrides]"
		echo "\"*wininet\"=\"native,builtin\""
		echo ""
		echo "[HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings]"
		echo "\"MaxConnectionsPerServer\"=dword:7fffffff"
		echo "\"MaxConnectionsPer1_0Server\"=dword:7fffffff"
	) > "$tmpfile"

	"$WINE" regedit "$tmpfile"
	local res=$?

	# Cleanup
	rm "$tmpfile"

	# Successful
	return "$res"

}

# > Install flash
# arguments:
# $1	- version path component
# $DOWNLOADFILE
install_flash()
{
	VER="$1"

	# Launch the installer
	if [ "$QUIETINSTALLATION" -eq 0 ]; then
		"$WINE" "$DOWNLOADFILE" 2>&1
	else
		"$WINE" "$DOWNLOADFILE" -install 2>&1 | progressbar "Please wait, installing ..." "Running $DEP"
	fi

	local installdir="$WINEPREFIX/drive_c/windows/system32/Macromed/Flash"

	local result=1
	case "$WINEARCH" in
		win32)
			[ -f "$installdir/NPSWF32_$VER.dll" ]; result=$?
			;;
		win64)
			[ -f "$installdir/NPSWF64_$VER.dll" ]; result=$?
			;;
		*)
			[ -f "$installdir/NPSWF32_$VER.dll" ] || [ -f "$installdir/NPSWF64_$VER.dll" ]; result=$?
			;;
	esac

	if [ "$result" -ne 0 ]; then
		echo "[$PRG] ERROR: Installer for $DEP did not run correctly or was aborted." >&2
		return 1
	fi


	local flashconfig="$installdir/mms.cfg"
	if ! grep -q "^OverrideGPUValidation=" "$flashconfig" 2>/dev/null; then
		(
			grep -v "^OverrideGPUValidation=" "$flashconfig" 2>/dev/null
			echo "OverrideGPUValidation=true"
		) > "$flashconfig.bak"

		if ! mv "$flashconfig.bak" "$flashconfig"; then
			echo "[$PRG] ERROR: Unable to change $DEP plugin settings." >&2
		fi
	fi

	# Successful
	return 0
}

# > Install shockwave
# arguments:
# $1	- version path component
# $DOWNLOADFILE
install_shockwave()
{
	VER="$1"

	# Launch the installer
	if [ "$QUIETINSTALLATION" -eq 0 ]; then
		"$WINE" "$DOWNLOADFILE" 2>&1
	else
		"$WINE" "$DOWNLOADFILE" /S 2>&1 | progressbar "Please wait, installing ..." "Running $DEP"
	fi

	local installdir="$WINEPREFIX/drive_c/windows/system32/Adobe/Director"
	if [ ! -f "$installdir/np32dsw_$VER.dll" ]; then
		echo "[$PRG] ERROR: Installer for $DEP did not run correctly or was aborted." >&2
		return 1
	fi

	 # Switch to OpenGL mode and disable fallback mode
	 local tmpfile=$(mktemp)
	 [ -f "$tmpfile" ] || return 1

	(
		echo "REGEDIT4"
		echo ""
		echo "[HKEY_CURRENT_USER\\Software\\Adobe\\Shockwave 12\\allowfallback]"
		echo "@=\"n\""
		echo ""
		echo "[HKEY_CURRENT_USER\\Software\\Adobe\\Shockwave 12\\renderer3dsetting]"
		echo "@=\"2\""
		echo ""
		echo "[HKEY_CURRENT_USER\\Software\\Adobe\\Shockwave 12\\renderer3dsettingPerm]"
		echo "@=\"2\""
	) > "$tmpfile"

	"$WINE" regedit "$tmpfile"
	local res=$?

	# Cleanup
	rm "$tmpfile"

	# Successful
	return "$res"
}

# > Install Unity3D 32 bit
# arguments: None
# $DOWNLOADFILE
install_unity3d()
{
	# Launch the installer
	if [ "$QUIETINSTALLATION" -eq 0 ]; then
		"$WINE" "$DOWNLOADFILE" /AllUsers 2>&1
	else
		"$WINE" "$DOWNLOADFILE" /S /AllUsers 2>&1 | progressbar "Please wait, installing ..." "Running $DEP"
	fi

	local installdir="$WINEPREFIX/drive_c/Program Files/Unity/WebPlayer/loader"
	if [ ! -f "$installdir/npUnity3D32.dll" ]; then
		echo "[$PRG] ERROR: Installer for $DEP did not run correctly or was aborted." >&2
		return 1
	fi

	# Successful
	return 0
}

# > Install Unity3D 64 bit
# arguments: None
# $DOWNLOADFILE
install_x64_unity3d()
{
	# Launch the installer
	if [ "$QUIETINSTALLATION" -eq 0 ]; then
		"$WINE" "$DOWNLOADFILE" /AllUsers 2>&1
	else
		"$WINE" "$DOWNLOADFILE" /S /AllUsers 2>&1 | progressbar "Please wait, installing ..." "Running $DEP"
	fi

	local installdir="$WINEPREFIX/drive_c/Program Files/Unity/WebPlayer64/loader-x64"
	if [ ! -f "$installdir/npUnity3D64.dll" ]; then
		echo "[$PRG] ERROR: Installer for $DEP did not run correctly or was aborted." >&2
		return 1
	fi

	# Successful
	return 0
}

# > Install Foxit PDF
# arguments: None
# $DOWNLOADFILE
install_foxitpdf()
{
	# Launch the installer
	#if [ "$QUIETINSTALLATION" -eq 0 ]; then
	#	"$WINE" "$DOWNLOADFILE" /AllUsers 2>&1
	#else
		"$WINE" msiexec.exe /i "$DOWNLOADFILE" ALLUSERS=1 /q /norestart MAKEDEFAULT=0 VIEW_IN_BROWSER=1 DESKTOP_SHORTCUT=0 AUTO_UPDATE=0 ADDLOCAL="FX_PDFVIEWER,FX_FIREFOXPLUGIN" REMOVE="ALL" 2>&1 | progressbar "Please wait, installing ..." "Running $DEP"
	#fi

	local installdir="$WINEPREFIX/drive_c/Program Files/Foxit Software/Foxit Reader/plugins"
	if [ ! -f "$installdir/npFoxitReaderPlugin.dll" ]; then
		echo "[$PRG] ERROR: Installer for $DEP did not run correctly or was aborted." >&2
		return 1
	fi

	# Successful
	return 0
}

# > Install Grandstream
# arguments: None
# $DOWNLOADFILE
install_grandstream()
{
	local tmpfile=$(mktemp_with_ext "exe")
	[ -f "$tmpfile" ] || return 1

	if ! unzip -p "$DOWNLOADFILE" "chrome_firefox_plugine_1.0.0.7.exe" > "$tmpfile"; then
		echo "[$PRG] ERROR: Unable to extract installer from zip file." >&2
		rm "$tmpfile"
		return 1
	fi

	# Launch the installer and delete the program afterwards
	"$WINE" "$tmpfile" 2>&1
	rm "$tmpfile"

	local installdir="$WINEPREFIX/drive_c/Program Files/WebControl"
	if [ ! -f "$installdir/npGS_Plugins.dll" ]; then
		echo "[$PRG] ERROR: Installer for $DEP did not run correctly or was aborted." >&2
		return 1
	fi

	# Successful
	return 0
}

# > Install mspatcha.dll
# arguments: None
# $DOWNLOADFILE
install_mspatcha()
{
	if ! install_cabextract mspatcha.dll; then return 1; fi

	# Setup wine dlloverride
	local tmpfile=$(mktemp)
	[ -f "$tmpfile" ] || return 1

	(
		echo "REGEDIT4"
		echo ""
		echo "[HKEY_CURRENT_USER\\Software\\Wine\\DllOverrides]"
		echo "\"*mspatcha\"=\"native,builtin\""
	) > "$tmpfile"

	"$WINE" regedit "$tmpfile"
	local res=$?

	# Cleanup
	rm "$tmpfile"

	# Successful
	return "$res"

}

# > Install Adobe Reader
# arguments: None
# $DOWNLOADFILE
install_adobereader()
{
	# Launch the installer
	if [ "$QUIETINSTALLATION" -eq 0 ]; then
		"$WINE" "$DOWNLOADFILE" 2>&1
	else
		"$WINE" "$DOWNLOADFILE" /msi EULA_ACCEPT=YES /qn 2>&1 | progressbar "Please wait, installing ..." "Running $DEP"
	fi

	local installdir="$WINEPREFIX/drive_c/Program Files/Adobe/Reader 11.0/Reader/AIR"
	if [ ! -f "$installdir/nppdf32.dll" ]; then
		echo "[$PRG] ERROR: Installer for $DEP did not run correctly or was aborted." >&2
		return 1
	fi

	# Disable sandbox as it does not work with Wine
	local tmpfile=$(mktemp)
	[ -f "$tmpfile" ] || return 1

	(
		echo "REGEDIT4"
		echo ""
		echo "[HKEY_CURRENT_USER\\Software\\Adobe\\Acrobat Reader\\11.0\\Privileged]"
		echo "\"bProtectedMode\"=dword:00000000"
	) > "$tmpfile"

	"$WINE" regedit "$tmpfile"
	local res=$?

	# Cleanup
	rm "$tmpfile"

	# Successful
	return "$res"
}

# > Install Widevine
# arguments: None
# $DOWNLOADFILE
install_widevine()
{
	local system32="$WINEPREFIX/drive_c/windows/system32"
	if ! unzip -p "$DOWNLOADFILE" "plugins/npwidevinemediaoptimizer.dll" > "$system32/npwidevinemediaoptimizer.dll"; then
		echo "[$PRG] ERROR: Unable to extract plugin from xip file." >&2
		return 1
	fi

	# Successful
	return 0
}

# > Install Hikvision
# arguments: None
# $DOWNLOADFILE
install_hikvision()
{
	if [ "$QUIETINSTALLATION" -eq 0 ]; then
		"$WINE" "$DOWNLOADFILE" 2>&1
	else
		"$WINE" "$DOWNLOADFILE" /silent 2>&1 | progressbar "Please wait, installing ..." "Running $DEP"
	fi

	local installdir="$WINEPREFIX/drive_c/Program Files/Web Components"
	if [ ! -f "$installdir/npWebVideoPlugin.dll" ]; then
		echo "[$PRG] ERROR: Installer for $DEP did not run correctly or was aborted." >&2
		return 1
	fi

	# Successful
	return 0
}

# > Install NP-ActiveX
# arguments:
# $1	- checksum of npactivex.dll
# $DOWNLOADFILE
install_npactivex()
{
	DLLSHA="$1"

	local system32="$WINEPREFIX/drive_c/windows/system32"

	# The exit code is 1, but the output is still valid
	unzip -p "$DOWNLOADFILE" "npactivex.dll" > "$system32/npactivex.dll"

	local installfile="$system32/npactivex.dll"
	if [ ! -f "$installfile" ] || [ "$DLLSHA" != "$(sha256sum "$installfile" | cut -d' ' -f1)" ]; then
		echo "[$PRG] ERROR: Unable to extract plugin from crx file." >&2
		return 1
	fi

	# Successful
	return 0
}

# > Install Roblox
# arguments:
# $1	- version "number"
# $DOWNLOADFILE
install_roblox()
{
	VER="$1"

	"$WINE" "$DOWNLOADFILE" 2>&1

	local installdir="$WINEPREFIX/drive_c/users/$USER/Local Settings/Application Data/RobloxVersions/version-$VER"
	if [ ! -f "$installdir/NPRobloxProxy.dll" ]; then
		echo "[$PRG] ERROR: Installer for $DEP did not run correctly or was aborted." >&2
		return 1
	fi

	if ! "$WINE" regsvr32 "$installdir/RobloxProxy.dll"; then
		echo "[$PRG] ERROR: Unable to register Roblox Launcher class." >&2
		return 1
	fi

	# Successful
	return 0
}

# > Install Vizeed RGR
# arguments: None
# $DOWNLOADFILE
install_vizzedrgr()
{
	# Launch the installer
	if [ "$QUIETINSTALLATION" -eq 0 ]; then
		"$WINE" msiexec.exe /i "$DOWNLOADFILE" 2>&1
	else
		"$WINE" msiexec.exe /quiet /i "$DOWNLOADFILE" 2>&1 | progressbar "Please wait, installing ..." "Running $DEP"
	fi

	local installdir="$WINEPREFIX/drive_c/Program Files/Vizzed/Vizzed Retro Game Room"
	if [ ! -f "$installdir/NpVizzedRgr.dll" ]; then
		echo "[$PRG] ERROR: Installer for $DEP did not run correctly or was aborted." >&2
		return 1
	fi

	# Successful
	return 0
}

# > Install ViewRight Plugin for Caiway
# arguments: None
# $DOWNLOADFILE
install_viewright_caiway()
{
	# Launch the installer
	if [ "$QUIETINSTALLATION" -eq 0 ]; then
		"$WINE" msiexec.exe /i "$DOWNLOADFILE" 2>&1
	else
		"$WINE" msiexec.exe /quiet /i "$DOWNLOADFILE" 2>&1 | progressbar "Please wait, installing ..." "Running $DEP"
	fi

	local installdir="$WINEPREFIX/drive_c/Program Files/Verimatrix/ViewRight Web"
	if [ ! -f "$installdir/npViewRight.dll" ]; then
		echo "[$PRG] ERROR: Installer for $DEP did not run correctly or was aborted." >&2
		return 1
	fi

	# Successful
	return 0
}

# Use fetch on FreeBSD if wget is not available
if command -v wget >/dev/null 2>&1; then
	download_file()
	{
		wget -O "$1" "$2"
	}
	get_download_size()
	{
		local filesize="$(wget -O- "$1" --spider --server-response 2>&1 | sed -ne '/Content-Length/{s/.*: //;p}')"
		local re='^[0-9]+$'
		if [[ "$filesize" -ne "0" ]] && [[ "$filesize" =~ $re ]]; then
			echo "$(($filesize/(1024*1024)))"
		else
			echo "N/A"
		fi
	}
elif command -v fetch >/dev/null 2>&1; then
	download_file()
	{
		fetch -o "$1" "$2"
	}
	get_download_size()
	{
		echo "N/A"
	}
else
	download_file()
	{
		echo "ERROR: Could neither find wget nor fetch. Unable to download file!" >&2
		return 1
	}
	get_download_size()
	{
		echo "N/A"
	}
fi

# Use shasum instead of sha256sum on MacOS / *BSD
if ! command -v sha256sum >/dev/null 2>&1 && command -v shasum >/dev/null 2>&1; then
	sha256sum()
	{
		shasum -a 256 "$1"
	}
fi

# Use md5 instead of md5sum on MacOS / *BSD
if ! command -v md5sum >/dev/null 2>&1 && command -v md5 >/dev/null 2>&1; then
	md5sum()
	{
		md5
	}
fi

# Check if some visual feedback is possible
if command -v zenity >/dev/null 2>&1; then
	progressbar()
	{
		WINDOWID="" zenity --progress --title="$1" --text="$2" --pulsate --width=400 --auto-close --no-cancel ||
		WINDOWID="" zenity --progress --title="$1" --text="$2" --pulsate --width=400 --auto-close
	}

elif command -v kdialog >/dev/null 2>&1 && command -v qdbus >/dev/null 2>&1; then
	#Check if qdbus is symlinked to qtchooser (for Arch Linux)
	QDBUSPATH=$(which qdbus)
	QDBUSPATH=$(readlink "$QDBUSPATH")
	if [ "$QDBUSPATH" == "qtchooser" ]; then
		QDBUSPATH="qtchooser -run-tool=qdbus -qt=4"
	else
		QDBUSPATH="qdbus"
	fi

	progressbar()
	{
		local dcopref=$(kdialog --title "$1" --progressbar "$2" 10)

		# Update the progress bar (not really the progress, but the user knows that something is going on)
		(
			local progress=1
			while true; do
				local err=$($QDBUSPATH $dcopref org.freedesktop.DBus.Properties.Set org.kde.kdialog.ProgressDialog value "$progress" 2>&1)
				if [ ! -z "$err" ]; then break; fi

				sleep 1

				(( progress++ ))
				if [ "$progress" -gt 10 ]; then progress=0; fi
			done
		) 0</dev/null &
		local dialogpid="$!"

		cat -

		kill "$dialogpid"
		$QDBUSPATH $dcopref org.kde.kdialog.ProgressDialog.close  &> /dev/null
	}

else
	progressbar()
	{
		cat -
	}
fi

# Print usage message when no arguments are given at all
if [ $# -eq 0 ]; then
	usage
	exit 0
fi

# Check for environment variables
if [ -z "$WINE" ] || [ -z "$WINEPREFIX" ]; then
	echo "[$PRG] ERROR: Missing necessary environment variables WINE and WINEPREFIX." >&2
	exit 1
fi

if [ ! -w "$WINEPREFIX" ]; then
	WINEPREFIX_PARENT="$(dirname "$WINEPREFIX")"
	if [ ! -w "$WINEPREFIX_PARENT" ] || [ ! -O "$WINEPREFIX_PARENT" ]; then
		echo "[$PRG] ERROR: You're running this script as a wrong user - WINEPREFIX or parent directory not owned by you." >&2
		exit 1
	fi
fi

# Silent installation
if [ -z "$QUIETINSTALLATION" ]; then
	QUIETINSTALLATION=0
fi

# Generate a lock file based on the wine prefix
LOCKFILE=$(echo "$WINEPREFIX" | md5sum | cut -d' ' -f1)
LOCKFILE="/tmp/wine-$LOCKFILE.tmp"

LOCKFD=9; eval "exec $LOCKFD> \"\$LOCKFILE\""
if ! flock -x -w 360 "$LOCKFD"; then
	echo "[$PRG] ERROR: Failed to obtain an installation lock in 6 minutes." >&2
	exit 1;
fi

# Close file descriptor (ensure that the lock is released when the installation is ready)
trap "EXITSTATUS=\$?; flock -u \"$LOCKFD\"; atexit; exit \"\$EXITSTATUS\"" 0

# Initialize wine if not done yet
if [ ! -f "$WINEPREFIX/system.reg" ]; then

	# Directory exists, but without system.reg - wine will assume wrong platform, so create dummy system.reg
	if [ -d "$WINEPREFIX" ] && [ ! -f "$WINEPREFIX/system.reg" ]; then
		if [ "$WINEARCH" == "win32" ] || [ "$WINEARCH" == "win64" ]; then
			echo -en "WINE REGISTRY Version 2\n\n#arch=$WINEARCH\n" > "$WINEPREFIX/system.reg"
			echo "[$PRG] Forced creation of a $WINEARCH wine prefix."
		fi
	fi

	DISPLAY="" "$WINE" wineboot.exe 2>&1 | progressbar "Please wait..." "Creating wine prefix"
fi

# Set default return value
RET=0

while [ $# -gt 0 ] ; do
	DEP="$1"; INS=(); URL=""; SHA=""; EXT=""; DOWNLOADFILE=""; shift
	case "$DEP" in
		wine-prefix)
			continue # The wine-prefix is created automatically for all packages
			;;
		wine-silverlight4-installer)
			INS=(install_silverlight "4.1.10329.0")
			URL="http://silverlight.dlservice.microsoft.com/download/6/A/1/6A13C54D-3F35-4082-977A-27F30ECE0F34/10329.00/runtime/Silverlight.exe"
			SHA="b0e476090206b2e61ba897de9151a31e0182c0e62e8abd528c35d3857ad6131c"
			;;
		wine-silverlight5.0-installer)
			INS=(install_silverlight "5.0.61118.0")
			URL="http://silverlight.dlservice.microsoft.com/download/5/5/7/55748E53-D673-4225-8072-4C7A377BB513/runtime/Silverlight.exe"
			SHA="dd45a55419026c592f8b6fc848dceface7e1ce98720bf13848a2e8ae366b29e8"
			;;
		wine-silverlight5.1-installer) # http://www.microsoft.com/getsilverlight/locale/en-us/html/Microsoft%20Silverlight%20Release%20History.htm
			INS=(install_silverlight "5.1.30514.0" "latest")
			URL="http://silverlight.dlservice.microsoft.com/download/F/8/C/F8C0EACB-92D0-4722-9B18-965DD2A681E9/30514.00/Silverlight.exe"
			SHA="afa7a7081d30b00a4f57c32932bd6d84940bb43b3f5feb0828ff988c80e2d485"
			;;
		wine-flash-installer) # http://www.adobe.com/de/software/flash/about/
			INS=(install_flash "16_0_0_235")
			URL="http://fpdownload.macromedia.com/get/flashplayer/pdc/16.0.0.235/install_flash_player.exe"
			SHA="4e83b1af33587cc7bcab870b9f30395f2d78a249c46cf8953ffcd928c8f9f07c"
			;;
		wine-flash-debug-installer)
			INS=(install_flash "16_0_0_235")
			URL="http://download.macromedia.com/pub/flashplayer/updaters/16/flashplayer_16_plugin_debug.exe"
			SHA="08fc11fc121464412a278aa3aee67bec62348b8ea687cf65286edb86fff0aa74"
			;;
		wine-widevine-installer) # http://www.widevine.com/download/videooptimizer/index.html
			INS=(install_widevine)
			URL="https://dl.google.com/widevine/6.0.0.12442/WidevineMediaOptimizer_Win.xpi"
			SHA="84cde1b83d8f5e4b287303a25e61227ce9a253268af6bd88b9a2f98c85129bc8"
			EXT="zip"
			;;
		wine-unity3d-installer)
			INS=(install_unity3d)
			URL="http://webplayer.unity3d.com/download_webplayer-3.x/UnityWebPlayer.exe"
			SHA="84568878b561f248869701f4f768ae22fb35ab025bb514919a86a5eeb4fe3e2f"
			;;
		wine-x64-unity3d-installer)
			INS=(install_x64_unity3d)
			URL="http://webplayer.unity3d.com/download_webplayer-3.x/UnityWebPlayerFull64.exe"
			SHA="a4ae24820ec2f1c87e26123227c1ccd330c8b0f69f7e2f2586761e174a300802"
			;;
		wine-adobereader-installer) # http://get.adobe.com/de/reader/otherversions/
			INS=(install_adobereader)
			URL="http://ardownload.adobe.com/pub/adobe/reader/win/11.x/11.0.08/en_US/AdbeRdr11008_en_US.exe"
			SHA="00dbd10f80e9451938d5a10e60b8c8dca2dac81c118618652bb49a62ca04c3b3"
			;;
		wine-foxitpdf-installer) # http://www.foxitsoftware.com/Secure_PDF_Reader/version_history.php
			INS=(install_foxitpdf)
			URL="http://cdn04.foxitsoftware.com/pub/foxit/reader/desktop/win/7.x/7.0/enu/EnterpriseFoxitReader706.1126_enu.msi"
			SHA="4be8aadcc4fea96277b4d879bb0320a9bd027ebfb2de1306399b4c29abe94206"
			;;
		wine-shockwave-installer) # http://get.adobe.com/de/shockwave/otherversions/
			INS=(install_shockwave "1215155")
			URL="http://fpdownload.macromedia.com/get/shockwave/default/english/win95nt/latest/sw_lic_full_installer.exe"
			SHA="ed1d5eda2ac41a311914f2b1fbaa6d60f7081c0fbea07c990d210a4313c696ba"
			;;
		wine-grandstream-installer)
			INS=(install_grandstream)
			URL="http://www.grandstream.com/products/tools/surveillance/webcontrl_plugin.zip"
			SHA="1162798378997373701967f3f0f291ae4c858e8cae29e55a5249e24f47f70df2"
			;;
		wine-mpg2splt-installer)
			INS=(install_cabextract mpg2splt.ax --reg)
			URL="http://download.microsoft.com/download/8/0/D/80D7E79D-C0E4-415A-BCCA-E229EAFE2679/dshow_nt.cab"
			SHA="984ed15e23a00a33113f0012277e1e680c95782ce2c44f414e7af14e28e3f1a2"
			;;
		wine-wininet-installer)
			INS=(install_wininet)
			URL="http://download.microsoft.com/download/6/f/c/6fcc07f8-62e1-459e-aab3-06faa3adacff/IE-KB884931-v2-x86-enu.exe"
			SHA="b3f31b0d523f03123e8def4f91ba2e64aaceb31d9bfe851516ad7f61b0268d4a"
			;;
		wine-mspatcha-installer)
			INS=(install_mspatcha)
			URL="http://download.microsoft.com/download/WindowsInstaller/Install/2.0/NT45/EN-US/InstMsiW.exe"
			SHA="4c3516c0b5c2b76b88209b22e3bf1cb82d8e2de7116125e97e128952372eed6b"
			;;
		wine-hikvision-installer)
			INS=(install_hikvision)
			URL="http://cctvone.com/fillib/IP%20Camera/Hikvision-2DC-Series/CD/IE%20Client/WebComponents.exe"
			SHA="04fec22ca61c657f6f46160e55334e3defeea0e193bc8ec6d7bb45c87e773361"
			;;
		wine-npactivex-installer) # https://code.google.com/p/np-activex/downloads/list
			INS=(install_npactivex "6a31dac35cfda77ef4be724f226c5b54404aec61a81e663e47d39c7c9dd1580e")
			URL="https://np-activex.googlecode.com/files/extension_1_5_0_7.crx"
			SHA="12ba6c79079f53172ca897f717911c2613219236dca5ae58662d41636490e7a9"
			EXT="zip"
			;;
		wine-roblox-installer)
			INS=(install_roblox "632471a80776450d")
			URL="http://setup.roblox.com/version-632471a80776450d-Roblox.exe"
			SHA="901505c1311cb592521ea6bec09ec793194d1ecb0630686e72a948850e55d632"
			;;
		wine-vizzedrgr-installer)
			INS=(install_vizzedrgr)
			URL="http://www.vizzed.co/VizzedRgrPlugin-v2.0.msi"
			SHA="ddc99b1a6902e30f355533620637a2d1b7d1ff3b1bd76a65cb1fbd78b2b396cb"
			;;
		wine-viewright-caiway-installer) # https://www.caiway.nl/site/nl/applicatie/multiscreentvplugins
			INS=(install_viewright_caiway)
			URL="https://www.caiway.nl/downloads/ViewRightWebInstaller-3.5.0.0_CaiW.msi"
			SHA="9436dea83e42204d0a9bc4d128c2f2693dd9c5f9636d5fa57441ef5886f3ab43"
			;;
		*)
			echo "[$PRG] ERROR: No installer script found for $DEP." >&2
			RET=1
			break
			;;
	esac

	# Is already installed?
	if is_installed "$SHA"; then
		echo "[$PRG] $DEP is already installed in '$WINEPREFIX'."
		continue
	fi

	echo "[$PRG] Downloading and running $DEP."

	# Fetch the download
	if ! download "$URL" "$SHA" "$EXT"; then
		echo "[$PRG] ERROR: Download of $DEP failed." >&2
		RET=1
		break
	fi

	# Do the installation
	if ! eval "${INS[@]}"; then
		echo "[$PRG] ERROR: Execution of $DEP failed." >&2
		RET=1
		break
	fi

	# Mark the package as installed
	mark_installed "$SHA"
done

exit "$RET"
-----BEGIN PGP SIGNATURE-----
Version: GnuPG v2

iQIcBAEBCAAGBQJUh5RCAAoJEAtrgXwcOwUzJ5AP/2vCwVW0SFCUEruC4KtSTTk7
fkgKjsEWvZNDVI/s3MEyR9U5Uty6TC8BxY18PbapErarU2OfBJHS1n1rLUou+B5s
Z6sUmaXacgqGB6EDp7Ppn3AKbjta1fYJGWnEl6+8q/74g8Klmk6bn0nZLug9hoXh
6xTrtfJrwwOgdKbW/NguVqGYtCJACrXSXfdI07OZdo/Gq9MvyXaUQuiybzsNG/EW
aEVjQVgS+XHCApsQltJ7/gXy4nI8xl3Mi2wWFn4sggPNWQl3w6/2lInme0JHiyxN
Qk3VNpgOu7Ycbyd4LMrcJARhKp9xddS2GNCbTV+xkWUABytmtTzl7MVQSboF8bs3
tbwSfgxM8H7FNHPp29xmfLFE13nSy7cGhMxV09VmCGWDyh/WK5YuZzbMirr9hxRr
4Xdxc2lu9aMKKS3Koxg4UM7tbswLHT+Cx8KjYnaxEcrw8GzFTxaGa+/Jh15CjWSr
MR5h9INFH0Elj5UaiutjYyn4b1njp9X3fhy4EwTM/dS/7UdzvsABPhnvNqkQrOMJ
/2vc2Ni7AQRx7+eJiaVVRUB1sVaWo+LyryvzwL6ox1CSo88weTWuDXTCgBCbO5gf
FXghAIRuqPgakeVtQD/UxcwJMKzMq78FpSvazo3kllsziiE7FZvMS4ZCytzVYD3s
LVgU5MzEaG9uhXus+wZ2
=7ctL
-----END PGP SIGNATURE-----
