Solar Weather Station I 

Please see [Solar Weather Station III](https://github.com/roblatour/SolarWeatherStationIII) for the latest update!

Features:

Gathers weather readings from multiple devices:

2 x BME280 - each providing temperature, humidity, and pressure

2 x DHT22 - each providing temperature, and humidity

1 x DS3231 – providing temperature (more can be added with updates to the sketch)

A setting determines how frequently readings are taken.

Programming determines how temperature, humidity, and pressure will be finalized and reported. For example, the average of all five temperature readings can be use, or alternatively the highest and lowest temperature readings can be excluded and only the average the remaining three used, etc.

Finalized readings may be directly reported to Weather Underground and/or PWS Weather (under the covers these may not be two separate services). Regardless, settings provide for directly reporting to one, or the other, or both. A free account, and the setup of a station on whichever, or both, service(s) you chose, is required for this.

Optionally finalized readings may be published via MQTT. As well, if you like additional information including detailed readings for each device, the server responses from Weather Underground and/or PWS Weather, and the time of the reading may also be published via MQTT.

Supports optional Over the Air (OTA) updates, with an OTA update window configurable as follows: open only for a specified time period associated with each reporting period, optionally, also only available at the top of the hour, optionally, also only during specific hours. I personally use, an OTA window open for 15 seconds, at the top of every hour, between 8:00 am and 10 pm inclusive. This cuts down on power consumption for my solar-based weather station.

Additionally, to help reduce power consumption, the BME280 and DHT22 sensors are only powered when needed (~4 seconds in each reporting period), and the ESP32 goes into deep sleep when not required.

Finally, to further reduce power consumption, I’ve personally physically disabled to power LEDs on the ESP32 and the DS3231.

The real-time clock (RTC) (DS3231) is used to: 
  retain time (recalibrated using an internet NTP server weekly), 
  wake the ESP32 from deep sleep at exact times,
  and take a temperature reading (as noted above)

links to all components can be found in the SolarWeatherStation.ino file.

(Optionally) Also included is the source code for a windows side service, which gathers readings reported via MQTT, maintains a log of reported readings (.csv format), and updates an internal (to your own network) Webpage displaying the current finalized reading values.

Recommended other components:
	Raspberry PI Mosquitto MQTT	
	Raspberry PI Apache Server

questions: info @ rlatour.com
