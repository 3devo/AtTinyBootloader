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

#include <SoftWire.h>
#include <ArduinoUnit.h>

#include "Constants.h"
#include "Util.h"
#include "PrintingSoftWire.h"

PrintingSoftWire bus(SDA, SCL);

struct Cfg {
  // A single test is done using these values, then they are changed
  // into various variations

  // Address to use after general call reset. If 0, no reset happens.
  uint8_t resetAddr = FIRST_ADDRESS;
  // Address to use for SET_I2C_ADDRESS. If 0, no set happens.
  uint8_t setAddr = 0;
  // Address to use for all tests. Overwritten by general call reset and
  // SET_I2C_ADDRESS
  uint8_t curAddr = 0;

  bool repStartAfterWrite = false;
  bool repStartAfterRead = false;
  bool skipWrite = false;

  // These are not changed, so set them here
  bool printRawData = false;
  bool displayAttached = true;

  enum {
    FLASH_ONCE, // Flash random data in the first run
    FLASH_ONE_BYTE_PER_RUN, // Additionallly change one byte on each run
    FLASH_EVERY_TIME, // Change every byte on every run. This will wear out the flash!
  };
  uint8_t flashWear = FLASH_ONCE;

  // When non-zero, only write this much bytes to flash on each test, to
  // speed up testing
  uint16_t maxWriteSize = 0;
} cfg;


bool write_command(uint8_t cmd, uint8_t *dataout, uint8_t len, uint8_t crc_xor = 0) {
  assertLessOrEqual(len + 2, MAX_MSG_LEN, "", false);
  assertAck(bus.startWrite(cfg.curAddr),"", false);
  assertAck(bus.llWrite(cmd), "", false);
  for (uint8_t i = 0; i < len; ++i)
    assertAck(bus.llWrite(dataout[i]), "", false);

  uint8_t crc = Crc().update(cmd).update(dataout, len).get();
  assertAck(bus.llWrite(crc ^ crc_xor), "", false);

  if (!cfg.repStartAfterWrite)
    bus.stop();
  return true;
}

bool read_status(uint8_t *status, uint8_t *datain, uint8_t okLen, uint8_t failLen, bool skip_start = false) {
  assertLessOrEqual(okLen + 3, MAX_MSG_LEN, "", false);
  assertLessOrEqual(failLen + 3, MAX_MSG_LEN, "", false);
  if (!skip_start) {
    assertAck(bus.startRead(cfg.curAddr), "", false);
  }
  assertTrue(status != nullptr, "", false);
  assertAck(bus.readThenAck(*status), "", false);

  uint8_t expectedLen;
  if (*status == Status::COMMAND_OK)
    expectedLen = okLen;
  else if (*status == Status::COMMAND_FAILED)
    expectedLen = failLen;
  else // All other errors have no data
    expectedLen = 0;

  uint8_t len;
  assertAck(bus.readThenAck(len), "", false);
  assertEqual(len, expectedLen, "", false);

  for (uint8_t i = 0; i < expectedLen; ++i)
    assertAck(bus.readThenAck(datain[i]), "", false);

  uint8_t crc;
  assertAck(bus.readThenNack(crc), "", false);

  uint8_t expectedCrc = Crc().update(*status).update(len).update(datain, expectedLen).get();
  assertEqual(crc, expectedCrc, "", false);
  if (!cfg.repStartAfterRead)
    bus.stop();
  return true;
}

bool run_transaction(uint8_t cmd, uint8_t *dataout, uint8_t len, uint8_t *status, uint8_t *datain = nullptr, uint8_t okLen = 0, uint8_t failLen = 0) {
  assertTrue(write_command(cmd, dataout, len), "", false);
  assertTrue(read_status(status, datain, okLen, failLen), "", false);
  return true;
}

bool run_transaction_ok(uint8_t cmd, uint8_t *dataout = nullptr, uint8_t len = 0, uint8_t *datain = nullptr, uint8_t okLen = 0, uint8_t failLen = 0) {
  uint8_t status;
  assertTrue(run_transaction(cmd, dataout, len, &status, datain, okLen, failLen), "", false);
  assertOk(status, "", false);
  return true;
}

test(010_general_call_reset) {
  if (!cfg.resetAddr) {
    skip();
    return;
  }

  // Both general call commands reset the i2c address, so test both of
  // them randomly (we can't tell the difference from the outside
  // anyway).
  uint8_t cmd = random(2) ? GeneralCallCommands::RESET : GeneralCallCommands::RESET_ADDRESS;
  assertAck(bus.startWrite(GENERAL_CALL_ADDRESS));
  assertAck(bus.llWrite(cmd));
  bus.stop();
  uint8_t oldAddr = cfg.curAddr;
  cfg.curAddr = cfg.resetAddr;
  delay(100);

  if (oldAddr && (oldAddr < FIRST_ADDRESS || oldAddr > LAST_ADDRESS)) {
    // If the old address is set, but outside the default range, check
    // that it no longer responds
    assertEqual(bus.startWrite(oldAddr), SoftWire::nack);
    bus.stop();
  }

  // Check for a response on the full address range
  for (uint8_t i = FIRST_ADDRESS; i < LAST_ADDRESS; ++i) {
    assertAck(bus.startWrite(i));
    bus.stop();
  }

  // After a reset, the display should be off
  if (cfg.displayAttached && cmd == GeneralCallCommands::RESET) {
    SoftWire::result_t res = bus.startWrite(DISPLAY_I2C_ADDRESS);
    bus.stop();

    // If the display is still on, wait a bit longer. When the attiny is
    // reset, the 3V3 regulator is switched off, but it takes a while
    // for its output capacitor to discharge, so the display controller
    // might stay on for a while (and since its reset is pulled up to
    // 3V3, it stays out of reset as well).
    if (res == SoftWire::ack) {
      delay(500);
      res = bus.startWrite(DISPLAY_I2C_ADDRESS);
      bus.stop();
    }
    assertEqual(res, SoftWire::nack);
  }
}

test(020_ack) {
  assertAck(bus.startWrite(cfg.curAddr));
  bus.stop();
}


test(030_protocol_version) {
  uint8_t data[2];
  assertTrue(run_transaction_ok(Commands::GET_PROTOCOL_VERSION, nullptr, 0, data, sizeof(data)));

  uint16_t version = data[0] << 8 | data[1];
  assertEqual(version, PROTOCOL_VERSION);
}

test(040_not_set_i2c_address) {
  if (!cfg.setAddr) {
    skip();
    return;
  }
  // Test that the set i2c address command does not work when a wrong
  // hardware type is given
  uint8_t type = random(HARDWARE_TYPE + 1, 256);
  uint8_t data[2] = { cfg.setAddr, type };
  write_command(Commands::SET_I2C_ADDRESS, data, sizeof(data));

  // The command should be ignored, so a read command should not be acked
  assertEqual(bus.startRead(cfg.curAddr), SoftWire::nack);
  bus.stop();

  // And neither on the new address
  assertEqual(bus.startRead(cfg.setAddr), SoftWire::nack);
  bus.stop();

  // But it should still ack writes on the old address
  assertAck(bus.startWrite(cfg.curAddr));
  bus.stop();
}

test(050_set_i2c_address) {
  if (!cfg.setAddr) {
    skip();
    return;
  }

  // Randomly use either the wildcard, or specific selector
  uint8_t type = random(2) ? 0x00 : HARDWARE_TYPE;
  uint8_t data[2] = { cfg.setAddr, type };
  write_command(Commands::SET_I2C_ADDRESS, data, sizeof(data));

  // The response might be available at the old or new address, try the
  // old address first, but it's ok if the old address nacks.
  SoftWire::result_t res = bus.startRead(cfg.curAddr);
  assertTrue(res == SoftWire::ack || res == SoftWire::nack);

  // Update the address, use this one for now
  uint8_t oldAddr = cfg.curAddr;
  cfg.curAddr = cfg.setAddr;
  // If an ack is received on the old address, continue reading the
  // reponse there (without sending another start). If not, read the
  // response from the new address.
  bool skip_start = (res == SoftWire::ack);
  uint8_t status;
  assertTrue(read_status(&status, NULL, 0, 0, skip_start));
  assertOk(status);

  // If the address actually changed, check that no response is received
  // from the old address
  if (oldAddr != cfg.setAddr) {
    assertEqual(bus.startRead(oldAddr), SoftWire::nack);
    bus.stop();
    assertEqual(bus.startWrite(oldAddr), SoftWire::nack);
    bus.stop();
  }

  for (uint8_t i = FIRST_ADDRESS; i < LAST_ADDRESS; ++i) {
    // Check that it no longer response to the default range, except to
    // the new address if it happens to be in that range.
    if (i == cfg.curAddr)
      assertAck(bus.startWrite(i));
    else
      assertEqual(bus.startWrite(i), SoftWire::nack);
    bus.stop();
  }

  // Check that commands work on the new address
  uint8_t protodata[2];
  assertTrue(run_transaction_ok(Commands::GET_PROTOCOL_VERSION, nullptr, 0, protodata, sizeof(protodata)));
}


test(060_reread_protocol_version) {
  uint8_t data[2];
  assertTrue(run_transaction_ok(Commands::GET_PROTOCOL_VERSION, nullptr, 0, data, sizeof(data)));

  uint16_t version = data[0] << 8 | data[1];
  assertEqual(version, PROTOCOL_VERSION);

  uint8_t status;
  assertTrue(read_status(&status, data, sizeof(data), 0));
  assertOk(status);
  version = data[0] << 8 | data[1];
  assertEqual(version, PROTOCOL_VERSION);
}

test(065_short_long_read) {
  uint8_t status, len, dummy;
  uint8_t data[2];

  // Send a command
  assertTrue(write_command(Commands::GET_PROTOCOL_VERSION, nullptr, 0));

  // Do a short read, just enough to read the length byte
  assertAck(bus.startRead(cfg.curAddr));
  assertAck(bus.readThenAck(status));
  assertOk(status);
  assertAck(bus.readThenNack(len));
  assertEqual(len, sizeof(data));
  if (!cfg.repStartAfterRead)
    bus.stop();

  // Check that we can still read a valid reply after that
  assertTrue(read_status(&status, data, sizeof(data), 0));
  assertOk(status);
  uint16_t version = data[0] << 8 | data[1];
  assertEqual(version, PROTOCOL_VERSION);

  // Do a long read, way past the CRC
  assertAck(bus.startRead(cfg.curAddr));
  assertAck(bus.readThenAck(status));
  assertOk(status);
  assertAck(bus.readThenAck(len));
  assertEqual(len, sizeof(data));
  for (uint8_t i = 0; i < 10; ++i) {
    assertAck(bus.readThenAck(dummy));
  }
  assertAck(bus.readThenNack(dummy));
  if (!cfg.repStartAfterRead)
    bus.stop();

  // Check that we can still read a valid reply after that
  assertTrue(read_status(&status, data, sizeof(data), 0));
  assertOk(status);
  version = data[0] << 8 | data[1];
  assertEqual(version, PROTOCOL_VERSION);
}

test(070_get_hardware_info) {
  uint8_t data[5];
  assertTrue(run_transaction_ok(Commands::GET_HARDWARE_INFO, nullptr, 0, data, sizeof(data)));

  assertEqual(data[0], HARDWARE_TYPE);
  assertEqual(data[1], HARDWARE_COMPATIBLE_REVISION);
  assertEqual(data[2], BOOTLOADER_VERSION);
  uint16_t flash_size = data[3] << 8 | data[4];
  assertEqual(flash_size, AVAILABLE_FLASH_SIZE);
}

test(071_get_hardware_revision) {
  if (PROTOCOL_VERSION < 0x0101)
    skip();

  uint8_t data[1];
  assertTrue(run_transaction_ok(Commands::GET_HARDWARE_REVISION, nullptr, 0, data, sizeof(data)));

  assertEqual(data[0], HARDWARE_REVISION);
}

test(075_get_serial_number) {
  uint8_t data[9];
  assertTrue(run_transaction_ok(Commands::GET_SERIAL_NUMBER, nullptr, 0, data, sizeof(data)));

  auto on_failure = [&data]() {
    Serial.print("serial = ");
    for (uint8_t i = 0; i < sizeof(data); ++i) {
      Serial.print(" 0x"); Serial.print(data[i], HEX);
    }
    Serial.println();
  };

  // This is not required by the protocol, but the current
  // implementation returns the unique data from the attiny chip, which
  // is a lot number, wafer number and x/y position. The lot number
  // seems to be an ASCII uppercase letter followed by 5 ASCII numbers,
  // so check that.
  assertMoreOrEqual(data[0], 'A', "", on_failure());
  assertLessOrEqual(data[0], 'Z', "", on_failure());
  for (uint8_t i = 1; i < 6; ++i) {
    assertMoreOrEqual(data[i], '0', "", on_failure());
    assertLessOrEqual(data[i], '9', "", on_failure());
  }
}

test(080_crc_error) {
  uint8_t status;
  uint8_t crc_xor = random(1, 256);
  assertTrue(write_command(Commands::GET_HARDWARE_INFO, nullptr, 0, crc_xor));
  // This expects a CRC error, so no data
  assertTrue(read_status(&status, nullptr, 0, 0));
  assertEqual(status, Status::INVALID_CRC);
}

test(100_command_not_supported) {
  uint8_t cmd = Commands::END_OF_COMMANDS;
  auto on_failure = [&cmd]() { Serial.print("cmd: "); Serial.println(cmd); };
  while (cmd != 0) {
    uint8_t status;
    assertTrue(run_transaction(cmd, nullptr, 0, &status), "", on_failure());
    assertEqual(status, Status::COMMAND_NOT_SUPPORTED, "", on_failure());
    ++cmd;
  }
}

test(110_power_up_display) {
  uint8_t data[1];
  assertTrue(run_transaction_ok(Commands::POWER_UP_DISPLAY, nullptr, 0, data, sizeof(data)));
  assertEqual(data[0], DISPLAY_CONTROLLER_TYPE);

  if (cfg.displayAttached) {
    // See if the display responds to its address
    assertAck(bus.startWrite(DISPLAY_I2C_ADDRESS));
    bus.stop();
  }
}

bool verify_flash(uint8_t *data, uint16_t len, uint8_t readlen) {
  uint16_t offset = 0;
  uint8_t i = 0;
  auto on_failure = [&i, &offset]() {
    Serial.print("offset = 0x");
    Serial.println(offset, HEX);
    Serial.print("i = 0x");
    Serial.println(i, HEX);
    return false;
  };

  uint8_t datain[readlen];
  while (offset < len) {
    uint8_t nextlen = min(readlen, len - offset);
    uint8_t dataout[3] = {(uint8_t)(offset >> 8), (uint8_t)offset, nextlen};
    assertTrue(run_transaction_ok(Commands::READ_FLASH, dataout, sizeof(dataout), datain, nextlen), "", on_failure());
    for (i = 0; i < nextlen; ++i)
      assertEqual(datain[i], data[offset + i], "", on_failure());
    offset += nextlen;
  }
  return true;
}

bool write_flash_cmd(uint16_t address, uint8_t *data, uint8_t len, uint8_t *status, uint8_t *reason) {
  uint8_t dataout[len + 2];

  dataout[0] = address >> 8;
  dataout[1] = address;
  memcpy(dataout + 2, data, len);
  assertTrue(run_transaction(Commands::WRITE_FLASH, dataout, len + 2, status, reason, 0, 1), "", false);
  return true;
}

bool write_flash(uint8_t *data, uint16_t len, uint8_t writelen, uint8_t *erase_count) {
  uint16_t offset = 0;
  auto on_failure = [&offset]() {
    Serial.print("offset = 0x");
    Serial.println(offset, HEX);
    return false;
  };

  while (offset < len) {
    uint8_t nextlen = min(writelen, len - offset);
    uint8_t status, reason;
    assertTrue(write_flash_cmd(offset, data + offset, nextlen, &status, &reason), "", on_failure());
    assertOk(status, "", on_failure());
    offset += nextlen;
  }
  assertTrue(run_transaction_ok(Commands::FINALIZE_FLASH, nullptr, 0, erase_count, 1, 1), "", on_failure());
  return true;
}

bool write_and_verify_flash(uint8_t *data, uint16_t len, uint8_t writelen, uint8_t readlen, uint8_t *erase_count) {
  auto on_failure = [readlen, writelen]() {
    Serial.print("writelen = ");
    Serial.println(writelen);
    Serial.print("readlen = ");
    Serial.println(readlen);
    return false;
  };
#ifdef TIME_WRITE
  unsigned long start = millis();
#endif
  assertTrue(write_flash(data, len, writelen, erase_count), "", on_failure());
#ifdef TIME_WRITE
  unsigned long now = millis();
  Serial.print("Write took ");
  Serial.print(now - start);
  Serial.println("ms");
#endif

#ifdef TIME_WRITE
  start = millis();
#endif
  assertTrue(verify_flash(data, len, readlen), "", on_failure());
#ifdef TIME_WRITE
  now = millis();
  Serial.print("Verify took ");
  Serial.print(now - start);
  Serial.println("ms");
#endif
  return true;
}

// Write random data to flash and read it back, using various message
// sizes
test(120_write_flash) {
  if (cfg.skipWrite) {
    skip();
    return;
  }
  uint8_t hwinfo[5];
  assertTrue(run_transaction_ok(Commands::GET_HARDWARE_INFO, nullptr, 0, hwinfo, sizeof(hwinfo)));
  uint16_t flash_size = hwinfo[3] << 8 | hwinfo[4];

  if (cfg.maxWriteSize && cfg.maxWriteSize < flash_size)
    flash_size = cfg.maxWriteSize;

  static uint8_t *data = NULL;
  static size_t data_len = 0;
  // The first time, we always generate fully random data, flash it, and
  // then change one byte. Since the previous testing session will likely have
  // generated the same contents (since the random seed is not random),
  // this ensures that at leaste one byte will really be different.
  bool full_random = (data == NULL) || (cfg.flashWear == Cfg::FLASH_EVERY_TIME);
  bool change_one = (data == NULL) || (cfg.flashWear == Cfg::FLASH_ONE_BYTE_PER_RUN);
  if (data_len  == 0) {
    data = (uint8_t*)malloc(flash_size);
    assertTrue(data != nullptr);
    data_len = flash_size;
  } else {
    assertEqual(data_len, flash_size);
  }

  assertMoreOrEqual(flash_size, 2U);
  if (full_random) {
    // The bootloader requires a RCALL or RJMP instruction at address 0,
    // so give it one
    data[0] = 0x00;
    data[1] = 0xC0;
    for (uint16_t i = 2; i < flash_size; ++i)
      data[i] = random();
  }

  // Write flash using various messages sizes
  uint8_t erase_count;
  assertTrue(write_and_verify_flash(data, flash_size, 1, 1, &erase_count));
  // If we didn't randomize, nothing should have changed (but if we did
  // randomize, we can't be sure that something changed).
  if (!full_random)
    assertEqual(erase_count, 0);

  if (change_one) {
    data[random(2, flash_size)] += random(256);
  }
  assertTrue(write_and_verify_flash(data, flash_size, 7, 16, &erase_count));
  assertEqual(erase_count, change_one ? 1 : 0);
  assertTrue(write_and_verify_flash(data, flash_size, 16, 3, &erase_count));
  assertEqual(erase_count, 0);
  // Max write and read size
  uint8_t writelen = MAX_MSG_LEN - 4; // cmd, 2xaddr, crc
  uint8_t readlen = MAX_MSG_LEN - 3; // status, len, 2xaddr
  assertTrue(write_and_verify_flash(data, flash_size, writelen, readlen, &erase_count));
  assertEqual(erase_count, 0);
  // Random sizes
  writelen = random(1, writelen);
  readlen = random(1, readlen);
  assertTrue(write_and_verify_flash(data, flash_size, writelen, readlen, &erase_count));
  assertEqual(erase_count, 0);
}

test(130_invalid_writes) {
  uint8_t data[16];
  uint8_t status, reason;

  // Read the current flash contents to prevent actually writing to flash
  uint8_t dataout[3] = {0, 0, sizeof(data)};
  assertTrue(run_transaction_ok(Commands::READ_FLASH, dataout, sizeof(dataout), data, sizeof(data)));

  // Start at 0 and skip a few bytes
  assertTrue(write_flash_cmd(0, data, 13, &status, &reason));
  assertOk(status);
  assertTrue(write_flash_cmd(17, data, 11, &status, &reason));
  assertEqual(status, Status::INVALID_ARGUMENTS);

  // Restart at 0 and then skip up to the next erase page
  assertTrue(write_flash_cmd(0, data, 13, &status, &reason));
  assertOk(status);
  assertTrue(write_flash_cmd(64, data, 11, &status, &reason));
  assertEqual(status, Status::INVALID_ARGUMENTS);
  // Packet should be ignored, so a subsequent write aligned with the
  // first write should work.
  assertTrue(write_flash_cmd(13, data, 13, &status, &reason));
  assertOk(status);

  // If address 0/1 does not contain an RJMP/RCALL, it should return
  // COMMAND_FAILED and reason=2. These are not required by the
  // protocol, they just test the bootloader implementation
  data[0] = 0x00;
  data[1] = 0x00;
  assertTrue(write_flash_cmd(0, data, 16, &status, &reason));
  assertOk(status);
  assertTrue(write_flash_cmd(16, data, 16, &status, &reason));
  assertOk(status);
  assertTrue(write_flash_cmd(32, data, 16, &status, &reason));
  assertOk(status);
  assertTrue(write_flash_cmd(48, data, 16, &status, &reason));
  assertEqual(status, Status::COMMAND_FAILED);
  assertEqual(reason, 2);

  // FINALIZE_FLASH should also fail with an invalid address 0/1
  assertTrue(write_flash_cmd(0, data, 2, &status, &reason));
  assertOk(status);
  assertTrue(run_transaction(Commands::FINALIZE_FLASH, nullptr, 0, &status, &reason, 1, 1));
  assertEqual(status, Status::COMMAND_FAILED);
  assertEqual(reason, 2);
}

void runTests() {
  static uint32_t count = 0;
  long seed = random();
  randomSeed(seed);

  Serial.println("****************************");
  Serial.print("Test #");
  Serial.println(++count);
  Serial.print("Random seed: ");
  Serial.println(seed);
  Serial.print("current addr = 0x");
  Serial.println(cfg.curAddr, HEX);
  Serial.print("reset addr = 0x");
  Serial.println(cfg.resetAddr, HEX);
  if (cfg.setAddr) {
    Serial.print("setting addr = 0x");
    Serial.println(cfg.setAddr, HEX);
  }
  if (cfg.repStartAfterWrite)
    Serial.println("using repstart after write");
  if (cfg.repStartAfterRead)
    Serial.println("using repstart after read");

  bus.print = cfg.printRawData;

  Test::resetDoneTests();

  // Run all tests to completion
  Test::runUntilDone();

  if (Test::getCurrentFailed()) {
    Serial.println("FAILED");
    while(true) /* nothing */;
  }

  // Randomly change these values on every run
  cfg.repStartAfterWrite = random(2);
  cfg.repStartAfterRead = random(2);
}

void runFixedTests() {
  // Do one run with the default settings, so you can easily change
  // settings in the cfg variable definition for a quick test.
  runTests();

  cfg.skipWrite = false;
  cfg.repStartAfterWrite = false;
  cfg.repStartAfterRead = false;
  cfg.setAddr = 0;

  // Run the tests for all allowed I2c addresses, without changing the
  // address
  for (uint8_t i = FIRST_ADDRESS; i < LAST_ADDRESS; ++i) {
      cfg.resetAddr = i;
      runTests();
  }

  // Pick a random address for the rest of these tests (excluding
  // LAST_ADDRESS)
  cfg.resetAddr = random(FIRST_ADDRESS, LAST_ADDRESS);
  // Change address to a low address
  cfg.setAddr = 0x01;
  runTests();
  // Change address to the same as used for the set address command
  cfg.setAddr = cfg.curAddr;
  runTests();
  // Change address to another address inside the range
  cfg.setAddr = LAST_ADDRESS;
  runTests();
  // Change address to a higher address
  cfg.setAddr = 0x82;
  runTests();
  // Change to another address without general call reset in between
  cfg.resetAddr = 0;
  cfg.setAddr = 0x7b;
  runTests();
  // And do general call resets again
  cfg.resetAddr = random(FIRST_ADDRESS, LAST_ADDRESS);

  // Test changing the address to each possible address (except address
  // 0, but including other reserved addresses for simplicity). Skip the
  // write test, since it is slow.
  cfg.skipWrite = true;
  for (uint8_t i = 1; i < 128; ++i) {
    if (cfg.displayAttached && i == DISPLAY_I2C_ADDRESS)
      continue;
    cfg.curAddr = random(FIRST_ADDRESS, LAST_ADDRESS + 1);
    cfg.setAddr = i;
    runTests();
  }
  cfg.skipWrite = false;
}

void runRandomTest() {
  do {
    cfg.setAddr = random(1, 128);
  } while (cfg.displayAttached && cfg.setAddr == DISPLAY_I2C_ADDRESS);
  cfg.resetAddr = random(FIRST_ADDRESS, LAST_ADDRESS+1);
  runTests();
}

#define DDR DDRD
#define PIN PIND
#define SCL_BIT (1 << PD0)
#define SDA_BIT (1 << PD1)

void setup() {
  Serial.begin(115200);

  bus.begin();
  // In case we were reset halfway through a transfer, read a dummy byte
  // and send a nack to make sure any active slave drops off the bus.
  uint8_t dummy;
  bus.readThenNack(dummy);
  bus.stop();

  // Check the macros above at runtime, unfortunately these
  // digitalPinTo* macros do not work at compiletime.
  if (&DDR != portModeRegister(digitalPinToPort(bus.getScl()))
      || &DDR != portModeRegister(digitalPinToPort(bus.getSda()))
      || SCL_BIT != digitalPinToBitMask(bus.getScl())
      || SDA_BIT != digitalPinToBitMask(bus.getSda())
  ) {
    Serial.println("Incorrect pin mapping");
    while(true) /* nothing */;
  }

  // These assume that pullups are disabled, so the PORT register is not
  // touched.
  bus.setSetSclLow([](const SoftWire *) { DDR |= SCL_BIT; });
  bus.setSetSclHigh([](const SoftWire *) { DDR &= ~SCL_BIT; });
  bus.setSetSdaLow([](const SoftWire *) { DDR |= SDA_BIT; });
  bus.setSetSdaHigh([](const SoftWire *) { DDR &= ~SDA_BIT; });
  bus.setReadScl([](const SoftWire *) { return (uint8_t)(PIN & SCL_BIT); });
  bus.setReadSda([](const SoftWire *) { return (uint8_t)(PIN & SDA_BIT); });

  // Run as fast as possible. With zero delay, the above direct pin
  // access results in about 12us clock period at 16Mhz
  bus.setDelay_us(0);

  Serial.println();
  Serial.println("****************************");
  Serial.println("Starting fixed tests");

  //bus.print = true;
  //Test::min_verbosity = TEST_VERBOSITY_ALL;
  runFixedTests();
  Serial.println("****************************");
  Serial.println();

  // Clear serial buffer, then wait for a newline
  while(Serial.read() >= 0) /* nothing */;
  Serial.println("Send a newline to start random tests");
  while(Serial.read() != '\n') /* nothing */;

  Serial.println();
  Serial.println("****************************");
  Serial.println("****************************");
  Serial.println("Starting random tests");
  while(true)
    runRandomTest();
}

void loop() { }
