/*
 * rtl-sdr, turns your Realtek RTL2832 based DVB dongle into a SDR receiver
 * rtl_nfc, simple NFC receiving tool
 * Thanks for Iskuri
 * Rewrited code from https://github.com/Iskuri/RTLSDR-NFC
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

#define BUFF_SIZE 16*1024*1024
#define RECEIVE_BUFF_SIZE 16384*8
#define MOVING_AVERAGE_MAX 512
#define READER_BUFFER_MAX_SIZE 512

void usage(void)
{
	fprintf(stderr,
		"rtl_nfc, a simple NFC decoder\n\n"
		"\t[-d device_index (default: 0)]\n"
		"\t[-v Version]\n"
		"\n");
	exit(1);
}

static int do_exit = 0;
static rtlsdr_dev_t *dev = NULL;

unsigned char* buffData;
uint32_t buffInc = 0;
uint32_t movingAverageValue = 0;
uint16_t movingAverageCount = 0;
uint16_t currAverage = 0;
uint16_t movingAverageValues[MOVING_AVERAGE_MAX];
uint16_t movingAverageBuffInc = 0;
uint32_t packetInc = 0;
uint16_t squares[256];

void calcCurrMovingAverage(uint16_t val);

void parseAmData(uint16_t* amData, uint32_t len);

int abs8(int x);

void computeSquares();

void buffCallback(unsigned char* buf, uint32_t len, void* ctx);

#ifdef _WIN32
BOOL WINAPI
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

void calcCurrMovingAverage(uint16_t val) {
	if (movingAverageCount > MOVING_AVERAGE_MAX) {
		movingAverageValue -= movingAverageValues[movingAverageBuffInc];
	} else {
		movingAverageCount++;
	}
	movingAverageValues[movingAverageBuffInc] = val;

	movingAverageValue += movingAverageValues[movingAverageBuffInc];

	movingAverageBuffInc++;

	if (movingAverageBuffInc >= MOVING_AVERAGE_MAX) {
		movingAverageBuffInc = 0;
	}

	currAverage = movingAverageValue / movingAverageCount;
}

void parseAmData(uint16_t* amData, uint32_t len) {
	uint16_t currAmData = 0;
	uint8_t readerThreshold;
	uint8_t tagThreshold;
	uint16_t buffAvg = 0;
	uint32_t i = 0;
	while (i < (len)) {
		currAmData = amData[i];
		calcCurrMovingAverage(currAmData);
		readerThreshold = (currAmData > (currAverage * 0.25));
		tagThreshold = i > 0 && (currAmData > (amData[i - 1] * 1.05));

		// handle state
		if (tagThreshold == 1) {
			// printf("FOUND THRESH\n");
		} else if (readerThreshold == 0) {
			// printf("Got down threshold, trying SOF\n");
			// escape possible wrong output - make more accurate later
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
			memset(packets, 0, READER_BUFFER_MAX_SIZE);

			while (done == 0) {
				uint8_t readerMask = 0x00;

				// i += 4;
				int j;
				for (j = 0; j < 4; j++) {
					readerThreshold = (amData[i] > (currAverage * 0.25));
					readerMask <<= 1;
					readerMask |= readerThreshold;
					i += 4;
				}

				// process modified miller
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

				// printf("Reader output: %02x\n",readerMask);
			}

			// process bits into packet
			for (j = 1; j < bitCount; j++) {
				if (bits[j] == 1) {
					packets[size] |= (1 << bit);
				}

				// printf("Bit: %d %01x - %02x\n",j,bits[j],packets[0]);

				parityInc++;
				bit++;

				if (parityInc >= 8) {
					j++;
					size++;
					parityInc = 0;
					bit = 0;
				}
			}

			if (bitCount >= 7) {
				int k;
				printf("%08x %d RD: ", packetInc, currAverage);
				for (k = 0; k < size; k++) {
					printf("%02x ", packets[k]);
				}
				printf("\n");
				/*
				printf("%08x RD(B): ",packetInc);
				for(int k = 0 ; k < bitCount ; k++) {
					printf("%01x",bits[k]);
				}
				printf("\n");
				*/
				packetInc++;
			}
		}

		i++;
	}

}

int abs8(int x) {
	if (x >= 127) {
		return x - 127;
	}
	return 127 - x;
}

void computeSquares() {
	int i, j;
	for (i = 0; i < 256; i++) {
		j = abs8(i);
		squares[i] = (uint16_t)(j * j);
	}
}

void buffCallback(unsigned char* buf, uint32_t len, void* ctx) {
	uint16_t demodAM[RECEIVE_BUFF_SIZE];
	uint32_t i;
	for (i = 0; i < len; i += 2) {
		demodAM[i / 2] = squares[buf[i]] + squares[buf[i + 1]];
	}

	parseAmData(demodAM, len / 2);
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
	uint16_t* amData;
	char version_string[20];

	memset(movingAverageValues, 0x00, MOVING_AVERAGE_MAX * sizeof(uint16_t));

	while ((opt = getopt(argc, argv, "d:v?")) != -1) {
		switch (opt) {
		case 'd':
			dev_index = verbose_device_search(optarg);
			dev_given = 1;
			break;
		case 'v':
			get_rtlsdr_version(version_string, sizeof(version_string));
        	printf("Version: %s\n", version_string);
			return 0;
			break;
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
	SetConsoleCtrlHandler( (PHANDLER_ROUTINE) sighandler, TRUE );
#endif
	computeSquares();

	buffData = malloc(BUFF_SIZE);

	gainCount = rtlsdr_get_tuner_gains(dev, allGains);
	rtlsdr_set_tuner_gain(dev, allGains[gainCount - 1]);

	rtlsdr_set_center_freq(dev, 27.12e6);

	rtlsdr_set_sample_rate(dev, 1.695e6);

	rtlsdr_reset_buffer(dev);

	rtlsdr_read_async(dev, buffCallback, NULL,
		12,
		RECEIVE_BUFF_SIZE
	);

	// while (1);
exit:
	rtlsdr_close(dev);
	free(buffData);

	return r >= 0 ? r : -r;
}
