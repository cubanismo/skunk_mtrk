#ifndef PRINTF_H_
#define PRINTF_H_

#if defined(USE_SKUNK)
extern int printf(const char *, ...);
#else
#define printf(...) 0
#endif

#endif /* PRINTF_H_ */
