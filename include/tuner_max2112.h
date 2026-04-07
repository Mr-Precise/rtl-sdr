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

#ifndef _MAX2112_H_
#define _MAX2112_H_

#define MAX2112_I2C_ADDR		0xC0
#define MAX2112_CHECK_ADDR		0x00
#define MAX2112_CHECK_VAL		0x80

#define MAX2112_XTAL_BUFFER_DIVIDER	4

#define MAX2112_NDIVMSB			0x00
#define MAX2112_NDIVLSB			0x01
#define MAX2112_CHARGEPUMP		0x02
#define MAX2112_FDIVMSB			0x03
#define MAX2112_FDIVLSB			0x04
#define MAX2112_XTALRDIV		0x05
#define MAX2112_PLL			0x06
#define MAX2112_VCO			0x07
#define MAX2112_LPF			0x08
#define MAX2112_CONTROL			0x09
#define MAX2112_SHUTDOWN		0x0A
#define MAX2112_TEST			0x0B
#define MAX2112_STATUS1			0x0C
#define MAX2112_STATUS2			0x0D


#define MAX2112_NDIVMSB_FRAC		(1 << 7)
#define MAX2112_NDIVMSB_N_14_8(n)	(n & 0x7F)
#define MAX2112_CHARGEPUMP_CPMP(n)	((n & 0x03) << 6)
#define MAX2112_CHARGEPUMP_CPLIN(n)	((n & 0x03) << 4)
#define MAX2112_XTALRDIV_XD(n)		((n & 0x07) << 5)
#define MAX2112_XTALRDIV_R(n)		(n & 0x0F)
#define MAX2112_PLL_D24(n)		((n & 0x01) << 7)
#define MAX2112_PLL_CPS(n)		((n & 0x01) << 6)
#define MAX2112_PLL_ICP(n)		((n & 0x01) << 5)
#define MAX2112_VCO_SEL(n)		((n & 0x1F) << 3)
#define MAX2112_VCO_VAS(n)		((n & 0x01) << 2)
#define MAX2112_VCO_ADL(n)		((n & 0x01) << 1)
#define MAX2112_VCO_ADE(n)		((n & 0x01) << 0)
#define MAX2112_CONTROL_STBY(n)		((n & 0x01) << 7)
#define MAX2112_CONTROL_PWDN(n)		((n & 0x01) << 5)
#define MAX2112_CONTROL_BBG(n)		(n & 0x0F)
#define MAX2112_SHUTDOWN_PLL(n)		((n & 0x01) << 6)
#define MAX2112_SHUTDOWN_DIV(n)		((n & 0x01) << 5)
#define MAX2112_SHUTDOWN_VCO(n)		((n & 0x01) << 4)
#define MAX2112_SHUTDOWN_BB(n)		((n & 0x01) << 3)
#define MAX2112_SHUTDOWN_RFMIX(n)	((n & 0x01) << 2)
#define MAX2112_SHUTDOWN_RFVGA(n)	((n & 0x01) << 1)
#define MAX2112_SHUTDOWN_FE(n)		((n & 0x01) << 0)
#define MAX2112_TEST_CPTST(n)		((n & 0x07) << 5)
#define MAX2112_TEST_TURBO(n)		((n & 0x01) << 3)
#define MAX2112_TEST_LDMUX(n)		(n & 0x07)
#define MAX2112_STATUS1_POR(d)		((d >> 7) & 0x01)
#define MAX2112_STATUS1_VASA(d)		((d >> 6) & 0x01)
#define MAX2112_STATUS1_VASE(d)		((d >> 5) & 0x01)
#define MAX2112_STATUS1_LD(d)		((d >> 4) & 0x01)
#define MAX2112_STATUS2_VCOSBR_RB(d)	((d & 0xF8) >> 4)
#define MAX2112_STATUS2_ADC_RB(d)	(d & 0x07)



int max2112_init(void *dev);
int max2112_exit(void *dev);
int max2112_set_freq(void *dev, uint32_t freq, uint32_t *lo_freq);
int max2112_set_bw(void *dev, int bw);
int max2112_set_gain(void *dev, int gain);
int max2112_set_if_gain(void *dev, int stage, int gain);
int max2112_set_gain_mode(void *dev, int manual);

#endif /* _MAX2112_H_ */