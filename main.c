/*
 * serial_lib.c
 *
 * Created: 1/2/2017 3:00:30 PM
 * Author : Alan Uomoto
 
ATTINY841 NOTES

FUSES
Reading the fuses:
avrdude -c usbtiny -p attiny841 -U lfuse:r:-:h

For an unmodified ATtiny841, this gives:
lfuse = 0x42
hfuse = 0xdf
efuse = 0xff

We noticed that the lfuse value listed as default in the datasheet
(p 219) is 0x62. Bit 5, which is not used, is programmed (value = 0)
on my device but the datasheet says it's not (value = 1).

CLOCK SPEED 8 MHz

The factory default divides the internal clock by 8, resulting in a
1 MHz clock speed. I changed this so that the chip runs at 8 MHz (no
clock division) by changing lfuse to 0xC2.
avrdude -c usbtiny -p attiny841 -U lfuse:w:0xC2:m
so:
lfuse = 0xc2
hfuse = 0xdf
efuse = 0xff

CLOCK SPEED 14.745600 MHz

To use a 14.745600 MHz crystal, change the CKSEL[3:0] bits (p 26 & p 220)
to 111X.

So lfuse becomes 0xce:

avrdude -c usbtiny -p attiny841 -U lfuse:w:0xCE:m

USART
There are two:
USART0: RXD0 is PA2 on pin 11, TXD0 is PA1 on pin 12
USART1: RXD1 is PA4 on pin 9, TXD1 is PA5 on pin 8


*/ 

#define F_CPU 14745600

#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>		// For ltoa()

#define BAUDRATE	9600
#define MYUBRR		((F_CPU / 16 / BAUDRATE) - 1)
#define TX0READY	(UCSR0A & (1 << UDRE0))
#define TX1READY	(UCSR1A & (1 << UDRE1))
#define RX0READY	(UCSR0A & (1 << RXC0))
#define RX1READY	(UCSR1A & (1 << RXC1))

// Function prototypes
void flashLED(uint8_t);
void initialize(void);
uint8_t serialRecvByte(uint8_t);
int16_t serialRecvNum(uint8_t);
void serialSendBin(uint8_t, uint8_t);
void serialSendByte(uint8_t, uint8_t);
void serialSendCRLF(uint8_t);
void serialSendNum16(uint8_t, int16_t);
void serialSendStr(uint8_t, char *);


int main(void)
{

	int16_t numbr;

	initialize();
	serialSendStr(0, "ABCDE\n\r");
	numbr = serialRecvNum(0);
	serialSendNum16(0, numbr);
	serialSendCRLF(0);

}

void initialize()
{

	// Setup serial port 0
	UBRR0H = (uint8_t) (MYUBRR >> 8);		// Set the baud rate
	UBRR0L = (uint8_t) MYUBRR;				// Set the baud rate (second half)
	UCSR0B = (1 << RXEN0) | (1 << TXEN0);	// Enable USART receive and transmit
	UCSR0C = (3 << UCSZ00);					// 8 bits, no parity, one stop bit

	// Setup serial port 1
	UBRR1H = (uint8_t) (MYUBRR >> 8);		// Set the baud rate
	UBRR1L = (uint8_t) MYUBRR;				// Set the baud rate (second half)
	UCSR1B = (1 << RXEN0) | (1 << TXEN0);	// Enable USART receive and transmit
	UCSR1C = (3 << UCSZ00);					// 8 bits, no parity, one stop bit

	// Set up the LED on the ATtiny841 Basic board
	DDRB |= (1 << PORTB2);					// LED is on PORTB2

	flashLED(5);

}

void flashLED(uint8_t ntimes)
{

	uint8_t i;

	for (i = 0; i < ntimes; i++) {
		PORTB |= (1 << PORTB2);
		_delay_ms(125);
		PORTB &= ~(1 << PORTB2);
		_delay_ms(125);
	}

}

/* SERIAL ROUTINES */

uint8_t serialRecvByte(uint8_t port)
{

	uint8_t b = 0;

	if (port == 0) {
		while (!RX0READY) {
			asm("nop");
		}
		b = UDR0;
	} else if (port == 1) {
		while (!RX1READY) {
			asm("nop");
		}
		b = UDR1;
	}

	return(b);

}

int16_t serialRecvNum(uint8_t port)
{
	char strBuf[7];
	uint8_t i;

	i = 0;
	for (;;) {
		strBuf[i] = serialRecvByte(port);
		serialSendByte(port, strBuf[i]);
		if (strBuf[i] == '\r') {
			strBuf[i] = '\0';
			serialSendByte(port, '\n');
			break;
		}
		if (i == 5) {
			if ((strBuf[0] != '+') && (strBuf[0] != '-')) {
				serialSendCRLF(port);
				serialSendByte(port, '?');
				serialSendCRLF(port);
				return(0);
			}
		}
		if (i == 6) {
			serialSendCRLF(port);
			serialSendByte(port, '?');
			serialSendCRLF(port);
			return(0);
		}
		i++;
	}
	return(atoi(strBuf));
}

void serialSendBin(uint8_t port, uint8_t number)
{

	char strBuf[7];

	serialSendStr(port, itoa(number, strBuf, 2));

}

void serialSendByte(uint8_t port, uint8_t c)
{

	if (port == 0) {
		while (!TX0READY) {
			asm("nop");
		}
		UDR0 = c;
	} else if (port == 1) {
		while (!TX1READY) {
			asm("nop");
		}
		UDR1 = c;
	}

}

void serialSendCRLF(uint8_t port)
{

	serialSendStr(port, "\n\r");

}

void serialSendNum16(uint8_t port, int16_t number)
{

	char strBuf[7];

	serialSendStr(port, ltoa((int32_t) number, strBuf, 10));

}


void serialSendStr(uint8_t port, char *str)
{

	uint8_t i;

	i = 0;

	while (str[i]) {
		serialSendByte(port, str[i++]);
	}

}
