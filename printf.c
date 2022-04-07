#include "sprintf.h"
#include "printf.h"

#if defined(USE_SKUNK)
#include "skunk.h"

/* Not thread safe at all */
static char pfbuf[SPRINTF_MAX+1];

int printf(const char *fmt, ...)
{
	va_list args;
	int foo;

	va_start(args, fmt);
	foo = vsprintf(pfbuf, fmt, args);
	va_end(args);

	skunkCONSOLEWRITE(pfbuf);

	return foo;
}
#endif /* defined(USE_SKUNK) */
