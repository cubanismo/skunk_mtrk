/*
 * Simple file manager for Jaguar NVRAM flash cards.
 * Copyright 1995 Atari Corporation.
 * All Rights Reserved.
 *
 * Version 1.01 2/22/95	Eric R. Smith
 */
#define NEEDEMBOSS 0		/* set to 0 if we don't need embossing code */

#include "nvm.h"
#include "olist.h"
#include "font.h"
#include "blit.h"
#include "joypad.h"
#include "stdlib.h"
#include "string.h"
#include "sprintf.h"
#include "printf.h"
#if defined(USE_SKUNK)
#include "skunk.h"
#endif

/* various definitions */
#if defined(STANDALONE)
#define TITLE "SKUNK STANDALONE TRACK"	/* name of this application as the user sees it */
#define OURNAME "MEMTRACK ALONE"	/* name of this application for file purposes */
#else /* defined(STANDALONE) */
#define TITLE "Skunk Memory Track Manager"	/* name of this application as the user sees it */
#define OURNAME "MEMORY TRACK"		/* name of this application for file purposes */
#endif /* defined(STANDALONE) */

#define DEFAULTBGCOLOR 0x7860		/* color of the background */
#define ALERTBOXCOLOR 0x7858	/* color of alert boxes */
#define SHADOWCOLOR 0x7800	/* color of shadows */
#define LIGHTCOLOR 0x7890	/* color of highlights */
#define MEDLIGHTCOLOR 0x7880	/* a slightly less intense highlight */

int bgcolor;			/* actual background color to use */
#define BGCOLOR bgcolor

#define TEXTCOLOR 0x7800	/* normal color for text */
#define SELTEXTCOLOR 0xa6a0	/* color for selected text */
#define BOXCOLOR 0xc4a0		/* color for cursor box */

#define DRAWWIDTH  280		/* width of usable screen area */
#define DRAWHEIGHT 200		/* height of usable screen area */
#define XOFFSET 4

/* external variables */
extern int VIDpal;		/* flag: 0 = NTSC, 1 = PAL */
extern int VID_vdb;		/* vertical display begin */
extern int VIDwidth;		/* width of screen in clock cycles */
extern FNThead medfnt[], bigfnt[];
extern FNThead arrowfnt[];

/* logo stuff */
#if NEEDEMBOSS
extern unsigned char jaglogo[];
extern unsigned short jaglogopal[];
unsigned short *logo;
#else
extern unsigned short emlogo[];			/* embossed Jaguar logo */
#define logo emlogo
#endif

/* external functions */
extern void VIDsync( void );
extern void BLTrect(void *, long, int, int, int, int, unsigned);
extern void BLTline(void *, long, int, int, int, int, unsigned);

/* we only need 1 object, the main screen: 320x288 16 bit CRY */
/* here is the ROM original, which gets copied to RAM and
 * then modified for PAL or NTSC modes
 */
union olist rom_olist[2] =
{
	{{OL_BITMAP,		/* type */
	 0, 0,			/* x, y */
	 0L,			/* link */
	 (void *)0,		/* data */
	 288, 320*2/4, 320/4,	/* height, dwidth, iwidth */
	 4, 2, 0, 0, 0,		/* depth, pitch, index, flags, firstpix */
	 0,0,0}},		/* scaling stuff */

	{{OL_STOP}}
};

/* copyright notice */
static char copyright[] = "Copyright 1995 Atari Corporation. All Rights Reserved.";

/* the RAM copy of the object list */
union olist ram_olist[2];

/* packed object lists: one for each screen */
long packed_scrn1[128];
long packed_scrn2[128];

/* memory for the screen buffer; this should be enough so that we can double buffer the screen */
char *screen;
char *screenguard;

/* blitter flags for the screen */
#define screenflags (WID320|PIXEL16|PITCH2)

/*
 * data type for the list of files
 */
typedef struct file {
/* first items much match definition of SEARCHBUF in nvm.h */
	long	size;				/* file size in bytes */
	char	appname[APPNAMELEN+1];		/* name of application that created file */
	char	filename[FILENAMELEN+1];	/* name of file */
	int	selected;			/* flag: 1 if file is selected */
} File;

/* various global variables */
int upperx, uppery;			/* upper left corner of the usable area on screen */
char *disp_ptr;				/* the buffer currently being displayed */
char *draw_ptr;				/* the buffer currently being drawn to */
File *file;				/* list of files */
int numfiles;				/* number of files on cart */
int topfile;				/* index of file being displayed on top line */
int cursfile;				/* index of cursor file */
char *NVMwork;				/* work area for NVM BIOS */
char *NVMworkguard;			/* magic string right after this (for debugging, to make sure the NVM Bios isn't overwriting */
long totalbytes;			/* total bytes on cartridge */
long freebytes;				/* free bytes on cartridge */

#ifdef TEST
#define NVM_BIOS ((long (*)(int,...))0x4004L)
#else
#define NVM_BIOS ((long (*)(int,...))0x2404L)
#if defined(STANDALONE)
#define NVM_MAGIC (*(long *)0x2400L)
#define GOOD_MAGIC 0x5f4e564dL
#endif /* defined(STANDALONE) */
#endif /* TEST */

#define NVM_Init(app_name, work_area)	NVM_BIOS(0, (app_name), (work_area))
#define NVM_Create(file, size)		NVM_BIOS(1, (file), (size))
#define NVM_Open(file)			NVM_BIOS(2, (file))
#define NVM_Close(handle)		NVM_BIOS(3, (handle))
#define NVM_Delete(app_name, file_name)	NVM_BIOS(4, (app_name), (file_name))
#define NVM_Read(handle, bufp, count)	NVM_BIOS(5, (handle), (bufp), (count))
#define NVM_Write(handle, bufp, count)	NVM_BIOS(6, (handle), (bufp), (count))
#define NVM_Sfirst(search_buf)		NVM_BIOS(7, (search_buf), 0L)
#define NVM_Snext(search_buf)		NVM_BIOS(8, (search_buf))
#define NVM_Lseek(handle, offset, flag)	NVM_BIOS(9, (handle), (offset), (flag))
#define NVM_Info(total, free)		NVM_BIOS(10, (total), (free))

/*
 * JOYPAD input function: reads the joypad, returns 
 */
static long lastjoy;		/* buttons held down on joypad 1 */
static unsigned long joytimer;
#define REPEATTIME 10		/* repeat time in frames */
#define SECONDREPEAT 9

long
JOYrepeat(JOYSTREAM *J)
{
	long buts;
	long edges;

	buts = JOYget(J) & (JOY_UP|JOY_DOWN|JOY_LEFT|JOY_RIGHT);
	edges = JOYedge(J);

	if (buts == lastjoy) {		/* still the same buttons held down? */
		joytimer++;
		if (joytimer > REPEATTIME) {
			joytimer = SECONDREPEAT;
			return buts|edges;
		} else {
			return edges;
		}
	}
	lastjoy = buts;
	joytimer = 0;
	return buts|edges;
}

/*
 * sorting functions
 */

/* current sorting function */
int (*cursort)(const void *, const void *);

/*
 * (1) for sorting files in alphabetical order
 */
int
alphacmp( const void *a, const void *b )
{
	const File *filea, *fileb;
	int i;

	filea = a;
	fileb = b;

	i = strcmp(filea->appname, fileb->appname);
	if (i == 0)
		i = strcmp(filea->filename, fileb->filename);

	return i;
}

/*
 * (2) for sorting files in size order, then alphabetical order
 */
int
sizecmp( const void *a, const void *b )
{
	const File *filea, *fileb;
	long diff;

	filea = a;
	fileb = b;

	diff = fileb->size - filea->size;

/* if the files are the same size, sort alphabetically */
	if (diff == 0)
		return alphacmp( a, b );
	return diff;
}

/*
 * function: switch screens
 */
void
SwitchScreen( void )
{
	char *temp;

	/* swap display and draw pointers */
	temp = draw_ptr;
	draw_ptr = disp_ptr;
	disp_ptr = temp;

	/* display the correct object list */
	if (disp_ptr == screen) {
		OLPset(packed_scrn1);
	} else {
		OLPset(packed_scrn2);
	}

	/* wait for vblank */
	VIDsync();
}

/*
 * get the list of files
 */
void
GetFiles( void )
{
	int status;
	SEARCHBUF s;
	int i;

	/* get disk information */
	NVM_Info(&totalbytes, &freebytes);

	/* count how many files there are */
	numfiles = 0;
	status = NVM_Sfirst(&s);
	while (status >= 0) {
		numfiles++;
		status = NVM_Snext(&s);
	}

	if (numfiles == 0) {
		file = (File *)0;
		return;
	}
	/* allocate space for the files */
	file = malloc( numfiles * sizeof(File) );

	/* now read the file info */
	NVM_Sfirst(&s);
	printf("Found file: %s %s\n", s.appname, s.filename);
	strcpy(file[0].appname, s.appname);
	strcpy(file[0].filename, s.filename);
	file[0].size = s.size;
	file[0].selected = 0;
	for (i = 1; i < numfiles; i++) {
		NVM_Snext(&s);
		printf("Found file: %s %s\n", s.appname, s.filename);
		strcpy(file[i].appname, s.appname);
		strcpy(file[i].filename, s.filename);
		file[i].size = s.size;
		file[i].selected = 0;
	}
	/* now sort the files */
	qsort(file, (size_t)numfiles, sizeof(file[0]), cursort);
}

/*
 * free files
 */
void
FreeFiles( void )
{
	free(file);
	file = 0;
}

/*
 * strip trailing blanks from a string
 */
void
StripBlanks( char *str )
{
	char *s = str;

	/* move to the end of the string */
	while (*s && *(s+1)) s++;

	/* now strip blanks */
	while ( (s >= str) && (*s == ' ') )
		*s-- = 0;
}

/*
 * draw a 3D box around (minx,miny,wide,high). If "inout" is 0, make the box look pushed in, otherwise pushed out.
 */
#define IN 0
#define OUT 1

void
Draw3DBox( int minx, int miny, int wide, int high, int inout )
{
	unsigned tcolor, lcolor;	/* color for top and left sides */
	unsigned bcolor, rcolor;	/* color for bottom and right sides */
	int maxx, maxy;

	maxx = minx + wide - 1;
	maxy = miny + high - 1;

	minx -= 2;
	miny -= 2;
	maxx += 2;
	maxy += 2;
	if (inout == IN) {
		tcolor = lcolor = SHADOWCOLOR;
		bcolor = LIGHTCOLOR;
		rcolor = MEDLIGHTCOLOR;
	} else {
		tcolor = LIGHTCOLOR;
		lcolor = MEDLIGHTCOLOR;
		bcolor = rcolor = SHADOWCOLOR;
	}
	BLTline(draw_ptr, screenflags, minx, miny, maxx, miny, tcolor);	/* top */
	BLTline(draw_ptr, screenflags, minx, miny+1, maxx-1, miny+1, tcolor);
	BLTline(draw_ptr, screenflags, minx, miny, minx, maxy, lcolor);	/* left edge */
	BLTline(draw_ptr, screenflags, minx+1, miny+1, minx+1, maxy-1, lcolor);

	BLTline(draw_ptr, screenflags, maxx, maxy, maxx, miny, rcolor);		/* right edge */
	BLTline(draw_ptr, screenflags, maxx-1, maxy-1, maxx-1, miny+1, rcolor);
	BLTline(draw_ptr, screenflags, maxx-1, maxy-1, minx+1, maxy-1, bcolor);		/* bottom */
	BLTline(draw_ptr, screenflags, maxx, maxy, minx, maxy, bcolor);

}
/*
 * draw a "normal" box around (minx,miny,wide,high). If "inout" is 0, make the box look pushed in, otherwise pushed out.
 */
void
DrawBox( int minx, int miny, int wide, int high, unsigned color )
{
	int maxx, maxy;

	maxx = minx + wide - 1;
	maxy = miny + high - 1;

	BLTline(draw_ptr, screenflags, minx, miny, maxx, miny, color);	/* top */
	BLTline(draw_ptr, screenflags, minx, miny, minx, maxy, color);	/* left edge */
	BLTline(draw_ptr, screenflags, maxx, maxy, maxx, miny, color);	/* right edge */
	BLTline(draw_ptr, screenflags, maxx, maxy, minx, maxy, color);	/* bottom edge */
}

/*
 * draw the screen
 */

void
DrawScreen( void )
{
	int cury, curx;
	int x;
	unsigned long box;
	long tmp;
	int fntheight;			/* height of text font */
	int fntwidth;			/* maximum width for text font (width of "M") */
	int filesonscreen;		/* number of files that can be displayed on screen */
	int maxx;			/* X value of right hand side of box */
	int i;
	int arrowh;			/* height of an up or down arrow */
	char buf[256];			/* sprintf buffer */

	cury = uppery;
	curx = upperx;

	/* fill whole screen with background color */
	/* BLTrect(draw_ptr, screenflags, 0, 0, 320, 288, BGCOLOR); */
	A1_BASE = draw_ptr;
	A1_FLAGS = screenflags|XADDPHR;
	A1_PIXEL = 0;
	A1_STEP = 0x000010000L | ((-320L) & 0x0000ffff);
	B_PATD[0] = ((long)BGCOLOR << 16) | BGCOLOR;
	B_PATD[1] = ((long)BGCOLOR << 16) | BGCOLOR;
	B_IINC = VIDpal ? 0x00ffff60 : 0x00ffff40;
	B_COUNT = (288L << 16) | 320;
	B_CMD = UPDA1|GOURD|PATDSEL;

	/* draw the Jaguar logo */
	A1_BASE = draw_ptr;
	A1_FLAGS = screenflags|XADDPIX;
	A1_PIXEL = ((long)(uppery + (DRAWHEIGHT-80)/2) << 16) | (upperx + (DRAWWIDTH-224)/2);
	tmp = 0x00010000L | ((-224L) & 0x0000ffff);
	A1_STEP = tmp;
	A2_STEP = tmp;

	A2_BASE = logo;
	A2_FLAGS = WID224|PIXEL16|PITCH1|XADDPIX;
	A2_PIXEL = 0;

	B_PATD[0] = 0;
	B_PATD[1] = 0;

	B_COUNT = (80L << 16L) | 224;
	B_CMD = SRCEN|DSTEN|ADDDSEL|UPDA1|UPDA2|DCOMPEN;

	/* draw title */
	box = FNTbox(TITLE, bigfnt);		/* find bounding box for text */
	x = upperx + (DRAWWIDTH - (box & 0x0000ffff))/2;	/* center title */
	box = FNTstr(x, cury, TITLE, draw_ptr, screenflags, bigfnt, TEXTCOLOR, 0);
	cury += (box >> 16);

	/* figure out how many file names will fit in the box */
	box = FNTbox("M", medfnt);
	fntheight = (box >> 16) + 1;
	fntwidth = (box & 0x0000ffff);
	filesonscreen = (DRAWHEIGHT - (cury+3))/fntheight;

	/* do sanity checks on the cursor vs. the number of files we have */
	if (cursfile >= numfiles)
		cursfile = numfiles - 1;
	else if ( (cursfile < 0) && (numfiles > 0) )
		cursfile = 0;

	/* see if the cursor has moved off the top or the bottom of the display area */
	if ( cursfile < topfile ) {
		topfile = (cursfile > 0) ? cursfile : 0;
	} else if (topfile + filesonscreen <= cursfile) {
		topfile = (cursfile - filesonscreen) + 1;
	}

	/* get size of arrows */
	arrowh = (FNTbox("A", arrowfnt) >> 16);

	/* if it is possible to scroll up, draw an up arrow */
	if (topfile > 0) {
		FNTstr(curx, cury-arrowh, "A", draw_ptr, screenflags, arrowfnt, TEXTCOLOR, 0);
	}
	cury += 3;

	/* if it is possible to scroll down, draw a down arrow at the bottom of the screen */
	if (topfile + filesonscreen < numfiles) {
		FNTstr(curx, cury+(filesonscreen*fntheight)+6, "B", draw_ptr, screenflags, arrowfnt, TEXTCOLOR, 0);
	}

	/* draw a 3D type box */
	Draw3DBox(curx, cury, DRAWWIDTH, (filesonscreen*fntheight)+2, IN);
	curx++;
	cury++;
	maxx = curx + DRAWWIDTH - 3;

	/* now draw the strings */
	for (i = topfile; (i < numfiles) && (i < topfile+filesonscreen); i++) {
		unsigned color;

		color = (file[i].selected) ? SELTEXTCOLOR : TEXTCOLOR;

#if 0
		sprintf(buf, "%s %s", file[i].appname, file[i].filename);
		FNTstr(curx, cury, buf, draw_ptr, screenflags, medfnt, color, 0);
#else
		FNTstr(curx, cury, file[i].appname, draw_ptr, screenflags, medfnt, color, 0);
		FNTstr(curx+(fntwidth*strlen(file[i].appname))+2, cury, file[i].filename, draw_ptr, screenflags, medfnt, color, 0);
#endif
		/* right align the size */
		sprintf(buf, "%ld", file[i].size);
		box = FNTbox(buf, medfnt);
		FNTstr( maxx - (box & 0x0000ffff), cury, buf, draw_ptr, screenflags, medfnt, color, 0 );

		/* if this is where the cursor is, draw it */
		if (i == cursfile) {
			/* draw a box around the text */
			DrawBox(curx-1,cury-1, (maxx-curx)+2, fntheight+2, BOXCOLOR);
		}
		cury += fntheight;
	}

	/* skip blank lines */
	if (numfiles < topfile+filesonscreen)
		cury += ((topfile+filesonscreen)-numfiles)*fntheight;

	/* skip border */
	cury += 4;

	/* finally, print info about free and used bytes */
	sprintf(buf, "%ld bytes free  /  %ld bytes total", freebytes, totalbytes );

	/* center the info */
	box = FNTbox(buf, medfnt);
	FNTstr( curx + ((maxx - curx) - (box & 0x0000ffff))/2, cury, buf, draw_ptr, screenflags, medfnt, TEXTCOLOR, 0 );
}

/*
 * Structures for alert boxes
 */

#define MAX_BUTTONS 3

struct button {
	int x, y, w, h;		/* x,y,w,h for button */
} alert_buttons[MAX_BUTTONS];

/*
 * Draw an alert box
 */
void
DrawAlert(char **text, char **buttons, int sel_button)
{
	int boxx, boxy, boxw, boxh;	/* x,y,w,h for alert box */
	int butw;			/* width of all buttons */
	int emwidth, emheight;		/* width, height of "M" in current font */
	long curbox;
	char *s;
	int i;

	curbox = FNTbox("M", bigfnt);
	emwidth = curbox & 0x0000ffff;
	emheight = (curbox >> 16);

	/* find dimensions of box to hold data */
	boxh = boxw = 0;

	/* check text */
	for (i = 0; (s = text[i]) != 0; i++) {
		curbox = FNTbox(s, bigfnt);
		boxh += (curbox >> 16) + 1;
		if ( (curbox & 0x0000ffff) > boxw )
			boxw = (curbox & 0x0000ffff);
	}
	/* add some space, and room for a line of buttons */
	boxh += 5;
	boxh += emheight+5;

	/* figure out width of buttons */
	butw = 0;
	for (i = 0; (s = buttons[i]) != 0; i++) {
		curbox = FNTbox(s, bigfnt);
		butw += (curbox & 0x0000ffff) + 4 + emwidth;
	}
	if (butw > boxw)
		boxw = butw;

	/* add a 1 pixel border */
	boxw += 2;
	boxh += 2;
	boxx = (320-boxw)/2 + XOFFSET;
	boxy = (288-boxh)/2;

	BLTrect( draw_ptr, screenflags, boxx, boxy, boxw, boxh, ALERTBOXCOLOR );
	Draw3DBox( boxx, boxy, boxw, boxh, OUT);

	/* skip the 1 pixel border */
	boxy++;

	/* draw the text, centered */
	for (i = 0; (s = text[i]) != 0; i++) {
		curbox = FNTbox(s, bigfnt);
		FNTstr(boxx + (boxw - (curbox & 0x0000ffff))/2, boxy, s, draw_ptr, screenflags, bigfnt, TEXTCOLOR, 0);
		boxy += (curbox >> 16) + 1;
	}
	/* skip the em-height border, and 2 lines for 3D effects */
	boxy += (emheight/2) + 2;

	/* draw the buttons */
	boxx += emwidth/2 + (boxw - butw)/2 + 2;
	for (i = 0; (s = buttons[i]) != 0; i++) {
		curbox = FNTbox(s, bigfnt);
		Draw3DBox(boxx, boxy, (int)(curbox & 0x0000ffff), (int)(curbox >> 16), (i == sel_button) ? IN : OUT);
		FNTstr(boxx, boxy, s, draw_ptr, screenflags, bigfnt, ((i == sel_button) ? SELTEXTCOLOR : TEXTCOLOR), 0); 
		if (i == sel_button)
			DrawBox(boxx, boxy, (int)(curbox & 0x0000ffff), (int)(curbox >> 16), BOXCOLOR);
		boxx += (curbox & 0x0000ffff) + 4 + emwidth;
	}
}

/*
 * DoAlert: process an alert box, return which button was selected
 */
int
DoAlert(char **text, char **buttons, int default_button)
{
	int sel_button = default_button;
	long joybuts;

	for(;;) {
		DrawScreen();
		DrawAlert(text, buttons, sel_button);
		SwitchScreen();
		joybuts = JOYrepeat(JOY1);
		if (joybuts & JOY_RIGHT) {
			if (buttons[sel_button+1] != 0)
				sel_button++;
		}
		if (joybuts & JOY_LEFT) {
			if (sel_button > 0)
				--sel_button;
		}
		if (joybuts & (FIRE_A|FIRE_B|FIRE_C)) {
			break;
		}
	}
	return sel_button;
}

/*
 * report an error
 */
char *errbuttons[] = { " Too Bad ", 0 };

void
Error( char *message )
{
	char *errtext[4];

	errtext[0] = "ERROR!";
	errtext[1] = " ";
	errtext[2] = message;
	errtext[3] = 0;

	DoAlert( errtext, errbuttons, 0 );
}

/*
 * draw a string input box
 * prompt = string to prompt with
 * inpdata = data that has been input
 * curspos = current cursor position
 */
void
DrawStrBox( char *prompt, char *inpstr, int curspos )
{
	int boxx, boxy, boxw, boxh;
	int emwidth, emheight;
	int maxlen;
	long curbox;
	int promptw;
	int i;
	char buf[2];		/* temporary buffer for printing characters */

	curbox = FNTbox("M", bigfnt);
	emwidth = curbox & 0x0000ffff;
	emheight = curbox >> 16;

	boxw = promptw = FNTbox(prompt, bigfnt) & 0x0000ffff;
	maxlen = strlen(inpstr);
	if ( (maxlen * emwidth) > boxw )
		boxw = maxlen * emwidth;
	/* add: 2 pixels to left and right, 2 pixels to top and bottom, 3 pixels between lines */
	boxw += 4;
	boxh = 2*emheight + 7;
	boxx = (320 - boxw)/2;
	boxy = (288 - boxh)/2;

	/* draw the box */
	BLTrect(draw_ptr, screenflags, boxx, boxy, boxw, boxh, ALERTBOXCOLOR);
	Draw3DBox(boxx, boxy, boxw, boxh, OUT);
	boxx += 2;
	boxy += 2;

	/* center the prompt string */
	FNTstr(boxx + ((boxw - 4) - promptw)/2, boxy, prompt, draw_ptr, screenflags, bigfnt, TEXTCOLOR, 0);
	boxy += (emheight+3);

	/* draw the input string, including cursor box if appropriate */
	buf[1] = 0;
	for (i = 0; inpstr[i]; i++) {
		buf[0] = inpstr[i];
		curbox = FNTstr(boxx, boxy, buf, draw_ptr, screenflags, bigfnt, TEXTCOLOR, 0);
		if (i == curspos)
			DrawBox(boxx-1, boxy-1, (int)(curbox&0x0000ffff)+2, (int)(curbox >> 16)+2, BOXCOLOR);
		boxx += (curbox & 0x0000ffff);
	}
}

/* utility functions: IncChar selects the next character from the character set, DecChar selects the previous */
int
IncChar( int c, char *charset )
{
	int firstchar = *charset;

	while (*charset && *charset != c)
		charset++;
	if (*charset && *(charset+1))
		return *(charset+1);
	else
		return firstchar;
}

int
DecChar( int c, char *charset )
{
	int prevchar;

	for(;;) {
		prevchar = *charset++;
		if (c == *charset) break;
		if (*charset == 0) break;
	}
	return prevchar;
}

/*
 * get a string from the user
 * prompt = message to print
 * inpstr = default input (probably all blanks)
 * charset = legal characters for input
 */

void
GetStr( char *prompt, char *inpstr, char *charset )
{
	int curspos;
	long joyinp;

	curspos = 0;
	for(;;) {
		DrawScreen();
		DrawStrBox( prompt, inpstr, curspos );
		SwitchScreen();
		joyinp = JOYrepeat(JOY1);
		if ( (joyinp & (FIRE_A|FIRE_B|FIRE_C)) ) {
			printf("Raw input string '%s'\n", inpstr);
			StripBlanks(inpstr);
			printf("stripped input string '%s'\n", inpstr);
			return;
		}
		if (joyinp & JOY_UP) {
			inpstr[curspos] = IncChar(inpstr[curspos], charset);
		}
		if (joyinp & JOY_DOWN) {
			inpstr[curspos] = DecChar(inpstr[curspos], charset);
		}
		if (joyinp & JOY_LEFT) {
			if (curspos > 0) --curspos;
		}
		if (joyinp & JOY_RIGHT) {
			if (inpstr[curspos+1] != 0) curspos++;
		}
	}
}

/*
 * DoDelete: request a deletion
 */
void
DoDelete( File *f )
{
	char buf[128];
	char *deltext[3];
	char *delbuttons[3];

	delbuttons[0] = " Yes ";
	delbuttons[1] = " No ";
	delbuttons[2] = 0;

	deltext[0] = "Delete";
	sprintf(buf, "%s%s?", f->appname, f->filename);
	deltext[1] = buf;
	deltext[2] = 0;
	if (DoAlert( deltext, delbuttons, 1 ) == 0) {
		/* Are you REALLY sure? */
		deltext[0] = "Are you sure?";
		deltext[1] = 0;
		if (DoAlert(deltext, delbuttons, 1) == 0) {
			deltext[0] = " Working... ";
			deltext[1] = 0;
			delbuttons[0] = 0;
			DrawScreen();
			DrawAlert( deltext, delbuttons, -1 );
			SwitchScreen();
			NVM_Delete(f->appname, f->filename);
		}
	}
}

/*
 * create a new file, with name similar to an existing one
 */
#define FILESET " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.:"
#define NUMSET "0123456789"

void
DoCreate( File *f )
{
	char appname[APPNAMELEN+1];
	char filename[FILENAMELEN+1];
	char sizestr[5];
	char buf[128];
	long size;
	int handle;

	/* initialize application and file names */
	if (f) {
		strcpy(appname, f->appname);
		strcpy(filename, f->filename);
	} else {
		strcpy(appname, "APPLICATION");
		strcpy(filename,"A FILE");
	}
	size = strlen(appname);
	memset(&appname[size], ' ', (APPNAMELEN-size)+1);
	appname[APPNAMELEN] = '\0';
	size = strlen(filename);
	memset(&filename[size], ' ', (FILENAMELEN-size)+1);
	filename[FILENAMELEN] = '\0';
	strcpy(sizestr, "0000");

	GetStr( "Enter application name:" , appname, FILESET);
	GetStr( "Enter file name:" , filename, FILESET);
	GetStr( "Enter file size:", sizestr, NUMSET);
	size = atol(sizestr);

	if (!(*appname) || !(*filename))
		return;

	if ((handle = NVM_Init(appname, NVMwork)) != 0) {
		sprintf(buf, "Error %d in NVM_Init\n", handle);
		Error(buf);
		return;
	}

	handle = NVM_Create( filename, size );
	if (handle < 0) {
		sprintf(buf, "Error %d in NVM_Create\n", handle);
		Error(buf);
	} else {
		NVM_Close(handle);
		NVM_Init(OURNAME, NVMwork);
	}
}

#define SAVEWORDS 20			/* number of words to save in the preferences file */

/* data for saved preferences:
 * word 0 = 0xbeef
 * word 1 = topfile
 * word 2 = cursfile
 * word 3 = 0 for sort by name, 1 for sort by size
 * last word is checksum
 */

int savedata[SAVEWORDS];

int
chksum( int *area )
{
	int i;
	int val;

	val = 0;
	for (i = 0; i < SAVEWORDS-1; i++) {
		val = 7*val + area[i];
	}
	return val;
}

/*
 * initialize interaction code
 */
void
InitInteract( void )
{
	int handle;
	long bytesread;
	int sum;

	handle = NVM_Open( "SETTINGS" );
	if (handle >= 0) {
		bytesread = NVM_Read(handle, savedata, sizeof(savedata));
		NVM_Close(handle);
		if (bytesread != sizeof(savedata)) {
			Error("Error reading settings");
			goto usedefaults;
		}
		if (savedata[0] != 0xbeef) {
			Error("Bad data in settings file");
			goto usedefaults;
		}
		sum = chksum( savedata );
		if (savedata[SAVEWORDS-1] != sum) {
			Error("Bad checksum in settings file");
			goto usedefaults;
		}
		topfile = savedata[1];
		cursfile = savedata[2];
		cursort = (savedata[3] == 0) ? alphacmp : sizecmp;
		return;
	}
usedefaults:
	topfile = 0;
	cursfile = 0;
	cursort = alphacmp;
}

/*
 * Save preferences
 */
void
SaveSettings(void)
{
	int handle;
	long byteswritten;

	savedata[0] = 0xbeef;
	savedata[1] = topfile;
	savedata[2] = cursfile;
	savedata[3] = (cursort == alphacmp) ? 0 : 1;
	savedata[SAVEWORDS-1] = chksum(savedata);
	handle = NVM_Create("SETTINGS", sizeof(savedata));
	if (handle < 0) {
		Error("Error creating file");
		return;
	}
	byteswritten = NVM_Write(handle, savedata, sizeof(savedata));
	if (byteswritten != sizeof(savedata)) {
		Error("Error during write");
	}
	NVM_Close(handle);
}

/*
 * ask the user how she wants the files sorted, and then
 * sort them. After this, ask if the setting should become
 * permanent.
 */
void
DoAskSort( int asksave )
{
	int i;
	char *sorttext[3];
	char *sortbuttons[3];

	int sortmethod;
#define ALPHASORT 0
#define SIZESORT 1

	/* find current sort method */
	sortmethod = (cursort == alphacmp) ? ALPHASORT : SIZESORT;

	/* ask which sort method is desired */
	sorttext[0] = "How do you want";
	sorttext[1] = "to sort the items?";
	sorttext[2] = 0;
	sortbuttons[ALPHASORT] = " By Name ";
	sortbuttons[SIZESORT] = " By Size ";
	sortbuttons[2] = 0;
	sortmethod = DoAlert(sorttext, sortbuttons, sortmethod);

	cursort = (sortmethod == ALPHASORT) ? alphacmp : sizecmp;

	/* save which file the cursor is on */
	if (cursfile >= 0)
		file[cursfile].selected = 1;

	/* now sort the files */
	qsort(file, (size_t)numfiles, sizeof(file[0]), cursort);
	/* put the cursor back on the same file */
	for (i = 0; i < numfiles; i++) {
		if (file[i].selected) {
			cursfile = i;
			file[i].selected = 0;
			break;
		}
	}

	DrawScreen();
	SwitchScreen();

	/* now ask about saving these settings */
	if (asksave) {
		sorttext[0] = "Save these settings?";
		sorttext[1] = 0;
		sortbuttons[0] = " Yes ";
		sortbuttons[1] = " No ";
		if (DoAlert(sorttext, sortbuttons, 1) == 0) {		/* user answered "yes" */
			sorttext[0] = " Working... ";			/* put up a message */
			sortbuttons[0] = 0;
			DrawScreen();
			DrawAlert( sorttext, sortbuttons, -1);
			SwitchScreen();
			SaveSettings();
		}
	}
}

/*
 * Clear the cartridge
 */
void
DoClearCart( void )
{
	char *text[3];
	char *buttons[3];
	char buf[128];
	int i;

	text[0] = "Erase all data on cartridge?";
	text[1] = 0;
	buttons[0] = " Yes ";
	buttons[1] = " No ";
	buttons[2] = 0;

	if (DoAlert(text, buttons, 1) != 0) {
		return;
	}
	text[0] = "Are you SURE?";
	if (DoAlert(text, buttons, 1) != 0) {
		return;
	}
	/* erase cart contents here */
	text[0] = buf;
	buttons[0] = 0;
	for (i = 0; i < numfiles; i++) {
		sprintf(buf, "%d items remaining", numfiles-i);
		DrawScreen();
		DrawAlert(text, buttons, -1);
		SwitchScreen();
		NVM_Delete(file[i].appname, file[i].filename);
	}
}

/*
 * test the cartridge by writing a big file on it
 */

#define NUMBLOCKS 8		/* number of test blocks to write */

void
DoTest( void )
{
	char *text[3];
	char *buttons[3];
	unsigned char *testdata;
	char buf[80];
	long testbytes;
	int i, c;
	long j;
	int handle;
	unsigned long randnum;
#define NEXTRANDOM() (randnum*1103515245L + 12345)
#define FIRSTRANDOM 0x9a319039L

	text[0] = "Do self test?";
	text[1] = 0;
	buttons[0] = " No ";
	buttons[1] = " Yes ";
	buttons[2] = 0;

	if (DoAlert(text, buttons, 0) != 1) {
		return;
	}

	text[0] = "Creating file";
	buttons[0] = 0;
	buttons[1] = 0;

	bgcolor = 0x0080;		/* blue screen */
	DrawScreen();
	DrawAlert(text, buttons, -1);
	SwitchScreen();

	testbytes = freebytes/NUMBLOCKS;
	testdata = malloc( testbytes );
	if ( (handle = NVM_Create( "TESTFILE", freebytes )) < 0 ) {
		bgcolor = 0xd280;	/* red screen */
		sprintf(buf, "Unable to create test file (err %d)", handle);
		Error( buf );
		bgcolor = DEFAULTBGCOLOR;
		free(testdata);
		return;
	}

	text[0] = buf;
	randnum = 1;
	for (i = 0; i < NUMBLOCKS; i++) {
		/* update the user about what's going on */
		sprintf(buf, "Writing block %d", i);
		DrawScreen();
		DrawAlert(text, buttons, -1);
		SwitchScreen();
		/* create some random data to write */
		for (j = 0; j < testbytes; j++) {
			randnum = NEXTRANDOM();
			c = (randnum >> 5) & 0x00ff;
			testdata[j] = c;
		}
		/* write the data */
		if (NVM_Write(handle, testdata, testbytes) != testbytes) {
			bgcolor = 0xd280;	/* red screen */
			strcpy( buf, "ERROR: Write error on file" );
			goto ret;
		}
	}

	/* now read the data back */
	NVM_Lseek(handle, 0L, 0);		/* reset to start of file */
	randnum = 1;				/* reset random number generator */
	for (i = 0; i < NUMBLOCKS; i++) {
		/* update the user about what's going on */
		sprintf(buf, "Reading block %d", i);
		DrawScreen();
		DrawAlert(text, buttons, -1);
		SwitchScreen();
		/* read back the data */
		if (NVM_Read(handle, testdata, testbytes) != testbytes) {
			bgcolor = 0xd280;	/* red screen */
			strcpy( buf, "ERROR: Read error on file" );
			goto ret;
		}
		/* verify the data */
		for (j = 0; j < testbytes; j++) {
			randnum = NEXTRANDOM();
			c = (randnum >> 5) & 0x00ff;
			if (testdata[j] != c) {
				bgcolor = 0xd280;	/* red screen */
				sprintf(buf, "VERIFY ERROR: wrote %02x read %02x\n", c, testdata[j]);
				goto ret;
			};
		}
	}
	bgcolor = 0x7f80;		/* green screen */
	sprintf(buf, "Free Memory OK");
ret:
	NVM_Close(handle);
	free(testdata);
	NVM_Delete( (char *)0, "TESTFILE");		/* clean up after ourselves */

	buttons[0] = " Continue ";
	DoAlert(text, buttons, 0);
	bgcolor = DEFAULTBGCOLOR;
	return;
}


/*
 * interact with the user
 * return 0 if interaction should continue
 * return 1 to reboot
 */
int
Interact( void )
{
	long buttons;
	long butsdown;

	for(;;) {
		DrawScreen();
		SwitchScreen();
		buttons = JOYrepeat(JOY1);
		butsdown = JOYget(JOY1);
		if (buttons & JOY_UP) {
			if (cursfile > 0) cursfile--;
		}
		if (buttons & JOY_DOWN) {
			if (cursfile < (numfiles-1))
				cursfile++;
		}
		if ( (buttons & (FIRE_A|FIRE_B|FIRE_C)) && (cursfile >= 0) ) {
			file[cursfile].selected = 1;
			DrawScreen();
			DoDelete(&file[cursfile]);
			return 0;
		}
		if ( (buttons & OPTION) ) {
		/* magic debugging keys */
	gotoption:
			if ( (butsdown & (KEY_H|KEY_S)) == (KEY_H|KEY_S) ) {
				if ( (butsdown & KEY_0) ) {
					DoTest();
				} else {
					DoClearCart();
				}
				return 0;
			}
			if ( (butsdown & (KEY_7|KEY_9)) == (KEY_7|KEY_9) ) {
				DoCreate((cursfile >= 0) ? &file[cursfile] : (File *)0);
				return 0;
			}
			if (butsdown & (KEY_1|KEY_3)) {
				DoAskSort(1);
			} else {
				DoAskSort(0);
			}
			return 0;
		}

		/* reset keys: if either KEY_H or KEY_S is toggled, check for both being down */
		if ( buttons & (KEY_H|KEY_S) ) {
			if ( (butsdown & (KEY_H|KEY_S)) == (KEY_H|KEY_S) ) {
				/* wait for keys to come back up, and look for option while we're at it */
				do {
					butsdown = JOYget(JOY1);
					if (butsdown & OPTION)
						goto gotoption;
				} while (butsdown);
				return 1;
			}
		}

	}
}

#if NEEDEMBOSS		/* no longer needed, pre-embossed logo is supplied */
/*
 * Emboss: emboss the Jaguar logo
 */
static inline long dist(long x, long y, long z)
{
	if (x < 0) x = -x;
	if (y < 0) y = -y;
	if (z < 0) z = -z;

	return (x+y+z);
}

unsigned short width45 = 4;

long Lx = -192;
long Ly = 110;
long Lz = 128;	/* light direction */

/* Emboss code adapted from Graphics Gems IV */
void
Emboss(unsigned short *bump, unsigned short *dst, unsigned short Width, unsigned short Height)
{
	long Nx, Ny, Nz, NzLz, NdotL;
	unsigned short *s1, *s2, *s3, shade, background;
	unsigned short x, y;

	/*
	 * constant z component of image surface normal - this depends on the
	 * image slope we wish to associate with an angle of 45 degrees, which
	 * depends on the width of the filter used to produce the source image.
	 */
	Nz = (6 * 255) / width45;
	NzLz = Nz * Lz;

	/* optimization for vertical normals: L.[0 0 1] */
	background = Lz;

	dst += Width;
	for (y = 1; y < Height-1; y++, bump += Width, dst++) {
		s1 = bump+1;
		s2 = s1+Width;
		s3 = s2+Width;
		dst++;
		for (x = 1; x < Width-1; x++, s1++, s2++, s3++) {
		/*
		 * compute the normal from the bump map. the type of the expression
		 * before the cast is compiler dependent. in some cases the sum is
		 * unsigned, in others it is signed. ergo, cast to signed.
		 */
			Nx = (int)(s1[-1] + s2[-1] + s3[-1] - s1[1] - s2[1] - s3[1]);
			Ny = (int)(s3[-1] + s3[0] + s3[1] - s1[-1] - s1[0] - s1[1]);

		/* shade with distant light source */
			if ( Nx == 0 && Ny == 0 )
				shade = background;
			else if ( (NdotL = Nx*Lx + Ny*Ly + NzLz) < 0 )
				shade = 0;
			else {
				shade = NdotL / dist(Nx, Ny, Nz);
			}
		/* now: shade is between 0 and 0xff (0xff being brightest) */
		/* we want to convert this to an offset relative to  0 */
		/* also, the brightness should be at most +-20 rather than +- 80 */
				*dst++ = (shade - 0x80)/4 & 0x00ff;
		}
	}
}
#endif

/*
 * Main entry point
 */
void
main( void )
{
	int finished;
#if NEEDEMBOSS
	long i;
	unsigned short *cry;		/* pointer to CRY logo */
	unsigned char *pix4;		/* pointer to original (4 bit) logo */
	unsigned short nyb;		/* 4 bit pixel */
	unsigned short color;
	unsigned short *tmplogo;
#endif

#if defined(USE_SKUNK)
	skunkRESET();
	skunkNOP();
	skunkNOP();
	printf("Skunk console initialized\n");
#endif // defined(USE_SKUNK)

	/* copy the object list to RAM */
	memcpy(ram_olist, rom_olist, sizeof(rom_olist));

	/* allocate the screen */
	screen = malloc( 320L*288L*2L*2L + 32);
	screenguard = screen+(320L*288L*2L*2L);
	strcpy(screenguard, "screen GUARD string");

	/* fill it with 0's */
	memset(screen, 0, 320L*288L*2L*2L);

	disp_ptr = screen;
	draw_ptr = screen+8;

	/* build screen 1 */
	/* VIDwidth contains the screen width in clock cycles; divide by 4 to get it in pixels */
	ram_olist[0].bit.xpos = ((VIDwidth/4) - 320)/2;
	ram_olist[0].bit.ypos = VID_vdb;
	ram_olist[0].bit.data = (void *)disp_ptr;
	OLbldto(ram_olist, packed_scrn1);

	/* build screen 2 */
	ram_olist[0].bit.data = (void *)draw_ptr;		/* 1 phrase = 4 words */
	OLbldto(ram_olist, packed_scrn2);

	if (VIDpal) {
		upperx = (320 - DRAWWIDTH)/2 + XOFFSET;
		uppery = (288 - DRAWHEIGHT)/2;
	} else {
		upperx = (320 - DRAWWIDTH)/2 + XOFFSET;
		uppery = (240 - DRAWHEIGHT)/2;
	}

	/* turn on the screen */
	OLPset(packed_scrn1);

	printf("Screen initialized\n");

#if NEEDEMBOSS
	/* copy the Jaguar logo into RAM */
	cry = logo = malloc(224L*80L*2L);
	pix4 = jaglogo;
	for (i = 0; i < 224L*80L/2; i++) {
		nyb = (*pix4 & 0x00f0) >> 4;
		color = jaglogopal[nyb];
		if (color) {
			color = (color & 0x00ff);	/* color is now the blending number between background (0) and foreground (ff) */
		}
		*cry++ = color;
		nyb = (*pix4 & 0x000f);
		color = jaglogopal[nyb];
		if (color) {
			color = (color & 0x00ff);	/* color is now the blending number between background (0) and foreground (ff) */
		}
		*cry++ = color;
		pix4++;
	}
	tmplogo = malloc(224L*80L*2L);
	memset(tmplogo, 0, 224L*80L*2L);

	Emboss(logo, tmplogo, 224, 80);
	free(logo);
	logo = tmplogo;
#endif

	/* initialize NVM BIOS */
	NVMwork = malloc(16*1024L + 20);
	NVMworkguard = NVMwork+(16*1024L);
	strcpy(NVMworkguard, "work GUARD string");

	/* initialize some draw variables just in case */
	totalbytes = freebytes = 0;
	cursfile = topfile = numfiles = 0;
	bgcolor = DEFAULTBGCOLOR;

#if defined(STANDALONE)
	if (NVM_MAGIC != GOOD_MAGIC) {
		Error("Memory Track BIOS not installed!");
		return;
	}
#endif /* defined(STANDALONE) */

	if (NVM_Init(OURNAME, NVMwork) != 0) {
		Error("Unable to access cartridge!");
		return;
	}

	printf("BIOS initialized\n");

	/* clear out the joystick */
	lastjoy = joytimer = 0;
	(void)JOYrepeat(JOY1);

	/* initialize interaction code */
	InitInteract();

	/* main loop */
	do {
		GetFiles();			/* read in the list of files available */
		finished = Interact();		/* wait for the user to do something */
		FreeFiles();			/* OK, we're done with the files */
	} while (!finished);
}
