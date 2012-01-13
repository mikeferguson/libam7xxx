/* am7xxx - communication with AM7xxx based USB Pico Projectors and DPFs
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
#include <endian.h>
#include <errno.h>

#include "am7xxx.h"

#define AM7XXX_VENDOR_ID  0x1de1
#define AM7XXX_PRODUCT_ID 0xc101

#if 1
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
#endif

static void dump_image_header(struct am7xxx_image_header *i)
{
	if (i == NULL)
		return;

	printf("Image header:\n");
	printf("format:      0x%08x (%u)\n", i->format, i->format);
	printf("width:       0x%08x (%u)\n", i->width, i->width);
	printf("height:      0x%08x (%u)\n", i->height, i->height);
	printf("image size:  0x%08x (%u)\n", i->image_size, i->image_size);
}

static void dump_header(struct am7xxx_header *h)
{
	if (h == NULL)
		return;

	printf("packet_type: 0x%08x (%u)\n", h->packet_type, h->packet_type);
	printf("unknown0:    0x%02hhx (%hhu)\n", h->unknown0, h->unknown0);
	printf("header_len:  0x%02hhx (%hhu)\n", h->header_len, h->header_len);
	printf("unknown2:    0x%02hhx (%hhu)\n", h->unknown2, h->unknown2);
	printf("unknown3:    0x%02hhx (%hhu)\n", h->unknown3, h->unknown3);

	switch(h->packet_type) {
	case AM7XXX_PACKET_TYPE_IMAGE:
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

static int send_data(am7xxx_device dev, uint8_t *buffer, unsigned int len)
{
	int ret;
	int transferred;

	dump_buffer(buffer, len);

	ret = libusb_bulk_transfer(dev, 1, buffer, len, &transferred, 0);
	if (ret != 0 || (unsigned int)transferred != len) {
		fprintf(stderr, "Error: ret: %d\ttransferred: %d (expected %u)\n",
			ret, transferred, len);
		return ret;
	}

	return 0;
}

static int send_header(am7xxx_device dev, struct am7xxx_header *h)
{
	union {
		struct am7xxx_header header;
		uint8_t buffer[sizeof (struct am7xxx_header)];
	} data;

	data.header = *h;

	return send_data(dev, data.buffer, sizeof (struct am7xxx_header));
}

am7xxx_device am7xxx_init(void)
{
	am7xxx_device dev;

	libusb_init(NULL);
	libusb_set_debug(NULL, 3);

	dev = libusb_open_device_with_vid_pid(NULL,
					      AM7XXX_VENDOR_ID,
					      AM7XXX_PRODUCT_ID);
	if (dev == NULL) {
		errno = ENODEV;
		perror("libusb_open_device_with_vid_pid");
		goto out_libusb_exit;
	}

	libusb_set_configuration(dev, 1);
	libusb_claim_interface(dev, 0);

	return dev;

out_libusb_exit:
	libusb_exit(NULL);
	return NULL;
}

void am7xxx_shutdown(am7xxx_device dev)
{
	if (dev) {
		libusb_close(dev);
		libusb_exit(NULL);
	}
}

int am7xxx_send_image(am7xxx_device dev,
		      am7xxx_image_format format,
		      unsigned int width,
		      unsigned int height,
		      uint8_t *image,
		      unsigned int size)
{
	int ret;
	struct am7xxx_header h = {
		.packet_type = htole32(AM7XXX_PACKET_TYPE_IMAGE),
		.unknown0    = 0x00,
		.header_len  = sizeof(struct am7xxx_image_header),
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
	dump_buffer(reference_image_header, sizeof(struct am7xxx_header));

	ret = send_header(dev, &h);
	if (ret < 0)
		return ret;

	if (image == NULL || size == 0)
		return 0;

	return send_data(dev, image, size);
}
