#ifndef EXWP_COMMON_H
#define EXWP_COMMON_H

/* Ewd socket path */
#define SOCK_PATH "/tmp/foo.sock"

/* Useful function-like macros */
#define die(...)    err(EXIT_FAILURE, __VA_ARGS__)
#define diex(...)   errx(EXIT_FAILURE, __VA_ARGS__)
#define lengthof(x) (sizeof(x) / sizeof(*(x)))
#define streq(x, y) (!strcmp(x, y))

/* Define unreachable() on pre-C23 */
#if __STDC_VERSION__ < 202311L
#	ifdef __GNUC__
#		define unreachable() __builtin_unreachable()
#	else
#		define unreachable()
#	endif
#endif

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

#endif /* !EXWP_COMMON_H */
