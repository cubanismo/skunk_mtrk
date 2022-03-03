/*
 * various utilities for doing blits
 */

#include "blit.h"

/*************************************************************************
int BLTwidth(long flags): return the blitter width for a given set of blitter flags
This is done by a table lookup on "widtab"; each entry in widtab consists
of two longs, the first being the width as an integer, the second being
the corresponding blitter bits.
*************************************************************************/
static unsigned int widtab[] = {
2,	0x0800,
4,	0x1000,
6,	0x1400,
8,	0x1800,
10,	0x1A00,
12,	0x1C00,
14,	0x1E00,
16,	0x2000,
20,	0x2200,
24,	0x2400,
28,	0x2600,
32,	0x2800,
40,	0x2A00,
48,	0x2C00,
56,	0x2E00,
64,	0x3000,
80,	0x3200,
96,	0x3400,
112,	0x3600,
128,	0x3800,
160,	0x3A00,
192,	0x3C00,
224,	0x3E00,
256,	0x4000,
320,	0x4200,
384,	0x4400,
448,	0x4600,
512,	0x4800,
640,	0x4A00,
768,	0x4C00,
896,	0x4E00,
1024,	0x5000,
1280,	0x5200,
1536,	0x5400,
1792,	0x5600,
2048,	0x5800,
2560,	0x5A00,
3072,	0x5C00,
3584,	0x5E00,
0,	0x0000
};

int
BLTwidth(long blitflag)
{
	unsigned *ptr;

	blitflag &= 0x7E00;

	ptr = widtab;
	while (ptr[0] != 0) {
		if (ptr[1] == blitflag) {
			return ptr[0];
		}
		ptr += 2;		/* skip the image width and blitter bits */
	}
	return -1;		/* punt */
}

/*
 * return the number of pixels in a phrase
 */
unsigned
BLTpixels_per_phrase( long blitflags )
{
	switch(blitflags & (PIXEL1|PIXEL2|PIXEL4|PIXEL8|PIXEL16|PIXEL32)) {
	case PIXEL1:
		return 64;
	case PIXEL2:
		return 32;
	case PIXEL4:
		return 16;
	case PIXEL8:
		return 8;
	case PIXEL16:
		return 4;
	default:
		return 2;
	}
}

/*
 * long
 * BLTstep( unsigned startx, unsigned width, long blitflags )
 * function to find the step value for a phrase or pixel mode blit
 * (for pixel mode, "pixels_per_phrase" will be 0)
 *
 * "startx" is the starting X value for the blit
 * "width" is the length of the blit
 * "blitflags" are the blitter flags
 */
long
BLTstep( unsigned startx, unsigned width, long blitflags )
{
	long endx;
	int stepx;
	unsigned pixels_per_phrase;

	pixels_per_phrase = BLTpixels_per_phrase( blitflags );
	stepx = -width;
	endx = ( startx + width ) & (pixels_per_phrase-1);
	if (endx > 0)
		stepx -= (pixels_per_phrase-endx);
	return 0x00010000 | ((long)stepx & 0x0000ffff);
}
