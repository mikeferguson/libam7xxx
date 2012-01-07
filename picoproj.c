/* picoproj - communication with AM7xxx based USB pico projectors
 *
 * Copyright (C) 2011  Antonio Ospite <ospite@studenti.unina.it>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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
#include <stdint.h>
#include <string.h>
#include <endian.h>
#include <errno.h>
#include <unistd.h>

typedef enum {
	AM7x01_PACKET_TYPE_INIT	   = 0x01,
	AM7x01_PACKET_TYPE_IMAGE   = 0x02,
	AM7x01_PACKET_TYPE_POWER   = 0x04,
	AM7x01_PACKET_TYPE_UNKNOWN = 0x05,
} am7x01_packet_type;

typedef enum {
	AM7x01_IMAGE_FORMAT_JPEG = 1,
} am7x01_image_format;

typedef enum {
	AM7x01_POWER_OFF  = 0,
	AM7x01_POWER_LOW  = 1,
	AM7x01_POWER_MID  = 2,
	AM7x01_POWER_HIGH = 3,
} am7x01_power_mode;

struct image_header {
	uint32_t format;
	uint32_t width;
	uint32_t height;
	uint32_t image_size;
};

struct power_header {
	uint32_t power_low;
	uint32_t power_mid;
	uint32_t power_high;
};

/*
 * Examples of packet headers:
 *
 * Image widget:
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * +02|00|00|00|00|10|3e|10|01|00|00|00|20|03|00|00|e0|01|00|00|53|E8|00|00+
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * Brightness widget:
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * +04|00|00|00|00|0c|ff|ff|00|00|00|00|00|00|00|00|00|00|00|00|00|00|00|00+
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

static uint8_t reference_image_header[] = {
	0x02, 0x00, 0x00, 0x00,
	0x00,
	0x10,
	0x3e,
	0x10,
	0x01, 0x00, 0x00, 0x00,
	0x20, 0x03, 0x00, 0x00,
	0xe0, 0x01, 0x00, 0x00,
	0x53, 0xE8, 0x00, 0x00
};

struct header {
	uint32_t packet_type;
	uint8_t unknown0;
	uint8_t header_len;
	uint8_t unknown2;
	uint8_t unknown3;
	union {
		struct image_header image;
		struct power_header power;
	} header_data;
};


static void dump_image_header(struct image_header *i)
{
	if (i == NULL)
		return;

	printf("Image header:\n");
	printf("format:      0x%08x (%u)\n", i->format, i->format);
	printf("width:       0x%08x (%u)\n", i->width, i->width);
	printf("height:      0x%08x (%u)\n", i->height, i->height);
	printf("image size:  0x%08x (%u)\n", i->image_size, i->image_size);
}

static void dump_header(struct header *h)
{
	if (h == NULL)
		return;

	printf("packet_type: 0x%08x (%u)\n", h->packet_type, h->packet_type);
	printf("unknown0:    0x%02hhx (%hhu)\n", h->unknown0, h->unknown0);
	printf("header_len:  0x%02hhx (%hhu)\n", h->header_len, h->header_len);
	printf("unknown2:    0x%02hhx (%hhu)\n", h->unknown2, h->unknown2);
	printf("unknown3:    0x%02hhx (%hhu)\n", h->unknown3, h->unknown3);

	switch(h->packet_type) {
	case AM7x01_PACKET_TYPE_IMAGE:
		dump_image_header(&(h->header_data.image));
		break;

	default:
		printf("Packet type not supported!\n");
		break;
	}

	fflush(stdout);
}

static inline unsigned int in_80chars(unsigned int i)
{
	return ((i+1) % (80/3));
}

static void dump_buffer(uint8_t *buffer, unsigned int len)
{
	unsigned int i;

	if (buffer == NULL || len == 0)
		return;

	for (i = 0; i < len; i++) {
		printf("%02hhX%c", buffer[i], (in_80chars(i) && (i < len - 1)) ? ' ' : '\n');
	}
	fflush(stdout);
}

static int send_data(uint8_t *buffer, unsigned int len)
{
	dump_buffer(buffer, len);
	return 0;
}

static int send_header(struct header *h)
{
	union {
		struct header header;
		uint8_t buffer[sizeof (struct header)];
	} data;

	data.header = *h;

	return send_data(data.buffer, sizeof (struct header));
}

static int send_image(am7x01_image_format format,
		      unsigned int width,
		      unsigned int height,
		      uint8_t *image,
		      unsigned int size)
{
	int ret;
	struct header h = {
		.packet_type = htole32(AM7x01_PACKET_TYPE_IMAGE),
		.unknown0    = 0x00,
		.header_len  = sizeof(struct image_header),
		.unknown2    = 0x3e,
		.unknown3    = 0x10,
		.header_data = {
			.image = {
				.format     = htole32(format),
				.width      = htole32(width),
				.height     = htole32(height),
				.image_size = htole32(size),
			},
		},
	};

	dump_header(&h);
	printf("\n");

	printf("Dump Buffers\n");
	dump_buffer(reference_image_header, sizeof(struct header));

	ret = send_header(&h);
	if (ret < 0)
		return ret;

	if (image == NULL || size == 0)
		return 0;

	return send_data(image, size);
}

static void usage(char *name)
{
	printf("usage: %s [OPTIONS]\n\n", name);
	printf("OPTIONS:\n");
	printf("\t-f <filename>\t\tthe image file to upload\n");
	printf("\t-F <format>\t\tthe image format to use (default is JPEG).\n");
	printf("\t\t\t\tSUPPORTED FORMATS:\n");
	printf("\t\t\t\t\t1 - JPEG\n");
	printf("\t-W <image width>\tthe width of the image to upload\n");
	printf("\t-H <image height>\tthe height of the image to upload\n");
	printf("\t-h \t\t\tthis help message\n");
}

int main(int argc, char *argv[])
{
	int ret;
	int opt;

	char filename[FILENAME_MAX] = {0};
	int format = AM7x01_IMAGE_FORMAT_JPEG;
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
			if (format != 1) {
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

	ret = send_image(format, width, height, image, size);
	if (ret < 0) {
		perror("send_image");
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
