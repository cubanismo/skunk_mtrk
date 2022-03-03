#include "blit.h"

/*
 * draw a line, using the blitter
 */
#define abs(x) ( ((x)< 0) ? (-(x)) : (x) )

void
BLTline(void *dest, long destflags, int x1, int y1, int x2, int y2, unsigned color)
{
	int linelen;
	long xinc, yinc;
	unsigned long a1inc, a1finc;

	x2 -= x1;
	y2 -= y1;

	A1_FLAGS = (destflags | XADDINC);
	A1_BASE = (long)dest;
	A1_PIXEL = (((unsigned long)y1) << 16) | (unsigned)x1;
	A1_FPIXEL = 0;

	linelen = abs(x2) + abs(y2);
	xinc = ((long)x2 << 16) / linelen;
	yinc = ((long)y2 << 16) / linelen;

	a1inc = (yinc & 0xffff0000) | ((xinc >> 16) & 0x0000ffff);
	a1finc = ((yinc & 0x0000ffff) << 16) | (xinc & 0x0000ffff);

	A1_INC = a1inc;
	A1_FINC = a1finc;
	B_COUNT = 0x00010000 | linelen;
	B_PATD[0] = color;

	B_CMD = PATDSEL;
}
