/*
 * rtl-sdr, turns your Realtek RTL2832 based DVB dongle into a SDR receiver
 * rtl_test, test and benchmark tool
 *
 * Copyright (C) 2012-2014 by Steve Markgraf <steve@steve-m.de>
 * Copyright (C) 2012-2014 by Kyle Keen <keenerd@gmail.com>
 * Copyright (C) 2014 by Michael Tatarinov <kukabu@gmail.com>
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

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifdef __APPLE__
#include <sys/time.h>
#else
#include <time.h>
#endif

#ifndef _WIN32
#include <unistd.h>
#else
#include <windows.h>
#include "getopt/getopt.h"
#endif

#include "rtl-sdr.h"
#include "convenience/convenience.h"

#ifdef _WIN32
#if defined(_MSC_VER) && (_MSC_VER < 1800)
#define round(x) (x > 0.0 ? floor(x + 0.5): ceil(x - 0.5))
#endif
#endif

#define DEFAULT_SAMPLE_RATE		2048000
#define DEFAULT_BUF_LENGTH		(16 * 16384)
#define MINIMAL_BUF_LENGTH		512
#define MAXIMAL_BUF_LENGTH		(256 * 16384)

#define MHZ(x)	((x)*1000*1000)

#define PPM_DURATION			10
#define PPM_DUMP_TIME			5

struct time_generic
/* holds all the platform specific values */
{
#ifndef _WIN32
	time_t tv_sec;
	long tv_nsec;
#else
	long tv_sec;
	long tv_nsec;
	int init;
	LARGE_INTEGER frequency;
	LARGE_INTEGER ticks;
#endif
};

static enum {
	NO_BENCHMARK,
	TUNER_BENCHMARK,
	PPM_BENCHMARK
} test_mode = NO_BENCHMARK;

static int do_exit = 0;
static rtlsdr_dev_t *dev = NULL;

static uint32_t samp_rate = DEFAULT_SAMPLE_RATE;

static uint32_t total_samples = 0;
static uint32_t dropped_samples = 0;

static unsigned int ppm_duration = PPM_DURATION;

void usage(void)
{
	fprintf(stderr,
		"rtl_test, a benchmark tool for RTL2832 based DVB-T receivers\n\n"
		"Usage:\n"
		"\t[-s samplerate (default: 2048000 Hz)]\n"
		"\t[-d device_index (default: 0)]\n"
		"\t[-t enable E4000 or R820T tuner range benchmark]\n"
#ifndef _WIN32
		"\t[-p[seconds] enable PPM error measurement (default: 10 seconds)]\n"
#endif
		"\t[-b output_block_size (default: 16 * 16384)]\n"
		"\t[-S force sync output (default: async)]\n");
	exit(1);
}

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

static void underrun_test(unsigned char *buf, uint32_t len, int mute)
{
	uint32_t i, lost = 0;
	static uint8_t bcnt, uninit = 1;

	if (uninit) {
		bcnt = buf[0];
		uninit = 0;
	}
	for (i = 0; i < len; i++) {
		if(bcnt != buf[i]) {
			lost += (buf[i] > bcnt) ? (buf[i] - bcnt) : (bcnt - buf[i]);
			bcnt = buf[i];
		}

		bcnt++;
	}

	total_samples += len;
	dropped_samples += lost;
	if (mute)
		return;
	if (lost)
		printf("lost at least %d bytes\n", lost);

}

#ifndef _WIN32
static int ppm_gettime(struct time_generic *tg)
{
	int rv = ENOSYS;
	struct timespec ts;

#ifdef __unix__
	rv = clock_gettime(CLOCK_MONOTONIC, &ts);
	tg->tv_sec = ts.tv_sec;
	tg->tv_nsec = ts.tv_nsec;
#elif __APPLE__
	struct timeval tv;

	rv = gettimeofday(&tv, NULL);
	tg->tv_sec = tv.tv_sec;
	tg->tv_nsec = tv.tv_usec * 1000;
#endif
	return rv;
}
#endif
 #ifdef _WIN32
static int ppm_gettime(struct time_generic *tg)
{
	int rv;
	int64_t frac;
	if (!tg->init) {
		QueryPerformanceFrequency(&tg->frequency);
		tg->init = 1;
	}
	rv = QueryPerformanceCounter(&tg->ticks);
	tg->tv_sec = tg->ticks.QuadPart / tg->frequency.QuadPart;
	frac = (int64_t)(tg->ticks.QuadPart - (tg->tv_sec * tg->frequency.QuadPart));
	tg->tv_nsec = (long)(frac * 1000000000L / (int64_t)tg->frequency.QuadPart);
	return !rv;
}
#endif

static int ppm_report(uint64_t nsamples, uint64_t interval)
{
	double real_rate, ppm;

	real_rate = nsamples * 1e9 / interval;
	ppm = 1e6 * (real_rate / (double)samp_rate - 1.);
	return (int)round(ppm);
}

static void ppm_test(uint32_t len)
{
	static uint64_t nsamples = 0;
	static uint64_t interval = 0;
	static uint64_t nsamples_total = 0;
	static uint64_t interval_total = 0;
	struct time_generic ppm_now;
	static struct time_generic ppm_recent;
	static enum {
		PPM_INIT_NO,
		PPM_INIT_DUMP,
		PPM_INIT_RUN
	} ppm_init = PPM_INIT_NO;

	ppm_gettime(&ppm_now);
	
	if (ppm_init != PPM_INIT_RUN) {
		/*
		 * Kyle Keen wrote:
		 * PPM_DUMP_TIME throws out the first N seconds of data.
		 * The dongle's PPM is usually very bad when first starting up,
		 * typically incorrect by more than twice the final value.
		 * Discarding the first few seconds allows the value to stabilize much faster.
		*/
		if (ppm_init == PPM_INIT_NO) {
			ppm_recent.tv_sec = ppm_now.tv_sec + PPM_DUMP_TIME;
			ppm_init = PPM_INIT_DUMP;
			return;
		}
		if (ppm_init == PPM_INIT_DUMP && ppm_recent.tv_sec < ppm_now.tv_sec)
			return;
		ppm_recent = ppm_now;
		ppm_init = PPM_INIT_RUN;
		return;
	}
	
	nsamples += (uint64_t)(len / 2UL);
	interval = (uint64_t)(ppm_now.tv_sec - ppm_recent.tv_sec);
	if (interval < ppm_duration)
		return;
	interval *= 1000000000UL;
	interval += (int64_t)(ppm_now.tv_nsec - ppm_recent.tv_nsec);
	nsamples_total += nsamples;
	interval_total += interval;
	printf("real sample rate: %i current PPM: %i cumulative PPM: %i\n",
		(int)((1000000000UL * nsamples) / interval),
		ppm_report(nsamples, interval),
		ppm_report(nsamples_total, interval_total));
	ppm_recent = ppm_now;
	nsamples = 0;
}

static void rtlsdr_callback(unsigned char *buf, uint32_t len, void *ctx)
{
	underrun_test(buf, len, 0);

	if (test_mode == PPM_BENCHMARK)
		ppm_test(len);
}

/* Controls the smallest band or band gap that tuner_benchmark() will notice */
static uint32_t max_step(uint32_t freq) {
	if (freq < 1e6)
		return 1e4;
	if (freq > 1e8)
		return 1e6;
	return freq / 1e2;
}

/* Controls the precision with which tuner_benchmark() will measure the edges of tuning bands */
static uint32_t min_step(uint32_t freq) {
	return 100;
}

static void report_band_start(uint32_t start) {
	fprintf(stderr, "Found a new band starting at %u Hz\n", start);
}

static void report_band(uint32_t low, uint32_t high) {
	fprintf(stderr, "Tuning band: %u - %u Hz\n", low, high);
}

void tuner_benchmark(void)
{
	uint32_t current = max_step(0);
	uint32_t band_start = 0;
	uint32_t low_bound = 0, high_bound = 0;
	char buf[20];
	enum { FIND_START, REFINE_START, FIND_END, REFINE_END } state;

	fprintf(stderr, "Testing tuner range. This may take a couple of minutes..\n");

	/* Scan for tuneable frequencies coarsely. When we find something,
	 * do a binary search to narrow down the exact edge of the band.
	 *
	 * This can potentially miss bands or band gaps that are smaller than max_step(freq)
	 * but it is a lot faster than exhaustively scanning everything.
	 */

	/* handle bands starting at 0Hz */
	if (rtlsdr_set_center_freq(dev, 0) < 0)
		state = FIND_START;
	else {
		band_start = 0;
		report_band_start(band_start);
		state = FIND_END;
	}

	while (current < 3e9 && !do_exit) {
		switch (state) {
		case FIND_START:
			/* scanning for the start of a new band */
			if (rtlsdr_set_center_freq(dev, current) < 0) {
				/* still looking for a band */
				low_bound = current;
				current += max_step(current);
			} else {
				/* new band, starting somewhere at or before current */
				/* low_bound < start <= current, refine it */
				high_bound = current;
				state = REFINE_START;
			}
			break;

		case REFINE_START:
			/* refining the start of a band */
			/* low_bound < bandstart <= high_bound */
			if (rtlsdr_set_center_freq(dev, current) == 0) {
				/* current is inside the band */
				/* low_bound < bandstart <= current */
				if (current - low_bound <= min_step(current)) {
					/* close enough. Say the band starts at current and go looking for the end of it. */
					band_start = current;
					report_band_start(band_start);
					low_bound = current;
					state = FIND_END;
				} else {
					/* try halfway between low_bound and current */
					high_bound = current;
					current = (current + low_bound) / 2;
				}
			} else {
				/* current is outside the band */
				/* current < bandstart <= high_bound */
				if (high_bound - current <= min_step(current)) {
					/* close enough. Say the band starts at high_bound and go looking for the end of it. */
					current = low_bound = band_start = high_bound;
					report_band_start(band_start);
					state = FIND_END;
				} else {
					/* try halfway betwen current and high_bound */
					low_bound = current;
					current = (current + high_bound) / 2;
				}
			}
			break;

		case FIND_END:
			/* scanning for the end of the current band */
			if (rtlsdr_set_center_freq(dev, current) == 0) {
				/* still looking for the end of the band */
				low_bound = current;
				current += max_step(current);
			} else {
				/* ran off the end of the band somewhere before current */
				/* low_bound <= bandend < current, refine it */
				high_bound = current;
				state = REFINE_END;
			}
			break;

		case REFINE_END:
			/* refining the end of a band */
			/* low_bound <= bandend < high_bound */
			if (rtlsdr_set_center_freq(dev, current) < 0) {
				/* current is outside the band */
				/* low_bound <= bandend < current */
				if (current - low_bound <= min_step(current)) {
					/* close enough. Say the band ends at low_bound and go looking for another band. */
					report_band(band_start, low_bound);
					low_bound = current;
					state = FIND_START;
				} else {
					/* try halfway between low_bound and current */
					high_bound = current;
					current = (current + low_bound) / 2;
				}
			} else {
				/* current is inside the band */
				/* current <= bandend < high_bound */
				if (high_bound - current <= min_step(current)) {
					/* close enough. Say the band ends at current and go looking for another band. */
					report_band(band_start, current);
					current = low_bound = high_bound;
					state = FIND_START;
				} else {
					/* try halfway betwen current and high_bound */
					low_bound = current;
					current = (current + high_bound) / 2;
				}
			}
			break;
		}
	}

	if (state == FIND_END)
		report_band(band_start, current);
	else if (state == REFINE_END)
		report_band(band_start, low_bound);
}

int main(int argc, char **argv)
{
#ifndef _WIN32
	struct sigaction sigact;
#endif
	int n_read, r, opt, i;
	int sync_mode = 0;
	uint8_t *buffer;
	int dev_index = 0;
	int dev_given = 0;
	uint32_t out_block_size = DEFAULT_BUF_LENGTH;
	int count;
	int gains[100];

	while ((opt = getopt(argc, argv, "d:s:b:tp::Sh")) != -1) {
		switch (opt) {
		case 'd':
			dev_index = verbose_device_search(optarg);
			dev_given = 1;
			break;
		case 's':
			samp_rate = (uint32_t)atof(optarg);
			break;
		case 'b':
			out_block_size = (uint32_t)atof(optarg);
			break;
		case 't':
			test_mode = TUNER_BENCHMARK;
			break;
		case 'p':
			test_mode = PPM_BENCHMARK;
			if (optarg)
				ppm_duration = atoi(optarg);
			break;
		case 'S':
			sync_mode = 1;
			break;
		case 'h':
		default:
			usage();
			break;
		}
	}

	if(out_block_size < MINIMAL_BUF_LENGTH ||
	   out_block_size > MAXIMAL_BUF_LENGTH ){
		fprintf(stderr,
			"Output block size wrong value, falling back to default\n");
		fprintf(stderr,
			"Minimal length: %u\n", MINIMAL_BUF_LENGTH);
		fprintf(stderr,
			"Maximal length: %u\n", MAXIMAL_BUF_LENGTH);
		out_block_size = DEFAULT_BUF_LENGTH;
	}

	buffer = malloc(out_block_size * sizeof(uint8_t));

	if (!dev_given) {
		dev_index = verbose_device_search("0");
	}

	if (dev_index < 0) {
		exit(1);
	}

	r = rtlsdr_open(&dev, (uint32_t)dev_index);
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
	count = rtlsdr_get_tuner_gains(dev, NULL);
	fprintf(stderr, "Supported gain values (%d): ", count);

	count = rtlsdr_get_tuner_gains(dev, gains);
	for (i = 0; i < count; i++)
		fprintf(stderr, "%.1f ", gains[i] / 10.0);
	fprintf(stderr, "\n");

	/* Set the sample rate */
	verbose_set_sample_rate(dev, samp_rate);

	if (test_mode == TUNER_BENCHMARK) {
		tuner_benchmark();
		goto exit;
	}

	/* Enable test mode */
	r = rtlsdr_set_testmode(dev, 1);

	/* Reset endpoint before we start reading from it (mandatory) */
	verbose_reset_buffer(dev);

	if ((test_mode == PPM_BENCHMARK) && !sync_mode) {
		fprintf(stderr, "Reporting PPM error measurement every %u seconds...\n", ppm_duration);
		fprintf(stderr, "Press ^C after a few minutes.\n");
	}

	if (test_mode == NO_BENCHMARK) {
		fprintf(stderr, "\nInfo: This tool will continuously"
				" read from the device, and report if\n"
				"samples get lost. If you observe no "
				"further output, everything is fine.\n\n");
	}

	if (sync_mode) {
		fprintf(stderr, "Reading samples in sync mode...\n");
		fprintf(stderr, "(Samples are being lost but not reported.)\n");
		while (!do_exit) {
			r = rtlsdr_read_sync(dev, buffer, out_block_size, &n_read);
			if (r < 0) {
				fprintf(stderr, "WARNING: sync read failed.\n");
				break;
			}

			if ((uint32_t)n_read < out_block_size) {
				fprintf(stderr, "Short read, samples lost, exiting!\n");
				break;
			}
			underrun_test(buffer, n_read, 1);
		}
	} else {
		fprintf(stderr, "Reading samples in async mode...\n");
		r = rtlsdr_read_async(dev, rtlsdr_callback, NULL,
				      0, out_block_size);
	}

	if (do_exit) {
		fprintf(stderr, "\nUser cancel, exiting...\n");
		fprintf(stderr, "Samples per million lost (minimum): %i\n", (int)(1000000L * dropped_samples / total_samples));
	}
	else
		fprintf(stderr, "\nLibrary error %d, exiting...\n", r);

exit:
	rtlsdr_close(dev);
	free (buffer);

	return r >= 0 ? r : -r;
}
