F_CPU=16000000
MMCU=atmega328p
CC=avr-gcc
BUSPIRATE=/dev/ttyUSB0
BAUD=9600

CFLAGS=-DF_CPU=$(F_CPU) -Os -mmcu=$(MMCU)

.PHONY: all
all: blink.elf echo.elf temp.elf

uart.o: CFLAGS+=-DBAUD=$(BAUD)
%.o: %.c
	$(CC) -c $(CFLAGS) $*.c

%.hex: %.elf
	avr-objcopy -j .text -j .data -O ihex $*.elf $*.hex

blink.elf: blink.o
	$(CC) $(CFLAGS) $^ -o blink.elf

echo.elf: echo.o buffer.o uart.o job.o
	$(CC) $(CFLAGS) $^ -o echo.elf

temp.elf: CFLAGS+=-Wl,-u,vfprintf -lprintf_flt -lm
temp.elf: temp.o buffer.o uart.o job.o timer.o bmp_085.o i2c.o
	$(CC) $(CFLAGS) $^ -o temp.elf

.PHONY: flash_%
flash_%: %.hex
	avrdude -c buspirate -P $(BUSPIRATE) -p atmega328p -V -U flash:w:$*.hex

.PHONY: clean
clean:
	rm -f *.elf *.hex *.o
