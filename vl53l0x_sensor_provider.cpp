#include "wled.h"
#include "sensor_bus.h"
#include <VL53L0X.h>

/*
 * VL53L0X time-of-flight distance sensor provider.
 *
 * Reads an ST VL53L0X over I2C (fixed address 0x29) in continuous mode and
 * pushes the reading into the Sensor Hub (see
 * ../sensor-hub/usermod_sensor_hub.cpp and ../sensor-hub/sensor_bus.h) as
 * "<prefix>_distance" (mm). This usermod never talks to MQTT, the JSON API
 * or the Info tab itself - the hub takes care of all of that once a
 * sensor is registered here.
 *
 * Wiring: SDA/SCL go to the I2C pins configured on WLED's own Config > LED
 * Preferences page (the shared "i2c_sda"/"i2c_scl" globals). WLED core
 * already calls Wire.begin() with those pins while loading cfg.json at
 * boot (wled00/cfg.cpp), before any usermod's setup() runs - so this
 * usermod only needs to confirm the pins are set, then use the shared Wire
 * bus. It must NOT call Wire.begin() itself.
 */

REGISTER_SENSOR_SLOT(_slotDistance, "_distance", SensorTypes::Distance, 0, 100);

class VL53L0XSensorUsermod : public Usermod {
  private:
    VL53L0X sensor;
    SensorHub* hub = nullptr;
    uint8_t distHandle = SENSOR_HANDLE_INVALID;

    bool enabled = true;
    bool sensorFound = false;
    bool initDone = false;

    unsigned long lastRead = 0;
    unsigned long lastBeginAttempt = 0;
    uint8_t consecutiveFailures = 0;

    // config
    uint16_t checkIntervalMs = 200; // how often to read the sensor
    String namePrefix = "vl53l0x";  // sensor name becomes "<prefix>_distance"
    uint8_t priority = 100;         // getValue() selection priority - lower wins among sensors of the same SensorType (see sensor_bus.h)

    static const char _name[];
    static const char _enabled[];
    static const char _checkInterval[];
    static const char _namePrefix[];
    static const char _priority[];

    bool beginSensor() {
      sensor.setBus(&Wire);
      sensor.setTimeout(500);
      if (!sensor.init()) return false;
      sensor.startContinuous();
      return true;
    }

    void registerSensors() {
      if (!hub || distHandle != SENSOR_HANDLE_INVALID) return; // already registered
      distHandle = hub->attachSensor(&_slotDistance, namePrefix.c_str(), 0, priority);
    }

  public:
    void setup() override {
      // I2C bus is configured (and Wire.begin() already called) via WLED's
      // own Config > LED Preferences page - nothing to do here if it's unset.
      // Don't persist this into 'enabled' (the user's own on/off switch) -
      // initDone (left false here) is what actually gates loop(), so a
      // later pin fix takes effect on the next boot instead of staying
      // stuck disabled.
      if (i2c_sda < 0 || i2c_scl < 0) return;
      sensorFound = beginSensor();
      initDone = true;
    }

    void loop() override {
      if (!enabled || !initDone) return;

      if (!hub) hub = getSensorHub(); // Sensor Hub usermod may finish init after us
      if (hub) registerSensors();

      unsigned long now = millis();

      if (!sensorFound) {
        // sensor missing at boot (or lost) - keep retrying rather than giving up forever
        if (now - lastBeginAttempt < 10000) return;
        lastBeginAttempt = now;
        sensorFound = beginSensor();
        if (!sensorFound) return;
      }

      if (now - lastRead < (unsigned long)checkIntervalMs) return;
      lastRead = now;

      uint16_t range = sensor.readRangeContinuousMillimeters();
      if (sensor.timeoutOccurred()) {
        consecutiveFailures++;
        if (hub && distHandle != SENSOR_HANDLE_INVALID && consecutiveFailures >= 3) hub->setSensorAvailable(distHandle, false);
        if (consecutiveFailures >= 10) sensorFound = false; // force a fresh begin() next loop
        return;
      }

      consecutiveFailures = 0;
      if (hub && distHandle != SENSOR_HANDLE_INVALID) {
        hub->setSensorAvailable(distHandle, true);
        hub->updateSensor(distHandle, (float)range);
      }
    }

    void addToConfig(JsonObject& root) override {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[FPSTR(_enabled)] = enabled;
      top[FPSTR(_checkInterval)] = checkIntervalMs;
      top[FPSTR(_namePrefix)] = namePrefix;
      top[FPSTR(_priority)] = priority;
    }

    bool readFromConfig(JsonObject& root) override {
      JsonObject top = root[FPSTR(_name)];
      bool configComplete = !top.isNull();
      configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled);
      configComplete &= getJsonValue(top[FPSTR(_checkInterval)], checkIntervalMs);
      configComplete &= getJsonValue(top[FPSTR(_namePrefix)], namePrefix);
      configComplete &= getJsonValue(top[FPSTR(_priority)], priority);
      return configComplete;
    }

    void appendConfigData(Print& settingsScript) override {
      settingsScript.print(F("addInfo('VL53L0XSensor:checkInterval',1,'milliseconds between sensor reads');"));
      settingsScript.print(F("addInfo('VL53L0XSensor:namePrefix',1,'sensor name becomes &lt;prefix&gt;_distance - must be unique across all sensor providers');"));
      settingsScript.print(F("addInfo('VL53L0XSensor:priority',1,'getValue() selection priority - lower wins if another provider also registers a Distance sensor');"));
    }
};

const char VL53L0XSensorUsermod::_name[]          PROGMEM = "VL53L0XSensor";
const char VL53L0XSensorUsermod::_enabled[]       PROGMEM = "enabled";
const char VL53L0XSensorUsermod::_checkInterval[] PROGMEM = "checkInterval";
const char VL53L0XSensorUsermod::_namePrefix[]    PROGMEM = "namePrefix";
const char VL53L0XSensorUsermod::_priority[]      PROGMEM = "priority";

static VL53L0XSensorUsermod vl53l0x_sensor;
REGISTER_USERMOD(vl53l0x_sensor);
