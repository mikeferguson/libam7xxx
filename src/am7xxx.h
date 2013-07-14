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

/**
 * @file
 * Public libam7xxx API.
 */

#ifndef __AM7XXX_H
#define __AM7XXX_H

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @typedef am7xxx_context
 *
 * An opaque data type representing a context.
 */
struct _am7xxx_context;
typedef struct _am7xxx_context am7xxx_context;

/**
 * @typedef am7xxx_device
 *
 * An opaque data type representing an am7xxx device.
 */
struct _am7xxx_device;
typedef struct _am7xxx_device am7xxx_device;

/**
 * A struct describing device specific properties.
 *
 * A user program may want to inspect these before providing data to the
 * device. For instance, when sending an image the user may want to rescale it
 * to the device native width and height in order to be sure the image will be
 * displayed in its entirety.
 */
typedef struct {
	unsigned int native_width;  /**< The device native width. */
	unsigned int native_height; /**< The device native height. */
} am7xxx_device_info;

/**
 * The verbosity level of logging messages.
 *
 * This can be set with am7xxx_set_log_level() and the level will be used
 * internally by libam7xxx to adjust the granularity of the information
 * exposed to the user about the internal library operations.
 */
typedef enum {
	AM7XXX_LOG_FATAL   = 0, /**< Fatal messages, the user application should stop if it gets one of this. */
	AM7XXX_LOG_ERROR   = 1, /**< Error messages, typically they describe API functions failures. */
	AM7XXX_LOG_WARNING = 2, /**< Warnings about conditions worth mentioning to the user. */
	AM7XXX_LOG_INFO    = 3, /**< Informations about the device operations. */
	AM7XXX_LOG_DEBUG   = 4, /**< Informations about the library internals. */
	AM7XXX_LOG_TRACE   = 5, /**< Verbose informations about the communication with the hardware. */
} am7xxx_log_level;

/**
 * The image formats accepted by the device.
 */
typedef enum {
	AM7XXX_IMAGE_FORMAT_JPEG = 1, /**< JPEG format. */
	AM7XXX_IMAGE_FORMAT_NV12 = 2, /**< Raw YUV in the NV12 variant. */
} am7xxx_image_format;

/**
 * The device power modes.
 *
 * An am7xxx device can operate in several power modes. A certain power mode
 * may have effect on the display brightness or on the device power
 * consumption.
 *
 * @note Most am7xxx devices come with a Y-shaped USB cable with a Master and
 * a Slave connector, higher power modes may require that both connectors are
 * plugged in to the host system for the device to work properly.
 *
 * @note At higher power modes some devices may use a fan to cool down the
 * internal hardware components, and hence may be noisier in this case.
 */
typedef enum {
	AM7XXX_POWER_OFF    = 0, /**< Display is powered off, no image shown. */
	AM7XXX_POWER_LOW    = 1, /**< Low power consumption but also low brightness. */
	AM7XXX_POWER_MIDDLE = 2, /**< Middle level of brightness. This and upper modes need both the Master and Slave USB connectors plugged. */
	AM7XXX_POWER_HIGH   = 3, /**< More brightness, but more power consumption. */
	AM7XXX_POWER_TURBO  = 4, /**< Max brightness and power consumption. */
} am7xxx_power_mode;

/**
 * The display zoom modes.
 *
 * An am7xxx device can display images using several zoom modes.
 *
 * @note Changing the zoom mode can change the aspect ratio of the displayed
 * image.
 *
 * @note On the zoom test screen the version of the firmware running on the
 * device is shown as well (e.g SPI_V21.0.0_2011.03.18).
 */
typedef enum {
	AM7XXX_ZOOM_ORIGINAL = 0, /**< Original Size, as retrieved via #am7xxx_device_info. */
	AM7XXX_ZOOM_H        = 1, /**< Zoom 1: H Scale (changes aspect ratio). */
	AM7XXX_ZOOM_H_V      = 2, /**< Zoom 2: H/V Scale (changes aspect ratio). */
	AM7XXX_ZOOM_TEST     = 3, /**< Zoom test screen, the firmware version is shown as well. */
} am7xxx_zoom_mode;

/**
 * Initialize the library context and data structures, and scan for devices.
 *
 * @param[out] ctx A pointer to the context the library will be used in.
 *
 * @return 0 on success, a negative value on error
 */
int am7xxx_init(am7xxx_context **ctx);

/**
 * Cleanup the library data structures and free the context.
 *
 * @param[in,out] ctx The context to free.
 */
void am7xxx_shutdown(am7xxx_context *ctx);

/**
 * Set verbosity level of log messages.
 *
 * @note The level is per-context.
 *
 * @note Messages of level AM7XXX_LOG_FATAL are always shown, regardless
 * of the value of the log_level parameter.
 *
 * @param[in] ctx The context to set the log level for
 * @param[in] log_level The verbosity level to use in the context (see @link am7xxx_log_level @endlink)
 */
void am7xxx_set_log_level(am7xxx_context *ctx, am7xxx_log_level log_level);

/**
 * Open an am7xxx_device according to a index.
 *
 * The semantics of the 'device_index' argument follows the order
 * of the devices as found when scanning the bus at am7xxx_init() time.
 *
 * @note When the user tries to open a device already opened the function
 * returns -EBUSY and the device is left open.
 *
 * @param[in] ctx The context to open the device in
 * @param[out] dev A pointer to the structure representing the device to open
 * @param[in] device_index The index of the device on the bus
 *
 * @return 0 on success, a negative value on error
 */
int am7xxx_open_device(am7xxx_context *ctx,
		       am7xxx_device **dev,
		       unsigned int device_index);

/**
 * Close an am7xxx_device.
 *
 * Close an am7xxx_device so that it becomes available for some other
 * user/process to open it.
 *
 * @param[in] dev A pointer to the structure representing the device to close
 *
 * @return 0 on success, a negative value on error
 */
int am7xxx_close_device(am7xxx_device *dev);

/**
 * Get info about an am7xxx device.
 *
 * Get information about a device, in the form of a
 * @link am7xxx_device_info @endlink structure.
 *
 * @param[in] dev A pointer to the structure representing the device to get info of
 * @param[out] device_info A pointer to the structure where to store the device info (see @link am7xxx_device_info @endlink)
 *
 * @return 0 on success, a negative value on error
 */
int am7xxx_get_device_info(am7xxx_device *dev,
			   am7xxx_device_info *device_info);

/**
 * Calculate the dimensions of an image to be shown on an am7xxx device.
 *
 * Before sending images bigger than the device native dimensions the user
 * needs to rescale them, this utility function does the calculation in a way
 * that the original image aspect ratio is preserved.
 *
 * @param[in] dev A pointer to the structure representing the device to get info of
 * @param[in] upscale Whether to calculate scaled dimensions for images smaller than the native dimensions
 * @param[in] original_width The width of the original image
 * @param[in] original_height The height of the original image
 * @param[out] scaled_width The width the rescaled image should have
 * @param[out] scaled_height The height the rescaled image should have
 *
 * @return 0 on success, a negative value on error
 */
int am7xxx_calc_scaled_image_dimensions(am7xxx_device *dev,
					unsigned int upscale,
					unsigned int original_width,
					unsigned int original_height,
					unsigned int *scaled_width,
					unsigned int *scaled_height);
/**
 * Send an image for display on an am7xxx device.
 *
 * This is the function that actually makes the device display something.
 * Static pictures can be sent just once and the device will keep showing them
 * until another image get sent or some command resets or turns off the display.
 *
 * @param[in] dev A pointer to the structure representing the device to get info of
 * @param[in] format The format the image is in (see @link am7xxx_image_format @endlink enum)
 * @param[in] width The width of the image
 * @param[in] height The height of the image
 * @param[in] image A buffer holding data in the format specified by the format parameter
 * @param[in] image_size The size in bytes of the image buffer
 *
 * @return 0 on success, a negative value on error
 */
int am7xxx_send_image(am7xxx_device *dev,
		      am7xxx_image_format format,
		      unsigned int width,
		      unsigned int height,
		      unsigned char *image,
		      unsigned int image_size);

/**
 * Set the power mode of an am7xxx device.
 *
 * @note When setting the mode to AM7XXX_POWER_OFF the display can't be turned
 * on again by using only am7xxx_set_power_mode(), am7xxx_set_zoom_mode() has
 * to be called first, the current guess is that the latter performs some
 * other resets beside setting the zoom mode.
 *
 * @param[in] dev A pointer to the structure representing the device to set power mode to
 * @param[in] power The power mode to put the device in (see #am7xxx_power_mode enum)
 *
 * @return 0 on success, a negative value on error
 *
 */
int am7xxx_set_power_mode(am7xxx_device *dev, am7xxx_power_mode power);

/**
 * Set the zoom mode of an am7xxx device.
 *
 * @note When setting the mode to AM7XXX_ZOOM_TEST, the calling program might
 * want to skip displaying actual images.
 *
 * @note It looks like that power mode and zoom mode are related somehow wrt.
 * resetting the operational mode after AM7XXX_POWER_OFF, applications can
 * restore the display properly using this combination:
 *  - Off: power mode 0, zoom mode 3
 *  - On: power mode != 0, zoom mode != 3
 *
 * @param[in] dev A pointer to the structure representing the device to set zoom mode to
 * @param[in] zoom The zoom mode to put the device in (see #am7xxx_zoom_mode enum)
 *
 * @return 0 on success, a negative value on error
 *
 */
int am7xxx_set_zoom_mode(am7xxx_device *dev, am7xxx_zoom_mode zoom);

#ifdef __cplusplus
}
#endif

#endif /* __AM7XXX_H */
