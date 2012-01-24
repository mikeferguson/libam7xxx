/* am7xxx - communication with AM7xxx based USB Pico Projectors and DPFs
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
#include <errno.h>

#include "am7xxx.h"
#include "serialize.h"

#define AM7XXX_VENDOR_ID  0x1de1
#define AM7XXX_PRODUCT_ID 0xc101

static void dump_image_header(struct am7xxx_image_header *i)
{
	if (i == NULL)
		return;

	printf("Image header:\n");
	printf("\tformat:     0x%08x (%u)\n", i->format, i->format);
	printf("\twidth:      0x%08x (%u)\n", i->width, i->width);
	printf("\theight:     0x%08x (%u)\n", i->height, i->height);
	printf("\timage size: 0x%08x (%u)\n", i->image_size, i->image_size);
}

static void dump_header(struct am7xxx_header *h)
{
	if (h == NULL)
		return;

	printf("packet_type:     0x%08x (%u)\n", h->packet_type, h->packet_type);
	printf("unknown0:        0x%02hhx (%hhu)\n", h->unknown0, h->unknown0);
	printf("header_data_len: 0x%02hhx (%hhu)\n", h->header_data_len, h->header_data_len);
	printf("unknown2:        0x%02hhx (%hhu)\n", h->unknown2, h->unknown2);
	printf("unknown3:        0x%02hhx (%hhu)\n", h->unknown3, h->unknown3);

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
	/* The 3 below is the length of "xx " where xx is the hex string
	 * representation of a byte */
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

#if DEBUG
	dump_buffer(buffer, len);
	printf("\n");
#endif

	ret = libusb_bulk_transfer(dev, 1, buffer, len, &transferred, 0);
	if (ret != 0 || (unsigned int)transferred != len) {
		fprintf(stderr, "Error: ret: %d\ttransferred: %d (expected %u)\n",
			ret, transferred, len);
		return ret;
	}

	return 0;
}

static void serialize_header(struct am7xxx_header *h, uint8_t *buffer)
{
	uint8_t **buffer_iterator = &buffer;

	put_le32(h->packet_type, buffer_iterator);
	put_8(h->unknown0, buffer_iterator);
	put_8(h->header_data_len, buffer_iterator);
	put_8(h->unknown2, buffer_iterator);
	put_8(h->unknown3, buffer_iterator);
	put_le32(h->header_data.data.field0, buffer_iterator);
	put_le32(h->header_data.data.field1, buffer_iterator);
	put_le32(h->header_data.data.field2, buffer_iterator);
	put_le32(h->header_data.data.field3, buffer_iterator);
}

static int send_header(am7xxx_device dev, struct am7xxx_header *h)
{
	uint8_t *buffer;
	int ret;

#if DEBUG
	dump_header(h);
	printf("\n");
#endif

	buffer = calloc(AM7XXX_HEADER_WIRE_SIZE, 1);
	if (buffer == NULL) {
		perror("calloc buffer");
		return -ENOMEM;
	}

	serialize_header(h, buffer);
	ret = send_data(dev, buffer, AM7XXX_HEADER_WIRE_SIZE);
	if (ret < 0)
		fprintf(stderr, "send_header: failed to send data.\n");

	free(buffer);
	return ret;
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
		.packet_type     = AM7XXX_PACKET_TYPE_IMAGE,
		.unknown0        = 0x00,
		.header_data_len = sizeof(struct am7xxx_image_header),
		.unknown2        = 0x3e,
		.unknown3        = 0x10,
		.header_data = {
			.image = {
				.format     = format,
				.width      = width,
				.height     = height,
				.image_size = size,
			},
		},
	};

	ret = send_header(dev, &h);
	if (ret < 0)
		return ret;

	if (image == NULL || size == 0)
		return 0;

	return send_data(dev, image, size);
}
