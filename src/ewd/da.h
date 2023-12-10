#ifndef MANGO_DA_H
#define MANGO_DA_H

#ifndef DA_FACTOR
#	define DA_FACTOR 2
#endif

#define __da_s(a) (sizeof(*(a)->buf))

#define da_init(a, n) \
	do { \
		(a)->cap = n; \
		(a)->len = 0; \
		(a)->buf = malloc((a)->cap * __da_s(a)); \
		if ((a)->buf == NULL) \
			err(EXIT_FAILURE, "malloc"); \
	} while (0)

#define da_append(a, x) \
	do { \
		if ((a)->len >= (a)->cap) { \
			(a)->cap = (a)->cap * DA_FACTOR + 1; \
			(a)->buf = realloc((a)->buf, (a)->cap * __da_s(a)); \
			if ((a)->buf == NULL) \
				err(EXIT_FAILURE, "realloc"); \
		} \
		(a)->buf[(a)->len++] = (x); \
	} while (0)

#define da_remove(a, i) da_remove_range((a), (i), (i) + 1)

#define da_remove_range(a, i, j) \
	do { \
		memmove((a)->buf + (i), (a)->buf + (j), ((a)->len - (j)) * __da_s(a)); \
		(a)->len -= j - i; \
	} while (0)

#endif /* !MANGO_DA_H */
