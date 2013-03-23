/* am7xxx_mode_switch - a simple usb_mode_switch for am7xxx devices
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
#include <errno.h>
#include <libusb.h>

#define AM7XXX_STORAGE_VID           0x1de1
#define AM7XXX_STORAGE_PID           0x1101
#define AM7XXX_STORAGE_CONFIGURATION 1
#define AM7XXX_STORAGE_INTERFACE     0
#define AM7XXX_STORAGE_OUT_EP        0x01

static unsigned char switch_command[] =
	"\x55\x53\x42\x43\x08\x70\x52\x89\x00\x00\x00\x00\x00\x00"
	"\x0c\xff\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";

int main(void)
{
	int ret;
	int transferred;
	libusb_device_handle *usb_device = NULL;

	unsigned int len;

	ret = libusb_init(NULL);
	if (ret < 0)
		goto out;

	libusb_set_debug(NULL, 3);

	usb_device = libusb_open_device_with_vid_pid(NULL,
						     AM7XXX_STORAGE_VID,
						     AM7XXX_STORAGE_PID);
	if (usb_device == NULL) {
		fprintf(stderr, "cannot open the device: %d.\n", errno);
		ret = -errno;
		goto out;
	}

	if (libusb_kernel_driver_active(usb_device, AM7XXX_STORAGE_INTERFACE)) {
		ret = libusb_detach_kernel_driver(usb_device,
						  AM7XXX_STORAGE_INTERFACE);
		if (ret < 0)
			fprintf(stderr, "Warning: cannot detach kernel driver.\n");
	} else {
		fprintf(stderr, "kernel driver not active.\n");
	}

	ret = libusb_set_configuration(usb_device, AM7XXX_STORAGE_CONFIGURATION);
	if (ret < 0) {
		fprintf(stderr, "cannot set configuration.\n");
		goto out_libusb_close;
	}

	ret = libusb_claim_interface(usb_device, AM7XXX_STORAGE_INTERFACE);
	if (ret < 0) {
		fprintf(stderr, "cannot claim interface.\n");
		goto out_libusb_close;
	}

	len = sizeof(switch_command);
	transferred = 0;
	ret = libusb_bulk_transfer(usb_device, AM7XXX_STORAGE_OUT_EP,
				   switch_command, len, &transferred, 0);
	if (ret != 0 || (unsigned int)transferred != len) {
		fprintf(stderr, "ret: %d\ttransferred: %d (expected %u)\n",
		      ret, transferred, len);
		goto out_libusb_release_interface;
	}

	fprintf(stderr, "OK, command sent!\n");

out_libusb_release_interface:
	libusb_release_interface(usb_device, AM7XXX_STORAGE_INTERFACE);
out_libusb_close:
	libusb_close(usb_device);
	usb_device = NULL;
out:
	libusb_exit(NULL);
	return ret;
}
