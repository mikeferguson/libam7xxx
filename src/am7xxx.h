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

#ifdef __cplusplus
extern "C" {
#endif


struct _am7xxx_context;
typedef struct _am7xxx_context am7xxx_context;

struct _am7xxx_device;
typedef struct _am7xxx_device am7xxx_device;

typedef enum {
	AM7XXX_LOG_FATAL   = 0,
	AM7XXX_LOG_ERROR   = 1,
	AM7XXX_LOG_WARNING = 2,
	AM7XXX_LOG_INFO    = 3,
	AM7XXX_LOG_DEBUG   = 4,
	AM7XXX_LOG_TRACE   = 5,
} am7xxx_log_level;

typedef enum {
	AM7XXX_IMAGE_FORMAT_JPEG = 1,
	AM7XXX_IMAGE_FORMAT_NV12 = 2,
} am7xxx_image_format;

typedef enum {
	AM7XXX_POWER_OFF    = 0,
	AM7XXX_POWER_LOW    = 1,
	AM7XXX_POWER_MIDDLE = 2,
	AM7XXX_POWER_HIGH   = 3,
	AM7XXX_POWER_TURBO  = 4,
} am7xxx_power_mode;

int am7xxx_init(am7xxx_context **ctx);

void am7xxx_shutdown(am7xxx_context *ctx);

void am7xxx_set_log_level(am7xxx_context *ctx, am7xxx_log_level log_level);

int am7xxx_open_device(am7xxx_context *ctx,
		       am7xxx_device **dev,
		       unsigned int device_index);

int am7xxx_close_device(am7xxx_device *dev);

int am7xxx_get_device_info(am7xxx_device *dev,
			   unsigned int *native_width,
			   unsigned int *native_height,
			   unsigned int *unknown0,
			   unsigned int *unknown1);

int am7xxx_send_image(am7xxx_device *dev,
		      am7xxx_image_format format,
		      unsigned int width,
		      unsigned int height,
		      unsigned char *image,
		      unsigned int size);

/*
 * NOTE: if we set the mode to AM7XXX_POWER_OFF we can't turn the
 * display on again by using only am7xxx_set_power_mode().
 *
 * Remember to mention that when writing the API doc.
 */
int am7xxx_set_power_mode(am7xxx_device *dev, am7xxx_power_mode mode);

#ifdef __cplusplus
}
#endif

#endif /* __AM7XXX_H */
