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
CPPSRC        += $(ARCH)/SelfProgram.cpp $(ARCH)/uart.cpp
CPPSRC        += $(ARCH)/$(BUS).cpp
OBJ            = $(CPPSRC:.cpp=.o)
ifeq ($(ARCH),attiny)
LDSCRIPT       = linker-script.x
MCU            = attiny841
FLASH_WRITE_SIZE    = SPM_PAGESIZE # Defined by avr-libc
FLASH_ERASE_SIZE    = 64
FLASH_APP_OFFSET    = 0
# Size of the bootloader area. Must be a multiple of the erase size
BL_SIZE        = 2048
else ifeq ($(ARCH),stm32)
OPENCM3_DIR         = libopencm3
DEVICE              = stm32g031k8t6
FLASH_WRITE_SIZE    = 8
FLASH_ERASE_SIZE    = 2048
# Bootloader is at the start of flash, so write app after it
FLASH_APP_OFFSET    = $(BL_SIZE)
FLASH_SIZE          = 65536
# Size of the bootloader area. Must be a multiple of the erase size
BL_SIZE             = 4096
endif

VERSION_SIZE   = 4
BL_VERSION     = 3

CXXFLAGS       =
CXXFLAGS      += -g3 -std=gnu++11
CXXFLAGS      += -Wall -Wextra
CXXFLAGS      += -Os -fpack-struct -fshort-enums
CXXFLAGS      += -flto -fno-fat-lto-objects
# I would think these are not required with -flto, but adding these
# removes a lot of unused functions that lto apparently leaves...
CXXFLAGS      += -ffunction-sections -fdata-sections -Wl,--gc-sections
CXXFLAGS      += -fno-exceptions

CXXFLAGS      += -DVERSION_SIZE=$(VERSION_SIZE)
CXXFLAGS      += -DFLASH_ERASE_SIZE=$(FLASH_ERASE_SIZE)
CXXFLAGS      += -DFLASH_WRITE_SIZE=$(FLASH_WRITE_SIZE)
CXXFLAGS      += -DFLASH_APP_OFFSET=$(FLASH_APP_OFFSET)
CXXFLAGS      += -DPROTOCOL_VERSION=$(PROTOCOL_VERSION) -DBOARD_TYPE_$(BOARD_TYPE)
CXXFLAGS      += -DHARDWARE_REVISION=$(CURRENT_HW_REVISION) -DHARDWARE_COMPATIBLE_REVISION=$(COMPATIBLE_HW_REVISION)
CXXFLAGS      += -DBL_VERSION=$(BL_VERSION)
ifeq ($(BUS),TwoWire)
CXXFLAGS      += -DUSE_I2C
else ifeq ($(BUS),Rs485)
CXXFLAGS      += -DUSE_RS485
endif

LDFLAGS        =
# Use a custom linker script
LDFLAGS       += -T $(LDSCRIPT)

ifeq ($(ARCH),attiny)
PREFIX         = avr-
SIZE_FORMAT    = avr

LDFLAGS       += -mmcu=$(MCU)

CXXFLAGS      += -mmcu=$(MCU) -DF_CPU=8000000UL

# Pass sizes to the script for positioning
LDFLAGS       += -Wl,--defsym=BL_SIZE=$(BL_SIZE)
LDFLAGS       += -Wl,--defsym=VERSION_SIZE=$(VERSION_SIZE)
# Pass ERASE_SIZE to the script to verify alignment
LDFLAGS       += -Wl,--defsym=FLASH_ERASE_SIZE=$(FLASH_ERASE_SIZE)
else ifeq ($(ARCH),stm32)
PREFIX         = arm-none-eabi-
SIZE_FORMAT    = berkely

CXXFLAGS      += -DSTM32
CXXFLAGS      += -DAPPLICATION_SIZE="($(FLASH_SIZE)-$(FLASH_APP_OFFSET))"
LDFLAGS       += -nostartfiles
LDFLAGS       += -specs=nano.specs
LDFLAGS       += -specs=nosys.specs
# TODO: Position VERSION constant
endif

CC             = $(PREFIX)gcc
OBJCOPY        = $(PREFIX)objcopy
OBJDUMP        = $(PREFIX)objdump
SIZE           = $(PREFIX)size

ifdef OPENCM3_DIR
include $(OPENCM3_DIR)/mk/genlink-config.mk
ifeq ($(LIBNAME),)
$(error libopencm3 library not found, compile it first with "make -C libopencm3 lib/stm32/g0 CFLAGS='-flto -fno-fat-lto-objects'")
endif
# These are generated by genlink-config (along with LDSCRIPT)
CXXFLAGS      += $(CPPFLAGS)
CXXFLAGS      += $(ARCH_FLAGS)
endif

ifdef CURRENT_HW_REVISION
  CURRENT_HW_REVISION_MAJOR=$(shell echo $$(($(CURRENT_HW_REVISION) / 0x10)))
  CURRENT_HW_REVISION_MINOR=$(shell echo $$(($(CURRENT_HW_REVISION) % 0x10)))

  FILE_NAME=bootloader-v$(BL_VERSION)-$(BOARD_TYPE)-$(CURRENT_HW_REVISION_MAJOR).$(CURRENT_HW_REVISION_MINOR)
endif

# Make sure that .o files are deleted after building, so we can build for multiple
# hw revisions without needing an explicit clean in between.
.INTERMEDIATE: $(OBJ)

default:
	$(MAKE) all ARCH=attiny BUS=TwoWire BOARD_TYPE=interfaceboard CURRENT_HW_REVISION=0x13 COMPATIBLE_HW_REVISION=0x01
	$(MAKE) all ARCH=attiny BUS=TwoWire BOARD_TYPE=interfaceboard CURRENT_HW_REVISION=0x14 COMPATIBLE_HW_REVISION=0x01
	$(MAKE) all ARCH=stm32 BUS=Rs485 BOARD_TYPE=gphopper CURRENT_HW_REVISION=0x10 COMPATIBLE_HW_REVISION=0x10

all: hex fuses size checksize

hex: $(FILE_NAME).hex

fuses:
ifdef ATTINY
	@if $(OBJDUMP) -s -j .fuse 2> /dev/null $(FILE_NAME).elf > /dev/null; then \
		$(OBJDUMP) -s -j .fuse $(FILE_NAME).elf; \
		echo "        ^^     Low"; \
		echo "          ^^   High"; \
		echo "            ^^ Extended"; \
	fi
endif

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

%.bin: %.elf
	$(OBJCOPY) -j .text -j '.text.*' -j .data -O binary $< $@

checksize: $(FILE_NAME).bin
	@if [ $$(stat -c '%s' $<) -gt $(BL_SIZE) ]; then \
		echo "Compiled size too big, maybe adjust BL_SIZE in Makefile?"; \
		false; \
	fi

# Rule to generate linker script
ifdef OPENCM3_DIR
include $(OPENCM3_DIR)/mk/genlink-rules.mk
endif

.PHONY: all lst hex clean fuses size

# pull in dependency info for *existing* .o files
-include $(OBJ:.o=.d)
