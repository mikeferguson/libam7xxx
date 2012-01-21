/* am7xxx - communication with AM7XXX based USB Pico Projectors and DPFs
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

#ifndef __AM7XXX_H
#define __AM7XXX_H

#include <stdint.h>
#include <libusb-1.0/libusb.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef libusb_device_handle *am7xxx_device;

typedef enum {
	AM7XXX_PACKET_TYPE_INIT	   = 0x01,
	AM7XXX_PACKET_TYPE_IMAGE   = 0x02,
	AM7XXX_PACKET_TYPE_POWER   = 0x04,
	AM7XXX_PACKET_TYPE_UNKNOWN = 0x05,
} am7xxx_packet_type;

typedef enum {
	AM7XXX_IMAGE_FORMAT_JPEG     = 1,
	AM7XXX_IMAGE_FORMAT_YUV_NV12 = 2,
} am7xxx_image_format;

typedef enum {
	AM7XXX_POWER_OFF  = 0,
	AM7XXX_POWER_LOW  = 1,
	AM7XXX_POWER_MID  = 2,
	AM7XXX_POWER_HIGH = 3,
} am7xxx_power_mode;

struct am7xxx_image_header {
	uint32_t format;
	uint32_t width;
	uint32_t height;
	uint32_t image_size;
};

struct am7xxx_power_header {
	uint32_t power_low;
	uint32_t power_mid;
	uint32_t power_high;
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
		struct am7xxx_image_header image;
		struct am7xxx_power_header power;
	} header_data;
};

am7xxx_device am7xxx_init(void);

void am7xxx_shutdown(am7xxx_device dev);

int am7xxx_send_image(am7xxx_device dev,
		      am7xxx_image_format format,
		      unsigned int width,
		      unsigned int height,
		      uint8_t *image,
		      unsigned int size);

#ifdef __cplusplus
}
#endif

#endif /* __AM7XXX_H */
