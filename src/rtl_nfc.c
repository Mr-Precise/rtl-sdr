/*
 * rtl-sdr, turns your Realtek RTL2832 based DVB dongle into a SDR receiver
 * rtl_nfc, simple NFC receiving tool
 * Thanks for Iskuri
 * Rewritten code from https://github.com/Iskuri/RTLSDR-NFC
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
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <windows.h>
#include "getopt/getopt.h"
#endif
#include <stdint.h>
#include <math.h>
#include "rtl-sdr.h"
#include "convenience/convenience.h"

#define RECEIVE_BUFF_SIZE (16384 * 8)
#define MOVING_AVERAGE_MAX 512
#define READER_BUFFER_MAX_SIZE 512

static int do_exit = 0;
static rtlsdr_dev_t *dev = NULL;

static uint32_t movingAverageValue = 0;
static uint16_t movingAverageCount = 0;
static uint16_t currAverage = 0;
static uint16_t movingAverageValues[MOVING_AVERAGE_MAX];
static uint16_t movingAverageBuffInc = 0;
static uint32_t packetInc = 0;
static uint16_t squares[256];
static uint16_t demodAM[RECEIVE_BUFF_SIZE / 2];

static void calcCurrMovingAverage(uint16_t val);
static void parseAmData(uint16_t* amData, uint32_t len);
static int abs8(int x);
static void computeSquares(void);
static void buffCallback(unsigned char* buf, uint32_t len, void* ctx);

void usage(void)
{
	fprintf(stderr,
		"rtl_nfc, a simple NFC decoder\n\n"
		"\t[-d device_index (default: 0)]\n"
		"\t[-v show version]\n"
		"\n");
	exit(1);
}

#ifdef _WIN32
static BOOL WINAPI
sighandler(int signum)
{
	if (CTRL_C_EVENT == signum) {
		fprintf(stderr, "Signal caught, exiting!\n");
		do_exit = 1;
		rtlsdr_cancel_async(dev);
		return TRUE;
	}
	return FALSE;
}
#else
static void sighandler(int signum)
{
	signal(SIGPIPE, SIG_IGN);
	fprintf(stderr, "Signal caught, exiting!\n");
	do_exit = 1;
	rtlsdr_cancel_async(dev);
}
#endif

static void calcCurrMovingAverage(uint16_t val)
{
	if (movingAverageCount >= MOVING_AVERAGE_MAX) {
		movingAverageValue -= movingAverageValues[movingAverageBuffInc];
	} else {
		movingAverageCount++;
	}
	movingAverageValues[movingAverageBuffInc] = val;
	movingAverageValue += val;

	movingAverageBuffInc++;
	if (movingAverageBuffInc >= MOVING_AVERAGE_MAX) {
		movingAverageBuffInc = 0;
	}

	currAverage = (uint16_t)(movingAverageValue / movingAverageCount);
}

static void parseAmData(uint16_t* amData, uint32_t len)
{
	uint16_t currAmData = 0;
	uint8_t readerThreshold;
	uint32_t i = 0;

	while (i < len) {
		currAmData = amData[i];
		calcCurrMovingAverage(currAmData);
		readerThreshold = (currAmData > (currAverage * 0.25));

		if (readerThreshold == 0 && (i + 18) <= len) {
			uint8_t bits[READER_BUFFER_MAX_SIZE * 8];
			uint8_t packets[READER_BUFFER_MAX_SIZE];
			uint8_t parityInc = 0;
			uint8_t bit = 0;
			uint16_t size = 0;
			uint8_t lastReaderMask = 0xff;
			uint32_t bitCount = 0;
			uint8_t done = 0;
			uint32_t j;

			i += 2;
			memset(packets, 0, sizeof(packets));

			while (done == 0) {
				uint8_t readerMask = 0x00;
				int sub;

				if ((i + 16) > len || bitCount >= sizeof(bits)) {
					done = 1;
					break;
				}

				for (sub = 0; sub < 4; sub++) {
					readerThreshold = (amData[i] > (currAverage * 0.25));
					readerMask <<= 1;
					readerMask |= readerThreshold;
					i += 4;
				}

				/* process modified miller */
				switch (readerMask) {
				case 0x07:
					bits[bitCount] = 0;
					break;
				case 0x0f:
					if (lastReaderMask == 0x0d) {
						bits[bitCount] = 0;
					} else {
						done = 1;
					}
					break;
				case 0x0d:
					bits[bitCount] = 1;
					break;
				default:
					done = 1;
					break;
				}

				bitCount++;
				lastReaderMask = readerMask;
			}

			/* process bits into packet */
			for (j = 1; j < bitCount; j++) {
				if (bits[j] == 1) {
					packets[size] |= (1 << bit);
				}

				parityInc++;
				bit++;

				if (parityInc >= 8) {
					j++; /* skip parity bit */
					size++;
					parityInc = 0;
					bit = 0;
					if (size >= READER_BUFFER_MAX_SIZE) {
						break;
					}
				}
			}

			/* Include any remaining partial byte (e.g. 7-bit REQA/WUPA short frames) */
			if (bit > 0 && size < READER_BUFFER_MAX_SIZE) {
				size++;
			}

			if (bitCount >= 7 && size > 0) {
				uint16_t k;
				printf("%08x %d RD: ", packetInc, currAverage);
				for (k = 0; k < size; k++) {
					printf("%02x ", packets[k]);
				}
				printf("\n");
				packetInc++;
			}
		}

		i++;
	}
}

static int abs8(int x)
{
	if (x >= 127) {
		return x - 127;
	}
	return 127 - x;
}

static void computeSquares(void)
{
	int i, j;
	for (i = 0; i < 256; i++) {
		j = abs8(i);
		squares[i] = (uint16_t)(j * j);
	}
}

static void buffCallback(unsigned char* buf, uint32_t len, void* ctx)
{
	uint32_t am_len = len / 2;
	uint32_t i;
	(void)ctx;

	if (am_len > RECEIVE_BUFF_SIZE / 2) {
		am_len = RECEIVE_BUFF_SIZE / 2;
	}

	for (i = 0; i < am_len; i++) {
		demodAM[i] = squares[buf[i * 2]] + squares[buf[i * 2 + 1]];
	}

	parseAmData(demodAM, am_len);
}

int main(int argc, char** argv)
{
#ifndef _WIN32
	struct sigaction sigact;
#endif
	int dev_index = 0;
	int dev_given = 0;
	int r, opt;
	int gainCount;
	int allGains[100];

	memset(movingAverageValues, 0x00, sizeof(movingAverageValues));

	while ((opt = getopt(argc, argv, "d:hv?")) != -1) {
		switch (opt) {
		case 'd':
			dev_index = verbose_device_search(optarg);
			dev_given = 1;
			break;
		case 'v':
			printf("librtlsdr version: %s\n", librtlsdr_get_version());
			return 0;
		case 'h':
		default:
			usage();
			break;
		}
	}

	if (!dev_given) {
		dev_index = verbose_device_search("0");
	}

	if (dev_index < 0) {
		exit(1);
	}

	r = rtlsdr_open(&dev, dev_index);
	if (r < 0) {
		fprintf(stderr, "Failed to open rtlsdr device #%d.\n", dev_index);
		exit(1);
	}
#ifndef _WIN32
	sigact.sa_handler = sighandler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
	sigaction(SIGQUIT, &sigact, NULL);
	sigaction(SIGPIPE, &sigact, NULL);
#else
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)sighandler, TRUE);
#endif
	computeSquares();

	gainCount = rtlsdr_get_tuner_gains(dev, allGains);
	if (gainCount > 0) {
		rtlsdr_set_tuner_gain_mode(dev, 1);
		rtlsdr_set_tuner_gain(dev, allGains[gainCount - 1]);
	} else {
		rtlsdr_set_tuner_gain_mode(dev, 0);
	}

	rtlsdr_set_center_freq(dev, 27120000);
	rtlsdr_set_sample_rate(dev, 1695000);

	rtlsdr_reset_buffer(dev);

	rtlsdr_read_async(dev, buffCallback, NULL,
		12,
		RECEIVE_BUFF_SIZE
	);

	rtlsdr_close(dev);

	return r >= 0 ? r : -r;
}
