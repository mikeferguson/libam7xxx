/* picoproj - test program for libam7xxx
 *
 * Copyright (C) 2012  Antonio Ospite <ospite@studenti.unina.it>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "am7xxx.h"

static void usage(char *name)
{
	printf("usage: %s [OPTIONS]\n\n", name);
	printf("OPTIONS:\n");
	printf("\t-f <filename>\t\tthe image file to upload\n");
	printf("\t-F <format>\t\tthe image format to use (default is JPEG).\n");
	printf("\t\t\t\tSUPPORTED FORMATS:\n");
	printf("\t\t\t\t\t1 - JPEG\n");
	printf("\t\t\t\t\t2 - YUV - NV12\n");
	printf("\t-W <image width>\tthe width of the image to upload\n");
	printf("\t-H <image height>\tthe height of the image to upload\n");
	printf("\t-h \t\t\tthis help message\n");
}

int main(int argc, char *argv[])
{
	int ret;
	int exit_code = EXIT_SUCCESS;
	int opt;

	char filename[FILENAME_MAX] = {0};
	int image_fd = -1;
	am7xxx_device dev;
	int format = AM7XXX_IMAGE_FORMAT_JPEG;
	int width = 800;
	int height = 480;
	uint8_t *image = NULL;
	unsigned int size = 59475;

	while ((opt = getopt(argc, argv, "f:F:W:H:h")) != -1) {
		switch (opt) {
		case 'f':
			strncpy(filename, optarg, FILENAME_MAX);
			break;
		case 'F':
			format = atoi(optarg);
			switch(format) {
			case AM7XXX_IMAGE_FORMAT_JPEG:
				fprintf(stdout, "JPEG format\n");
				break;
			case AM7XXX_IMAGE_FORMAT_YUV_NV12:
				fprintf(stdout, "NV12 format\n");
				break;
			default:
				fprintf(stderr, "Unsupported format\n");
				exit(EXIT_FAILURE);
			}
			break;
		case 'W':
			width = atoi(optarg);
			if (width < 0) {
				fprintf(stderr, "Unsupported width\n");
				exit(EXIT_FAILURE);
			}
			break;
		case 'H':
			height = atoi(optarg);
			if (height < 0) {
				fprintf(stderr, "Unsupported height\n");
				exit(EXIT_FAILURE);
			}
			break;
		default: /* '?' */
			usage(argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	if (filename[0] != '\0') {
		struct stat st;
		
		image_fd = open(filename, O_RDONLY);
		if (image_fd < 0) {
			perror("open");
			exit_code = EXIT_FAILURE;
			goto out;
		}
		if (fstat(image_fd, &st) < 0) {
			perror("fstat");
			exit_code = EXIT_FAILURE;
			goto out_close_image_fd;
		}
		size = st.st_size;

		image = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, image_fd, 0);
		if (image == NULL) {
			perror("mmap");
			exit_code = EXIT_FAILURE;
			goto out_close_image_fd;
		}
	}

	dev = am7xxx_init();
	if (dev == NULL) {
		perror("am7xxx_init");
		exit_code = EXIT_FAILURE;
		goto out_munmap;
	}

	ret = am7xxx_send_image(dev, format, width, height, image, size);
	if (ret < 0) {
		perror("am7xxx_send_image");
		exit_code = EXIT_FAILURE;
		goto cleanup;
	}

	exit_code = EXIT_SUCCESS;

cleanup:
	am7xxx_shutdown(dev);

out_munmap:
	if (image != NULL) {
		ret = munmap(image, size);
		if (ret < 0)
			perror("munmap");
	}

out_close_image_fd:
	if (image_fd >= 0) {
		ret = close(image_fd);
		if (ret < 0)
			perror("close");
	}

out:
	exit(exit_code);
}
