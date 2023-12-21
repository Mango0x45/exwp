#ifndef EXWP_COMMON_H
#define EXWP_COMMON_H

#include <err.h>
#include <stdint.h>
#include <stdlib.h>

/* Useful function-like macros */
#define die(...)    err(EXIT_FAILURE, __VA_ARGS__)
#define diex(...)   errx(EXIT_FAILURE, __VA_ARGS__)
#define lengthof(x) (sizeof(x) / sizeof(*(x)))
#define streq(x, y) (!strcmp(x, y))

/* Shorter type names */
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

/* Nice when used with sizeof() */
typedef u32 xrgb;

void *xmalloc(size_t);
void *xrealloc(void *, size_t);

/* Get path to the ewd socket */
const char *ewd_sock_path(void);

#endif /* !EXWP_COMMON_H */
