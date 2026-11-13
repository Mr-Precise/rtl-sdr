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

#ifdef _WIN32

#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "rtl_wine_usbfs.h"

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

/* forward declaration from librtlsdr.c */
typedef struct rtlsdr_dongle {
	uint16_t vid;
	uint16_t pid;
	const char *name;
} rtlsdr_dongle_t;

extern rtlsdr_dongle_t *find_known_device(uint16_t vid, uint16_t pid);

/* usbfs ioctl structs */
struct usbdevfs_ctrltransfer {
	uint8_t  bRequestType;
	uint8_t  bRequest;
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t wLength;
	uint32_t timeout; /* milliseconds */
	void    *data;
};

struct usbdevfs_bulktransfer {
	uint32_t ep;
	uint32_t len;
	uint32_t timeout;
	void    *data;
};

struct usbdevfs_urb {
	uint8_t  type;
	uint8_t  endpoint;
	int32_t  status;
	uint32_t flags;
	void    *buffer;
	int32_t  buffer_length;
	int32_t  actual_length;
	int32_t  start_frame;
	int32_t  number_of_packets;
	int32_t  error_count;
	uint32_t signr;
	void    *usercontext;
};

/*
 * Linux system call numbers:
 * Defined by the Linux kernel ABI in <asm/unistd_32.h> and <asm/unistd_64.h>.
 * In Linux, syscall numbers for an architecture are fixed ABI and never change.
 */
#if defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64)
#define LINUX_NR_READ              0
#define LINUX_NR_WRITE             1
#define LINUX_NR_OPEN              2
#define LINUX_NR_CLOSE             3
#define LINUX_NR_IOCTL             16
#define LINUX_NR_GETDENTS64        217
#else
#define LINUX_NR_READ              3
#define LINUX_NR_WRITE             4
#define LINUX_NR_OPEN              5
#define LINUX_NR_CLOSE             6
#define LINUX_NR_IOCTL             54
#define LINUX_NR_GETDENTS64        220
#endif

/*
 * Linux ioctl request code encoding:
 * Standard kernel macros from <asm-generic/ioctl.h> and <linux/usbdevice_fs.h>.
 *
 * Each 32-bit ioctl request is composed of 4 bitfields:
 *   bits 31..30 : Direction (0 = none, 1 = write, 2 = read, 3 = read/write)
 *   bits 29..16 : Size of data structure in bytes (sizeof(type))
 *   bits 15..8  : Type / subsystem magic character ('U' = 0x55 for USB usbfs)
 *   bits 7..0   : Command sequence number
 *
 * Example: USBDEVFS_CONTROL is _IOWR('U', 0, struct usbdevfs_ctrltransfer):
 *   Direction = 3 (0xC0000000)
 *   Size      = 16 bytes on 32-bit (0x00100000) -> 0xc0105500
 *             = 24 bytes on 64-bit (0x00180000) -> 0xc0185500
 *   Type      = 'U' (0x00005500)
 *   Nr        = 0   (0x00000000)
 *
 * Using sizeof(...) allows the compiler to calculate the exact ioctl code
 * automatically for both 32-bit and 64-bit builds.
 */
#define LINUX_IOC(dir, type, nr, size) \
	(((uint32_t)(dir)  << 30) | \
	 ((uint32_t)(size) << 16) | \
	 ((uint32_t)(type) << 8)  | \
	 ((uint32_t)(nr)   << 0))

#define LINUX_IO(type, nr)             LINUX_IOC(0, (type), (nr), 0)
#define LINUX_IOR(type, nr, size)      LINUX_IOC(2, (type), (nr), (size))
#define LINUX_IOW(type, nr, size)      LINUX_IOC(1, (type), (nr), (size))
#define LINUX_IOWR(type, nr, size)     LINUX_IOC(3, (type), (nr), (size))

#define USBDEVFS_CONTROL           LINUX_IOWR('U', 0, sizeof(struct usbdevfs_ctrltransfer))
#define USBDEVFS_BULK              LINUX_IOWR('U', 2, sizeof(struct usbdevfs_bulktransfer))
#define USBDEVFS_SUBMITURB         LINUX_IOR ('U', 10, sizeof(struct usbdevfs_urb))
#define USBDEVFS_DISCARDURB        LINUX_IO  ('U', 11)
#define USBDEVFS_REAPURB           LINUX_IOW ('U', 12, sizeof(void *))
#define USBDEVFS_REAPURBNDELAY     LINUX_IOW ('U', 13, sizeof(void *))
#define USBDEVFS_CLAIMINTERFACE    LINUX_IOR ('U', 15, sizeof(uint32_t))
#define USBDEVFS_RELEASEINTERFACE  LINUX_IOR ('U', 16, sizeof(uint32_t))
#define USBDEVFS_RESET             LINUX_IO  ('U', 20)

struct linux_dirent64 {
	uint64_t        d_ino;
	int64_t         d_off;
	unsigned short  d_reclen;
	unsigned char   d_type;
	char            d_name[1];
};

#if defined(__GNUC__) && defined(__i386__)
static intptr_t linux_syscall(intptr_t nr, intptr_t arg1, intptr_t arg2, intptr_t arg3)
{
	intptr_t res;
	__asm__ volatile (
		"int $0x80"
		: "=a" (res)
		: "a" (nr), "b" (arg1), "c" (arg2), "d" (arg3)
		: "memory"
	);
	return res;
}
#elif defined(_MSC_VER) && defined(_M_IX86)
static intptr_t linux_syscall(intptr_t nr, intptr_t arg1, intptr_t arg2, intptr_t arg3)
{
	intptr_t res;
	__asm {
		mov eax, nr
		mov ebx, arg1
		mov ecx, arg2
		mov edx, arg3
		int 0x80
		mov res, eax
	}
	return res;
}
#elif defined(__GNUC__) && defined(__x86_64__)
static intptr_t linux_syscall(intptr_t nr, intptr_t arg1, intptr_t arg2, intptr_t arg3)
{
	intptr_t res;
	uint8_t *teb = (uint8_t *)NtCurrentTeb();
	uint8_t prev = teb[0x340];
	teb[0x340] = 0; /* temporarily allow Linux syscall via Syscall User Dispatch */
	__asm__ volatile (
		"syscall"
		: "=a" (res)
		: "a" (nr), "D" (arg1), "S" (arg2), "d" (arg3)
		: "rcx", "r11", "memory"
	);
	teb[0x340] = prev; /* restore Wine NT dispatch */
	return res;
}
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_AMD64))
/* MSVC x64 does not support inline assembly; we execute a small 56-byte thunk */
static const uint8_t s_msvc64_thunk_code[] = {
	0x57,                                                 /* push rdi */
	0x56,                                                 /* push rsi */
	0x65, 0x48, 0x8b, 0x04, 0x25, 0x30, 0x00, 0x00, 0x00, /* mov rax, gs:[0x30] (TEB) */
	0x44, 0x8a, 0x90, 0x40, 0x03, 0x00, 0x00,             /* mov r10b, [rax+0x340] */
	0xc6, 0x80, 0x40, 0x03, 0x00, 0x00, 0x00,             /* mov byte ptr [rax+0x340], 0 */
	0x41, 0x52,                                           /* push r10 */
	0x50,                                                 /* push rax */
	0x48, 0x89, 0xc8,                                     /* mov rax, rcx (nr) */
	0x48, 0x89, 0xd7,                                     /* mov rdi, rdx (arg1) */
	0x4c, 0x89, 0xc6,                                     /* mov rsi, r8  (arg2) */
	0x4c, 0x89, 0xca,                                     /* mov rdx, r9  (arg3) */
	0x0f, 0x05,                                           /* syscall */
	0x41, 0x5b,                                           /* pop r11 (TEB) */
	0x41, 0x5a,                                           /* pop r10 (prev) */
	0x45, 0x88, 0x93, 0x40, 0x03, 0x00, 0x00,             /* mov [r11+0x340], r10b */
	0x5e,                                                 /* pop rsi */
	0x5f,                                                 /* pop rdi */
	0xc3                                                  /* ret */
};
typedef intptr_t (*linux_syscall_fn_t)(intptr_t nr, intptr_t arg1, intptr_t arg2, intptr_t arg3);
static linux_syscall_fn_t s_linux_syscall_msvc64 = NULL;

static intptr_t linux_syscall(intptr_t nr, intptr_t arg1, intptr_t arg2, intptr_t arg3)
{
	if (!s_linux_syscall_msvc64) {
		DWORD old_protect;
		void *exec_mem = VirtualAlloc(NULL, sizeof(s_msvc64_thunk_code),
		                              MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (!exec_mem)
			return -1;
		memcpy(exec_mem, s_msvc64_thunk_code, sizeof(s_msvc64_thunk_code));
		VirtualProtect(exec_mem, sizeof(s_msvc64_thunk_code), PAGE_EXECUTE_READ, &old_protect);
		FlushInstructionCache(GetCurrentProcess(), exec_mem, sizeof(s_msvc64_thunk_code));
		s_linux_syscall_msvc64 = (linux_syscall_fn_t)exec_mem;
	}
	return s_linux_syscall_msvc64(nr, arg1, arg2, arg3);
}
#else
static intptr_t linux_syscall(intptr_t nr, intptr_t arg1, intptr_t arg2, intptr_t arg3)
{
	(void)nr; (void)arg1; (void)arg2; (void)arg3;
	return -1;
}
#endif

static wine_usb_device_t s_devices[MAX_WINE_DEVICES];
static uint32_t s_device_count = 0;
static int s_scanned = 0;

typedef struct wine_async_stream {
	int in_use;
	int fd;
	struct usbdevfs_urb *urbs;
	uint32_t urb_count;
	volatile int active;
} wine_async_stream_t;

static wine_async_stream_t s_streams[MAX_WINE_DEVICES];

/* check if running in wine with access to host usbfs */
int wine_usbfs_detected(void)
{
	static int detected = -1;
	HMODULE ntdll;

	if (detected >= 0)
		return detected;

	detected = 0;
	ntdll = GetModuleHandleA("ntdll.dll");
	if (ntdll && (GetProcAddress(ntdll, "wine_get_version") ||
	              GetProcAddress(ntdll, "wine_server_call"))) {
		/* check direct linux syscall access to /dev/bus/usb */
		int fd = linux_syscall(LINUX_NR_OPEN, (intptr_t)"/dev/bus/usb", 0 /* O_RDONLY */, 0);
		if (fd >= 0) {
			linux_syscall(LINUX_NR_CLOSE, fd, 0, 0);
			detected = 1;
		} else {
			/* fallback check via drive Z: */
			WIN32_FIND_DATAA fd_win;
			HANDLE h = FindFirstFileA("Z:\\dev\\bus\\usb\\*", &fd_win);
			if (h != INVALID_HANDLE_VALUE) {
				FindClose(h);
				detected = 1;
			}
		}
	}
	return detected;
}

/* read ascii string descriptor from device */
static int wine_get_string_ascii(int fd, uint8_t index, char *dest, int max_len)
{
	unsigned char buf[256];
	struct usbdevfs_ctrltransfer ctrl;
	int res;
	int i;
	int len;

	if (!index || !dest || max_len <= 0) {
		if (dest && max_len > 0)
			dest[0] = '\0';
		return -1;
	}

	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.bRequestType = 0x80;
	ctrl.bRequest = 0x06; /* GET_DESCRIPTOR */
	ctrl.wValue = (uint16_t)((0x03 << 8) | index);
	ctrl.wIndex = 0x0409; /* English (US) */
	/* realtek firmware truncates wLength > 255 to 0 */
	ctrl.wLength = 255;
	ctrl.timeout = 1000;
	ctrl.data = buf;

	res = linux_syscall(LINUX_NR_IOCTL, fd, (intptr_t)USBDEVFS_CONTROL, (intptr_t)&ctrl);
	if (res < 2 || buf[1] != 0x03) {
		dest[0] = '\0';
		return -1;
	}

	len = (buf[0] - 2) / 2;
	if (len >= max_len)
		len = max_len - 1;

	for (i = 0; i < len; i++) {
		dest[i] = (char)buf[2 + i * 2];
	}
	dest[len] = '\0';
	return len;
}

static void wine_usbfs_probe_file(const char *posix_path)
{
	struct usb_device_descriptor_raw {
		uint8_t  bLength;
		uint8_t  bDescriptorType;
		uint16_t bcdUSB;
		uint8_t  bDeviceClass;
		uint8_t  bDeviceSubClass;
		uint8_t  bDeviceProtocol;
		uint8_t  bMaxPacketSize0;
		uint16_t idVendor;
		uint16_t idProduct;
		uint16_t bcdDevice;
		uint8_t  iManufacturer;
		uint8_t  iProduct;
		uint8_t  iSerialNumber;
		uint8_t  bNumConfigurations;
	} desc;
	struct usbdevfs_ctrltransfer ctrl;
	rtlsdr_dongle_t *info;
	wine_usb_device_t *d;
	int fd;
	int r;

	if (s_device_count >= MAX_WINE_DEVICES)
		return;

	fd = linux_syscall(LINUX_NR_OPEN, (intptr_t)posix_path, 2 /* O_RDWR */, 0);
	if (fd < 0)
		return;

	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.bRequestType = 0x80;
	ctrl.bRequest = 0x06;
	ctrl.wValue = 0x0100;
	ctrl.wLength = 18;
	ctrl.timeout = 1000;
	ctrl.data = &desc;

	r = linux_syscall(LINUX_NR_IOCTL, fd, (intptr_t)USBDEVFS_CONTROL, (intptr_t)&ctrl);
	if (r == 18) {
		info = find_known_device(desc.idVendor, desc.idProduct);
		if (info && s_device_count < MAX_WINE_DEVICES) {
			d = &s_devices[s_device_count];
			strncpy(d->posix_path, posix_path, sizeof(d->posix_path) - 1);
			d->vid = desc.idVendor;
			d->pid = desc.idProduct;
			if (info->name)
				strncpy(d->product, info->name, sizeof(d->product) - 1);

			/* read device string descriptors */
			wine_get_string_ascii(fd, desc.iManufacturer, d->manufact, sizeof(d->manufact));
			wine_get_string_ascii(fd, desc.iProduct, d->product, sizeof(d->product));
			wine_get_string_ascii(fd, desc.iSerialNumber, d->serial, sizeof(d->serial));

			if (d->product[0] == '\0' && info->name)
				strncpy(d->product, info->name, sizeof(d->product) - 1);

			s_device_count++;
		}
	}
	linux_syscall(LINUX_NR_CLOSE, fd, 0, 0);
}

/* scan for supported dongles under /dev/bus/usb */
int wine_usbfs_scan_devices(void)
{
	int bus_fd;
	char bus_buf[1024];
	int bus_n;

	s_device_count = 0;
	memset(s_devices, 0, sizeof(s_devices));

	/* primary scan method: direct linux syscalls (bypasses wine drive mappings) */
	bus_fd = linux_syscall(LINUX_NR_OPEN, (intptr_t)"/dev/bus/usb", 0 /* O_RDONLY */, 0);
	if (bus_fd >= 0) {
		while ((bus_n = linux_syscall(LINUX_NR_GETDENTS64, bus_fd, (intptr_t)bus_buf, sizeof(bus_buf))) > 0) {
			int bus_pos = 0;
			while (bus_pos < bus_n) {
				struct linux_dirent64 *d_bus = (struct linux_dirent64 *)(bus_buf + bus_pos);
				if (d_bus->d_name[0] != '.') {
					char bus_dir[64];
					int dev_dir_fd;
					snprintf(bus_dir, sizeof(bus_dir), "/dev/bus/usb/%s", d_bus->d_name);
					dev_dir_fd = linux_syscall(LINUX_NR_OPEN, (intptr_t)bus_dir, 0 /* O_RDONLY */, 0);
					if (dev_dir_fd >= 0) {
						char dev_buf[1024];
						int dev_n;
						while ((dev_n = linux_syscall(LINUX_NR_GETDENTS64, dev_dir_fd, (intptr_t)dev_buf, sizeof(dev_buf))) > 0) {
							int dev_pos = 0;
							while (dev_pos < dev_n) {
								struct linux_dirent64 *d_dev = (struct linux_dirent64 *)(dev_buf + dev_pos);
								if (d_dev->d_name[0] != '.') {
									char dev_path[64];
									snprintf(dev_path, sizeof(dev_path), "%s/%s", bus_dir, d_dev->d_name);
									wine_usbfs_probe_file(dev_path);
								}
								dev_pos += d_dev->d_reclen;
							}
						}
						linux_syscall(LINUX_NR_CLOSE, dev_dir_fd, 0, 0);
					}
				}
				bus_pos += d_bus->d_reclen;
			}
		}
		linux_syscall(LINUX_NR_CLOSE, bus_fd, 0, 0);
		s_scanned = 1;
		return s_device_count;
	}

	/* fallback scan method: via Wine drive Z: */
	{
		WIN32_FIND_DATAA fd_bus;
		HANDLE h_bus = FindFirstFileA("Z:\\dev\\bus\\usb\\*", &fd_bus);
		if (h_bus != INVALID_HANDLE_VALUE) {
			do {
				char dev_search[MAX_PATH];
				WIN32_FIND_DATAA fd_dev;
				HANDLE h_dev;

				if (fd_bus.cFileName[0] == '.')
					continue;
				if (!(fd_bus.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
					continue;

				snprintf(dev_search, sizeof(dev_search), "Z:\\dev\\bus\\usb\\%s\\*", fd_bus.cFileName);
				h_dev = FindFirstFileA(dev_search, &fd_dev);
				if (h_dev == INVALID_HANDLE_VALUE)
					continue;

				do {
					char posix_path[64];
					if (fd_dev.cFileName[0] == '.')
						continue;
					if (fd_dev.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
						continue;

					snprintf(posix_path, sizeof(posix_path), "/dev/bus/usb/%s/%s",
					         fd_bus.cFileName, fd_dev.cFileName);
					wine_usbfs_probe_file(posix_path);
				} while (FindNextFileA(h_dev, &fd_dev));
				FindClose(h_dev);
			} while (FindNextFileA(h_bus, &fd_bus));
			FindClose(h_bus);
		}
	}

	s_scanned = 1;
	return s_device_count;
}

uint32_t wine_usbfs_get_device_count(void)
{
	wine_usbfs_scan_devices();
	return s_device_count;
}

const wine_usb_device_t *wine_usbfs_get_device(uint32_t index)
{
	if (!s_scanned)
		wine_usbfs_scan_devices();

	if (index >= s_device_count)
		return NULL;

	return &s_devices[index];
}

const char *wine_usbfs_get_device_name(uint32_t index)
{
	const wine_usb_device_t *dev = wine_usbfs_get_device(index);
	if (!dev)
		return "";
	return dev->product;
}

int wine_usbfs_get_device_usb_strings(uint32_t index, char *manufact, char *product, char *serial)
{
	const wine_usb_device_t *dev = wine_usbfs_get_device(index);
	if (!dev)
		return -1;

	if (manufact) {
		strncpy(manufact, dev->manufact, 255);
		manufact[255] = '\0';
	}
	if (product) {
		strncpy(product, dev->product, 255);
		product[255] = '\0';
	}
	if (serial) {
		strncpy(serial, dev->serial, 255);
		serial[255] = '\0';
	}
	return 0;
}

int wine_usbfs_get_index_by_serial(const char *serial)
{
	uint32_t i;
	if (!serial)
		return -1;

	if (!s_scanned)
		wine_usbfs_scan_devices();

	for (i = 0; i < s_device_count; i++) {
		if (strcmp(s_devices[i].serial, serial) == 0)
			return (int)i;
	}
	return -1;
}

int wine_usbfs_open(uint32_t index)
{
	uint32_t iface = 0;
	int fd;
	int r;

	if (!s_scanned)
		wine_usbfs_scan_devices();

	if (index >= s_device_count)
		return -1;

	fd = linux_syscall(LINUX_NR_OPEN, (intptr_t)s_devices[index].posix_path, 2 /* O_RDWR */, 0);
	if (fd < 0) {
		fprintf(stderr, "wine_usbfs: failed to open %s (errno %d)\n",
		        s_devices[index].posix_path, -fd);
		return fd;
	}

	r = linux_syscall(LINUX_NR_IOCTL, fd, (intptr_t)USBDEVFS_CLAIMINTERFACE, (intptr_t)&iface);
	if (r < 0) {
		fprintf(stderr, "wine_usbfs: failed to claim interface 0 on %s (error %d)\n",
		        s_devices[index].posix_path, r);
		linux_syscall(LINUX_NR_CLOSE, fd, 0, 0);
		return r;
	}

	return fd;
}

void wine_usbfs_close(int fd)
{
	uint32_t iface = 0;
	int i;

	if (fd < 0)
		return;

	for (i = 0; i < MAX_WINE_DEVICES; i++) {
		if (s_streams[i].in_use && s_streams[i].fd == fd) {
			s_streams[i].active = 0;
			s_streams[i].urbs = NULL;
			s_streams[i].urb_count = 0;
			s_streams[i].in_use = 0;
			s_streams[i].fd = -1;
			break;
		}
	}

	linux_syscall(LINUX_NR_IOCTL, fd, (intptr_t)USBDEVFS_RELEASEINTERFACE, (intptr_t)&iface);
	linux_syscall(LINUX_NR_CLOSE, fd, 0, 0);
}

int wine_usbfs_reset_device(int fd)
{
	if (fd < 0)
		return -1;
	return linux_syscall(LINUX_NR_IOCTL, fd, (intptr_t)USBDEVFS_RESET, 0);
}

int wine_usbfs_control_transfer(int fd, uint8_t bmRequestType, uint8_t bRequest,
                               uint16_t wValue, uint16_t wIndex,
                               unsigned char *data, uint16_t wLength, unsigned int timeout)
{
	struct usbdevfs_ctrltransfer ctrl;
	int res;

	if (fd < 0)
		return -1;

	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.bRequestType = bmRequestType;
	ctrl.bRequest = bRequest;
	ctrl.wValue = wValue;
	ctrl.wIndex = wIndex;
	ctrl.wLength = wLength;
	ctrl.timeout = timeout ? timeout : 1000;
	ctrl.data = data;

	res = linux_syscall(LINUX_NR_IOCTL, fd, (intptr_t)USBDEVFS_CONTROL, (intptr_t)&ctrl);
	return res;
}

int wine_usbfs_bulk_transfer(int fd, unsigned char endpoint,
                             unsigned char *data, int length,
                             int *transferred, unsigned int timeout)
{
	struct usbdevfs_bulktransfer bulk;
	int res;

	if (fd < 0)
		return -1;

	memset(&bulk, 0, sizeof(bulk));
	bulk.ep = endpoint;
	bulk.len = (uint32_t)length;
	bulk.timeout = timeout ? timeout : 1000;
	bulk.data = data;

	res = linux_syscall(LINUX_NR_IOCTL, fd, (intptr_t)USBDEVFS_BULK, (intptr_t)&bulk);
	if (res < 0) {
		if (transferred)
			*transferred = 0;
		return res;
	}

	if (transferred)
		*transferred = res;

	return 0;
}

int wine_usbfs_cancel_async(int fd)
{
	uint32_t i;
	if (fd < 0)
		return 0;

	for (i = 0; i < MAX_WINE_DEVICES; i++) {
		if (s_streams[i].in_use && s_streams[i].fd == fd) {
			if (s_streams[i].active && s_streams[i].urbs) {
				uint32_t j;
				for (j = 0; j < s_streams[i].urb_count; j++) {
					linux_syscall(LINUX_NR_IOCTL, fd, (intptr_t)USBDEVFS_DISCARDURB, (intptr_t)&s_streams[i].urbs[j]);
				}
			}
			return 0;
		}
	}
	return 0;
}

int wine_usbfs_read_async(int fd, rtlsdr_read_async_cb_t cb, void *ctx,
                          uint32_t buf_num, uint32_t buf_len,
                          int *async_status, int *async_cancel)
{
	unsigned char **buffers;
	struct usbdevfs_urb *urbs;
	wine_async_stream_t *stream = NULL;
	uint32_t i;
	int r = 0;
	int in_flight = 0;
	uint32_t submitted_count = 0;

	if (fd < 0 || !async_status || !async_cancel)
		return -1;

	if (*async_status != 0 /* RTLSDR_INACTIVE */)
		return -2;

	for (i = 0; i < MAX_WINE_DEVICES; i++) {
		if (s_streams[i].in_use && s_streams[i].fd == fd)
			return -2; /* already streaming on this device */
	}
	for (i = 0; i < MAX_WINE_DEVICES; i++) {
		if (!s_streams[i].in_use) {
			stream = &s_streams[i];
			stream->in_use = 1;
			stream->fd = fd;
			break;
		}
	}
	if (!stream)
		return -1;

	if (buf_num == 0)
		buf_num = 15;
	if (buf_len == 0 || buf_len % 512 != 0)
		buf_len = 262144;

	buffers = (unsigned char **)malloc(buf_num * sizeof(unsigned char *));
	urbs = (struct usbdevfs_urb *)malloc(buf_num * sizeof(struct usbdevfs_urb));
	if (!buffers || !urbs) {
		if (buffers) free(buffers);
		if (urbs) free(urbs);
		stream->in_use = 0;
		stream->fd = -1;
		return -1;
	}

	for (i = 0; i < buf_num; i++) {
		buffers[i] = (unsigned char *)malloc(buf_len);
		if (!buffers[i]) {
			uint32_t j;
			for (j = 0; j < i; j++) free(buffers[j]);
			free(buffers);
			free(urbs);
			stream->in_use = 0;
			stream->fd = -1;
			return -1;
		}
	}

	*async_status = 2; /* RTLSDR_RUNNING */
	*async_cancel = 0;

	stream->urbs = urbs;
	stream->urb_count = buf_num;
	stream->active = 1;

	for (i = 0; i < buf_num; i++) {
		memset(&urbs[i], 0, sizeof(struct usbdevfs_urb));
		urbs[i].type = 3; /* USBDEVFS_URB_TYPE_BULK */
		urbs[i].endpoint = 0x81;
		urbs[i].buffer = buffers[i];
		urbs[i].buffer_length = buf_len;
		urbs[i].usercontext = &urbs[i];

		r = linux_syscall(LINUX_NR_IOCTL, fd, (intptr_t)USBDEVFS_SUBMITURB, (intptr_t)&urbs[i]);
		if (r < 0) {
			fprintf(stderr, "wine_usbfs: failed to submit URB %u (error %d)\n", i, r);
			break;
		}
		in_flight++;
		submitted_count++;
	}

	if (submitted_count == 0) {
		*async_status = 0;
		if (stream && stream->fd == fd) {
			stream->active = 0;
			stream->urbs = NULL;
			stream->urb_count = 0;
			stream->in_use = 0;
			stream->fd = -1;
		}
		for (i = 0; i < buf_num; i++) free(buffers[i]);
		free(buffers);
		free(urbs);
		return -1;
	}

	while (*async_status == 2 /* RTLSDR_RUNNING */ && !*async_cancel) {
		struct usbdevfs_urb *reaped = NULL;
		r = linux_syscall(LINUX_NR_IOCTL, fd, (intptr_t)USBDEVFS_REAPURB, (intptr_t)&reaped);
		if (r < 0) {
			if (*async_status != 2 || *async_cancel)
				break;
			continue;
		}

		if (reaped) {
			in_flight--;

			if (reaped->status == 0 && reaped->actual_length > 0) {
				if (cb)
					cb((unsigned char *)reaped->buffer, reaped->actual_length, ctx);
			} else if (reaped->status < 0) {
				break;
			}

			if (*async_status == 2 && !*async_cancel && reaped->status == 0) {
				reaped->status = 0;
				reaped->actual_length = 0;
				r = linux_syscall(LINUX_NR_IOCTL, fd, (intptr_t)USBDEVFS_SUBMITURB, (intptr_t)reaped);
				if (r < 0) {
					fprintf(stderr, "wine_usbfs: failed to resubmit URB (error %d)\n", r);
					break;
				}
				in_flight++;
			}
		}
	}

	/* cancel in-flight transfers */
	*async_status = 1; /* RTLSDR_CANCELING */
	wine_usbfs_cancel_async(fd);

	/*
	 * Safely wait for all in-flight USB Request Block to be completed and reaped by the kernel.
	 * Because USBDEVFS_DISCARDURB unlinks transfers asynchronously, we must not
	 * free buffer memory while the USB host controller is still releasing it.
	 */
	while (in_flight > 0) {
		struct usbdevfs_urb *reaped = NULL;
		r = linux_syscall(LINUX_NR_IOCTL, fd, (intptr_t)USBDEVFS_REAPURB, (intptr_t)&reaped);
		if (r < 0) {
			if (r == -4 /* -EINTR */)
				continue;
			/* If REAPURB fails (e.g. device was unplugged), do non-blocking check */
			if (linux_syscall(LINUX_NR_IOCTL, fd, (intptr_t)USBDEVFS_REAPURBNDELAY, (intptr_t)&reaped) == 0 && reaped) {
				in_flight--;
				continue;
			}
			break;
		}
		if (reaped)
			in_flight--;
	}

	if (stream && stream->fd == fd) {
		stream->active = 0;
		stream->urbs = NULL;
		stream->urb_count = 0;
		stream->in_use = 0;
		stream->fd = -1;
	}

	for (i = 0; i < buf_num; i++) {
		free(buffers[i]);
	}
	free(buffers);
	free(urbs);

	*async_status = 0; /* RTLSDR_INACTIVE */
	return 0;
}

#endif /* _WIN32 */
