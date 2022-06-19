#include "HitecDServo.h"

HitecDServo::HitecDServo() : pin(-1) { }

void HitecDServo::attach(int _pin) {
  pin = _pin;
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);

  pinBitMask = digitalPinToBitMask(pin);
  uint8_t port = digitalPinToPort(pin);
  pinInputRegister = portInputRegister(port);
  pinOutputRegister = portOutputRegister(port);
}

bool HitecDServo::attached() {
  return (pin != -1);
}

void HitecDServo::detach() {
  pin = -1;
}

int HitecDServo::readModelNumber() {
  uint16_t modelNumber;
  int res = readReg(0x00, &modelNumber);
  if (res < 0) return res;
  return modelNumber;
}

/* We're bit-banging a 115200 baud serial connection, so we need precise timing.
The AVR libraries have a macro _delay_us() that delays a precise number of
microseconds, using compile-time floating-point math. However, we also need to
compensate for the time spent executing non-noop instructions, which depends on
the CPU frequency. DELAY_US_COMPENSATED(us, cycles) will delay for an amount of
time such that if 'cycles' non-noop instruction cycles are executed, the total
time elapsed will be 'us'. */
#define DELAY_US_COMPENSATED(us, cycles) _delay_us((us) - (cycles)/(F_CPU/1e6))

void HitecDServo::writeByte(uint8_t val) {
  /* Write start bit. Note polarity is inverted, so start bit is HIGH. */
  *pinOutputRegister |= pinBitMask;

  /* We're operating at 115200 baud, so theoretically there should be an 8.68us
  interval between edges. In practice, this loop seems to take about 25 clock
  cycles per iteration, so compensate for that. */
  DELAY_US_COMPENSATED(8.68, 25);

  for (int m = 0x001; m != 0x100; m <<= 1) {
    if (val & m) {
      *pinOutputRegister &= ~pinBitMask;
    } else {
      *pinOutputRegister |= pinBitMask;
    }
    DELAY_US_COMPENSATED(8.68, 25);
  }

  /* Write stop bit. */
  *pinOutputRegister &= ~pinBitMask;
  DELAY_US_COMPENSATED(8.68, 25);
}

int HitecDServo::readByte() {
  /* Wait up to 4ms for start bit. The "/ 15" factor arises because this loop
  empirically takes about 15 clock cycles per iteration. */
  int timeout_counter = F_CPU * 0.004 / 15;
  while (!(*pinInputRegister & pinBitMask)) {
    if (--timeout_counter == 0) {
      return HITECD_ERR_NO_SERVO;
    }
  }

  /* Delay until approximate center of first data bit. */
  DELAY_US_COMPENSATED(8.68*1.5, 32);

  /* Read data bits */
  uint8_t val = 0;
  for (int m = 0x001; m != 0x100; m <<= 1) {
    if(!(*pinInputRegister & pinBitMask)) {
      val |= m;
    }
    DELAY_US_COMPENSATED(8.68, 19);
  }

  /* We expect to see stop bit (low) */
  if (*pinInputRegister & pinBitMask) {
    return HITECD_ERR_CORRUPT;
  }

  return val;
}

void HitecDServo::writeReg(uint8_t reg, uint16_t val) {
  uint8_t old_sreg = SREG;
  cli();

  writeByte((uint8_t)0x96);
  writeByte((uint8_t)0x00);
  writeByte(reg);
  writeByte((uint8_t)0x02);
  uint8_t low = val & 0xFF;
  writeByte(low);
  uint8_t high = (val >> 8) & 0xFF;
  writeByte(high);
  uint8_t checksum = (0x00 + reg + 0x02 + low + high) & 0xFF;
  writeByte(checksum);

  SREG = old_sreg;
}

int HitecDServo::readReg(uint8_t reg, uint16_t *val_out) {
  uint8_t old_sreg = SREG;
  cli();

  writeByte((uint8_t)0x96);
  writeByte((uint8_t)0x00);
  writeByte(reg);
  writeByte((uint8_t)0x00);
  uint8_t checksum = (0x00 + reg + 0x00) & 0xFF;
  writeByte(checksum);
  digitalWrite(pin, LOW);

  SREG = old_sreg;

  delay(14);

  /* Note, most of the pull-up force is actually provided by the 2k resistor;
  the microcontroller pullup by itself is nowhere near strong enough. */
  pinMode(pin, INPUT_PULLUP);

  /* At this point, the servo should be pulling the pin low. If the pin goes
  high when we release the line, then no servo is connected. */
  if (digitalRead(pin) != LOW) {
    delay(2);
    pinMode(pin, OUTPUT);
    return HITECD_ERR_NO_SERVO;
  }

  old_sreg = SREG;
  cli();

  int const_0x69 = readByte();
  int mystery = readByte();
  int reg2 = readByte();
  int const_0x02 = readByte();
  int low = readByte();
  int high = readByte();
  int checksum2 = readByte();

  SREG = old_sreg;
  
  delay(1);

  /* At this point, the servo should have released the line, allowing the
  2k resistor to pull it high. If the pin is not high, probably no resistor is
  present. */
  if (digitalRead(pin) != HIGH) {
    pinMode(pin, OUTPUT);
    return HITECD_ERR_NO_RESISTOR;
  }

  pinMode(pin, OUTPUT);

  /* Note, readByte() can return HITECD_ERR_NO_SERVO if it times out. But, we
  know the servo is present, or else we'd have hit either HITECD_ERR_NO_SERVO or
  HITECD_ERR_NO_RESISTOR above. So this is unlikely to happen unless something's
  horribly wrong. So for simplicity, we just round this off to
  HITECD_ERR_CORRUPT. */

  if (const_0x69 != 0x69) return HITECD_ERR_CORRUPT;
  if (mystery < 0) return HITECD_ERR_CORRUPT;
  if (reg2 != reg) return HITECD_ERR_CORRUPT;
  if (const_0x02 != 0x02) return HITECD_ERR_CORRUPT;
  if (low < 0) return HITECD_ERR_CORRUPT;
  if (high < 0) return HITECD_ERR_CORRUPT;
  if (checksum2 != ((mystery + reg2 + const_0x02 + low + high) & 0xFF)) {
    return HITECD_ERR_CORRUPT;
  }

  *val_out = low + (high << 8);
  return HITECD_OK;
}
