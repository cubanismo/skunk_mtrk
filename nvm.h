/*
 * NVM BIOS header file
 *
 * How it works:
 * The NVM is broken up into 512 byte blocks.
 * The whole of NVM is 128K, i.e. 256 blocks.
 * Blocks 0-7 (the first 4K) are reserved for
 * system use.
 *
 * Block 0 is the file allocation table (FAT). Each
 * FAT table entry is either 0 (for a free block),
 * 1 (for the end of a FAT chain), or a number indicating
 * the next block in a file. The first 8 entries in
 * the FAT are reserved for system use.
 *
 * Blocks 1-7 are the volume table of contents.
 * This consists of 170 directory entries, each
 * 20 bytes long. A directory entry consists of:
 *
 * starting block #		2 bytes
 * number of blocks in file	2 bytes
 * application name		10 bytes (15 characters)
 * file name			6 bytes (9 characters)
 *
 * File and application names are packed with
 * 3 characters per word; each character may come
 * from a 40 character set consisting of:
 *
 * capital letters
 * space
 * the numbers 0-9
 * the following punctuation marks: : ' .
 *
 * Empty directory entries are indicated by starting
 * blocks of 0.
 */

/* size of packed file names (in words) */
#define APPPACKSIZE	5
#define FILEPACKSIZE	3

/* size of unpacked file names (including trailing nulls) */
#define APPNAMELEN	15
#define FILENAMELEN	9

typedef unsigned char byte;
typedef unsigned short word;

typedef unsigned short PACK3;

typedef struct direntry {
	byte	startblock;
	byte	numblocks;
	PACK3	appname[APPPACKSIZE];
	PACK3	filename[FILEPACKSIZE];
} DIRENTRY;

/*
 * type for searches
 */
typedef struct searchbuf {
	long	size;
	char	appname[APPNAMELEN+1];
	char	filename[FILENAMELEN+1];
} SEARCHBUF;


#define NUMFILES	3	/* maximum number of opened files */

#define CHKSUMOFFSET	0	/* offset from start of memory to checksum */
#define VERSOFFSET	2	/* offset from start of memory to version number */
#define USEDSPACEOFFSET	3	/* offset from start of memory to byte holding amount of used space */
#define REAL_FATOFFSET	8	/* byte for first real FAT entry */

#define BLOCKSIZE	512
#define FATBLOCKS	1	/* blocks used by File Allocation Table */
#define DIROFFSET	(BLOCKSIZE)	/* start of directory table */
#define DIRBLOCKS	7	/* blocks allocated to directory */
#define TOTALBLOCKS	256	/* total blocks in cart */

#ifdef DEBUGGING
#define NUMDIRENTRIES	4	/* number of directory entries */
#else
#define NUMDIRENTRIES	((DIRBLOCKS*BLOCKSIZE)/sizeof(DIRENTRY))	/* number of directory entries */
#endif

#define FIRSTFATBLOCK	0
#define FATOFFSET	0
#define FIRSTDIRBLOCK	FATBLOCKS
#define FIRSTDATABLOCK	(DIRBLOCKS+FATBLOCKS)
#define DATAOFFSET	(FIRSTDATABLOCK*BLOCKSIZE)	/* byte where data starts */
#define DATABLOCKS	(TOTALBLOCKS-FIRSTDATABLOCK)	/* number of blocks of data */

/* codes for FAT table entries */
#define FAT_FREE	0		/* this must be good for you (OK, it's a free FAT table entry) */
#define FAT_END		1		/* end of a FAT chain */
#define FAT_EMPTYFILE	2		/* fat table entry for 0 length files */

/* size of NV RAM sectors */
#define SECTORSIZE (16*1024)

/* possible error codes */
#define ENOINIT		-1	/* NVM_INIT has not been called */
#define ENOSPC		-2	/* insufficient space for Create() call */
#define EFILNF		-3	/* file not found in open() or search() */
#define EINVFN		-4	/* invalid function */
#define ERANGE		-5	/* seek out of range */
#define ENFILES		-6	/* no more file handles available */
#define EIHNDL		-7	/* invalid handle passed to function */
