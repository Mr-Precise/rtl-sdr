/*
 * rtl-sdr, turns your Realtek RTL2832 based DVB dongle into a SDR receiver
 * wine usbfs fallback backend
 *
 * Copyright (C) 2026 by Precise <https://github.com/Mr-Precise>
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

#ifndef RTL_WINE_USBFS_H
#define RTL_WINE_USBFS_H

#ifdef _WIN32

#include <stdint.h>
#include "rtl-sdr.h"

#define MAX_WINE_DEVICES 16

typedef struct wine_usb_device {
	char posix_path[64];
	uint16_t vid;
	uint16_t pid;
	char manufact[256];
	char product[256];
	char serial[256];
} wine_usb_device_t;

#ifdef __cplusplus
extern "C" {
#endif

int wine_usbfs_detected(void);
int wine_usbfs_scan_devices(void);
uint32_t wine_usbfs_get_device_count(void);
const wine_usb_device_t *wine_usbfs_get_device(uint32_t index);
const char *wine_usbfs_get_device_name(uint32_t index);
int wine_usbfs_get_device_usb_strings(uint32_t index, char *manufact, char *product, char *serial);
int wine_usbfs_get_index_by_serial(const char *serial);

int wine_usbfs_open(uint32_t index);
void wine_usbfs_close(int fd);
int wine_usbfs_reset_device(int fd);

int wine_usbfs_control_transfer(int fd, uint8_t bmRequestType, uint8_t bRequest,
                               uint16_t wValue, uint16_t wIndex,
                               unsigned char *data, uint16_t wLength, unsigned int timeout);

int wine_usbfs_bulk_transfer(int fd, unsigned char endpoint,
                             unsigned char *data, int length,
                             int *transferred, unsigned int timeout);

int wine_usbfs_read_async(int fd, rtlsdr_read_async_cb_t cb, void *ctx,
                          uint32_t buf_num, uint32_t buf_len,
                          int *async_status, int *async_cancel);

int wine_usbfs_cancel_async(int fd);

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 */
#endif /* RTL_WINE_USBFS_H */
