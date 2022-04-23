#!/bin/sh
#
# mkrel.sh: Creates a release package from a Skunk Memory Track build.
#
# Usage: mkrel.sh
#
# The version number is automatically extracted from rom.rom to avoid
# typos/mismatches.
# 

# Extract version number from rom.rom magic header:
MAJOR=`xxd -C -g 1 -s 17 -l 1 rom.rom | cut -f 2 -d ' '`
MINOR=`xxd -C -g 1 -s 18 -l 1 rom.rom | cut -f 2 -d ' '`
MICRO=`xxd -C -g 1 -s 19 -l 1 rom.rom | cut -f 2 -d ' '`

# Convert the hex fields to decimal
MAJOR=`echo "ibase=16; ${MAJOR}" | bc`
MINOR=`echo "ibase=16; ${MINOR}" | bc`
MICRO=`echo "ibase=16; ${MICRO}" | bc`

# Get the full version string. Note larger hex numbers will use three decimal
# digits. Don't try to parse version as <2 digits>_<2 digits>_<2 digits>
VERSION=`printf "%02d_%02d_%02d" "${MAJOR}" "${MINOR}" "${MICRO}"`

echo "Building release package for version '${VERSION}'"

ROM="skunk_mtrk-${VERSION}.rom"
COFF="stand_mtrk-${VERSION}.cof"
PKG="skunk_mtrk-${VERSION}.zip"

if [ -f "${ROM}" ]; then
	echo  "FAILURE: Refusing to overwrite existing file: '${ROM}'"
	exit 1
fi

if [ -f "${COFF}" ]; then
	echo  "FAILURE: Refusing to overwrite existing file: '${COFF}'"
	exit 1
fi

if [ -f "${PKG}" ]; then
	echo  "FAILURE: Refusing to overwrite existing file: '${PKG}'"
	exit 1
fi

cp rom.rom "${ROM}"
cp stand.cof "${COFF}"

echo "Creating ${PKG}..."
zip "${PKG}" "README.md" "${ROM}" "${COFF}"
echo "Done."

echo -n "Cleaning up..."
rm "${ROM}" "${COFF}"
echo "Done."

echo ""
echo "**********************************"
echo "*                                *"
echo "*            SUCCESS!            *"
echo "*                                *"
echo "**********************************"
