#include <err.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define die(...)  err(EXIT_FAILURE, __VA_ARGS__)
#define diex(...) errx(EXIT_FAILURE, __VA_ARGS__)

typedef uint8_t u8;

int
main(int argc, char **argv)
{
	u8 *img;
	int w, h;
	size_t size;
	FILE *fp;

	if (argc != 2) {
		fputs("Usage: png2xrgb file\n", stderr);
		exit(EXIT_FAILURE);
	}

	if ((fp = fopen(argv[1], "rb")) == NULL)
		die("fopen: %s", argv[1]);
	if ((img = stbi_load_from_file(fp, &w, &h, NULL, 4)) == NULL)
		diex("stbi_load_from_file: %s", argv[1]);
	fclose(fp);

	size = h * w * 4;
	for (size_t i = 0; i < size; i += 4) {
		img[i + 3] = img[i + 2];
		img[i + 2] = img[i + 1];
		img[i + 1] = img[i + 0];
	}

	write(STDOUT_FILENO, img, size);
	return EXIT_SUCCESS;
}
