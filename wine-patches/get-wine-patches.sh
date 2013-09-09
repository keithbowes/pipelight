#!/usr/bin/env bash
OUTPUTDIR="./"
FORCE=""

usage ()
{
	echo ""
	echo "Usage : ./get-wine-patches.sh [--out=DIRECTORY] [--force]"
	echo ""
	echo "This script downloads the wine-patches included in the last wine-compholio"
	echo "release to the current working directory or alternatively to the directory"
	echo "specified by --out."
	echo ""
}

while [[ $# > 0 ]] ; do
	case "$1" in
		--out=*)
			OUTPUTDIR=${1#*=}
			;;
		--out)
			shift
			OUTPUTDIR=$1
			;;
		--force)
			FORCE="true"
			;;
		--help)
			usage
			exit
			;;
		*)
			echo "Error: unknown argument $1" >&2
			exit 1
			;;
	esac
	shift
done

# Ensure that the output directory exists
if [ ! -d "$OUTPUTDIR" ]; then
	echo "Error: output directory $OUTPUTDIR doesn't exist" >&2
	exit 1
fi

# Ensure that there are no other patches in the output directory
NUMPATCHES=$(find "$OUTPUTDIR" -type f -iname "*.patch" | wc -l)
if [ "$NUMPATCHES" -ne 0 ] && [ "$FORCE" = "" ]; then
	echo "Error: output directory already contains some patch files -" >&2
	echo "       if you want to continue anyway use --force" >&2
	exit 1
fi

# Get latest release version of wine-compholio
echo -n " * Checking for last wine-compholio release ... "
LATESTRELEASE=$(wget -O - -o /dev/null -- "http://ppa.launchpad.net/ehoover/compholio/ubuntu/dists/saucy/main/source/Sources" | \
	grep -A100 "^Package: *wine-compholio$" | grep "^Version:" | head -n1 | cut -d" " -f2-)
RET=$?
if [ "$RET" -ne 0 ] || [ "$LATESTRELEASE" = "" ]; then
	echo "failed!"
	echo ""
	echo "Error: wget returned exitcode $RET" >&2
	exit 1
fi
echo "$LATESTRELEASE"

LATESTRELEASECLEAN=$(echo "$LATESTRELEASE" | cut -d"~" -f1)

# Create a temp directory for extracting the patch
TEMPDIR=$(mktemp -d)
if [ $? -ne 0 ] || [ "$TEMPDIR" = "" ] || [ ! -d "$TEMPDIR" ]; then
	echo ""
	echo "Error: failed to create temp directory" >&2
	exit 1
fi

# Delete directory on exit automatically
trap 'rm -rf "$TEMPDIR"' EXIT

# Find the source DIFF tarball corresponding to the version
echo -n " * Downloading wine-compholio_${LATESTRELEASE}.diff.gz ... "
DIFFFILE="$TEMPDIR/diff"
wget -O - -o /dev/null -- "https://launchpad.net/~ehoover/+archive/compholio/+files/wine-compholio_${LATESTRELEASE}.diff.gz" | \
	gzip -d --stdout > "$DIFFFILE"
RET=$?
if [ "$RET" -ne 0 ] || [ ! -f "$DIFFFILE" ] || [ ! -s "$DIFFFILE" ]; then
	echo "failed!"
	echo ""
	echo "Error: wget returned exitcode $RET" >&2
	exit 1
fi
echo "done"

# Only extract the wine-patches
echo -n " * Extracting wine patches ... "
patch -p1 -d "$TEMPDIR/" < <(filterdiff -z -i '[^/]*/patches/*' "$DIFFFILE") &> /dev/null
RET=$?
if [ "$RET" -ne 0 ]; then
	echo "failed!"
	echo ""
	echo "Error: patch failed with exitcode $RET" >&2
	exit 1
fi
echo "done"

# Copy the files
echo -n " * Copying patches to output directory ... "
RET=0
echo -n "" > "$TEMPDIR/SHA256SUMS"
while read FILENAME; do
	cp "$TEMPDIR/patches/$FILENAME" "$OUTPUTDIR" 2> /dev/null
	if [ $? -ne 0 ]; then
		RET=$?
	fi
	SHA256=$(sha256sum "$TEMPDIR/patches/$FILENAME" | cut -d' ' -f1)
	echo "$SHA256  $FILENAME" >> "$TEMPDIR/SHA256SUMS"
done < <(ls "$TEMPDIR/patches")

if [ "$RET" -ne 0 ]; then
	echo "failed!"
	echo ""
	echo "Error: copy failed with exitcode $RET" >&2
	exit 1
fi
echo "done"

# Download checksum file if not available
if [ ! -f "SHA256SUMS-$LATESTRELEASECLEAN" ]; then
	echo -n " * Trying to download SHA256SUMS-$LATESTRELEASECLEAN ... "
	wget -O "SHA256SUMS-$LATESTRELEASECLEAN" -o /dev/null -- "https://bitbucket.org/api/1.0/repositories/mmueller2012/pipelight/raw/master/wine-patches/SHA256SUMS-$LATESTRELEASECLEAN"
	RET="$?"
	if [ "$RET" -ne 0 ]; then
		echo "failed!"
		if [ -f "SHA256SUMS-$LATESTRELEASECLEAN" ]; then
			rm "SHA256SUMS-$LATESTRELEASECLEAN"
		fi
	else
		echo "done"
	fi
fi

echo ""

# Display the result to the user if everything was successful
echo "The following patches have been installed to $OUTPUTDIR:"
while read FILENAME; do
	echo " - $FILENAME"
done < <(ls "$TEMPDIR/patches")
echo ""

# Verify checksums
if [ -f "SHA256SUMS-$LATESTRELEASECLEAN" ]; then
	diff "$TEMPDIR/SHA256SUMS" "SHA256SUMS-$LATESTRELEASECLEAN" &> /dev/null
	RET="$?"
	if [ "$RET" -ne 0 ]; then
		echo "Error: extracted patches don't match SHA256SUMS-$LATESTRELEASECLEAN file" >&2
		exit 1
	fi
	echo "Checksums okay!"
else
	echo "Warning: No SHA256SUMS-$LATESTRELEASECLEAN file found, unable to validate files" >&2
fi

# Temp directory will be removed automatically by the trap
exit 0