/*
 * make the assembler include file for asmnvm.s
 */

#include <stdio.h>
#include <stdlib.h>
#include "nvm.h"

/*
 * calculate the log base 2 of an unsigned long
 */

int
intlog2(unsigned long n)
{
	int log;
	unsigned long val = 1;

	for (log = 0; log < 32; log++) {
		if (val == n) break;
		val = val << 1;
	}
	if (log > 31) {
		fprintf(stderr, "ERROR: intlog2 called with argument %ld which is not a power of 2\n", n);
		exit(1);
	}
	return log;
}

int
main(int argc, char **argv)
{
	FILE *f;

	f = fopen("nvm.inc", "w");
	if (!f) {
		perror("nvm.inc");
		return 1;
	}
	fprintf(f, "; NVM BIOS assembly language include file\n");
	fprintf(f, "; generated automatically from nvm.h\n");
	fprintf(f, "; *** DO NOT EDIT ***\n\n");
	fprintf(f, "; To change something here, change it in nvm.h and\n");
	fprintf(f, "; remake (the makefile dependencies should take care\n");
	fprintf(f, "; of the rest.\n");
	fprintf(f, "; For documentation & comments, see nvm.h.\n\n");

	fprintf(f, "APPPACKSIZE  = %d\n", APPPACKSIZE);
	fprintf(f, "FILEPACKSIZE = %d\n", FILEPACKSIZE);
	fprintf(f, "APPNAMELEN   = %d\n", APPNAMELEN);
	fprintf(f, "FILENAMELEN  = %d\n", FILENAMELEN);

	fprintf(f, "DIRENTRYSIZE = %d\n", (int)sizeof(DIRENTRY));

	fprintf(f, "NUMFILES     = %d\n", NUMFILES);
	fprintf(f, "CHKSUMOFFSET = %d\n", CHKSUMOFFSET);
	fprintf(f, "VERSOFFSET   = %d\n", VERSOFFSET);
	fprintf(f, "USEDSPACEOFFSET = %d\n", USEDSPACEOFFSET);
	fprintf(f, "FATOFFSET    = %d\n", FATOFFSET);
	fprintf(f, "FIRSTFATBLOCK = %d\n", FIRSTFATBLOCK);
	fprintf(f, "REAL_FATOFFSET  = %d\n", REAL_FATOFFSET);
	fprintf(f, "BLOCKSIZE    = %d\n", BLOCKSIZE);
	fprintf(f, "LOG2_BLOCKSIZE = %d\n", intlog2(BLOCKSIZE));
	/*fprintf(f, "SECTORSIZE   = %d\n", SECTORSIZE);
	fprintf(f, "LOG2_SECTORSIZE = %d\n", intlog2(SECTORSIZE));*/
	fprintf(f, "FATBLOCKS    = %d\n", FATBLOCKS);
	fprintf(f, "DIROFFSET    = %d\n", (int)DIROFFSET);
	fprintf(f, "DIRBLOCKS    = %d\n", (int)DIRBLOCKS);
	fprintf(f, "TOTALBLOCKS  = %d\n", TOTALBLOCKS);
	fprintf(f, "NUMDIRENTRIES = %d\n", (int)NUMDIRENTRIES);
	fprintf(f, "FIRSTDATABLOCK = %d\n", (int)FIRSTDATABLOCK);
	fprintf(f, "FIRSTDIRBLOCK = %d\n", FIRSTDIRBLOCK);
	fprintf(f, "DATAOFFSET   = %d\n", DATAOFFSET);
	fprintf(f, "DATABLOCKS   = %d\n", DATABLOCKS);
	fprintf(f, "\n");
	fprintf(f, "FAT_FREE     = %d\n", FAT_FREE);
	fprintf(f, "FAT_END      = %d\n", FAT_END);
	fprintf(f, "FAT_EMPTYFILE = %d\n", FAT_EMPTYFILE);
	fprintf(f, "\nENOINIT = %d\n", ENOINIT);
	fprintf(f, "ENOSPC  = %d\n", ENOSPC);
	fprintf(f, "EFILNF  = %d\n", EFILNF);
	fprintf(f, "EINVFN  = %d\n", EINVFN);
	fprintf(f, "ERANGE  = %d\n", ERANGE);
	fprintf(f, "ENFILES = %d\n", ENFILES);
	fprintf(f, "EIHNDL  = %d\n", EIHNDL);

	fclose(f);
	return 0;
}

