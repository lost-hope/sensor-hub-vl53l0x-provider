# VL53L0X Sensor Provider

A [Sensor Hub](../sensor-hub/readme.md) provider usermod for the ST
VL53L0X time-of-flight distance sensor - registers `vl53l0x_distance`
(mm) with the hub by default, which then handles MQTT, Home Assistant
discovery, the JSON API and the Info tab. Runs the sensor in continuous
ranging mode.

## Hardware

Wire SDA/SCL to the I2C pins configured on WLED's own **Config > LED
Preferences** page (shared across all I2C usermods, fixed sensor address
`0x29`). This usermod does not call `Wire.begin()` itself. Retries
`begin()` every 10s if the sensor isn't found; after 3 consecutive failed
reads (or ranging timeouts) the sensor is marked unavailable in Home
Assistant, after 10 it re-attempts `begin()`.

## Usage

Self-contained out-of-tree usermod (see `library.json` for its
`pololu/VL53L0X` dependency). Add it to `custom_usermods` next to the
[Sensor Hub](../sensor-hub/readme.md) itself.

## Usermod Settings

| Setting | Default | Description |
|---|---|---|
| Enabled | on | Master on/off switch (also auto-disabled if I2C pins aren't configured) |
| Check interval | 200 ms | How often the sensor is read |
| Name prefix | `vl53l0x` | Sensor name becomes `<prefix>_distance` - must be unique across every provider registered with the hub |
