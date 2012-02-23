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
#include <string.h>
#include <errno.h>
#include <libusb.h>

#include "am7xxx.h"
#include "serialize.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

struct am7xxx_usb_device_descriptor {
	const char *name;
	uint16_t vendor_id;
	uint16_t product_id;
};

static struct am7xxx_usb_device_descriptor supported_devices[] = {
	{
		.name       = "Acer C110",
		.vendor_id  = 0x1de1,
		.product_id = 0xc101,
	},
	{
		.name       = "Philips/Sagemcom PicoPix 1020",
		.vendor_id  = 0x21e7,
		.product_id = 0x000e,
	},
};

/* The header size on the wire is known to be always 24 bytes, regardless of
 * the memory configuration enforced by different architechtures or compilers
 * for struct am7xxx_header
 */
#define AM7XXX_HEADER_WIRE_SIZE 24

struct _am7xxx_device {
	libusb_device_handle *usb_device;
	uint8_t buffer[AM7XXX_HEADER_WIRE_SIZE];
	am7xxx_device *next;
};

struct _am7xxx_context {
	libusb_context *usb_context;
	am7xxx_device *devices_list;
};

typedef enum {
	AM7XXX_PACKET_TYPE_DEVINFO = 0x01,
	AM7XXX_PACKET_TYPE_IMAGE   = 0x02,
	AM7XXX_PACKET_TYPE_POWER   = 0x04,
	AM7XXX_PACKET_TYPE_UNKNOWN = 0x05,
} am7xxx_packet_type;

struct am7xxx_generic_header {
	uint32_t field0;
	uint32_t field1;
	uint32_t field2;
	uint32_t field3;
};

struct am7xxx_devinfo_header {
	uint32_t native_width;
	uint32_t native_height;
	uint32_t unknown0;
	uint32_t unknown1;
};

struct am7xxx_image_header {
	uint32_t format;
	uint32_t width;
	uint32_t height;
	uint32_t image_size;
};

struct am7xxx_power_header {
	uint32_t bit2;
	uint32_t bit1;
	uint32_t bit0;
};

/*
 * Examples of packet headers:
 *
 * Image header:
 * 02 00 00 00 00 10 3e 10 01 00 00 00 20 03 00 00 e0 01 00 00 53 E8 00 00
 *
 * Power header:
 * 04 00 00 00 00 0c ff ff 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
 */

struct am7xxx_header {
	uint32_t packet_type;
	uint8_t unknown0;
	uint8_t header_data_len;
	uint8_t unknown2;
	uint8_t unknown3;
	union {
		struct am7xxx_generic_header data;
		struct am7xxx_devinfo_header devinfo;
		struct am7xxx_image_header image;
		struct am7xxx_power_header power;
	} header_data;
};


static void dump_devinfo_header(struct am7xxx_devinfo_header *d)
{
	if (d == NULL)
		return;

	printf("Info header:\n");
	printf("\tnative_width:  0x%08x (%u)\n", d->native_width, d->native_width);
	printf("\tnative_height: 0x%08x (%u)\n", d->native_height, d->native_height);
	printf("\tunknown0:      0x%08x (%u)\n", d->unknown0, d->unknown0);
	printf("\tunknown1:      0x%08x (%u)\n", d->unknown1, d->unknown1);
}

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

static void dump_power_header(struct am7xxx_power_header *p)
{
	if (p == NULL)
		return;

	printf("Power header:\n");
	printf("\tbit2: 0x%08x (%u)\n", p->bit2, p->bit2);
	printf("\tbit1: 0x%08x (%u)\n", p->bit1, p->bit1);
	printf("\tbit0: 0x%08x (%u)\n", p->bit0, p->bit0);
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
	case AM7XXX_PACKET_TYPE_DEVINFO:
		dump_devinfo_header(&(h->header_data.devinfo));
		break;

	case AM7XXX_PACKET_TYPE_IMAGE:
		dump_image_header(&(h->header_data.image));
		break;

	case AM7XXX_PACKET_TYPE_POWER:
		dump_power_header(&(h->header_data.power));
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

static int read_data(am7xxx_device *dev, uint8_t *buffer, unsigned int len)
{
	int ret;
	int transferred = 0;

	ret = libusb_bulk_transfer(dev->usb_device, 0x81, buffer, len, &transferred, 0);
	if (ret != 0 || (unsigned int)transferred != len) {
		fprintf(stderr, "Error: ret: %d\ttransferred: %d (expected %u)\n",
			ret, transferred, len);
		return ret;
	}

#if DEBUG
	printf("\n<-- received\n");
	dump_buffer(buffer, len);
	printf("\n");
#endif

	return 0;
}

static int send_data(am7xxx_device *dev, uint8_t *buffer, unsigned int len)
{
	int ret;
	int transferred = 0;

#if DEBUG
	printf("\nsending -->\n");
	dump_buffer(buffer, len);
	printf("\n");
#endif

	ret = libusb_bulk_transfer(dev->usb_device, 1, buffer, len, &transferred, 0);
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

static void unserialize_header(uint8_t *buffer, struct am7xxx_header *h)
{
	uint8_t **buffer_iterator = &buffer;

	h->packet_type = get_le32(buffer_iterator);
	h->unknown0 = get_8(buffer_iterator);
	h->header_data_len = get_8(buffer_iterator);
	h->unknown2 = get_8(buffer_iterator);
	h->unknown3 = get_8(buffer_iterator);
	h->header_data.data.field0 = get_le32(buffer_iterator);
	h->header_data.data.field1 = get_le32(buffer_iterator);
	h->header_data.data.field2 = get_le32(buffer_iterator);
	h->header_data.data.field3 = get_le32(buffer_iterator);
}

static int read_header(am7xxx_device *dev, struct am7xxx_header *h)
{
	int ret;

	ret = read_data(dev, dev->buffer, AM7XXX_HEADER_WIRE_SIZE);
	if (ret < 0)
		goto out;

	unserialize_header(dev->buffer, h);

#if DEBUG
	printf("\n");
	dump_header(h);
	printf("\n");
#endif

	ret = 0;

out:
	return ret;
}

static int send_header(am7xxx_device *dev, struct am7xxx_header *h)
{
	int ret;

#if DEBUG
	printf("\n");
	dump_header(h);
	printf("\n");
#endif

	serialize_header(h, dev->buffer);
	ret = send_data(dev, dev->buffer, AM7XXX_HEADER_WIRE_SIZE);
	if (ret < 0)
		fprintf(stderr, "send_header: failed to send data.\n");

	return ret;
}

static am7xxx_device *add_new_device(am7xxx_device **devices_list)
{
	am7xxx_device *new_device;

	new_device = malloc(sizeof(*new_device));
	if (new_device == NULL) {
		perror("malloc");
		return NULL;
	}
	memset(new_device, 0, sizeof(*new_device));

	if (*devices_list == NULL) {
		*devices_list = new_device;
	} else {
		am7xxx_device *prev = *devices_list;
		while (prev->next)
			prev = prev->next;
		prev->next = new_device;
	}
	return new_device;
}

static am7xxx_device *find_device(am7xxx_device *devices_list,
				  unsigned int device_index)
{
	unsigned int i = 0;
	am7xxx_device *current = devices_list;

	while (current && i++ < device_index)
		current = current->next;

	return current;
}

typedef enum {
	SCAN_OP_BUILD_DEVLIST,
	SCAN_OP_OPEN_DEVICE,
} scan_op;

/**
 * This is where the central logic of multi-device support is.
 *
 * When 'op' == SCAN_OP_BUILD_DEVLIST the parameters 'open_device_index' and
 * 'dev' are ignored; the function returns 0 on success and a negative value
 * on error.
 *
 * When 'op' == SCAN_OP_OPEN_DEVICE the function opens the supported USB
 * device with index 'open_device_index' and returns the correspondent
 * am7xxx_device in the 'dev' parameter; the function returns 0 on success,
 * 1 if the device was already open and a negative value on error.
 * 
 * NOTES:
 * if scan_devices() fails when called with 'op' == SCAN_OP_BUILD_DEVLIST,
 * the caller might want to call am7xxx_shutdown() in order to remove
 * devices possibly added before the failure.
 */
static int scan_devices(am7xxx_context *ctx, scan_op op,
			unsigned int open_device_index, am7xxx_device **dev)
{
	int num_devices;
	libusb_device** list;
	unsigned int current_index;
	int i;
	int ret;

	if (ctx == NULL) {
		fprintf(stderr, "%s: context must not be NULL!\n", __func__);
		return -EINVAL;
	}
	if (op == SCAN_OP_BUILD_DEVLIST && ctx->devices_list != NULL) {
		fprintf(stderr, "%s: device scan done already? Abort!\n", __func__);
		return -EINVAL;
	}

	num_devices = libusb_get_device_list(ctx->usb_context, &list);
	if (num_devices < 0) {
		ret = -ENODEV;
		goto out;
	}

	current_index = 0;
	for (i = 0; i < num_devices; i++) {
		struct libusb_device_descriptor desc;
		unsigned int j;

		ret = libusb_get_device_descriptor(list[i], &desc);
		if (ret < 0)
			continue;

		for (j = 0; j < ARRAY_SIZE(supported_devices); j++) {
			if (desc.idVendor == supported_devices[j].vendor_id
			    && desc.idProduct == supported_devices[j].product_id) {

				if (op == SCAN_OP_BUILD_DEVLIST) {
					am7xxx_device *new_device;
					/* debug */
					printf("am7xxx device found, index: %d, name: %s\n",
					       current_index,
					       supported_devices[j].name);
					new_device = add_new_device(&(ctx->devices_list));
					if (new_device == NULL) {
						/* XXX, the caller may want
						 * to call am7xxx_shutdown() if
						 * we fail here, as we may have
						 * added some devices already
						 */
						ret = -ENODEV;
						goto out;
					}
				} else if (op == SCAN_OP_OPEN_DEVICE &&
					   current_index == open_device_index) {

					*dev = find_device(ctx->devices_list, open_device_index);
					if (*dev == NULL) {
						ret = -ENODEV;
						goto out;
					}

					/* the usb device has already been opened */
					if ((*dev)->usb_device) {
						ret = 1;
						goto out;
					}

					ret = libusb_open(list[i], &((*dev)->usb_device));
					if (ret < 0)
						goto out;

					libusb_set_configuration((*dev)->usb_device, 1);
					libusb_claim_interface((*dev)->usb_device, 0);
					goto out;
				}
				current_index++;
			}
		}
	}

	/* if we made it up to here we didn't find any device to open */
	if (op == SCAN_OP_OPEN_DEVICE) {
		ret = -ENODEV;
		goto out;
	}

out:
	libusb_free_device_list(list, 1);
	return ret;
}

int am7xxx_init(am7xxx_context **ctx)
{
	int ret = 0;

	*ctx = malloc(sizeof(**ctx));
	if (*ctx == NULL) {
		perror("malloc");
		ret = -ENOMEM;
		goto out;
	}
	memset(*ctx, 0, sizeof(**ctx));

	ret = libusb_init(&((*ctx)->usb_context));
	if (ret < 0)
		goto out_free_context;

	libusb_set_debug((*ctx)->usb_context, 3);

	ret = scan_devices(*ctx, SCAN_OP_BUILD_DEVLIST , 0, NULL);
	if (ret < 0) {
		fprintf(stderr, "%s: scan_devices failed\n", __func__);
		am7xxx_shutdown(*ctx);
		goto out;
	}

	return 0;

out_free_context:
	free(*ctx);
	*ctx = NULL;
out:
	return ret;
}

void am7xxx_shutdown(am7xxx_context *ctx)
{
	am7xxx_device *current;

	if (ctx == NULL) {
		fprintf(stderr, "%s: context must not be NULL!\n", __func__);
		return;
	}

	current = ctx->devices_list;
	while (current) {
		am7xxx_device *next = current->next;
		am7xxx_close_device(current);
		free(current);
		current = next;
	}

	libusb_exit(ctx->usb_context);
	free(ctx);
	ctx = NULL;
}

int am7xxx_open_device(am7xxx_context *ctx, am7xxx_device **dev,
		       unsigned int device_index)
{
	int ret;

	if (ctx == NULL) {
		fprintf(stderr, "%s: context must not be NULL!\n", __func__);
		return -EINVAL;
	}

	ret = scan_devices(ctx, SCAN_OP_OPEN_DEVICE, device_index, dev);
	if (ret < 0) {
		errno = ENODEV;
	} else if (ret > 0) {
		/* warning */
		fprintf(stderr, "%s: device %d already open\n", __func__, device_index);
		errno = EBUSY;
		ret = -EBUSY;
	}

	return ret;
}

int am7xxx_close_device(am7xxx_device *dev)
{
	if (dev == NULL) {
		fprintf(stderr, "%s: dev must not be NULL!\n", __func__);
		return -EINVAL;
	}
	if (dev->usb_device) {
		libusb_release_interface(dev->usb_device, 0);
		libusb_close(dev->usb_device);
		dev->usb_device = NULL;
	}
	return 0;
}

int am7xxx_get_device_info(am7xxx_device *dev,
			   unsigned int *native_width,
			   unsigned int *native_height,
			   unsigned int *unknown0,
			   unsigned int *unknown1)
{
	int ret;
	struct am7xxx_header h = {
		.packet_type     = AM7XXX_PACKET_TYPE_DEVINFO,
		.unknown0        = 0x00,
		.header_data_len = 0x00,
		.unknown2        = 0x3e,
		.unknown3        = 0x10,
		.header_data = {
			.devinfo = {
				.native_width  = 0,
				.native_height = 0,
				.unknown0      = 0,
				.unknown1      = 0,
			},
		},
	};

	ret = send_header(dev, &h);
	if (ret < 0)
		return ret;

	ret = read_header(dev, &h);
	if (ret < 0)
		return ret;

	*native_width = h.header_data.devinfo.native_width;
	*native_height = h.header_data.devinfo.native_height;
	*unknown0 = h.header_data.devinfo.unknown0;
	*unknown1 = h.header_data.devinfo.unknown1;

	return 0;
}

int am7xxx_send_image(am7xxx_device *dev,
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

int am7xxx_set_power_mode(am7xxx_device *dev, am7xxx_power_mode mode)
{
	int ret;
	struct am7xxx_header h = {
		.packet_type     = AM7XXX_PACKET_TYPE_POWER,
		.unknown0        = 0x00,
		.header_data_len = sizeof(struct am7xxx_power_header),
		.unknown2        = 0x3e,
		.unknown3        = 0x10,
	};

	switch(mode) {
	case AM7XXX_POWER_OFF:
		h.header_data.power.bit2 = 0;
		h.header_data.power.bit1 = 0;
		h.header_data.power.bit0 = 0;
		break;

	case AM7XXX_POWER_LOW:
		h.header_data.power.bit2 = 0;
		h.header_data.power.bit1 = 0;
		h.header_data.power.bit0 = 1;

	case AM7XXX_POWER_MIDDLE:
		h.header_data.power.bit2 = 0;
		h.header_data.power.bit1 = 1;
		h.header_data.power.bit0 = 0;
		break;

	case AM7XXX_POWER_HIGH:
		h.header_data.power.bit2 = 0;
		h.header_data.power.bit1 = 1;
		h.header_data.power.bit0 = 1;
		break;

	case AM7XXX_POWER_TURBO:
		h.header_data.power.bit2 = 1;
		h.header_data.power.bit1 = 0;
		h.header_data.power.bit0 = 0;
		break;

	default:
		fprintf(stderr, "Unsupported power mode.\n");
		return -EINVAL;
	};

	ret = send_header(dev, &h);
	if (ret < 0)
		return ret;

	return 0;
}
