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
#include <stdarg.h>
#include <errno.h>
#include <libusb.h>
#include <math.h>

#include "am7xxx.h"
#include "serialize.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

/* If we're not using GNU C, elide __attribute__
 * taken from: http://unixwiz.net/techtips/gnu-c-attributes.html)
 */
#ifndef __GNUC__
#  define  __attribute__(x)  /*NOTHING*/
#endif

/* Control shared library symbols visibility */
#if defined _WIN32 || defined __CYGWIN__
	#define AM7XXX_PUBLIC __declspec(dllexport)
	#define AM7XXX_LOCAL
#else
	#if __GNUC__ >= 4
		#define AM7XXX_PUBLIC __attribute__ ((visibility ("default")))
		#define AM7XXX_LOCAL  __attribute__ ((visibility ("hidden")))
	#else
		#define AM7XXX_PUBLIC
		#define AM7XXX_LOCAL
	#endif
#endif

static void log_message(am7xxx_context *ctx,
			int level,
			const char *function,
			int line,
			const char *fmt,
			...) __attribute__ ((format (printf, 5, 6)));

#define fatal(...)        log_message(NULL, AM7XXX_LOG_FATAL,   __func__, __LINE__, __VA_ARGS__)
#define error(ctx, ...)   log_message(ctx,  AM7XXX_LOG_ERROR,   __func__, __LINE__, __VA_ARGS__)
#define warning(ctx, ...) log_message(ctx,  AM7XXX_LOG_WARNING, __func__, 0,        __VA_ARGS__)
#define info(ctx, ...)    log_message(ctx,  AM7XXX_LOG_INFO,    __func__, 0,        __VA_ARGS__)
#define debug(ctx, ...)   log_message(ctx,  AM7XXX_LOG_DEBUG,   __func__, 0,        __VA_ARGS__)
#define trace(ctx, ...)   log_message(ctx,  AM7XXX_LOG_TRACE,   NULL,     0,        __VA_ARGS__)

struct am7xxx_usb_device_descriptor {
	const char *name;
	uint16_t vendor_id;
	uint16_t product_id;
	uint8_t configuration;    /* The bConfigurationValue of the device */
	uint8_t interface_number; /* The bInterfaceNumber of the device */
};

static const struct am7xxx_usb_device_descriptor supported_devices[] = {
	{
		.name       = "Acer C110",
		.vendor_id  = 0x1de1,
		.product_id = 0xc101,
		.configuration    = 2,
		.interface_number = 0,
	},
	{
		.name       = "Acer C112",
		.vendor_id  = 0x1de1,
		.product_id = 0x5501,
		.configuration    = 2,
		.interface_number = 0,
	},
	{
		.name       ="Aiptek PocketCinema T25",
		.vendor_id  = 0x08ca,
		.product_id = 0x2144,
		.configuration    = 2,
		.interface_number = 0,
	},
	{
		.name       = "Philips/Sagemcom PicoPix 1020",
		.vendor_id  = 0x21e7,
		.product_id = 0x000e,
		.configuration    = 2,
		.interface_number = 0,
	},
	{
		.name       = "Philips/Sagemcom PicoPix 2055",
		.vendor_id  = 0x21e7,
		.product_id = 0x0016,
		.configuration    = 2,
		.interface_number = 0,
	},
	{
		.name       = "Philips/Sagemcom PicoPix 2330",
		.vendor_id  = 0x21e7,
		.product_id = 0x0019,
		.configuration    = 1,
		.interface_number = 0,
	},
};

/* The header size on the wire is known to be always 24 bytes, regardless of
 * the memory configuration enforced by different architectures or compilers
 * for struct am7xxx_header
 */
#define AM7XXX_HEADER_WIRE_SIZE 24

struct _am7xxx_device {
	libusb_device_handle *usb_device;
	uint8_t buffer[AM7XXX_HEADER_WIRE_SIZE];
	am7xxx_device_info *device_info;
	am7xxx_context *ctx;
	const struct am7xxx_usb_device_descriptor *desc;
	am7xxx_device *next;
};

struct _am7xxx_context {
	libusb_context *usb_context;
	int log_level;
	am7xxx_device *devices_list;
};

typedef enum {
	AM7XXX_PACKET_TYPE_DEVINFO = 0x01,
	AM7XXX_PACKET_TYPE_IMAGE   = 0x02,
	AM7XXX_PACKET_TYPE_POWER   = 0x04,
	AM7XXX_PACKET_TYPE_ZOOM    = 0x05,
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

struct am7xxx_zoom_header {
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

/* Direction of the communication from the host point of view */
#define AM7XXX_DIRECTION_OUT 0 /* host -> device */
#define AM7XXX_DIRECTION_IN  1 /* host <- device */

struct am7xxx_header {
	uint32_t packet_type;
	uint8_t direction;
	uint8_t header_data_len;
	uint8_t unknown2;
	uint8_t unknown3;
	union {
		struct am7xxx_generic_header data;
		struct am7xxx_devinfo_header devinfo;
		struct am7xxx_image_header image;
		struct am7xxx_power_header power;
		struct am7xxx_zoom_header zoom;
	} header_data;
};


#ifdef DEBUG
static void debug_dump_devinfo_header(am7xxx_context *ctx, struct am7xxx_devinfo_header *d)
{
	if (ctx == NULL || d == NULL)
		return;

	debug(ctx, "Info header:\n");
	debug(ctx, "\tnative_width:  0x%08x (%u)\n", d->native_width, d->native_width);
	debug(ctx, "\tnative_height: 0x%08x (%u)\n", d->native_height, d->native_height);
	debug(ctx, "\tunknown0:      0x%08x (%u)\n", d->unknown0, d->unknown0);
	debug(ctx, "\tunknown1:      0x%08x (%u)\n", d->unknown1, d->unknown1);
}

static void debug_dump_image_header(am7xxx_context *ctx, struct am7xxx_image_header *i)
{
	if (ctx == NULL || i == NULL)
		return;

	debug(ctx, "Image header:\n");
	debug(ctx, "\tformat:     0x%08x (%u)\n", i->format, i->format);
	debug(ctx, "\twidth:      0x%08x (%u)\n", i->width, i->width);
	debug(ctx, "\theight:     0x%08x (%u)\n", i->height, i->height);
	debug(ctx, "\timage size: 0x%08x (%u)\n", i->image_size, i->image_size);
}

static void debug_dump_power_header(am7xxx_context *ctx, struct am7xxx_power_header *p)
{
	if (ctx == NULL || p == NULL)
		return;

	debug(ctx, "Power header:\n");
	debug(ctx, "\tbit2: 0x%08x (%u)\n", p->bit2, p->bit2);
	debug(ctx, "\tbit1: 0x%08x (%u)\n", p->bit1, p->bit1);
	debug(ctx, "\tbit0: 0x%08x (%u)\n", p->bit0, p->bit0);
}

static void debug_dump_zoom_header(am7xxx_context *ctx, struct am7xxx_zoom_header *z)
{
	if (ctx == NULL || z == NULL)
		return;

	debug(ctx, "Zoom header:\n");
	debug(ctx, "\tbit1: 0x%08x (%u)\n", z->bit1, z->bit1);
	debug(ctx, "\tbit0: 0x%08x (%u)\n", z->bit0, z->bit0);
}

static void debug_dump_header(am7xxx_context *ctx, struct am7xxx_header *h)
{
	if (ctx == NULL || h == NULL)
		return;

	debug(ctx, "BEGIN\n");
	debug(ctx, "packet_type:     0x%08x (%u)\n", h->packet_type, h->packet_type);
	debug(ctx, "direction:       0x%02hhx (%hhu) (%s)\n", h->direction, h->direction,
	      h->direction == AM7XXX_DIRECTION_IN ? "IN" :
	      h->direction == AM7XXX_DIRECTION_OUT ? "OUT" :
	      "UNKNOWN");
	debug(ctx, "header_data_len: 0x%02hhx (%hhu)\n", h->header_data_len, h->header_data_len);
	debug(ctx, "unknown2:        0x%02hhx (%hhu)\n", h->unknown2, h->unknown2);
	debug(ctx, "unknown3:        0x%02hhx (%hhu)\n", h->unknown3, h->unknown3);

	switch(h->packet_type) {
	case AM7XXX_PACKET_TYPE_DEVINFO:
		debug_dump_devinfo_header(ctx, &(h->header_data.devinfo));
		break;

	case AM7XXX_PACKET_TYPE_IMAGE:
		debug_dump_image_header(ctx, &(h->header_data.image));
		break;

	case AM7XXX_PACKET_TYPE_POWER:
		debug_dump_power_header(ctx, &(h->header_data.power));
		break;

	case AM7XXX_PACKET_TYPE_ZOOM:
		debug_dump_zoom_header(ctx, &(h->header_data.zoom));
		break;

	default:
		debug(ctx, "Packet type not supported!\n");
		break;
	}
	debug(ctx, "END\n\n");
}

static inline unsigned int in_80chars(unsigned int i)
{
	/* The 3 below is the length of "xx " where xx is the hex string
	 * representation of a byte */
	return ((i+1) % (80/3));
}

static void trace_dump_buffer(am7xxx_context *ctx, const char *message,
			      uint8_t *buffer, unsigned int len)
{
	unsigned int i;

	if (ctx == NULL || buffer == NULL || len == 0)
		return;

	trace(ctx, "\n");
	if (message)
		trace(ctx, "%s\n", message);

	for (i = 0; i < len; i++) {
		trace(ctx, "%02hhX%c", buffer[i], (in_80chars(i) && (i < len - 1)) ? ' ' : '\n');
	}
	trace(ctx, "\n");
}
#else
static void debug_dump_header(am7xxx_context *ctx, struct am7xxx_header *h)
{
	(void)ctx;
	(void)h;
}

static void trace_dump_buffer(am7xxx_context *ctx, const char *message,
			      uint8_t *buffer, unsigned int len)
{
	(void)ctx;
	(void)message;
	(void)buffer;
	(void)len;
}
#endif /* DEBUG */

static int read_data(am7xxx_device *dev, uint8_t *buffer, unsigned int len)
{
	int ret;
	int transferred = 0;

	ret = libusb_bulk_transfer(dev->usb_device, 0x81, buffer, len, &transferred, 0);
	if (ret != 0 || (unsigned int)transferred != len) {
		error(dev->ctx, "ret: %d\ttransferred: %d (expected %u)\n",
		      ret, transferred, len);
		return ret;
	}

	trace_dump_buffer(dev->ctx, "<-- received", buffer, len);

	return 0;
}

static int send_data(am7xxx_device *dev, uint8_t *buffer, unsigned int len)
{
	int ret;
	int transferred = 0;

	trace_dump_buffer(dev->ctx, "sending -->", buffer, len);

	ret = libusb_bulk_transfer(dev->usb_device, 0x1, buffer, len, &transferred, 0);
	if (ret != 0 || (unsigned int)transferred != len) {
		error(dev->ctx, "ret: %d\ttransferred: %d (expected %u)\n",
		      ret, transferred, len);
		return ret;
	}

	return 0;
}

static void serialize_header(struct am7xxx_header *h, uint8_t *buffer)
{
	uint8_t **buffer_iterator = &buffer;

	put_le32(h->packet_type, buffer_iterator);
	put_8(h->direction, buffer_iterator);
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
	h->direction = get_8(buffer_iterator);
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

	if (h->direction == AM7XXX_DIRECTION_IN) {
		ret = 0;
	} else {
		error(dev->ctx,
		      "Expected an AM7XXX_DIRECTION_IN packet, got one with direction = %d. Weird!\n",
		      h->direction);
		ret = -EINVAL;
	}

	debug_dump_header(dev->ctx, h);

out:
	return ret;
}

static int send_header(am7xxx_device *dev, struct am7xxx_header *h)
{
	int ret;

	debug_dump_header(dev->ctx, h);

	/* For symmetry with read_header() we should check here for
	 * h->direction == AM7XXX_DIRECTION_OUT but we just ensure that in all
	 * the callers and save some cycles here.
	 */

	serialize_header(h, dev->buffer);

	ret = send_data(dev, dev->buffer, AM7XXX_HEADER_WIRE_SIZE);
	if (ret < 0)
		error(dev->ctx, "failed to send data\n");

	return ret;
}

/* When level == AM7XXX_LOG_FATAL do not check the log_level from the context
 * and print the message unconditionally, this makes it possible to print
 * fatal messages even early on initialization, before the context has been
 * set up */
static void log_message(am7xxx_context *ctx,
			int level,
			const char *function,
			int line,
			const char *fmt,
			...)
{
	va_list ap;

	if (level == AM7XXX_LOG_FATAL || (ctx && level <= ctx->log_level)) {
		if (function) {
			fprintf(stderr, "%s", function);
			if (line)
				fprintf(stderr, "[%d]", line);
			fprintf(stderr, ": ");
		}

		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
	}

	return;
}

static am7xxx_device *add_new_device(am7xxx_context *ctx,
				     const struct am7xxx_usb_device_descriptor *desc)
{
	am7xxx_device **devices_list;
	am7xxx_device *new_device;

	if (ctx == NULL) {
		fatal("context must not be NULL!\n");
		return NULL;
	}

	new_device = malloc(sizeof(*new_device));
	if (new_device == NULL) {
		fatal("cannot allocate a new device (%s)\n", strerror(errno));
		return NULL;
	}
	memset(new_device, 0, sizeof(*new_device));

	new_device->ctx = ctx;
	new_device->desc = desc;

	devices_list = &(ctx->devices_list);

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

static am7xxx_device *find_device(am7xxx_context *ctx,
				  unsigned int device_index)
{
	unsigned int i = 0;
	am7xxx_device *current;

	if (ctx == NULL) {
		fatal("context must not be NULL!\n");
		return NULL;
	}

	current = ctx->devices_list;
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
		fatal("context must not be NULL!\n");
		return -EINVAL;
	}
	if (op == SCAN_OP_BUILD_DEVLIST && ctx->devices_list != NULL) {
		error(ctx, "device scan done already? Abort!\n");
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
			if (desc.idVendor == supported_devices[j].vendor_id &&
			    desc.idProduct == supported_devices[j].product_id) {

				if (op == SCAN_OP_BUILD_DEVLIST) {
					am7xxx_device *new_device;
					info(ctx, "am7xxx device found, index: %d, name: %s\n",
					     current_index,
					     supported_devices[j].name);
					new_device = add_new_device(ctx, &supported_devices[j]);
					if (new_device == NULL) {
						/* XXX, the caller may want
						 * to call am7xxx_shutdown() if
						 * we fail here, as we may have
						 * added some devices already
						 */
						debug(ctx, "Cannot create a new device\n");
						ret = -ENODEV;
						goto out;
					}
				} else if (op == SCAN_OP_OPEN_DEVICE &&
					   current_index == open_device_index) {

					*dev = find_device(ctx, open_device_index);
					if (*dev == NULL) {
						ret = -ENODEV;
						goto out;
					}

					/* the usb device has already been opened */
					if ((*dev)->usb_device) {
						debug(ctx, "(*dev)->usb_device already set\n");
						ret = 1;
						goto out;
					}

					ret = libusb_open(list[i], &((*dev)->usb_device));
					if (ret < 0) {
						debug(ctx, "libusb_open failed\n");
						goto out;
					}

					/* XXX, the device is now open, if any
					 * of the calls below fail we need to
					 * close it again before bailing out.
					 */

					ret = libusb_set_configuration((*dev)->usb_device,
								       (*dev)->desc->configuration);
					if (ret < 0) {
						debug(ctx, "libusb_set_configuration failed\n");
						debug(ctx, "Cannot set configuration %hhu\n",
						      (*dev)->desc->configuration);
						goto out_libusb_close;
					}

					ret = libusb_claim_interface((*dev)->usb_device,
								     (*dev)->desc->interface_number);
					if (ret < 0) {
						debug(ctx, "libusb_claim_interface failed\n");
						debug(ctx, "Cannot claim interface %hhu\n",
						      (*dev)->desc->interface_number);
out_libusb_close:
						libusb_close((*dev)->usb_device);
						(*dev)->usb_device = NULL;
						goto out;
					}

					goto out;
				}
				current_index++;
			}
		}
	}

	/* if we made it up to here we didn't find any device to open */
	if (op == SCAN_OP_OPEN_DEVICE) {
		error(ctx, "Cannot find any device to open\n");
		ret = -ENODEV;
		goto out;
	}

	/* everything went fine when building the device list */
	ret = 0;
out:
	libusb_free_device_list(list, 1);
	return ret;
}

/* Public API */

AM7XXX_PUBLIC int am7xxx_init(am7xxx_context **ctx)
{
	int ret = 0;

	*ctx = malloc(sizeof(**ctx));
	if (*ctx == NULL) {
		fatal("cannot allocate the context (%s)\n", strerror(errno));
		ret = -ENOMEM;
		goto out;
	}
	memset(*ctx, 0, sizeof(**ctx));

	/* Set the highest log level during initialization */
	(*ctx)->log_level = AM7XXX_LOG_TRACE;

	ret = libusb_init(&((*ctx)->usb_context));
	if (ret < 0)
		goto out_free_context;

	libusb_set_debug((*ctx)->usb_context, 3);

	ret = scan_devices(*ctx, SCAN_OP_BUILD_DEVLIST , 0, NULL);
	if (ret < 0) {
		error(*ctx, "scan_devices() failed\n");
		am7xxx_shutdown(*ctx);
		goto out;
	}

	/* Set a quieter log level as default for normal operation */
	(*ctx)->log_level = AM7XXX_LOG_ERROR;
	return 0;

out_free_context:
	free(*ctx);
	*ctx = NULL;
out:
	return ret;
}

AM7XXX_PUBLIC void am7xxx_shutdown(am7xxx_context *ctx)
{
	am7xxx_device *current;

	if (ctx == NULL) {
		fatal("context must not be NULL!\n");
		return;
	}

	current = ctx->devices_list;
	while (current) {
		am7xxx_device *next = current->next;
		am7xxx_close_device(current);
		free(current->device_info);
		free(current);
		current = next;
	}

	libusb_exit(ctx->usb_context);
	free(ctx);
	ctx = NULL;
}

AM7XXX_PUBLIC void am7xxx_set_log_level(am7xxx_context *ctx, am7xxx_log_level log_level)
{
	ctx->log_level = log_level;
}

AM7XXX_PUBLIC int am7xxx_open_device(am7xxx_context *ctx, am7xxx_device **dev,
		       unsigned int device_index)
{
	int ret;

	if (ctx == NULL) {
		fatal("context must not be NULL!\n");
		return -EINVAL;
	}

	ret = scan_devices(ctx, SCAN_OP_OPEN_DEVICE, device_index, dev);
	if (ret < 0) {
		errno = ENODEV;
		goto out;
	} else if (ret > 0) {
		warning(ctx, "device %d already open\n", device_index);
		errno = EBUSY;
		ret = -EBUSY;
		goto out;
	}

	/* Philips/Sagemcom PicoPix projectors require that the DEVINFO packet
	 * is the first one to be sent to the device in order for it to
	 * successfully return the correct device information.
	 *
	 * So, if there is not a cached version of it (from a previous open),
	 * we ask for device info at open time,
	 */
	if ((*dev)->device_info == NULL) {
		ret = am7xxx_get_device_info(*dev, NULL);
		if (ret < 0)
			error(ctx, "cannot get device info\n");
	}

out:
	return ret;
}

AM7XXX_PUBLIC int am7xxx_close_device(am7xxx_device *dev)
{
	if (dev == NULL) {
		fatal("dev must not be NULL!\n");
		return -EINVAL;
	}
	if (dev->usb_device) {
		libusb_release_interface(dev->usb_device, dev->desc->interface_number);
		libusb_close(dev->usb_device);
		dev->usb_device = NULL;
	}
	return 0;
}

AM7XXX_PUBLIC int am7xxx_get_device_info(am7xxx_device *dev,
			   am7xxx_device_info *device_info)
{
	int ret;
	struct am7xxx_header h = {
		.packet_type     = AM7XXX_PACKET_TYPE_DEVINFO,
		.direction       = AM7XXX_DIRECTION_OUT,
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

	if (dev->device_info) {
		memcpy(device_info, dev->device_info, sizeof(*device_info));
		return 0;
	}

	ret = send_header(dev, &h);
	if (ret < 0)
		return ret;

	ret = read_header(dev, &h);
	if (ret < 0)
		return ret;

	if (h.packet_type != AM7XXX_PACKET_TYPE_DEVINFO) {
		error(dev->ctx, "expected packet type: %d, got %d instead!\n",
		      AM7XXX_PACKET_TYPE_DEVINFO, h.packet_type);
		errno = ENOTSUP;
		return -ENOTSUP;
	}

	dev->device_info = malloc(sizeof(*dev->device_info));
	if (dev->device_info == NULL) {
		error(dev->ctx, "cannot allocate a device info (%s)\n",
		       strerror(errno));
		return -ENOMEM;
	}
	memset(dev->device_info, 0, sizeof(*dev->device_info));

	dev->device_info->native_width = h.header_data.devinfo.native_width;
	dev->device_info->native_height = h.header_data.devinfo.native_height;
#if 0
	/* No reason to expose these in the public API until we know what they mean */
	dev->device_info->unknown0 = h.header_data.devinfo.unknown0;
	dev->device_info->unknown1 = h.header_data.devinfo.unknown1;
#endif

	return 0;
}

AM7XXX_PUBLIC int am7xxx_calc_scaled_image_dimensions(am7xxx_device *dev,
					unsigned int upscale,
					unsigned int original_width,
					unsigned int original_height,
					unsigned int *scaled_width,
					unsigned int *scaled_height)
{

	am7xxx_device_info device_info;
	float width_ratio;
	float height_ratio;
	int ret;

	ret = am7xxx_get_device_info(dev, &device_info);
	if (ret < 0) {
		error(dev->ctx, "cannot get device info\n");
		return ret;
	}

	/*
	 * Check if we need to rescale; if the input image fits the native
	 * dimensions there is no need to, unless we want to upscale.
	 */
	if (!upscale &&
	    original_width <= device_info.native_width &&
	    original_height <= device_info.native_height ) {
		debug(dev->ctx, "CASE 0, no rescaling, the original image fits already\n");
		*scaled_width = original_width;
		*scaled_height = original_height;
		return 0;
	}

	/* Input dimensions relative to the device native dimensions */
	width_ratio =  (float)original_width / device_info.native_width;
	height_ratio = (float)original_height / device_info.native_height;

	if (width_ratio > height_ratio) {
		/*
		 * The input is proportionally "wider" than the device viewport
		 * so its height needs to be adjusted
		 */
		debug(dev->ctx, "CASE 1, original image wider, adjust the scaled height\n");
		*scaled_width = device_info.native_width;
		*scaled_height = (unsigned int)lroundf(original_height / width_ratio);
	} else if (width_ratio < height_ratio) {
		/*
		 * The input is proportionally "taller" than the device viewport
		 * so its width needs to be adjusted
		 */
		debug(dev->ctx, "CASE 2 original image taller, adjust the scaled width\n");
		*scaled_width = (unsigned int)lroundf(original_width / height_ratio);
		*scaled_height = device_info.native_height;
	} else {
		debug(dev->ctx, "CASE 3, just rescale, same aspect ratio already\n");
		*scaled_width = device_info.native_width;
		*scaled_height = device_info.native_height;
	}
	debug(dev->ctx, "scaled dimensions: %dx%d\n", *scaled_width, *scaled_height);

	return 0;
}

AM7XXX_PUBLIC int am7xxx_send_image(am7xxx_device *dev,
		      am7xxx_image_format format,
		      unsigned int width,
		      unsigned int height,
		      uint8_t *image,
		      unsigned int image_size)
{
	int ret;
	struct am7xxx_header h = {
		.packet_type     = AM7XXX_PACKET_TYPE_IMAGE,
		.direction       = AM7XXX_DIRECTION_OUT,
		.header_data_len = sizeof(struct am7xxx_image_header),
		.unknown2        = 0x3e,
		.unknown3        = 0x10,
		.header_data = {
			.image = {
				.format     = format,
				.width      = width,
				.height     = height,
				.image_size = image_size,
			},
		},
	};

	ret = send_header(dev, &h);
	if (ret < 0)
		return ret;

	if (image == NULL || image_size == 0) {
		warning(dev->ctx, "Not sending any data, check the 'image' or 'image_size' parameters\n");
		return 0;
	}

	return send_data(dev, image, image_size);
}

AM7XXX_PUBLIC int am7xxx_set_power_mode(am7xxx_device *dev, am7xxx_power_mode power)
{
	int ret;
	struct am7xxx_header h = {
		.packet_type     = AM7XXX_PACKET_TYPE_POWER,
		.direction       = AM7XXX_DIRECTION_OUT,
		.header_data_len = sizeof(struct am7xxx_power_header),
		.unknown2        = 0x3e,
		.unknown3        = 0x10,
	};

	switch(power) {
	case AM7XXX_POWER_OFF:
		h.header_data.power.bit2 = 0;
		h.header_data.power.bit1 = 0;
		h.header_data.power.bit0 = 0;
		break;

	case AM7XXX_POWER_LOW:
		h.header_data.power.bit2 = 0;
		h.header_data.power.bit1 = 0;
		h.header_data.power.bit0 = 1;
		break;

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
		error(dev->ctx, "Unsupported power mode.\n");
		return -EINVAL;
	};

	ret = send_header(dev, &h);
	if (ret < 0)
		return ret;

	return 0;
}

AM7XXX_PUBLIC int am7xxx_set_zoom_mode(am7xxx_device *dev, am7xxx_zoom_mode zoom)
{
	int ret;
	struct am7xxx_header h = {
		.packet_type     = AM7XXX_PACKET_TYPE_ZOOM,
		.direction       = AM7XXX_DIRECTION_OUT,
		.header_data_len = sizeof(struct am7xxx_zoom_header),
		.unknown2        = 0x3e,
		.unknown3        = 0x10,
	};

	switch(zoom) {
	case AM7XXX_ZOOM_ORIGINAL:
		h.header_data.zoom.bit1 = 0;
		h.header_data.zoom.bit0 = 0;
		break;

	case AM7XXX_ZOOM_H:
		h.header_data.zoom.bit1 = 0;
		h.header_data.zoom.bit0 = 1;
		break;

	case AM7XXX_ZOOM_H_V:
		h.header_data.zoom.bit1 = 1;
		h.header_data.zoom.bit0 = 0;
		break;

	case AM7XXX_ZOOM_TEST:
		h.header_data.zoom.bit1 = 1;
		h.header_data.zoom.bit0 = 1;
		break;

	default:
		error(dev->ctx, "Unsupported zoom mode.\n");
		return -EINVAL;
	};

	ret = send_header(dev, &h);
	if (ret < 0)
		return ret;

	return 0;
}
