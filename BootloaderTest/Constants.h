/*
Copyright 2017 3devo (www.3devo.eu)

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdint.h>

#pragma once

#define TEST_SUBJECT_ATTINY
//#define TEST_SUBJECT_STM32

struct Status {
  enum {
    COMMAND_OK            = 0x00,
    COMMAND_FAILED        = 0x01,
    COMMAND_NOT_SUPPORTED = 0x02,
    INVALID_TRANSFER      = 0x03,
    INVALID_CRC           = 0x04,
    INVALID_ARGUMENTS     = 0x05,
  };
};

struct Commands {
  enum {
    GET_PROTOCOL_VERSION  = 0x00,
    SET_ADDRESS           = 0x01,
    POWER_UP_DISPLAY      = 0x02,
    GET_HARDWARE_INFO     = 0x03,
    GET_SERIAL_NUMBER     = 0x04,
    START_APPLICATION     = 0x05,
    WRITE_FLASH           = 0x06,
    FINALIZE_FLASH        = 0x07,
    READ_FLASH            = 0x08,
    GET_HARDWARE_REVISION = 0x09,
    END_OF_COMMANDS
  };
};

struct GeneralCallCommands {
  static const uint8_t RESET = 0x06;
  static const uint8_t RESET_ADDRESS = 0x04;
};

static const uint8_t GENERAL_CALL_ADDRESS = 0;
static const uint8_t FIRST_ADDRESS = 8;
static const uint8_t LAST_ADDRESS = 15;
static const uint8_t MAX_MSG_LEN = 32;

// Expected values
static const uint16_t PROTOCOL_VERSION = 0x0101;
#if defined(TEST_SUBJECT_ATTINY)
static const uint8_t HARDWARE_TYPE = 0x01;
static const uint8_t HARDWARE_COMPATIBLE_REVISION = 0x01;
static const uint8_t HARDWARE_REVISION = 0x14;
static const uint16_t AVAILABLE_FLASH_SIZE = 8192-2048-2;
static const bool SUPPORTS_DISPLAY = true;
#elif defined(TEST_SUBJECT_STM32)
static const uint8_t HARDWARE_TYPE = 0x02;
static const uint8_t HARDWARE_COMPATIBLE_REVISION = 0x10;
static const uint8_t HARDWARE_REVISION = 0x10;
static const uint16_t AVAILABLE_FLASH_SIZE = 65536-4096;
static const bool SUPPORTS_DISPLAY = false;
#endif
static const uint8_t BOOTLOADER_VERSION = 0x02;

// As returned by POWER_UP_DISPLAY
static const uint8_t DISPLAY_CONTROLLER_TYPE = 0x01;
static const uint8_t DISPLAY_I2C_ADDRESS = 0x3C;
