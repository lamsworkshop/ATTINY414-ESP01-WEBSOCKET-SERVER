SRC = $(shell ls -r *.c)
OBJ = $(SRC:.c=.o)
TARGET = out
MCU = attiny414

CFLAGS = -mmcu=$(MCU) -Os -fno-unroll-loops -fshort-enums
LDFLAGS = -mmcu=$(MCU) -Wl,-Map=$(TARGET).map

all: $(TARGET).hex

%.hex: %.elf
	avr-objcopy -O ihex -R .eeprom -R .fuse -R .lock -R .signature $< $@
#	avr-objcopy -j .eeprom --set-section-flags=.eeprom="alloc,load" --change-section-lma .eeprom=0 -O ihex out.elf eeprom.hex

%.elf: $(OBJ)
	avr-gcc $(LDFLAGS) -o $@ $^
	avr-size $@

%.o: %.c
	avr-gcc $(CFLAGS) -o $@ -c $<

clean:
	rm -rf *.o $(TARGET).*

flash:
	pymcuprog erase -m flash -d $(MCU) -t uart -u /dev/ttyUSB0
	pymcuprog write -f $(TARGET).hex -d $(MCU) -t uart -u /dev/ttyUSB0

test:
	minicom -D /dev/ttyUSB0 -b 115200

reset:
	pymcuprog reset -d $(MCU) -t uart -u /dev/ttyUSB0

# sudo apt install gcc-avr binutils-avr avr-libc -y
# pip install pymcuprog --break-system-packages
# avr-objcopy -O ihex -R .eeprom -R .fuse -R .lock -R .signature $< $@
# avr-objcopy -j .eeprom --set-section-flags=.eeprom="alloc,load" --change-section-lma .eeprom=0 -O ihex firmware.elf eeprom.hex
