#ifndef SPRINTF_H_
#define SPRINTF_H_

#include <stdarg.h>

#define SPRINTF_MAX 4096

extern int vsprintf(char *buf, const char *fmt, va_list args);
extern int sprintf(char *str, const char *fmt, ...);

#endif /* SPRINTF_H_ */
