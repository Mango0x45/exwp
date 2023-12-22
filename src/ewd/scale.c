#include <sys/param.h>

#include <pixman.h>

#include "common.h"

void
scale(u8 *restrict dst, u32 dw, u32 dh, const u8 *restrict src, u32 sw, u32 sh)
{
	double s;
	pixman_image_t *simg, *dimg;
	pixman_transform_t tfrm;
	pixman_f_transform_t ftfrm;

	simg = pixman_image_create_bits(PIXMAN_x8r8g8b8, sw, sh, (u32 *)src,
	                                sw * sizeof(xrgb));
	pixman_image_set_filter(simg, PIXMAN_FILTER_BEST, NULL, 0);

	s = MAX((double)sw / dw, (double)sh / dh);

	pixman_f_transform_init_scale(&ftfrm, s, s);
	pixman_transform_from_pixman_f_transform(&tfrm, &ftfrm);
	pixman_image_set_transform(simg, &tfrm);
	dimg = pixman_image_create_bits(PIXMAN_a8r8g8b8, dw, dh, (u32 *)dst,
	                                dw * sizeof(xrgb));
	pixman_image_composite(PIXMAN_OP_OVER, simg, NULL, dimg, s, s, 0, 0, 0, 0,
	                       dw, dh);
	pixman_image_unref(simg);
	pixman_image_unref(dimg);
}
