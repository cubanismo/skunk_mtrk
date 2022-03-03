#include "blit.h"

extern long BLTstep( unsigned startx, unsigned width, long blitflags );

/*
 * BLTrect(void *dst_addr, long dst_flags, int x, int y, int w, int h, int color)
 * blit a rectangle to a (phrase aligned) memory region; always done in
 * phrase mode
 */

void
BLTrect(void *dst_addr, long dst_flags, int x, int y, int w, int h, unsigned color)
{
	unsigned long color2;

	A1_BASE = (long)dst_addr;
	A1_FLAGS = (dst_flags & 0xFFF0FFFFL)|XADDPHR;
	A1_PIXEL = ((long)y << 16) | (unsigned)x;
	A1_FPIXEL = 0;
	A1_STEP = BLTstep(x, w, dst_flags);

	color2 = ((long)color << 16) | color;
	B_PATD[0] = color2;
	B_PATD[1] = color2;
	B_COUNT = ((long)h << 16) | (unsigned)w;
	B_CMD = PATDSEL|UPDA1;
}
