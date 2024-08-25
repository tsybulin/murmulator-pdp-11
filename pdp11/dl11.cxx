#include <cstdlib>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include "kb11.h"
#include "dl11.h"
#include "graphics.h"
#include <stdio.h>
#include "pico/stdlib.h"

extern KB11 cpu;

static bool keypressed = false;


DL11::DL11() {
}

void DL11::clearterminal() {
	rcsr = 0;
	xcsr = 0x80;
	rbuf = 0;
	xbuf = 0;
	count = 0;
}

static int _kbhit()
{
	return uart_is_readable(PIN_UART_ID);
}

void DL11::serial_putchar(char c)
{
	while (!uart_is_writable(PIN_UART_ID));
	uart_putc(PIN_UART_ID,c);
}
char DL11::serial_getchar()
{
	return uart_getc(PIN_UART_ID);
}

void DL11::poll() {
	if (!rcvrdone()) {
		// unit not busy
		if (true)
			if (_kbhit() || keypressed) {
				char ch = serial_getchar();
				gprintf("DL11: poll char 0x%02X", ch) ;
				serial_putchar('Z') ;
				serial_putchar('h') ;
				serial_putchar('o') ;
				serial_putchar('p') ;
				serial_putchar('A') ;
				serial_putchar('\n') ;
				count = 0;
				if (true) {
					rbuf = ch & 0x7f;
					rcsr |= 0x80;
					if (rcsr & 0x40) {
						cpu.interrupt(INTDLR, 4);
					}
				}
				else {
					keypressed = false;
				}
			}
	}


	if (xbuf) {
		xcsr |= 0x80;
		xbuf = 0;
		if (xcsr & 0x40) {
			cpu.interrupt(INTDLT, 4);
		}
	}
	else {
		if (iflag == 1) {
			cpu.interrupt(INTDLT, 4);
			iflag = 2;
		}
	}
}

uint16_t DL11::read16(uint32_t a) {
	int i;

	switch (a & 7) {
	case 00:
		return rcsr;
	case 02:
		rcsr &= ~0x80;
		return rbuf;
	case 04:
		return xcsr;
	case 06:
		return xbuf;
	default:
		gprintf("Dl11: read from invalid address %06o\n", a);
		trap(INTBUS);
	}
}

void DL11::write16(uint32_t a, uint16_t v) {
	switch (a & 7) {
		case 00:
			rcsr = ((rcsr & 0200) ^ (v & ~0200));
			break;
		case 02:
			//rcsr &= ~0x80;
			break;
		case 04:
			xcsr = ((xcsr & 0200) ^ (v & ~0200));
		if ((xcsr & 0300) == 0300 && iflag == 0)
			iflag = 1;
		if (iflag == 2)
			iflag = 0;
			break;
		case 06:
		xbuf = v & 0x7f;
		serial_putchar(v & 0x7f);
		xbuf |= 0200; // Allow for nulls !!!!
		xcsr &= ~0x80;
		iflag = 0;
		break;
	default:
		gprintf("Dl11: write to invalid address %06o\n", a);
		trap(INTBUS);
	}
}

