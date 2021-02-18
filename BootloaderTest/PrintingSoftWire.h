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

#pragma once

#include <SoftWire.h>
#include <stdint.h>
#include <Arduino.h>

/**
 * Wrapper around SoftWire that prints every byte transferred.
 *
 * SoftWire does not use virtual methods, so any methods SoftWire calls
 * on itself will not be overwritten (e.g. the low-level methods).
 *
 * Additionally, this tracks the current start/stop state and
 * automatically uses repeatedStart when no stop was given before.
 */
class PrintingSoftWire : private SoftWire {
  public:
    bool print = false;

    // Inherit constructors
    using SoftWire::SoftWire;
    using SoftWire::begin;
    using SoftWire::setTimeout_ms;
    using SoftWire::getScl;
    using SoftWire::getSda;
    using SoftWire::setSetSclLow;
    using SoftWire::setSetSclHigh;
    using SoftWire::setSetSdaLow;
    using SoftWire::setSetSdaHigh;
    using SoftWire::setReadSda;
    using SoftWire::setReadScl;
    using SoftWire::setDelay_us;

    void printByte(uint8_t b) {
      if (!print) return;
      if (b < 0x10) Serial.write('0');
      Serial.print(b, HEX);
    }

    void printStart(uint8_t address, bool write, bool repStart) {
      if (!print) return;
      Serial.print(repStart ? "Sr" : "S");
      printByte(address);
      Serial.print(write ? "+W" : "+R");
    }

    result_t printResult(result_t res) {
      if (!print) return res;
      switch(res) {
        case nack:
          Serial.print("n");
          break;
        case timedOut:
          Serial.print("t");
          break;
      }
      Serial.print(" ");
      return res;
    }

    void stop() {
      if (print)
        Serial.println('P');
      SoftWire::stop();
      started = false;
    }

    result_t startWrite(uint8_t a) {
      printStart(a, true, started);
      if (!started) {
        started = true;
        return printResult(SoftWire::startWrite(a));
      } else {
        return printResult(SoftWire::repeatedStartWrite(a));
      }
    }

    result_t startRead(uint8_t a) {
      printStart(a, false, started);
      if (!started) {
        started = true;
        return printResult(SoftWire::startRead(a));
      } else {
        return printResult(SoftWire::repeatedStartRead(a));
      }
    }

    result_t llWrite(uint8_t b) {
      printByte(b);
      return printResult(SoftWire::llWrite(b));
    }

    result_t readThenAck(uint8_t& b) {
      result_t res = SoftWire::readThenAck(b);
      printByte(b);
      printResult(res);
      return res;
    }

    result_t readThenNack(uint8_t& b) {
      result_t res = SoftWire::readThenNack(b);
      printByte(b);
      printResult(res == ack ? nack : res);
      return res;
    }
  private:
    bool started = false;
};
