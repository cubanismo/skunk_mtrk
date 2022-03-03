/* #define DEBUGGING */

#include "nvm.h"
#include <stdarg.h>

extern byte NVRAM[];
#ifdef DEBUGGING
extern int debug_flag;
#include <stdio.h>
#endif


/*------------------------------------
Global variables
-------------------------------------*/

/* packed form of application name */
PACK3 gl_appname[APPPACKSIZE];

/* packed form of current file name */
PACK3 gl_filename[FILEPACKSIZE];

/* structures for open files */
struct filestruct {
	unsigned int startblock;	/* starting block for file, or 0 if file is not open */
	unsigned long offset;	/* current file offset */
}file_handle[NUMFILES];

/* 16K buffer for use by read/write routines
 * this is 0L if NVM_init hasn't been called
 */
byte *scratchbuf;

/* current location for search first/next */
short search_location;

/* current search flags; 0 = all applications,
   1 = only this application
 */
short search_flags;

#define NUMCHARS 40
static const byte charset[NUMCHARS] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
	'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
	'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
	'Y', 'Z', '0', '1', '2', '3', '4', '5',
	'6', '7', '8', '9', ':', '\'', '.', ' '
/* space must be the last character */
};

/*
 * Memory access functions; GETBYTE(n) gets byte
 * "n" from NV RAM, and PUTBYTE(n, data) sets byte
 * "n" in the NV RAM to the value "data".
 * GETBYTE() is unbuffered, but PUTBYTE() is buffered,
 * and must ultimately be followed by a FLUSH() if
 * data is to be written back
 */

static unsigned short cur_sector;		/* which 8K sector are we writing on? */

static void FLUSH(void)
{
	unsigned int i, base;
#ifdef DEBUGGING
	extern int sleep(int);
#endif

	if (cur_sector != -1) {
#ifdef DEBUGGING
		sleep(2);			/* simulate the time it takes to erase a block */
#endif
		base = cur_sector*SECTORSIZE;
		for (i = 0; i < SECTORSIZE; i++) {
			NVRAM[base+i] = scratchbuf[i];
		}
		cur_sector = -1;
	}
}

static int
GETBYTE(unsigned long n)
{
	unsigned int sectornum, sectoroffset;

	sectornum = (n/SECTORSIZE);
	sectoroffset = (n%SECTORSIZE);

	/* check first for data that's in the write buffer but hasn't
	 * actually been flushed back to the cart
	 */
	if (sectornum == cur_sector) {
		return scratchbuf[sectoroffset];
	}

	return NVRAM[(n)];
}

static void
PUTBYTE(unsigned long n, int data)
{
	unsigned int sectornum, sectoroffset;

	sectornum = (n/SECTORSIZE);
	sectoroffset = (n%SECTORSIZE);

	if (sectornum != cur_sector) {
		unsigned int i, base;

		FLUSH();

		/* read the new sector into the buffer */
		/* NOTE: GETBYTE() will work correctly (unbuffered) because FLUSH() invalidated cur_sector */
		base = sectornum*SECTORSIZE;
		for (i = 0; i < SECTORSIZE; i++) {
			scratchbuf[i] = GETBYTE(base+i);
		}
		cur_sector = sectornum;
	}
	scratchbuf[sectoroffset] = data;
}


static void
pack2ascii( PACK3 *iname, byte *exname, int numwords )
{
	PACK3 p;
	unsigned let1, let2, let3;

	while (numwords > 0) {
		p = *iname++;
		let3 = (p % NUMCHARS);
		p /= NUMCHARS;
		let2 = (p % NUMCHARS);
		p /= NUMCHARS;
		let1 = p;
		*exname++ = charset[let1];
		*exname++ = charset[let2];
		*exname++ = charset[let3];
		--numwords;
	}
	*exname++ = 0;
}

static unsigned
fivebit( byte c, int *endseen )
{
	unsigned i;

	if (c == 0)
		*endseen = 1;
	if (*endseen)
		return NUMCHARS-1;
	if ( (c >= 'a') && (c <= 'z') )
		return (c - 'a');
	for (i = 0; i < NUMCHARS-1; i++) {
		if (c == charset[i])
			break;
	}
	return i;	/* if the character wasn't found, use space (last character) */
}

static void
ascii2pack( byte *exname, PACK3 *iname, int numwords )
{
	PACK3 p;
	unsigned let1, let2, let3;
	int eosflag;

	eosflag = 0;
	while (numwords > 0) {
		let1 = fivebit(*exname++, &eosflag);
		let2 = fivebit(*exname++, &eosflag);
		let3 = fivebit(*exname++, &eosflag);
		p = let3 + NUMCHARS*(let2+(NUMCHARS*let1));
		*iname++ = p;
		--numwords;
	}
}

/*
 * see if two packed arrays match
 * return 1 if they do, 0 if they don't
 */
static int
matchpack( PACK3 *pa, PACK3 *pb, int size )
{
	word *a, *b;

	a = (word *)pa;
	b = (word *)pb;

	while (size > 0) {
		if (*a != *b)
			return 0;
		a++;
		b++;
		--size;
	}
	return 1;
}

/*
 * Read/Write directory entry #n into the given buffer
 */

static void
DirRW( int rwflag, int n, byte *dbuf )
{
	unsigned long byteptr;
	unsigned long i;

	byteptr = DIROFFSET+(n*sizeof(DIRENTRY));
	for (i = 0; i < sizeof(DIRENTRY); i++) {
		if (rwflag == 0)		/* reading */
			*dbuf++ = GETBYTE(byteptr++);
		else
			PUTBYTE(byteptr++, *dbuf++);
	}
}

/*
 * Read directory entry #n into the given buffer
 */
static inline void
FetchDir( int n, DIRENTRY *dptr )
{
	DirRW(0, n, (byte *)dptr);
}

/*
 * Write directory entry #n from the given buffer
 */
static inline void
PutDir( int n, byte *dbuf )
{
	DirRW(1, n, dbuf);
}

/*
 * Allocate "numblocks" blocks from the FAT; return
 * the first block allocated on success, or a (negative) error
 * code on failure
 */
static int
AllocBlocks( int numblocks )
{
	unsigned int usedspace;
	unsigned int firstblock;
	unsigned int lastblock;
	unsigned int i;

	if (numblocks == 0) return FAT_EMPTYFILE;		/* special marker for empty files */

	/* make sure there's enough room */
	usedspace = GETBYTE(USEDSPACEOFFSET) + numblocks;
	if (usedspace >= DATABLOCKS) {
#ifdef DEBUGGING
		printf("AllocBlocks: usedspace = %d, DATABLOCKS = %d\n", usedspace, DATABLOCKS);
#endif
		return ENOSPC;
	}

	/* update the amount of space used */
	PUTBYTE(USEDSPACEOFFSET, usedspace);

	/* now look for blocks to allocate */
	firstblock = 0;
	lastblock = 0;
	i = FIRSTDATABLOCK;
	while (numblocks > 0 && i < TOTALBLOCKS) {
		if (GETBYTE(FATOFFSET+i) == FAT_FREE) {
			if (lastblock > 0)
				PUTBYTE(FATOFFSET+lastblock, i);
			else
				firstblock = i;
			lastblock = i;
			numblocks--;
		}
		i++;
	}
	if (numblocks != 0) {		/* FATAL ERROR */
		return ERANGE;
	}
	PUTBYTE( FATOFFSET+lastblock, FAT_END );
#ifdef DEBUGGING
	printf("AllocBlocks: returning %d\n", firstblock);
#endif
	return firstblock;
}

/*
 * FreeBlocks( int blocknum )
 * Free the FAT blocks allocated in the chain starting at "blocknum"
 */
static void
FreeBlocks( unsigned int blocknum )
{
	unsigned int nextblock;
	unsigned int usedblocks;

	if (blocknum <= FAT_EMPTYFILE)
		return;
	usedblocks = GETBYTE( USEDSPACEOFFSET );
	while(usedblocks > 0) {
		nextblock = GETBYTE(FATOFFSET+blocknum);
		PUTBYTE(FATOFFSET+blocknum, FAT_FREE);
		if (nextblock == FAT_FREE)
			break;
		usedblocks--;
		if (nextblock == FAT_END)
			break;
		blocknum = nextblock;
	}
	PUTBYTE( USEDSPACEOFFSET, usedblocks );
}

/*
 * FindFile( appname, filename, dirptr )
 * find which directory block (if any)
 * refers to the indicated file, and copy it
 * into the space pointed to by "dirptr"
 * returns EFILNF if the file is not
 * found
 */
static int
FindFile( PACK3 *appname, PACK3 *filename, DIRENTRY *d )
{
	int i;

	for (i = 0; i < NUMDIRENTRIES; i++) {
		FetchDir(i, d );
		if (d->startblock &&			/* deleted entries have starting block 0 */
		    matchpack(d->appname, appname, APPPACKSIZE) &&
		    matchpack(d->filename, filename, FILEPACKSIZE))
			return i;
	}
	return EFILNF;
}

/*
 * delete a file with the given application name
 * and file name
 */

static int
DeleteFile( PACK3 *appname, PACK3 *filename )
{
	DIRENTRY d;
	int i;
	unsigned long frombyte;

#ifdef DEBUGGING
	if (debug_flag) {
		char appbuf[80], filebuf[80];
		pack2ascii(appname, appbuf, APPPACKSIZE);
		pack2ascii(filename, filebuf, FILEPACKSIZE);
		printf("Delete File: App:[%s] File:[%s]\n", appbuf, filebuf);
	}
#endif
	/* find the file's directory entry */
	i = FindFile(appname, filename, &d );
	if (i < 0) return i;		/* file not found, maybe? */

	/* OK, now mark it as deleted by putting 0 in for the
	 * starting block and number of blocks (in the copy
	 * actually in NVRAM; the local copy in d remains
	 * untouched, so we can free its blocks)
	 */
	frombyte = DIROFFSET + i*sizeof(DIRENTRY);
	PUTBYTE(frombyte, 0);
	frombyte++;
	PUTBYTE(frombyte, 0);

	/* free the FAT chain associated with this file */
	FreeBlocks( d.startblock );
	return 0;

}

static inline long
NVM_init( char *appname )
{
	int i;

	ascii2pack(appname, gl_appname, APPPACKSIZE);
	cur_sector = -1;		/* not currently using any sector */
	for (i = 0; i < NUMFILES; i++) {
		file_handle[i].startblock = 0;
	}

	/* we probably should run a checksum and do
	 * version number checking here!!
	 */
	return 0;
}

/*
 * same function works for either open or create;
 * if openflag is 1, we're just opening, otherwise
 * we're creating
 */


static inline long
NVM_createopen( int openflag, char *filename, long size )
{
	int i;
	int dirnum;
	int handle;
	DIRENTRY d;
	int numblocks;
	int startblock;

	/* make sure we can open a file */
	for (handle = 0; handle < NUMFILES; handle++) {
		if (file_handle[handle].startblock == 0) goto OKtoopen;
	}
	return ENFILES;

OKtoopen:
	ascii2pack(filename, gl_filename, FILEPACKSIZE);
	if (openflag == 1) {
		/* just open the file; we don't have to (or want to) create it */
		i = FindFile(gl_appname, gl_filename, &d);
		if (i == EFILNF)
			return i;
		startblock = d.startblock;
	} else {
		/* create the file */
		/* delete any existing file with this name */
		(void)DeleteFile(gl_appname, gl_filename);

		/* see if there are any free directory entries */
		for (dirnum = 0; dirnum < NUMDIRENTRIES; dirnum++) {
			FetchDir( dirnum, &d );
			if (d.startblock == 0)		/* found a free one */
				goto dirOK;
		}
		/* oops! No directory entry found */
#ifdef DEBUGGING
		printf("NVM_create: no free directory entries\n");
#endif
		return ENOSPC;
	dirOK:

		/* allocate the space for the file */
		numblocks = (size+BLOCKSIZE-1)/BLOCKSIZE;
		startblock = AllocBlocks( numblocks );

		if (startblock <= 0)
			return ENOSPC;		/* oops! no space found for the file */

		d.startblock = startblock;
		d.numblocks = numblocks;
		for (i = 0; i < APPPACKSIZE; i++)
			d.appname[i] = gl_appname[i];
		for (i = 0; i < FILEPACKSIZE; i++)
			d.filename[i] = gl_filename[i];

		PutDir( dirnum, (char *)&d );
	}

	file_handle[handle].startblock = startblock;
	file_handle[handle].offset = 0;
	return handle;
}

static inline long
NVM_delete( char *appname, char *filename )
{
	PACK3 lcl_appname[APPPACKSIZE];
	PACK3 *appptr;

	ascii2pack(filename, gl_filename, FILEPACKSIZE);
	if (appname) {
		ascii2pack(appname, lcl_appname, APPPACKSIZE);
		appptr = lcl_appname;
	} else appptr = gl_appname;
	return DeleteFile(appptr, gl_filename);
}
	
static inline long
NVM_close( int handle )
{
	file_handle[handle].startblock = 0;
	return 0;
}

/*
 * utility function: converts a byte offset in a file (which starts at 'startblock')
 * to an absolute offset in memory
 * returns ERANGE if we go past the end of file
 */
static long
file_offset( unsigned startblock, long offset )
{
	if (offset < 0) return ERANGE;
	while (offset >= BLOCKSIZE) {
		if (startblock == FAT_FREE || startblock == FAT_END || startblock == FAT_EMPTYFILE)
			return ERANGE;
		startblock = GETBYTE(FATOFFSET+startblock);
		offset -= BLOCKSIZE;
	}
	if (startblock == FAT_FREE || startblock == FAT_END || startblock == FAT_EMPTYFILE)
		return ERANGE;
	return offset+startblock*BLOCKSIZE;
}

static inline long
NVM_seek(int handle, long offset, int flag)
{
	long retval;

	if (file_handle[handle].startblock == 0)
		return EIHNDL;
	if (flag == 1)
		offset += file_handle[handle].offset;
	retval = file_offset( file_handle[handle].startblock, offset);
	if (retval < 0) return retval;
	return offset;
}

static inline long
NVM_rw(int rw, int handle, byte *ioptr, long count)
{
	unsigned startblock;
	long offset;
	long foffset;
	long bytesdone;

	startblock = file_handle[handle].startblock;
	if (startblock == 0) return EIHNDL;
	offset = file_handle[handle].offset;

	bytesdone = 0;
	while (bytesdone < count) {
		foffset = file_offset( startblock, offset );
		if (foffset < 0) break;
		offset++;
		bytesdone++;
		if (rw == 1) {			/* write command */
			PUTBYTE( foffset, *ioptr );
			ioptr++;
		} else {
			*ioptr++ = GETBYTE( foffset );
		}
	}
	file_handle[handle].offset = offset;	/* update file offset */
	return bytesdone;
}

static inline int
NVM_initsearch( long arg )
{
	if (arg == 0 || arg == 1)
		search_flags = arg;
	else
		return EINVFN;
	search_location = 0;
	return 0;
}

static inline long
NVM_search( SEARCHBUF *sb )
{
	DIRENTRY d;
	int found = 0;

	while (search_location < NUMDIRENTRIES && !found) {
		FetchDir( search_location, &d );
		search_location++;
		if (d.startblock != 0) {	/* found a valid entry */
		/* stop if the pattern matches the search pattern */
			if (search_flags == 0 || matchpack(d.appname, gl_appname, APPPACKSIZE));
				found = 1;
		}
	}

	if (!found) {
		return EFILNF;
	}
	sb->size = d.numblocks * BLOCKSIZE;
	pack2ascii(d.appname, sb->appname, APPPACKSIZE);
	pack2ascii(d.filename, sb->filename, FILEPACKSIZE);
	return 0;
}

static inline long
NVM_inquire( long *totalspace, long *freespace )
{
	*totalspace = DATABLOCKS*(long)BLOCKSIZE;
	*freespace = (DATABLOCKS-GETBYTE(USEDSPACEOFFSET))*(long)BLOCKSIZE;
	return 0;
}

long
NVM_Bios( short opcode, ... )
{
	long ret;
	char *appname, *filename;
	short handle;
	long size;
	va_list args;
	SEARCHBUF *sb;
	long sflags;
	byte *iobuf;
	short flag;

	va_start(args, opcode);

	if (opcode == 0) {		/* NVM_Init call */
		appname = va_arg(args, char *);
		scratchbuf = va_arg(args, byte *);
		return NVM_init( appname );
	}

	if (scratchbuf == 0)
		return ENOINIT;

	switch( opcode ) {
	case 1: /* create */
	case 2:	/* open */
		filename = va_arg(args, char *);
		size = va_arg(args, long);		/* will be dummy for open() */
		ret = NVM_createopen( opcode-1, filename, size );
		break;
	case 3: /* close */
		handle = va_arg(args, short);
		ret = NVM_close( handle );
		break;
	case 4: /* delete */
		appname = va_arg(args, char *);
		filename = va_arg(args, char *);
		ret = NVM_delete( appname, filename );
		break;
	case 5:	/* read */
	case 6: /* write */
		handle = va_arg(args, short);
		iobuf = va_arg(args, byte *);
		size = va_arg(args, long);
		/* opcode - 5 will be 0 for read and 1 for write */
		ret = NVM_rw( opcode - 5, handle, iobuf, size );
		break;
	case 7: /* search first */
	case 8: /* search next */
		sb = va_arg(args, SEARCHBUF *);
		if (opcode == 7) {
			sflags = va_arg(args, long);
			ret = NVM_initsearch( sflags );
			if (ret < 0)		/* an error? */
				return ret;
		}
		ret = NVM_search( sb );
		break;
	case 9: /* seek */
		handle = va_arg(args, short);
		size = va_arg(args, long);
		flag = va_arg(args, short);
		ret = NVM_seek( handle, size, flag );
		break;
	case 10: /* inquire */
		{ long *totspc, *freespc;
		totspc = va_arg(args, long *);
		freespc = va_arg(args, long *);
		ret = NVM_inquire( totspc, freespc );
		break;
		}
	default:
		ret = EINVFN;
	}
	FLUSH();	/* flush the scratch buffer, if necessary */
	return ret;
}

