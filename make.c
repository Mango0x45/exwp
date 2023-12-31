#define _GNU_SOURCE
#include <errno.h>
#include <glob.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cbs.h"

#define CC     "cc"
#define PREFIX "/usr/local"

#define CFLAGS \
	"-Wall", "-Wextra", "-Wpedantic", "-Werror", "-D_GNU_SOURCE", "-std=c2x", \
		"-pipe"
#define CFLAGS_DEBUG    "-g", "-ggdb3"
#define CFLAGS_RELEASE  "-O3", "-march=native", "-mtune=native", "-flto"
#define LDFLAGS_RELEASE "-flto"

#define streq(x, y) (!strcmp(x, y))

/* put, run, n’ clear */
#define cmdprc(c) \
	do { \
		int rv; \
		cmdput(c); \
		if ((rv = cmdexec(c))) \
			diex("%s failed with exit-code %d", *c._argv, rv); \
		cmdclr(&c); \
	} while (0)

[[noreturn]] static void usage(void);
static int globerr(const char *, int);
static char *ctoo(const char *);
static char *_mkoutpath(const char **, size_t);
#define mkoutpath(...) \
	_mkoutpath((const char **)_vtoa(__VA_ARGS__), lengthof(_vtoa(__VA_ARGS__)))

static void build_common(void);
static void build_ewctl(void);
static void build_ewd(void);

static bool debug;

struct {
	char *buf[128];
	size_t len;
} cobjs;

void
usage(void)
{
	fputs("Usage: make [-cds]\n"
	      "       make [-cs] install\n"
	      "       make clean\n",
	      stderr);
	exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
	int opt;
	bool cflag, sflag;

	cbsinit(argc, argv);
	rebuild();

	cflag = sflag = false;
	while ((opt = getopt(argc, argv, "cds")) != -1) {
		switch (opt) {
		case 'c':
			cflag = true;
			break;
		case 'd':
			debug = true;
			break;
		case 's':
			sflag = true;
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (!cflag && !sflag)
		cflag = sflag = true;

	if (chdir(dirname(*(argv - optind))) == -1)
		die("chdir: %s", *argv);

	if (argc > 0) {
		if (streq(*argv, "clean")) {
			cmd_t c = {0};
			cmdadd(&c, "find", "src", "-type", "f", "(", "-name", "*.o", "-or",
			       "-name", "ewctl", "-or", "-name", "ewd", ")", "-delete");
			cmdprc(c);
		} else if (streq(*argv, "install")) {
			cmd_t c = {0};
			char *bin, *man1, *man7;

			bin = mkoutpath("/bin");
			man1 = mkoutpath("/share/man/man1");
			man7 = mkoutpath("/share/man/man7");

			cmdadd(&c, "mkdir", "-p", bin, man1);
			if (sflag)
				cmdadd(&c, man7);
			cmdprc(c);
			if (cflag) {
				cmdadd(&c, "cp", "src/ewctl/ewctl", bin);
				cmdprc(c);
				cmdadd(&c, "cp", "src/ewctl/ewctl.1", man1);
				cmdprc(c);
			}
			if (sflag) {
				cmdadd(&c, "cp", "src/ewd/ewd", bin);
				cmdprc(c);
				cmdadd(&c, "cp", "src/ewd/ewd.1", man1);
				cmdprc(c);
				cmdadd(&c, "cp", "src/ewd/ewd.7", man7);
				cmdprc(c);
			}
		} else
			usage();
	} else {
		if (!binexists("pkg-config"))
			diex("pkg-config must be installed");

		build_common();
		if (cflag)
			build_ewctl();
		if (sflag)
			build_ewd();
	}

	return EXIT_SUCCESS;
}

int
globerr(const char *epath, int eerrno)
{
	errno = eerrno;
	die("glob: %s", epath);
}

char *
ctoo(const char *s)
{
	char *o = strdup(s);
	size_t n = strlen(s);
	if (!o)
		die("strdup");
	o[n - 1] = 'o';
	return o;
}

void
build_common(void)
{
	int rv;
	cmd_t c = {0};
	glob_t g;

	if ((rv = glob("src/common/*.c", 0, globerr, &g)) && rv != GLOB_NOMATCH)
		die("glob");

	for (size_t i = 0; i < g.gl_pathc; i++) {
		char *src = g.gl_pathv[i];
		char *dst = ctoo(src);
		if (foutdated(dst, src, "src/common/common.h")) {
			cmdadd(&c, CC, CFLAGS);
			if (debug)
				cmdadd(&c, CFLAGS_DEBUG);
			else
				cmdadd(&c, CFLAGS_RELEASE);
			cmdadd(&c, "-o", dst, "-c", src);
			cmdprc(c);
		}
		cobjs.buf[cobjs.len++] = dst;
	}

	globfree(&g);
}

void
build_ewctl(void)
{
	int rv;
	cmd_t c = {0};
	glob_t g;
	struct strv v = {0};

	pcquery(&v, "libjxl", PKGC_CFLAGS);
	pcquery(&v, "libjxl_threads", PKGC_CFLAGS);

	if ((rv = glob("src/ewctl/*.c", 0, globerr, &g)) && rv != GLOB_NOMATCH)
		die("glob");

	for (size_t i = 0; i < g.gl_pathc; i++) {
		char *src = g.gl_pathv[i];
		char *dst = ctoo(src);
		if (foutdated(dst, src, "src/common/common.h")) {
			cmdadd(&c, CC, CFLAGS);
			if (debug)
				cmdadd(&c, CFLAGS_DEBUG);
			else
				cmdadd(&c, CFLAGS_RELEASE);
			cmdaddv(&c, v.buf, v.len);
			cmdadd(&c, "-Isrc/common", "-o", dst, "-c", src);
			cmdprc(c);
		}
		free(dst);
	}

	strvfree(&v);
	globfree(&g);

	pcquery(&v, "libjxl", PKGC_LIBS);
	pcquery(&v, "libjxl_threads", PKGC_LIBS);

	if ((rv = glob("src/ewctl/*.o", 0, globerr, &g)) && rv != GLOB_NOMATCH)
		die("glob");

	if (foutdatedv("src/ewctl/ewctl", (const char **)g.gl_pathv, g.gl_pathc)) {
		cmdadd(&c, CC);
		if (!debug)
			cmdadd(&c, LDFLAGS_RELEASE);
		cmdaddv(&c, v.buf, v.len);
		cmdadd(&c, "-o", "src/ewctl/ewctl");
		cmdaddv(&c, g.gl_pathv, g.gl_pathc);
		cmdaddv(&c, cobjs.buf, cobjs.len);
		cmdprc(c);
	}

	strvfree(&v);
	free(c._argv);
}

void
build_ewd(void)
{
	int rv;
	cmd_t c = {0};
	glob_t g;
	struct strv v = {0};

	pcquery(&v, "pixman-1", PKGC_CFLAGS);
	pcquery(&v, "wayland-client", PKGC_CFLAGS);

	if ((rv = glob("src/ewd/*.c", 0, globerr, &g)) && rv != GLOB_NOMATCH)
		die("glob");
	if ((rv = glob("src/ewd/proto/*.c", GLOB_APPEND, globerr, &g))
	    && rv != GLOB_NOMATCH)
	{
		die("glob");
	}

	for (size_t i = 0; i < g.gl_pathc; i++) {
		char *src = g.gl_pathv[i];
		char *dst = ctoo(src);
		if (foutdated(dst, src, "src/ewd/da.h", "src/ewd/types.h",
		              "src/common/common.h"))
		{
			cmdadd(&c, CC, CFLAGS);

			/* Lots of wayland event listeners have unused parameters */
			if (streq(src, "src/ewd/main.c"))
				cmdadd(&c, "-Wno-unused-parameter");

			if (debug)
				cmdadd(&c, CFLAGS_DEBUG);
			else
				cmdadd(&c, CFLAGS_RELEASE);

			cmdaddv(&c, v.buf, v.len);
			cmdadd(&c, "-Isrc/common", "-o", dst, "-c", src);
			cmdprc(c);
		}
		free(dst);
	}

	strvfree(&v);
	globfree(&g);

	pcquery(&v, "pixman-1", PKGC_LIBS);
	pcquery(&v, "wayland-client", PKGC_LIBS);

	if ((rv = glob("src/ewd/*.o", 0, globerr, &g)) && rv != GLOB_NOMATCH)
		die("glob");
	if ((rv = glob("src/ewd/proto/*.o", GLOB_APPEND, globerr, &g))
	    && rv != GLOB_NOMATCH)
	{
		die("glob");
	}

	if (foutdatedv("src/ewd/ewd", (const char **)g.gl_pathv, g.gl_pathc)) {
		cmdadd(&c, CC);
		if (!debug)
			cmdadd(&c, LDFLAGS_RELEASE);
		cmdaddv(&c, v.buf, v.len);
		cmdadd(&c, "-o", "src/ewd/ewd");
		cmdaddv(&c, g.gl_pathv, g.gl_pathc);
		cmdaddv(&c, cobjs.buf, cobjs.len);
		cmdprc(c);
	}

	strvfree(&v);
	free(c._argv);
}

char *
_mkoutpath(const char **xs, size_t n)
{
#define trycat(s) \
	do { \
		if (strlcat(buf, (s), PATH_MAX) >= PATH_MAX) \
			goto toolong; \
	} while (0)

	char *p, *buf;

	buf = bufalloc(NULL, PATH_MAX, sizeof(char));
	buf[0] = 0;

	if (p = getenv("DESTDIR"), p && *p)
		trycat(p);

	p = getenv("PREFIX");
	trycat(p && *p ? p : PREFIX);

	for (size_t i = 0; i < n; i++)
		trycat(xs[i]);

	return buf;

toolong:
	errno = ENAMETOOLONG;
	die(__func__);
#undef trycat
}
