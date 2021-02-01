# Makefile to compile bootloader-attiny
#
# Copyright 2017 Matthijs Kooijman <matthijs@stdin.nl>
#
# Permission is hereby granted, free of charge, to anyone obtaining a
# copy of this document to do whatever they want with them without any
# restriction, including, but not limited to, copying, modification and
# redistribution.
#
# NO WARRANTY OF ANY KIND IS PROVIDED.
#
# To compile, just make sure that avr-gcc and friends are in your path
# and type "make".
PROTOCOL_VERSION = 0x0101

CPPSRC         = $(wildcard *.cpp)
OBJ            = $(CPPSRC:.cpp=.o)
LDSCRIPT       = linker-script.x
MCU            = attiny841
FLASH_WRITE_SIZE    = SPM_PAGESIZE # Defined by avr-libc
FLASH_ERASE_SIZE    = 64

# Size of the bootloader area. Must be a multiple of the erase size
BL_SIZE        = 2048
VERSION_SIZE   = 4
BL_VERSION     = 2

CXXFLAGS       =
CXXFLAGS      += -g3 -std=gnu++11
CXXFLAGS      += -Wall -Wextra
CXXFLAGS      += -Os -fpack-struct -fshort-enums
CXXFLAGS      += -flto -fno-fat-lto-objects

CXXFLAGS      += -DVERSION_SIZE=$(VERSION_SIZE)
CXXFLAGS      += -DFLASH_ERASE_SIZE=$(FLASH_ERASE_SIZE)
CXXFLAGS      += -DFLASH_WRITE_SIZE=$(FLASH_WRITE_SIZE)
CXXFLAGS      += -DFLASH_APP_OFFSET=$(FLASH_APP_OFFSET)
CXXFLAGS      += -DPROTOCOL_VERSION=$(PROTOCOL_VERSION) -DBOARD_TYPE_$(BOARD_TYPE)
CXXFLAGS      += -DHARDWARE_REVISION=$(CURRENT_HW_REVISION) -DHARDWARE_COMPATIBLE_REVISION=$(COMPATIBLE_HW_REVISION)
CXXFLAGS      += -DBL_VERSION=$(BL_VERSION)

LDFLAGS        =
# Use a custom linker script
LDFLAGS       += -T $(LDSCRIPT)

PREFIX         = avr-
SIZE_FORMAT    = avr

LDFLAGS       += -mmcu=$(MCU)

CXXFLAGS      += -mmcu=$(MCU) -DF_CPU=8000000UL

# Pass sizes to the script for positioning
LDFLAGS       += -Wl,--defsym=BL_SIZE=$(BL_SIZE)
LDFLAGS       += -Wl,--defsym=VERSION_SIZE=$(VERSION_SIZE)
# Pass ERASE_SIZE to the script to verify alignment
LDFLAGS       += -Wl,--defsym=FLASH_ERASE_SIZE=$(FLASH_ERASE_SIZE)

CC             = $(PREFIX)gcc
OBJCOPY        = $(PREFIX)objcopy
OBJDUMP        = $(PREFIX)objdump
SIZE           = $(PREFIX)size


ifdef CURRENT_HW_REVISION
  CURRENT_HW_REVISION_MAJOR=$(shell echo $$(($(CURRENT_HW_REVISION) / 0x10)))
  CURRENT_HW_REVISION_MINOR=$(shell echo $$(($(CURRENT_HW_REVISION) % 0x10)))

  FILE_NAME=attinybootloader-v$(BL_VERSION)-$(BOARD_TYPE)-$(CURRENT_HW_REVISION_MAJOR).$(CURRENT_HW_REVISION_MINOR)
endif

# Make sure that .o files are deleted after building, so we can build for multiple
# hw revisions without needing an explicit clean in between.
.INTERMEDIATE: $(OBJ)

default:
	$(MAKE) all BOARD_TYPE=interfaceboard CURRENT_HW_REVISION=0x13 COMPATIBLE_HW_REVISION=0x01
	$(MAKE) all BOARD_TYPE=interfaceboard CURRENT_HW_REVISION=0x14 COMPATIBLE_HW_REVISION=0x01

all: hex fuses size

hex: $(FILE_NAME).hex

fuses:
	@if $(OBJDUMP) -s -j .fuse 2> /dev/null $(FILE_NAME).elf > /dev/null; then \
		$(OBJDUMP) -s -j .fuse $(FILE_NAME).elf; \
		echo "        ^^     Low"; \
		echo "          ^^   High"; \
		echo "            ^^ Extended"; \
	fi

size:
	$(SIZE) --format=$(SIZE_FORMAT) $(FILE_NAME).elf

clean:
	rm -rf $(OBJ) $(OBJ:.o=.d) *.elf *.hex *.lst *.map

$(FILE_NAME).elf: $(OBJ) $(LDSCRIPT) $(LIBDEPS)
	$(CC) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

%.o: %.cpp
	$(CC) $(CXXFLAGS) -MMD -MP -c -o $@ $<

%.lst: %.elf
	$(OBJDUMP) -h -S $< > $@

%.hex: %.elf
	$(OBJCOPY) -j .text -j '.text.*' -j .data -O ihex $< $@

.PHONY: all lst hex clean fuses size

# pull in dependency info for *existing* .o files
-include $(OBJ:.o=.d)
