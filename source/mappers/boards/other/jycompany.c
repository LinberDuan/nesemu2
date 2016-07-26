/***************************************************************************
 *   Copyright (C) 2013-2016 by James Holodnak                             *
 *   jamesholodnak@gmail.com                                               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "mappers/mapperinc.h"

static u8 irqmode, irqcounter, irqprescaler, irqenabled, irqxor, irqwait;
static u8 mode, mirror, ntram, chrblock;
static u8 prgbank[4];
static u8 chrbanklo[8];
static u8 chrbankhi[8];
static u8 ntbank[8];
static u8 ram[8];
static u8 mul[2];
static u8 dip = 0;

static u8 reverse(u8 byte)
{
	u8 ret = 0;

	ret |= (byte & 0x01) << 6;
	ret |= (byte & 0x02) << 4;
	ret |= (byte & 0x04) << 2;
	ret |= (byte & 0x08) << 0;
	ret |= (byte & 0x10) >> 2;
	ret |= (byte & 0x20) >> 4;
	ret |= (byte & 0x40) >> 6;
	return(ret);
}

static void sync_prg()
{
	u8 lastbank;

	//unmap prg at $6000-7FFF
	if ((mode & 0x80) == 0) {
		mem_setprg8(8,0);
	}

	lastbank = (mode & 4) ? prgbank[3] : 0x7F;
	switch (mode & 3) {
	case 0:
		if (mode & 0x80)
			mem_setprg8(0x6, (prgbank[3] * 4) + 3);
		mem_setprg32(0x8, lastbank);
		break;
	case 1:
		if (mode & 0x80)
			mem_setprg8(0x6, (prgbank[3] * 2) + 1);
		mem_setprg16(0x8, prgbank[1]);
		mem_setprg16(0xC, lastbank);
		break;
	case 2:
		if (mode & 0x80)
			mem_setprg8(0x6, prgbank[3]);
		mem_setprg8(0x8, prgbank[0]);
		mem_setprg8(0xA, prgbank[1]);
		mem_setprg8(0xC, prgbank[2]);
		mem_setprg8(0xE, lastbank);
		break;
	case 3:
		if (mode & 0x80)
			mem_setprg8(0x6, reverse(prgbank[3]));
		mem_setprg8(0x8, reverse(prgbank[0]));
		mem_setprg8(0xA, reverse(prgbank[1]));
		mem_setprg8(0xC, reverse(prgbank[2]));
		mem_setprg8(0xE, reverse(lastbank));
		break;
	}

}

#define chrbank(nn)			(chrbanklo[nn] | (chrbankhi[nn] << 8))
#define chrblockbank(nn)	(chrbanklo[nn] | ((chrblock & 0x1F) << 8))

void sync_chr()
{
	switch (((mode & 0x18) | (chrblock & 0x20)) >> 3) {

	//block mode enabled
	case 0:
		mem_setchr8(0, chrblockbank(0));
		break;

	case 1:
		mem_setchr4(0, chrblockbank(0));
		mem_setchr4(4, chrblockbank(4));
		break;

	case 2:
		mem_setchr2(0, chrblockbank(0));
		mem_setchr2(2, chrblockbank(2));
		mem_setchr2(4, chrblockbank(4));
		mem_setchr2(6, chrblockbank(6));
		break;

	case 3:
		mem_setchr1(0, chrblockbank(0));
		mem_setchr1(1, chrblockbank(1));
		mem_setchr1(2, chrblockbank(2));
		mem_setchr1(3, chrblockbank(3));
		mem_setchr1(4, chrblockbank(4));
		mem_setchr1(5, chrblockbank(5));
		mem_setchr1(6, chrblockbank(6));
		mem_setchr1(7, chrblockbank(7));
		break;

	//block mode disabled
	case 4:
		mem_setchr8(0, chrbank(0));
		break;

	case 5:
		mem_setchr4(0, chrbank(0));
		mem_setchr4(4, chrbank(4));
		break;

	case 6:
		mem_setchr2(0, chrbank(0));
		mem_setchr2(2, chrbank(2));
		mem_setchr2(4, chrbank(4));
		mem_setchr2(6, chrbank(6));
		break;

	case 7:
		mem_setchr1(0, chrbank(0));
		mem_setchr1(1, chrbank(1));
		mem_setchr1(2, chrbank(2));
		mem_setchr1(3, chrbank(3));
		mem_setchr1(4, chrbank(4));
		mem_setchr1(5, chrbank(5));
		mem_setchr1(6, chrbank(6));
		mem_setchr1(7, chrbank(7));
		break;
	}
}

void sync_nt()
{
	switch (mirror & 3) {
	case 0: mem_setmirroring(MIRROR_V); break;
	case 1: mem_setmirroring(MIRROR_H); break;
	case 2: mem_setmirroring(MIRROR_1L); break;
	case 3: mem_setmirroring(MIRROR_1H); break;
	}
}

static void sync()
{
	sync_prg();
	sync_chr();
	sync_nt();
}

static u8 read5(u32 addr)
{
	u8 ret = addr >> 8;

	switch (addr) {
	case 0x5000:
		ret = dip;
	case 0x5800:
		ret = (mul[0] * mul[1]) & 0xFF;
		break;
	case 0x5801:
		ret = (mul[0] * mul[1]) >> 8;
		break;

	case 0x5803:
	case 0x5804:
	case 0x5805:
	case 0x5806:
	case 0x5807:
		ret = ram[addr & 7];
		break;

	}
	log_printf("read5: $%04X = $%02X\n", addr, ret);
	return(ret);
}

static void write5(u32 addr, u8 data)
{
	switch (addr) {
	case 0x5800:
	case 0x5801:
		mul[addr & 1] = data;
		break;

	case 0x5803:
	case 0x5804:
	case 0x5805:
	case 0x5806:
	case 0x5807:
		ram[addr & 7] = data;
		break;
	}
	log_printf("write5: $%04X = $%02X\n", addr, data);
}

static void write(u32 addr, u8 data)
{
	switch (addr & 0xF000) {
	case 0x8000:
		prgbank[addr & 3] = data;
		sync_prg();
		break;
	case 0x9000:
		chrbanklo[addr & 7] = data;
		sync_chr();
		break;
	case 0xA000:
		chrbankhi[addr & 7] = data;
		sync_chr();
		break;
	case 0xB000:
		ntbank[addr & 7] = data;
		sync_nt();
		break;
	case 0xC000:
		switch (addr & 7) {
		case 0:
			break;
		case 1:
			irqmode = data;
			log_printf("irqmode = %02X @ PC = %04X\n", data, nes->cpu.pc);
			break;
		case 2:
			irqenabled = 0;
			cpu_clear_irq(IRQ_MAPPER);
			break;
		case 3:
			irqenabled = 1;
			break;
		case 4:
			irqprescaler = data ^ irqxor;
			log_printf("irqprescaler = %02X @ PC = %04X\n", data, nes->cpu.pc);
			break;
		case 5:
			irqcounter = data ^ irqxor;
			log_printf("irqcounter = %02X @ PC = %04X\n", data, nes->cpu.pc);
			break;
		case 6:
			irqxor = data;
			break;
		case 7:
			break;
		}
		break;
	case 0xD000:
		switch (addr & 3) {
		case 0:
			mode = data;
			log_printf("mode = %02X @ PC = %04X\n", data, nes->cpu.pc);
			sync_prg();
			sync_chr();
			sync_nt();
			break;
		case 1:
			mirror = data;
			sync_nt();
			break;
		case 2:
			ntram = data;
			sync_nt();
			break;
		case 3:
			chrblock = data;
			sync_chr();
			break;
		}
		break;
	case 0xE000:
		break;
	case 0xF000:
		break;
	}
}

static void reset(int hard)
{
	int i;

	mem_setreadfunc(5, read5);
	mem_setwritefunc(5, write5);
	for (i = 8; i<16; i++)
		mem_setwritefunc(i, write);

	if (hard) {
		mode = 0;
		mirror = 0;
		ntram = 0;
		chrblock = 0;
		for (i = 0; i < 8; i++) {
			prgbank[i & 3] = 0;
			chrbanklo[i] = 0;
			chrbankhi[i] = 0;
			ntbank[i] = 0;
		}
	}

	irqcounter = irqprescaler = 0;
	irqenabled = irqmode = 0;
	irqxor = 0;
	irqwait = 0;

	sync_prg();
	sync_chr();
	sync_nt();
}

static int irqline = 0;

static void irqclock()
{
	unsigned char mask;
	if (irqmode & 0x4)
		mask = 0x7;
	else	mask = 0xFF;
	if ((irqmode & 0xC0) == 0x80)
	{
		irqprescaler--;
		if ((irqprescaler & mask) == mask)
		{
			irqcounter--;
			if (irqcounter == 0xFF) {
				cpu_set_irq(IRQ_MAPPER);
				irqline = SCANLINE;
			}
		}
	}
	if ((irqmode & 0xC0) == 0x40)
	{
		irqprescaler++;
		if (!(irqprescaler & mask))
		{
			irqcounter++;
			if (!irqcounter) {
				cpu_set_irq(IRQ_MAPPER);
				irqline = SCANLINE;
			}
		}
	}
}

static u32 irqaddr;

static void ppucycle()
{
	if (irqenabled && (irqmode & 3) == 1) {
		if ((irqaddr & 0x1000) == 0 && (nes->ppu.busaddr & 0x1000))
			irqclock();
		irqaddr = nes->ppu.busaddr;
	}
}

void video_updaterawpixel(int line, int pixel, u32 s);

static void cpucycle()
{
	int i;

	if (LINECYCLES == 0) {
		if (irqline) {
			for (i = 0; i < 256; i++) {
				video_updaterawpixel(irqline, i, 0x00FF00);
			}
		}
		irqline = 0;
	}
	if (irqenabled && (irqmode & 3) == 0) {
		irqclock();
	}
}

static void state(int mode, u8 *data)
{

}

MAPPER(B_JYCOMPANY, reset, ppucycle, cpucycle, state);
