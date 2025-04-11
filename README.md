# Air Quality Sensor

This is an air quality sensor with Arduino Nano ESP32, DHT22 temperature and humidity sensor, and MH-Z19C CO₂ sensor.
See [hobby.rapp-martin.de/projects/air-quality-dashboards/](https://hobby.rapp-martin.de/projects/air-quality-dashboards/) for a description.

## Electronics

The `electronics` folder contains a KiCAD project including schematic and PCB design.

## Software

The `software` folder contains an Arduino project. Following adjustments are required:
* set WiFI SSID and password,
* set URL of the InfluxDB server, and
* set InfluxDB token.

The PCB features a DIP switch that can be used to select the sensor ID between 1 and 6.

Sensor ID 0 can be used for testing. The sensor does not push its measurements to the InfluxDB but only prints them to the serial connection.

### CO₂ calibration

Sensor ID 7 is used for CO₂ calibration. Place the sensor in the open, select sensor ID 7, and power on for CO₂ calibration. Wait until the green LED blinks. See the MH-Z19C datasheet for more details.
