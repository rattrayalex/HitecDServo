#ifndef HitecDServo_h
#define HitecDServo_h

#include <Arduino.h>

#define HITECD_OK 1
#define HITECD_ERR_NO_SERVO -1
#define HITECD_ERR_NO_RESISTOR -2
#define HITECD_ERR_CORRUPT -3
#define HITECD_ERR_UNSUPPORTED_MODEL -4
#define HITECD_ERR_NOT_ATTACHED -5

const char *hitecdErrToString(int err);

struct HitecDServoConfig {
  /* The default constructor initializes the settings to factory-default values.

  `rawAngleFor850`, `rawAngleFor1500`, and `rawAngleFor2150` will be set to -1; this isn't the
  factory-default value, but it will cause `writeConfig()` to keep the factory-
  default value. */
  HitecDServoConfig();

  /* `id` is an arbitrary number from 0 to 254. Intended for keeping track of
  multiple servos. No effect on servo behavior. */
  uint8_t id;
  static const uint8_t defaultId = 0;

  /* `counterclockwise=false` if increasing pulse widths make the servo turn
  clockwise. `counterclockwise=true` if increasing pulse widths make the servo
  turn counterclockwise.
  
  Note: Switching the servo direction will invert the meaning of the
  `rawAngleFor850`, `rawAngleFor2150`, and `rawAngleFor1500` settings. If
  you've changed those settings to non-default values, you can use the following
  formulas to convert between the clockwise values and equivalent
  counterclockwise values:
    ccwSettings.rawAngleFor850   = 16383 - cwSettings.rawAngleFor2150;
    ccwSettings.rawAngleFor1500 = 16383 - cwSettings.rawAngleFor1500;
    ccwSettings.rawAngleFor2150  = 16383 - cwSettings.rawAngleFor850;
  */
  bool counterclockwise;
  static const bool defaultCounterclockwise = false;

  /* `speed` defines how fast the servo moves to a new position, as a percentage
  of maximum speed. Legal values are 10, 20, 30, 40, 50, 60, 70, 80, 90, 100. */
  int8_t speed;
  static const int8_t defaultSpeed = 100;

  /* `deadband` defines the servo deadband width. Small values make the servo
  more precise, but it may jitter; or if multiple servos are physically
  connected in parallel, they may fight each other. Larger values make the servo
  more stable, but it will not react to small adjustments. Legal values are
  1 (most precise), 2, 3, 4, 5, 6, 7, 8, 9, 10 (least jitter). */
  int8_t deadband;
  static const int8_t defaultDeadband = 1;

  /* `softStart` limits how fast the servo moves when power is first applied.
  If the servo is at the wrong position when power is first applied, soft start
  can help prevent damage. This has no effect on how fast the servo moves during
  normal operation. Legal values are 20, 40, 60, 80, 100. Setting
  `softStart=100` means the servo starts at full power immediately. */
  int8_t softStart;
  static const int8_t defaultSoftStart = 20;

  /* `rawAngleFor850`, `rawAngleFor1500` and `rawAngleFor2150` define how servo
  pulse widths are related to physical servo positions:
  - `rawAngleFor850` defines the position that corresponds to a 850us pulse
  - `rawAngleFor1500` defines the position that corresponds to a 1500us pulse
  - `rawAngleFor2150` defines the position that corresponds to a 2150us pulse

  Raw angles are defined by numbers ranging from 0 to 16383 (=2**14-1). If
  `counterclockwise=false`, higher numbers are clockwise. But if
  `counterclockwise=true`, higher numbers are counterclockwise. This means that
  `rawAngleFor850 < rawAngleFor1500 < rawAngleFor2150` regardless of the value
  of `counterclockwise`.

  If you call writeConfig() with these values set to -1, then the
  factory-default values will be kept. */
  int16_t rawAngleFor850, rawAngleFor1500, rawAngleFor2150;

  /* Convenience functions to return the factory-default raw angles for the
  given servo model. Right now this only works for the D485HW; other models will
  return -1. */
  static int16_t defaultRawAngleFor850(int modelNumber);
  static int16_t defaultRawAngleFor1500(int modelNumber);
  static int16_t defaultRawAngleFor2150(int modelNumber);

  /* Convenience functions to return the min/max raw angles that the servo
  can be safely driven to without hitting physical stops. (This may vary
  slightly from servo to servo; these are conservative values.) */
  static int16_t minSafeRawAngle(int modelNumber);
  static int16_t maxSafeRawAngle(int modelNumber);

  /* If the servo isn't receiving a signal, it will move to a default position
  defined by a pulse width of `failSafe` microseconds. If `failSafeLimp=true`,
  then instead the servo will go limp. If `failSafe=0` and `failSafeLimp=false`,
  the servo will hold its previous position (this is the default). */
  int16_t failSafe;
  bool failSafeLimp;
  static const int16_t defaultFailSafe = 0;
  static const bool defaultFailSafeLimp = false;

  /* If the servo is overloaded or stalled, then it will automatically reduce
  power by `overloadProtection` percent to prevent damage. Legal values are:
  - 100 (no overload protection; the default)
  - 10 (reduce power by 10%, so 90% of max power)
  - 20 (reduce power by 20%, so 80% of max power)
  - 30 (reduce power by 30%, so 70% of max power)
  - 40 (reduce power by 40%, so 60% of max power)
  - 50 (reduce power by 50%, so 50% of max power)
  TODO why is 100 no overload protection, not 0*/
  int8_t overloadProtection;
  static const int8_t defaultOverloadProtection = 100;

  /* `smartSense` is a Hitec proprietary feature that "allows the servo to
  analyse operational feed back and automatically make on the fly parameter
  adjustments to optimize performance". */
  bool smartSense;
  static const bool defaultSmartSense = true;

  /* `sensitivityRatio` ranges from 819 to 4095. Higher values will make the
  servo react faster to changes in input, but it may jitter more. Lower values
  will make the servo more stable, but it may feel sluggish. If
  `smartSense=true`, then `sensitivityRatio` is ignored. */
  int16_t sensitivityRatio;
  static const int16_t defaultSensitivityRatio = 4095;
};

class HitecDServo {
public:
  HitecDServo();

  int attach(int pin);
  bool attached();
  void detach();

  /* Write the servo's target point (i.e. tell the servo to move somewhere).
  - writeTargetMicroseconds() expresses the target as microseconds of PWM width.
  - writeTargetQuarterMicros() expresses the target as quarter-microseconds of
    PWM width, which is more precise. In both cases, the target will be sent to
    the servo via the serial protocol. */
  void writeTargetMicroseconds(int16_t microseconds);
  void writeTargetQuarterMicros(int16_t quarterMicros);

  /* Reads the servo's current point. You can use this to measure the servo's
  progress towards its target point. These three methods return the same value,
  but expressed in different units. */
  int16_t readCurrentMicroseconds();
  int16_t readCurrentQuarterMicros();
  int16_t readCurrentRawAngle();

  /* Returns the servo's model number, e.g. 485 for a D485HW model. */
  int readModelNumber();

  /* Retrieves the configuration from the servo. */
  int readConfig(HitecDServoConfig *configOut);

  /* Resets the servo to its factory-default configuration, then uploads the
  given configuration. Note: Right now, this only works for the D485HW model.
  Other models will return an error. */
  int writeConfig(const HitecDServoConfig &config);

  /* It's dangerous to change the configuration of a non-D485HW model; this
  hasn't been tested, and might damage the servo. If you're willing to take the
  risk, you can use writeConfigUnknownModelThisMightDamageTheServo() with
  bypassModelNumberCheck=true, to bypass the logic that checks the servo model.
  */
  int writeConfigUnknownModelThisMightDamageTheServo(
    const HitecDServoConfig &config,
    bool bypassModelNumberCheck);

  /* Directly read/write registers on the servo. Don't use this unless you know
  what you're doing. (The only reason these methods are declared public is so
  that tests, etc. can access them.) */
  int readRawRegister(uint8_t reg, uint16_t *val_out);
  void writeRawRegister(uint8_t reg, uint16_t val);

private:
  void writeByte(uint8_t value);
  int readByte();

  int pin;
  uint8_t pinBitMask;
  volatile uint8_t *pinInputRegister, *pinOutputRegister;

  int modelNumber;
  int16_t rawAngleFor850, rawAngleFor1500, rawAngleFor2150;
};

#endif /* HitecDServo_h */
