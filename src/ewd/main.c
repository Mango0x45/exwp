#include <sys/mman.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>

#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-util.h>

#include "common.h"
#include "da.h"
#include "scale.h"
#include "types.h"

#include "proto/wlr-layer-shell-unstable-v1.h"

#define SOCK_BACKLOG 128

struct output {
	u32 name;          /* Wayland output name */
	bool safe_to_draw; /* Safe to draw new frame? */
	char *human_name;  /* Human-readable name (e.g. ‘eDP-1’) */

	/* Display- and image dimensions */
	struct {
		u32 w, h;
	} disp, img;

	struct {
		u8 *p;
		size_t size;
	} buf;

	wl_buffer_t *wl_buf;
	wl_output_t *wl_out;
	wl_surface_t *surf;
	zwlr_layer_surface_v1_t *layer;
};

/* Wayland listener event handlers */
static void buf_free(void *, wl_buffer_t *);
static void ls_close(void *, zwlr_layer_surface_v1_t *);
static void ls_conf(void *, zwlr_layer_surface_v1_t *, u32, u32, u32);
static void out_desc(void *, wl_output_t *, const char *);
static void out_done(void *, wl_output_t *);
static void out_geom(void *, wl_output_t *, i32, i32, i32, i32, i32,
                     const char *, const char *, i32);
static void out_mode(void *, wl_output_t *, u32, i32, i32, i32);
static void out_name(void *, wl_output_t *, const char *);
static void out_scale(void *, wl_output_t *, i32);
static void reg_add(void *, wl_registry_t *, u32, const char *, u32);
static void reg_del(void *, wl_registry_t *, u32);
static void shm_fmt(void *, wl_shm_t *, u32);

/* Normal functions */
static void cleanup(void);
static void clear(struct output *);
static void draw(struct output *);
static bool mkbuf(struct output *, u8 *, u32, u32);
static void out_layer_free(struct output *);
static void surf_create(struct output *);

static wl_compositor_t *comp;
static wl_display_t *disp;
static wl_registry_t *reg;
static wl_shm_t *shm;
static zwlr_layer_shell_v1_t *lshell;

/* We use this to check in the cleanup routine whether or not we need to unlink
   the daemon’s socket.  Without this check, starting a second daemon while a
   first daemon is already running would cause the exiting second daemon to
   unlink the first daemons socket. */
bool sock_bound = false;

static struct {
	struct output *buf;
	size_t len, cap;
} outputs;

static const wl_buffer_listener_t buf_listener = {
	.release = buf_free,
};

static const wl_output_listener_t out_listener = {
	.description = out_desc,
	.done = out_done,
	.geometry = out_geom,
	.mode = out_mode,
	.name = out_name,
	.scale = out_scale,
};

static const wl_registry_listener_t reg_listener = {
	.global = reg_add,
	.global_remove = reg_del,
};

static const wl_shm_listener_t shm_listener = {
	.format = shm_fmt,
};

static const zwlr_layer_surface_v1_listener_t ls_listener = {
	.configure = ls_conf,
	.closed = ls_close,
};

int
main(int argc, char **argv)
{
	/* Helper macro to reference an FD (e.g. ‘FD(SOCK)’) */
#define FD(x) fds[FD_##x].fd

	int opt;
	bool fg = false;
	sigset_t mask;
	struct option longopts[] = {
		{"foreground", no_argument, 0, 'f'},
		{"help",       no_argument, 0, 'h'},
		{NULL,         0,           0, 0  },
	};
	struct sockaddr_un saddr = {
		.sun_family = AF_UNIX,
	};
	struct pollfd fds[] = {
		{.events = POLLIN},
		{.events = POLLIN},
		{.events = POLLIN},
	};
	enum {
		FD_WAY,
		FD_SIG,
		FD_SOCK,
	};

	*argv = basename(*argv);
	while ((opt = getopt_long(argc, argv, "fh", longopts, NULL)) != -1) {
		switch (opt) {
		case 'f':
			fg = true;
			break;
		case 'h':
			execlp("man", "man", "1", *argv, NULL);
			die("execlp: man 1 %s", *argv);
		default:
			fprintf(stderr, "Usage: %s [-f | -h]\n", *argv);
			exit(EXIT_FAILURE);
		}
	}

	atexit(cleanup);
	da_init(&outputs, 8);

	/* Connect to the Wayland display and register all the global objects.  The
	   first roundtrip doesn’t register any current outputs, so we need to
	   roundtrip twice. */
	if (!(disp = wl_display_connect(NULL)))
		diex("Failed to connect to wayland display");
	if (!(reg = wl_display_get_registry(disp)))
		diex("Failed to obtain registry handle");

	wl_registry_add_listener(reg, &reg_listener, NULL);
	wl_display_roundtrip(disp);

	if (!comp)
		diex("Failed to obtain compositor handle");
	if (!shm)
		diex("Failed to obtain shm handle");
	if (!lshell)
		diex("Failed to obtain layer_shell handle");

	wl_display_roundtrip(disp);

	/* Poll for SIGINT and SIGQUIT and properly disconnect from the server
	   instead of crashing and burning when we receive them */
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGQUIT);
	sigprocmask(SIG_BLOCK, &mask, NULL);

	FD(WAY) = wl_display_get_fd(disp);
	if ((FD(SIG) = signalfd(-1, &mask, 0)) == -1)
		die("signalfd");

	/* Setup daemon socket */
	memcpy(saddr.sun_path, ewd_sock_path(), sizeof(saddr.sun_path));
	if ((FD(SOCK) = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		die("socket");
	if (bind(FD(SOCK), &saddr, sizeof(saddr)) == -1) {
		if (errno != EADDRINUSE)
			die("bind");
		diex("socket at %s already exists; another daemon may already be "
		     "running",
		     saddr.sun_path);
	}
	sock_bound = true;
	if (listen(FD(SOCK), SOCK_BACKLOG) == -1)
		die("listen");

	if (!fg && daemon(0, 0) == -1)
		die("daemon");

	for (;;) {
		/* Helper macro to check for events (e.g. ‘EVENT(SOCK, POLLIN)’) */
#define EVENT(x, e) (fds[FD_##x].revents & e)

		int ret;

		wl_display_flush(disp);
		do
			ret = poll(fds, lengthof(fds), -1);
		while (ret == -1 && errno == EINTR);
		if (ret == -1)
			die("poll");

		if (EVENT(WAY, POLLHUP) || EVENT(SIG, POLLIN))
			break;

		if (EVENT(WAY, POLLIN)) {
			if (wl_display_dispatch(disp) == -1)
				break;
		} else if (EVENT(SOCK, POLLIN)) {
			int cfd, mfd;
			char *name = NULL;
			size_t nlen;
			u32 w, h;
			u8 *src, fdbuf[CMSG_SPACE(sizeof(int))];
			struct iovec iovs[] = {
				{.iov_base = &w,    .iov_len = sizeof(w)   },
				{.iov_base = &h,    .iov_len = sizeof(h)   },
				{.iov_base = &nlen, .iov_len = sizeof(nlen)},
			};
			struct msghdr msg = {
				.msg_iov = iovs,
				.msg_iovlen = lengthof(iovs),
				.msg_control = fdbuf,
				.msg_controllen = sizeof(fdbuf),
			};
			struct cmsghdr *cmsg;

			src = MAP_FAILED;
			cfd = mfd = -1;
			if ((cfd = accept(FD(SOCK), NULL, NULL)) == -1) {
				warn("accept");
				goto err;
			}
			if (recvmsg(cfd, &msg, 0) == -1) {
				warn("recvmsg");
				goto err;
			}

			cmsg = CMSG_FIRSTHDR(&msg);
			memcpy(&mfd, CMSG_DATA(cmsg), sizeof(mfd));

			if (nlen) {
				struct iovec iov = {.iov_len = nlen};

				iov.iov_base = name = xmalloc(nlen + 1);
				if (readv(cfd, &iov, 1) == -1) {
					warn("readv");
					goto err;
				}
				name[nlen] = 0;
			}

			if ((src = mmap(NULL, w * h * sizeof(xrgb), PROT_READ, MAP_PRIVATE,
			                mfd, 0))
			    == MAP_FAILED)
			{
				warn("mmap");
				goto err;
			}

			da_foreach (&outputs, out) {
				if (!name || streq(out->human_name, name)) {
					if (w * h == 0)
						clear(out);
					else {
						out->img.w = w;
						out->img.h = h;
						if (!mkbuf(out, src, w, h))
							goto err;
						draw(out);
					}
				}
			}

err:
			free(name);
			if (mfd != -1)
				close(mfd);
			if (cfd != -1)
				close(cfd);
			if (src != MAP_FAILED)
				munmap(src, w * h * sizeof(xrgb));
		}
#undef EVENT
	}

	for (size_t i = 0; i < lengthof(fds); i++)
		close(fds[i].fd);
	return EXIT_SUCCESS;
#undef FD
}

void
surf_create(struct output *out)
{
	u32 anchor;
	wl_region_t *input, *opaque;

	if (!comp || !lshell)
		return;

	out->surf = wl_compositor_create_surface(comp);

	input = wl_compositor_create_region(comp);
	opaque = wl_compositor_create_region(comp);
	wl_surface_set_input_region(out->surf, input);
	wl_surface_set_opaque_region(out->surf, opaque);
	wl_region_destroy(input);
	wl_region_destroy(opaque);

	out->layer = zwlr_layer_shell_v1_get_layer_surface(
		lshell, out->surf, out->wl_out, ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND,
		"wallpaper");

	anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP
	       | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT
	       | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM
	       | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
	zwlr_layer_surface_v1_set_exclusive_zone(out->layer, -1);
	zwlr_layer_surface_v1_set_anchor(out->layer, anchor);
	zwlr_layer_surface_v1_add_listener(out->layer, &ls_listener, out);

	wl_surface_commit(out->surf);
}

void
clear(struct output *out)
{
	out_layer_free(out);
	surf_create(out);
}

bool
mkbuf(struct output *out, u8 *src, u32 w, u32 h)
{
	int mfd = -1;
	bool rv = false;
	wl_shm_pool_t *pool;

	out->buf.size = out->disp.w * out->disp.h * sizeof(xrgb);

	if ((mfd = memfd_create("ewd-shm", 0)) == -1) {
		warn("memfd_create");
		goto err;
	}
	if (ftruncate(mfd, out->buf.size) == -1) {
		warn("ftruncate");
		goto err;
	}
	if ((out->buf.p = mmap(NULL, out->buf.size, PROT_READ | PROT_WRITE,
	                       MAP_SHARED, mfd, 0))
	    == MAP_FAILED)
	{
		warn("mmap");
		goto err;
	}

	if (!(pool = wl_shm_create_pool(shm, mfd, out->buf.size))) {
		warnx("Failed to create shm pool");
		goto err;
	}
	if (!(out->wl_buf = wl_shm_pool_create_buffer(
			  pool, 0, out->disp.w, out->disp.h, out->disp.w * sizeof(xrgb),
			  WL_SHM_FORMAT_XRGB8888)))
	{
		warnx("Failed to create shm pool buffer");
		goto err;
	}
	wl_shm_pool_destroy(pool);

	scale(out->buf.p, out->disp.w, out->disp.h, src, w, h);
	rv = true;
err:
	close(mfd);
	return rv;
}

void
draw(struct output *out)
{
	wl_buffer_add_listener(out->wl_buf, &buf_listener, out);
	wl_surface_attach(out->surf, out->wl_buf, 0, 0);
	wl_surface_damage_buffer(out->surf, 0, 0, out->disp.w, out->disp.h);
	wl_surface_commit(out->surf);
}

void
reg_add(void *data, wl_registry_t *reg, u32 name, const char *iface, u32 ver)
{
#define assert_ver(n) \
	if (ver < n) { \
		diex("The v%" PRIu32 " %s interface is required, but the compositor " \
		     "only supports v%" PRIu32, \
		     n, iface, ver); \
	}
#define is(s) streq(iface, s.name)

	if (is(wl_compositor_interface)) {
		assert_ver(4);
		comp = wl_registry_bind(reg, name, &wl_compositor_interface, 4);
	} else if (is(wl_shm_interface)) {
		shm = wl_registry_bind(reg, name, &wl_shm_interface, 1);
		wl_shm_add_listener(shm, &shm_listener, NULL);
	} else if (is(wl_output_interface)) {
		struct output *out;
		struct wl_output *wl_out;

		assert_ver(4);
		wl_out = wl_registry_bind(reg, name, &wl_output_interface, 4);
		da_append(&outputs, ((struct output){.wl_out = wl_out, .name = name}));
		out = &outputs.buf[outputs.len - 1];
		wl_output_add_listener(wl_out, &out_listener, out);
		surf_create(out);
	} else if (is(zwlr_layer_shell_v1_interface)) {
		assert_ver(2);
		lshell = wl_registry_bind(reg, name, &zwlr_layer_shell_v1_interface, 2);
	}
#undef is
#undef assert_ver
}

void
reg_del(void *data, wl_registry_t *reg, u32 name)
{
	da_foreach (&outputs, out) {
		if (out->name == name) {
			out_layer_free(out);
			if (out->wl_out)
				wl_output_release(out->wl_out);
			free(out->human_name);
			da_remove(&outputs, out - outputs.buf);
			return;
		}
	}
}

void
out_mode(void *data, wl_output_t *out, u32 flags, i32 w, i32 h, i32 fps)
{
	((struct output *)data)->disp.w = w;
	((struct output *)data)->disp.h = h;
}

void
out_name(void *data, wl_output_t *wl_out, const char *name)
{
	if (!(((struct output *)data)->human_name = strdup(name)))
		warn("strdup: %s", name);
}

void
out_scale(void *data, wl_output_t *wl_out, i32 scale)
{
}

void
out_geom(void *data, wl_output_t *out, i32 x, i32 y, i32 pw, i32 ph, i32 sp,
         const char *make, const char *model, i32 tform)
{
}

void
out_desc(void *data, wl_output_t *out, const char *name)
{
}

void
out_done(void *data, wl_output_t *out)
{
}

void
ls_conf(void *data, zwlr_layer_surface_v1_t *surf, u32 serial, u32 w, u32 h)
{
	struct output *out = data;
	zwlr_layer_surface_v1_ack_configure(surf, serial);

	/* If the size of the last committed buffer has not changed, we don’t need
	   to do anything. */
	if (out->safe_to_draw && out->disp.w == w && out->disp.h == h)
		wl_surface_commit(out->surf);
	else {
		out->disp.w = w;
		out->disp.h = h;
		out->safe_to_draw = true;
	}
}

void
ls_close(void *data, zwlr_layer_surface_v1_t *surf)
{
	struct output *out = data;

	/* Don’t trust ‘output’ to be valid, in case compositor destroyed if
	   before calling closed() */
	da_foreach (&outputs, p) {
		if (out->name == p->name) {
			out_layer_free(out);
			return;
		}
	}
}

/* Unused; all wayland compositors must support XRGB8888 */
void
shm_fmt(void *data, wl_shm_t *shm, u32 fmt)
{
}

void
out_layer_free(struct output *out)
{
	if (out->layer) {
		zwlr_layer_surface_v1_destroy(out->layer);
		out->layer = NULL;
	}
	if (out->surf) {
		wl_surface_destroy(out->surf);
		out->surf = NULL;
	}

	out->name = 0;
	out->safe_to_draw = false;
}

void
buf_free(void *data, wl_buffer_t *wl_buf)
{
	struct output *out = data;
	wl_buffer_destroy(out->wl_buf);
	munmap(out->buf.p, out->buf.size);
}

void
cleanup(void)
{
	if (sock_bound)
		unlink(ewd_sock_path());
	da_foreach (&outputs, out) {
		if (out->wl_out)
			wl_output_release(out->wl_out);
		out_layer_free(out);
		free(out->human_name);
	}
	free(outputs.buf);
	if (lshell)
		zwlr_layer_shell_v1_destroy(lshell);
	if (shm)
		wl_shm_destroy(shm);
	if (comp)
		wl_compositor_destroy(comp);
	if (reg)
		wl_registry_destroy(reg);
	if (disp)
		wl_display_disconnect(disp);
}
