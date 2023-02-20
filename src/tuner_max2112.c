/*
 * Maxim MAX2112 Tuner Driver for RTL-SDR
 *
 * Copyright (C) 2014 MattW (opendous _AT_ opendous _DOT_ com)
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


// Notes: - RF gain is set by analog signal from the RTL2832U to the GC1 pin
//        - refer to MAX2121 Datasheet Pg16 for programming instructions

// TODO:  - convert set_freq to return int64_t as right now it returns 32-bit int
//          so frequencies above 2147MHz won't work properly

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <windows.h>
#endif

#include "rtlsdr_i2c.h"
#include "tuner_max2112.h"

#ifdef _WIN32
#if defined(_MSC_VER) && (_MSC_VER < 1800)
#define round(x) (x > 0.0 ? floor(x + 0.5): ceil(x - 0.5))
#endif
#endif

#define DEBUGINFO	0

// minimum and maximum values yield theoretical 548MHz to 7229MHz tuning range
#define nMin	19
#define nMax	251
#define fMin	1
#define fMax	1048575




static int max2112_writereg(void *dev, uint8_t reg, uint8_t val)
{
	uint8_t data[2];
	data[0] = reg;
	data[1] = val;

	if (rtlsdr_i2c_write_fn(dev, MAX2112_I2C_ADDR, data, 2) < 0)
	{
		return -1;
	}

	return 0;
}

static int max2112_readreg(void *dev, uint8_t reg, uint8_t *val)
{
	uint8_t data = reg;

	if (rtlsdr_i2c_write_fn(dev, MAX2112_I2C_ADDR, &data, 1) < 0)
	{
		return -1;
	}

	if (rtlsdr_i2c_read_fn(dev, MAX2112_I2C_ADDR, &data, 1) < 0)
	{
		return -1;
	}

	*val = data;

	return 0;
}


void max2112_registerdump(void *dev)
{
	uint8_t regval = 0;
	uint8_t i = 0;

	#if 0

	fprintf(stderr, "\n[MAX2112] Register Dump\n");

	for (i = 0; i < 14; i++)
	{
		if ( 0 > max2112_readreg(dev, i, &regval) )
		{
			// failed to read register
			fprintf(stderr, "[MAX2112]\tRegister 0x%02X Access Failed\n", i);
		} else
		{
			fprintf(stderr, "[MAX2112]\tRegister 0x%02X = 0x%02X\n", i, regval);
		}
	}

	fprintf(stderr, "\n\n");

	#endif
}


// return -1 - I2C read or write failed
// -2 for n value out of range
// -3; // VCO has failed to lock
int max2112_set_freq(void *dev, uint32_t tunefreq)
{

	uint32_t rtlfreq = 0;
	uint32_t fout = 0;
	double calculatedout = 0;
	float fratio = 0;
	float nval = 0;
	float fval = 0;
	uint32_t f = 0;
	uint8_t n = 0;
	uint8_t regval = 0;
max2112_registerdump(dev);

	// use default clock of 28.8MHz if get_tuner_clock fails to avoid /0
	rtlfreq = rtlsdr_get_tuner_clock(dev);
	if ( 0 == rtlfreq )
	{
		rtlfreq = 28800000;
	}


	// set up the PLL register
	//D24 7  1   VCO divider setting
	//       0 = Divide by 2. Use for LO frequencies ≥ 1125MHz
	//       1 = Divide by 4. Use for LO frequencies < 1125MHz
	if ( 1125000000 > tunefreq )
	{
		// Charge-pump current is controlled by VAS (CPS=1)
		// set D24 bit to divide-by-4 the VCO output
		regval = (uint8_t)(MAX2112_PLL_D24(0) | MAX2112_PLL_CPS(0) | MAX2112_PLL_ICP(1));
	} else
	{
		// Charge-pump current is controlled by VAS (CPS=1)
		// clear D24 bit to divide-by-2 the VCO output
		regval = (uint8_t)(MAX2112_PLL_D24(0) | MAX2112_PLL_CPS(0) | MAX2112_PLL_ICP(1));
	}

	// write the PLL register	
	if ( 0 > max2112_writereg(dev, MAX2112_PLL, regval) )
	{
		fprintf(stderr, "[MAX2112] Failed to write register PLL\n");
		return -1; // failed to write register
	}


	// output freqeuncy to tuner input frequency ratio
	fratio = (float)(((float)tunefreq) / ((float)rtlfreq));

	// separate the integer and fractional parts of the ratio
	fval = modff(fratio, &nval);

	n = (uint8_t)nval;
	f = (uint32_t)(round(fval * ((float)(1 << 20))));

	#if 0
		fprintf(stderr, "[MAX2112] set_freq: tunefreq=%dHz, rtlfreq=%dHz, ratio=%f, n=%d=0x%02X, f=%d=0x%06X\n", tunefreq, rtlfreq, fratio, n, n, f, f);
	#endif

	// calculate the exact output frequency
	calculatedout = ((double)rtlfreq) * (((double)(n)) + ((double)(((float)(f)) / ((float)(1 << 20)))));
	fout = (uint32_t)(calculatedout);

	#if 0
		fprintf(stderr, "[MAX2112] Output Frequency = %f = %dHz, diff=%dHz\n", calculatedout, fout, (tunefreq-fout));
	#endif


	// write the N Divider LSB register since range is only 19 to 251
	if ( (nMin > n)  ||  (nMax < n) )
	{
		fprintf(stderr, "[MAX2112] n value out of range\n");
		return -2; // n value is out of range
	}
	regval = n;
	if ( 0 > max2112_writereg(dev, MAX2112_NDIVLSB, regval) )
	{
		fprintf(stderr, "[MAX2112] Failed to write register NDIVLSB\n");
		return -1; // failed to write register
	}


	// write the XTAL Buffer and Reference Divider register to Divide-by-1
	regval = (MAX2112_XTALRDIV_XD(MAX2112_XTAL_BUFFER_DIVIDER) | MAX2112_XTALRDIV_R(1));
	if ( 0 > max2112_writereg(dev, MAX2112_XTALRDIV, regval) )
	{
		fprintf(stderr, "[MAX2112] Failed to write register XTALRDIV\n");
		return -1; // failed to write register
	}


	// write the F Divider MSB nibble F[19:16] and CP linearity to 01
	regval = ((MAX2112_CHARGEPUMP_CPLIN(1)) | ((uint8_t)(f >> 16)));
	if ( 0 > max2112_writereg(dev, MAX2112_CHARGEPUMP, regval) )
	{
		fprintf(stderr, "[MAX2112] Failed to write register CHARGEPUMP\n");
		return -1; // failed to write register
	}


	// write the F Divider MSB byte F[15:8]
	regval = ((uint8_t)(f >> 8));
	if ( 0 > max2112_writereg(dev, MAX2112_FDIVMSB, regval) )
	{
		fprintf(stderr, "[MAX2112] Failed to write register FMSB\n");
		return -1; // failed to write register
	}


	// write the F Divider LSB byte F[7:0]
	regval = ((uint8_t)(f));
	if ( 0 > max2112_writereg(dev, MAX2112_FDIVLSB, regval) )
	{
		fprintf(stderr, "[MAX2112] Failed to write register FLSB\n");
		return -1; // failed to write register
	}


	// read Status Byte1 to see if VCO has locked
	if ( 0 > max2112_readreg(dev, MAX2112_STATUS1, &regval) )
	{
		fprintf(stderr, "[MAX2112] Failed to read register STATUS1\n");
		return -1; // failed to read register
	}


	// check that VCO autoselection was successful
	if ( 0 == MAX2112_STATUS1_VASA(regval) )
	{
		fprintf(stderr, "[MAX2112] VCO did not autoselect for %uHz!\n", tunefreq);
	}


	// check that VCO has locked
	if ( 0 == MAX2112_STATUS1_LD(regval) )
	{
		//#if DEBUGINFO
		#if 1
			fprintf(stderr, "[MAX2112] PLL did not lock for %uHz!  STATUS1(0x%02X)=0x%02X\n", tunefreq, MAX2112_STATUS1, regval);
		#endif

		return -3; // VCO has failed to lock
		// TODO - return to previously working tuner frequency
	}


	#if 0
		// read Status Byte2 to see if VCO has locked
		if ( 0 > max2112_readreg(dev, MAX2112_STATUS2, &regval) )
		{
			fprintf(stderr, "[MAX2112] Failed to read register STATUS2\n");
			return -1; // failed to read register
		}

		// print current VCO Band
		fprintf(stderr, "[MAX2112] Current VCO Band = %d, ADC=%d\n", MAX2112_STATUS2_VCOSBR_RB(regval), MAX2112_STATUS2_ADC_RB(regval));
	#endif

max2112_registerdump(dev);
	return (fout >> 2);
}


int max2112_init(void *dev)
{
	uint8_t regval = 0;

max2112_registerdump(dev);

	// write the N Divider register	to set FRACtional mode
	regval = (uint8_t)MAX2112_NDIVMSB_FRAC;
	if ( 0 > max2112_writereg(dev, MAX2112_NDIVMSB, regval) )
	{
		fprintf(stderr, "[MAX2112] Failed to write register NDIVMSB\n");
		return -1; // failed to write register
	}

	// write the XTAL Buffer and Reference Divider register to Divide-by-1
	regval = (MAX2112_XTALRDIV_XD(MAX2112_XTAL_BUFFER_DIVIDER) | MAX2112_XTALRDIV_R(1));
	if ( 0 > max2112_writereg(dev, MAX2112_XTALRDIV, regval) )
	{
		fprintf(stderr, "[MAX2112] Failed to write register \n");
		return -1; // failed to write register
	}

	// write the Test register to enable fast lock - allows lock-detect reading
	regval = MAX2112_TEST_TURBO(1);
	if ( 0 > max2112_writereg(dev, MAX2112_TEST, regval) )
	{
		fprintf(stderr, "[MAX2112] Failed to write register TEST\n");
		return -1; // failed to write register
	}

	// write the Baseband Lowpass filter register to 0 for 4MHz bandwidth
	regval = 0;
	if ( 0 > max2112_writereg(dev, MAX2112_LPF, regval) )
	{
		fprintf(stderr, "[MAX2112] Failed to write register LPF\n");
		return -1; // failed to write register
	}

	// Charge-pump current is controlled by VAS (CPS=1)
	// set D24 bit to divide-by-4 the VCO output
	regval = (uint8_t)(MAX2112_PLL_D24(1) | MAX2112_PLL_CPS(0) | MAX2112_PLL_ICP(1));
	// write the PLL register	
	if ( 0 > max2112_writereg(dev, MAX2112_PLL, regval) )
	{
		fprintf(stderr, "[MAX2112] Failed to write register PLL\n");
		return -1; // failed to write register
	}

	// write the VCO register to enable automatic VCO selection
	regval = MAX2112_VCO_VAS(1);
	// write the PLL register	
	if ( 0 > max2112_writereg(dev, MAX2112_VCO, regval) )
	{
		fprintf(stderr, "[MAX2112] Failed to write register VCO\n");
		return -1; // failed to write register
	}

	// write the VCO register to enable automatic VCO selection
	regval = 0x00;
	// write the Control register for normal operation and 0dB BB Gain
	if ( 0 > max2112_writereg(dev, MAX2112_CONTROL, regval) )
	{
		fprintf(stderr, "[MAX2112] Failed to write register Control\n");
		return -1; // failed to write register
	}

	// n=32, f=1034012=0x0FC71C will default to 950MHz RF Frequency

	// write the default N Divider LSB value
	regval = 72;
	if ( 0 > max2112_writereg(dev, MAX2112_NDIVLSB, regval) )
	{
		fprintf(stderr, "[MAX2112] Failed to write register NDIVLSB\n");
		return -1; // failed to write register
	}

	// write the F Divider MSB nibble F[19:16] and CP linearity to 01
	regval = ((MAX2112_CHARGEPUMP_CPLIN(1)) | ((uint8_t)(0x0F)));
	if ( 0 > max2112_writereg(dev, MAX2112_CHARGEPUMP, regval) )
	{
		fprintf(stderr, "[MAX2112] Failed to write register CHARGEPUMP\n");
		return -1; // failed to write register
	}

	// write the F Divider MSB byte F[15:8]
	regval = 0xC7;
	if ( 0 > max2112_writereg(dev, MAX2112_FDIVMSB, regval) )
	{
		fprintf(stderr, "[MAX2112] Failed to write register FMSB\n");
		return -1; // failed to write register
	}

	// write the F Divider LSB byte F[7:0]
	regval = 0x1C;
	if ( 0 > max2112_writereg(dev, MAX2112_FDIVLSB, regval) )
	{
		fprintf(stderr, "[MAX2112] Failed to write register FLSB\n");
		return -1; // failed to write register
	}


	// read Status Byte1 to see if VCO has locked
	if ( 0 > max2112_readreg(dev, MAX2112_STATUS1, &regval) )
	{
		fprintf(stderr, "[MAX2112] Failed to read register STATUS1\n");
		return -1; // failed to read register
	}

	// check that VCO autoselection was successful
	if ( 0 == MAX2112_STATUS1_VASA(regval) )
	{
		fprintf(stderr, "[MAX2112] VCO did not autoselect for 950MHz!\n");
	}


	// check that VCO has locked
	if ( 0 == MAX2112_STATUS1_LD(regval) )
	{
		fprintf(stderr, "[MAX2112] PLL did not lock for 950MHz!  STATUS1(0x%02X)=0x%02X\n", MAX2112_STATUS1, regval);
		//return -3; // VCO has failed to lock
	}

max2112_registerdump(dev);

	return 0;
}



int max2112_exit(void *dev)
{

	// enter Standby mode
	if ( 0 > max2112_writereg(dev, MAX2112_CONTROL, 0x80) )
	{
		return -1; // failed to write register
	}

	return 0;
}



int max2112_set_bw(void *dev, int bw)
{

	uint8_t regval = (uint8_t)bw;

	// enter Standby mode
	if ( 0 > max2112_writereg(dev, MAX2112_CONTROL, regval) )
	{
		return -1; // failed to write register
	}

	return (4000000 + (((int)regval - 12) * 290000));
}



int max2112_set_gain(void *dev, int gain)
{

	uint8_t regval = MAX2112_CONTROL_BBG((uint8_t)gain);

	// enter Standby mode
	if ( 0 > max2112_writereg(dev, MAX2112_CONTROL, regval) )
	{
		return -1; // failed to write register
	}

	return 0;
}



int max2112_set_if_gain(void *dev, int stage, int gain)
{
	uint8_t regval = (uint8_t)gain;

	// enter Standby mode
	if ( 0 > max2112_writereg(dev, MAX2112_CONTROL, regval) )
	{
		return -1; // failed to write register
	}

	return 0;
}


int max2112_set_gain_mode(void *dev, int manual)
{
	uint8_t regval = (uint8_t)manual;

	// enter Standby mode
	if ( 0 > max2112_writereg(dev, MAX2112_CONTROL, regval) )
	{
		return -1; // failed to write register
	}

	return 0;
}
