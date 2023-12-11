#include <sys/mman.h>
#include <sys/stat.h>

#include <err.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <jxl/decode.h>
#include <jxl/thread_parallel_runner.h>

typedef uint8_t u8;
typedef uint32_t xrgb;

#define die(...)    err(EXIT_FAILURE, __VA_ARGS__)
#define diex(...)   errx(EXIT_FAILURE, __VA_ARGS__)
#define streq(x, y) (!strcmp(x, y))

u8 *process(const char *, int, size_t *);

static void
usage(const char *argv0)
{
	fprintf(stderr,
	        "Usage: %s [-d display] [file]\n"
	        "       %s -h\n",
	        argv0, argv0);
	exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
	int opt;
	size_t nthrds;
	void *tpr;
	JxlDecoder *d;
	JxlDecoderStatus res;
	JxlPixelFormat fmt = {
		.align = 0,
		.data_type = JXL_TYPE_UINT8,
		.endianness = JXL_NATIVE_ENDIAN,
		.num_channels = sizeof(xrgb),
	};
	struct {
		u8 *buf;
		size_t size;
	} img, pix;
	struct option longopts[] = {
		{"display", required_argument, 0, 'd'},
		{"help",    no_argument,       0, 'h'},
		{NULL,      0,                 0, 0  },
	};

	*argv = basename(*argv);
	while ((opt = getopt_long(argc, argv, "d:h", longopts, NULL)) != -1) {
		switch (opt) {
		case 'd':
			diex("TODO: Allow selecting a display");
		case 'h':
			if (execlp("man", "man", "1", *argv, NULL) == -1)
				die("execlp: man 1 %s", *argv);
			break;
		default:
			usage(*argv);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc > 1)
		usage(*argv);
	if (argc == 0 || streq(argv[0], "-"))
		img.buf = process("-", STDIN_FILENO, &img.size);
	else {
		int fd = open(argv[0], O_RDONLY);
		if (fd == -1)
			die("open: %s", argv[0]);
		img.buf = process(argv[0], fd, &img.size);
		close(fd);
	}

	if ((d = JxlDecoderCreate(NULL)) == NULL)
		diex("Failed to allocate JXL decoder");

	nthrds = JxlThreadParallelRunnerDefaultNumWorkerThreads();
	tpr = JxlThreadParallelRunnerCreate(NULL, nthrds);
	if (tpr == NULL)
		diex("Failed to allocate parallel runner");

	if (JxlDecoderSetParallelRunner(d, JxlThreadParallelRunner, tpr))
		diex("Failed to set parallel runner");

	if (JxlDecoderSubscribeEvents(d, JXL_DEC_FULL_IMAGE))
		diex("Failed to subscribe to events");

	if (JxlDecoderSetInput(d, img.buf, img.size))
		diex("Failed to set input data");

	while ((res = JxlDecoderProcessInput(d)) != JXL_DEC_FULL_IMAGE) {
		switch (res) {
		case JXL_DEC_NEED_IMAGE_OUT_BUFFER:
			if (JxlDecoderImageOutBufferSize(d, &fmt, &pix.size))
				diex("Failed to get image output buffer size");
			if ((pix.buf = malloc(pix.size)) == NULL)
				die("malloc");
			if (JxlDecoderSetImageOutBuffer(d, &fmt, pix.buf, pix.size))
				diex("Failed to set image output buffer");
			break;
		case JXL_DEC_NEED_MORE_INPUT:
			diex("Input image was truncated");
		case JXL_DEC_ERROR:;
			JxlSignature sig = JxlSignatureCheck(pix.buf, pix.size);
			die("Failed to decode file: %s",
			    (sig == JXL_SIG_CODESTREAM || sig == JXL_SIG_CONTAINER)
			        ? "Possibly file"
			        : "Not a JPEG file");
		default:
			warnx("Unexpected result from JxlDecoderProcessInput: %d", res);
		}
	}

	JxlThreadParallelRunnerDestroy(tpr);
	JxlDecoderDestroy(d);

	/* TODO: memfd_create() and send FD to dæmon */
	write(STDOUT_FILENO, pix.buf, pix.size);
	return EXIT_SUCCESS;
}

u8 *
process(const char *name, int fd, size_t *size)
{
	u8 *buf = NULL;
	struct stat sb;

	if (fstat(fd, &sb) == -1)
		die("fstat: %s", name);

	if (S_ISREG(sb.st_mode)) {
		*size = sb.st_size;
		buf = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
		if (buf == MAP_FAILED)
			die("mmap: %s", name);
	} else {
		size_t len, cap;
		ssize_t nr;
		len = cap = 0;

		/* Not the best; but good enough for now */
		do {
			cap += sb.st_blksize;
			if ((buf = realloc(buf, cap)) == NULL)
				die("realloc");
			len += nr = read(fd, buf + len, sb.st_blksize);
		} while (nr > 0);
		if (nr == -1)
			die("read: %s", name);
		*size = len;
	}
	return buf;
}
