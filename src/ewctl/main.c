#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <jxl/codestream_header.h>
#include <jxl/decode.h>
#include <jxl/thread_parallel_runner.h>
#include <jxl/types.h>

#include "common.h"

#define warnx(...) \
	do { \
		warnx(__VA_ARGS__); \
		rv = EXIT_FAILURE; \
	} while (0)

struct bs {
	u8 *buf;
	size_t size;
};

struct img {
	int fd;
	u8 *buf;
	u32 w, h;
	size_t size;
};

static void srv_msg(int, struct img, char *);
static void abgr2argb(struct img);
static struct img jxl_decode(struct bs);
static u8 *process(const char *, int, size_t *);

static int rv;
static bool cflag;

[[noreturn]] static void
usage(const char *argv0)
{
	fprintf(stderr,
	        "Usage: %s [-d name] [file]\n"
	        "       %s -c | -h\n",
	        argv0, argv0);
	exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
	char *name = "";
	int opt, sockfd;
	struct bs img_e;
	struct img img_d;
	struct sockaddr_un saddr = {
		.sun_family = AF_UNIX,
	};
	struct option longopts[] = {
		{"clear",   no_argument,       0, 'c'},
		{"display", required_argument, 0, 'd'},
		{"help",    no_argument,       0, 'h'},
		{NULL,      0,                 0, 0  },
	};

	*argv = basename(*argv);
	while ((opt = getopt_long(argc, argv, "cd:h", longopts, NULL)) != -1) {
		switch (opt) {
		case 'c':
			cflag = true;
			break;
		case 'd':
			name = optarg;
			break;
		case 'h':
			execlp("man", "man", "1", *argv, NULL);
			die("execlp: man 1 %s", *argv);
		default:
			usage(*argv);
		}
	}

	argc -= optind;
	argv += optind;

	if (cflag) {
		if (argc == 1)
			warnx("Ignoring file argument ‘%s’", argv[0]);

		img_d = (struct img){0};
	} else {
		if (argc > 1)
			usage(*argv);
		if (argc == 0 || streq(argv[0], "-"))
			img_e.buf = process("-", STDIN_FILENO, &img_e.size);
		else {
			int fd = open(argv[0], O_RDONLY);
			if (fd == -1)
				die("open: %s", argv[0]);
			img_e.buf = process(argv[0], fd, &img_e.size);
			close(fd);
		}
		img_d = jxl_decode(img_e);
	}

	memcpy(saddr.sun_path, ewd_sock_path(), sizeof(saddr.sun_path));
	if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		die("socket");
	if (connect(sockfd, &saddr, sizeof(saddr)) == -1) {
		if (errno == ENOENT)
			diex("ewd daemon is not running");
		die("connect: %s", saddr.sun_path);
	}
	abgr2argb(img_d);
	srv_msg(sockfd, img_d, name);

	close(sockfd);
	close(img_d.fd);
	return rv;
}

void
srv_msg(int sockfd, struct img mmf, char *name)
{
	size_t nlen = strlen(name);
	u8 fd_buf[CMSG_SPACE(sizeof(int))];
	struct iovec iovs[] = {
		{.iov_base = &mmf.w, .iov_len = sizeof(mmf.w)},
		{.iov_base = &mmf.h, .iov_len = sizeof(mmf.h)},
		{.iov_base = &nlen,  .iov_len = sizeof(nlen) },
		{.iov_base = name,   .iov_len = nlen         },
	};
	struct msghdr msg = {
		.msg_iov = iovs,
		.msg_iovlen = lengthof(iovs),
		.msg_control = fd_buf,
		.msg_controllen = sizeof(fd_buf),
	};
	struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(int));
	memcpy(CMSG_DATA(cmsg), &mmf.fd, sizeof(mmf.fd));

	if (sendmsg(sockfd, &msg, 0) == -1)
		die("sendmsg");
}

void
abgr2argb(struct img f)
{
	for (size_t i = 0; i < f.size; i += 4) {
		u8 tmp = f.buf[i + 0];
		f.buf[i + 0] = f.buf[i + 2];
		f.buf[i + 2] = tmp;
	}
}

struct img
jxl_decode(struct bs img)
{
	void *tpr;
	size_t nthrds;
	struct img pix;
	JxlDecoder *d;
	JxlDecoderStatus res;
	JxlBasicInfo info;
	JxlPixelFormat fmt = {
		.align = 0,
		.data_type = JXL_TYPE_UINT8,
		.endianness = JXL_NATIVE_ENDIAN,
		.num_channels = sizeof(xrgb),
	};

	if (!(d = JxlDecoderCreate(NULL)))
		diex("Failed to allocate JXL decoder");

	nthrds = JxlThreadParallelRunnerDefaultNumWorkerThreads();
	if (!(tpr = JxlThreadParallelRunnerCreate(NULL, nthrds)))
		diex("Failed to allocate parallel runner");

	if (JxlDecoderSetParallelRunner(d, JxlThreadParallelRunner, tpr))
		diex("Failed to set parallel runner");

	if (JxlDecoderSubscribeEvents(d, JXL_DEC_FULL_IMAGE))
		diex("Failed to subscribe to events");

	if (JxlDecoderSetInput(d, img.buf, img.size))
		diex("Failed to set input data");
	JxlDecoderCloseInput(d);

	while ((res = JxlDecoderProcessInput(d)) != JXL_DEC_FULL_IMAGE) {
		switch (res) {
		case JXL_DEC_NEED_IMAGE_OUT_BUFFER:
			if (JxlDecoderImageOutBufferSize(d, &fmt, &pix.size))
				diex("Failed to get image output buffer size");
			if ((pix.fd = memfd_create("ewctl-mem", 0)) == -1)
				die("memfd_create");
			if (ftruncate(pix.fd, pix.size) == -1)
				die("ftruncate");
			pix.buf = mmap(NULL, pix.size, PROT_READ | PROT_WRITE, MAP_SHARED,
			               pix.fd, 0);
			if (pix.buf == MAP_FAILED)
				die("mmap");
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

	if (JxlDecoderGetBasicInfo(d, &info))
		diex("Failed to get dimensions of JXL image");

	pix.w = info.xsize;
	pix.h = info.ysize;

	JxlThreadParallelRunnerDestroy(tpr);
	JxlDecoderDestroy(d);

	return pix;
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
