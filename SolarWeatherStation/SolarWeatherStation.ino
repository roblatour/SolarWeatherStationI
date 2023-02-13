/*

	Backyard weather station v10.0
	Copyright Rob Latour, 2022
	License: MIT

	https://github.com/roblatour/SolarWeatherStation

	(all parts listed below are basically what I used, over time I suspect the links will break)

  ESP32:
	physical board          ESP32 Devkit c_V4  ESP32-WROOM-32U  with external antenna)
	                        https://www.aliexpress.com/item/4000851307120.html

                                Board Manager - ESP32 version 1.0.6 
	             		board definition used from Arduino library - DOIT ESP32 Devkit 1


	Antenna:
	https://www.aliexpress.com/item/4001054693109.html

	2 x BME280:
	https://www.aliexpress.com/item/32912100752.html  (3.3v)

	2 x DHT22:
	https://www.aliexpress.com/item/32759901711.html

	1 x DS3231 (RTC):
	https://www.aliexpress.com/item/1005001648015853.htm  (blue)

	1 x Solar charger board:
	https://www.dfrobot.com/product-1712.html

	1 x Solar panel (110 mm x136 mm)
	https://www.aliexpress.com/item/33009858730.html?spm=a2g0s.9042311.0.0.27424c4dCM0AUB

	2 x 18650 Batteries:
	(I used two batteries in the case, which ment a soldering hack to the solar charger board to connect another battery holder - you might be able to get away with just one battery)

	extra battery holder (if needed):
	https://www.aliexpress.com/item/32806159516.html (pack of 20)

	Short USB wire to connect ESP32 to Solar charger board
	https://www.aliexpress.com/item/33059150976.html

	Wiring:
	see https://oshwlab.com/RobLatour/weatherstation_copy_copy_copy

	custom PCB:
	https://oshwlab.com/RobLatour/weatherstation_copy_copy_copy

	custom 3D printed case:
	https://www.thingiverse.com/thing:4779465
	(a tight fit, but doable with PCB + solar charger above + two batteries - that is what I used)

    Bonus:
	I find working with most ESP32 on a breadboard bothersome because they are not breadboard friendly, so I also designed this 3d printable solution for that:
	https://www.thingiverse.com/thing:4523270


	Other Arduion libraries used (with thanks to their authors):
	- https://github.com/adafruit/Adafruit_BME280_Library
	- https://github.com/adafruit/Adafruit_Sensor
	- https://github.com/JChristensen/DS3232RTC
	- https://github.com/PaulStoffregen/Time
        - https://github.com/knolleary/pubsubclient

*/

#include "arduino_user_settings.h"
#include "arduino_user_secrets.h"

// WiFi and secure connect stuff

#include <WiFi.h>

const char * wifi_name = SECRET_WIFI_SSID;
const char * wifi_pass = SECRET_WIFI_PASSWORD;

// Time stuff

#include <DS3232RTC.h>
#include <time.h>

DS3232RTC rtc;
#include "driver/rtc_io.h"

const char* ntpServer = NTP_SERVER_POOL;
int   NumberOfMinutesBetweenReadings;
const gpio_num_t interruptPin = INTERRUPT_PIN;

int NumberOfRestartsBeforeAnRTCUpdateFromAnNTPServerIsNeeded;
RTC_DATA_ATTR int NumberOfRestartsSinceLastRTCUpdateFromAnNTPServer = 0;

struct tm timeinfo;

bool Time_OK;

// BME280 sensor stuff

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#define I2C_SDA BME280_SDA_PIN
#define I2C_SCL BME280_SCL_PIN
#define SEALEVELPRESSURE_HPA (1013.25)

const int NumberOfBMEDevices = NUMBER_OF_BME280_DEVICES;

Adafruit_BME280 bme[NumberOfBMEDevices];
const int BME280_SDO_Pins[NumberOfBMEDevices] = BME_SDO_PIN_NUMBERS;

// DHT22 sensor stuff

#include <DHT.h>
#define DHTTYPE    DHT22

const int NumberOfDHTDevices = NUMBER_OF_DHT22_DEVICES;
const int DHT22_DAT_Pins[NUMBER_OF_DHT22_DEVICES] = DHT_DAT_PIN_NUMBERS;

// Reading are stored here

char  Reading_UTC_Date_and_Time[23];
float Reading_Temperature[NumberOfBMEDevices + NumberOfDHTDevices + 1];  // the +1 is for the DS3231 RTC
float Reading_Humidity[NumberOfBMEDevices + NumberOfDHTDevices];
float Reading_Pressure[NumberOfBMEDevices + NumberOfDHTDevices];

float Final_Temperature;
float Final_Humidity;
float Final_Pressure;

const float ReadingUnavailable = 18670701;


// Weather Service stuff

#include <WiFiClientSecure.h>

WiFiClientSecure SecureWeatherClient;

const String PWS_Host = "pwsupdate.pwsweather.com";
const String PWS_Folder = "/api/v1/submitwx?";

const String UNDRGRND_Host = "rtupdate.wunderground.com";
const String UNDRGRND_Folder = "/weatherstation/updateweatherstation.php?";

const String SoftwareType = "ESP32DIY";

String LastPWSResponse;
String LastUndergroundResponse;

// MQTT stuff

#include <PubSubClient.h>
WiFiClient InsecureClient;
PubSubClient MQTTclient(InsecureClient);


// OTA update stuff

#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

bool OTAUpdateInProgress = false;
int  OTAWindowInSeconds = OTA_WINDOW_IN_SECONDS;


//********************************************************************************************************************************************************************

void Setup_Serial() {

  if (DEBUG_ENABLED) {
    Serial.begin(SERIAL_PORT_SPEED);
    Serial.println("****************************************************************");
    Serial.println("");
    Serial.println("Solar Weather Station v10");
    Serial.println("2023-02-13");
    Serial.println("by Rob Latour, 2023");
    Serial.println("");
  }

}

//********************************************************************************************************************************************************************

void Toggle_Power_to_Sensors(bool ON) {

  if (ON)
    digitalWrite(POWER_SENSOR_CONTROLLER_PIN, HIGH);
  else
    digitalWrite(POWER_SENSOR_CONTROLLER_PIN, LOW);

}

void Setup_Sensor_Power_Control() {

  pinMode(POWER_SENSOR_CONTROLLER_PIN , OUTPUT);

}

//********************************************************************************************************************************************************************

void Setup_RTC() {

#define RTC_CONTROL 0x0e
#define BBSQW 6

  // makes sure the wakeup interrupt will not fire again until it is re-enabled later on (in the RTC_Sleep subroutine)

  rtc.begin();
  rtc.writeRTC(RTC_CONTROL, (rtc.readRTC(RTC_CONTROL) | _BV(BBSQW)));

  rtc.setAlarm(DS3232RTC::ALM1_MATCH_DATE, 0, 0, 0, 1);
  rtc.alarm(DS3232RTC::ALARM_1);
  rtc.alarmInterrupt(DS3232RTC::ALARM_1, false);
  rtc.clearAlarm(DS3232RTC::ALARM_1);

  rtc.setAlarm(DS3232RTC::ALM2_MATCH_DATE, 0, 0, 0, 1);
  rtc.alarm(DS3232RTC::ALARM_2);
  rtc.alarmInterrupt(DS3232RTC::ALARM_2, false);
  rtc.clearAlarm(DS3232RTC::ALARM_2);

}

//********************************************************************************************************************************************************************

bool Setup_WiFi() {

  bool ReturnValue;  // returns true if a connection is made

  int  numberOfSecondsToTryToConnect = 3;
  int  counter;
  bool notyetconnected = true;

  while ( (notyetconnected) && (numberOfSecondsToTryToConnect <= 10) ) {

    if (DEBUG_ENABLED)
      Serial.print("Attempting to connect to WiFi");

    //WiFi.mode(WIFI_STA);
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(wifi_name, wifi_pass);
    delay(1000);

    counter = 0;

    while ( ( WiFi.status() != WL_CONNECTED ) && (counter < numberOfSecondsToTryToConnect) )
    {
      if (DEBUG_ENABLED)
        Serial.print(".");
      counter++;
      delay (1000);
    }
    
    if (DEBUG_ENABLED)
       Serial.println("");

    if ( WiFi.status() == WL_CONNECTED )
    {
      notyetconnected = false;
    }
    else
    {

      // at this point the system has not yet connected
      // so increase the number of seconds by three, reset things (which will take two seconds), and try to connect again
      // however give up if the number of seconds to try and connect goes beyond 10
      // total try time will have been ( 3 + 2 ) + ( 6 + 2 ) + ( 9 + 2 ) = with 24 seconds having been spent in total

      numberOfSecondsToTryToConnect = numberOfSecondsToTryToConnect + 3;

      if (numberOfSecondsToTryToConnect <= 10) {

        WiFi.disconnect(true);
        delay(1000);
        //WiFi.mode(WIFI_STA);
        WiFi.mode(WIFI_AP_STA);
        delay(1000);

      }

    }

  };

  if (notyetconnected) {

    if (DEBUG_ENABLED)
      Serial.println("Could not connect to WiFi");

    ReturnValue = false;

  } else {

    if (DEBUG_ENABLED) {

      String message;
      message = "Connected to ";
      message.concat(wifi_name);
      message.concat(" at IP address ");
      message.concat(WiFi.localIP().toString());

      Serial.println(message);

    }

    ReturnValue = true;

  }

  return ReturnValue;

};


//********************************************************************************************************************************************************************

bool MQTT_Connect() {

  bool ReturnValue = false;

  MQTTclient.setServer(MQTT_SERVER_ADDESS, MQTT_SERVER_PORT);

  int attempts = 0;

  while ((!MQTTclient.connected()) && attempts <= 3)  {

    if (DEBUG_ENABLED)
      Serial.print("Attempting MQTT connection...");

    if (MQTTclient.connect(MQTT_CLIENT_NAME, MQTT_USER_NAME, MQTT_USER_PASSWORD)) {

      ReturnValue = true;

      if (DEBUG_ENABLED)
        Serial.println("connected");

    } else {

      if (DEBUG_ENABLED) {
        Serial.print("failed, rc=");
        Serial.print(MQTTclient.state());
        Serial.println(" trying again in a couple seconds");
      }
      delay(2000);

    }

    attempts++;

  }

  if (DEBUG_ENABLED)
    if (attempts > 3)
      Serial.println("Could not connect to MQTT server");

  return ReturnValue;

}

void Publish_A_Reading(String SubTopic, float Value) {

  String TopicString = MQTT_TOPIC;
  TopicString.concat("/");
  TopicString.concat(SubTopic);
  int    TopicStringLength = TopicString.length() + 1;
  char   TopicCharArray[TopicStringLength];
  TopicString.toCharArray(TopicCharArray, TopicStringLength);

  String ReadingString = String(Value);
  int    ReadingStringLength = ReadingString.length() + 1;
  char   ReadingCharArray[ReadingStringLength];
  ReadingString.toCharArray(ReadingCharArray, ReadingStringLength);

  MQTTclient.publish(TopicCharArray, ReadingCharArray);

  if (DEBUG_ENABLED)
    Serial.print(TopicString); Serial.print(" "); Serial.println(ReadingString);

}


void Publish_A_Status(String SubTopic, String PostingResultsString) {

  String TopicString = MQTT_TOPIC;
  TopicString.concat("/");
  TopicString.concat(SubTopic);
  int    TopicStringLength = TopicString.length() + 1;
  char   TopicCharArray[TopicStringLength];
  TopicString.toCharArray(TopicCharArray, TopicStringLength);

  int    PostingResultsStringLength = PostingResultsString.length() + 1;
  char   PostingResultsCharArray[PostingResultsStringLength];
  PostingResultsString.toCharArray(PostingResultsCharArray, PostingResultsStringLength);

  MQTTclient.publish(TopicCharArray, PostingResultsCharArray);

  if (DEBUG_ENABLED)
    Serial.print(TopicString); Serial.print(" "); Serial.println(PostingResultsString);

}

void Publish_Readings_to_MQTTServer() {

  // publishes the temperature, pressure, and humidity to mqtt
  // the date is not published

  if ( ( Final_Temperature == ReadingUnavailable ) && ( Final_Humidity == ReadingUnavailable ) && ( Final_Pressure == ReadingUnavailable ) ) {
    return;    // there is nothing to report so exit early
  }

  if (MQTT_ENABLED) {

    if (DEBUG_ENABLED)
      Serial.println("MQTT publishing underway:");

    if (MQTT_Connect()) {


      // Publish temperature readings

      if (!MQTT_REPORT_FINALIZED_NUMBERS_ONLY) {


        // publish the current (UTC) time

        String TopicString = MQTT_TOPIC;
        TopicString.concat("/");
        TopicString.concat("current-UTC-time");
        int    TopicStringLength = TopicString.length() + 1;
        char   TopicCharArray[TopicStringLength];
        TopicString.toCharArray(TopicCharArray, TopicStringLength);

        String ReadingString = Reading_UTC_Date_and_Time;
        int    ReadingStringLength = ReadingString.length() + 1;
        char   ReadingCharArray[ReadingStringLength];
        ReadingString.toCharArray(ReadingCharArray, ReadingStringLength);

        MQTTclient.publish(TopicCharArray, ReadingCharArray);

        if (!MQTT_REPORT_FINALIZED_NUMBERS_ONLY) {

          int DeviceNumber = 0;

          for (int i = 0; i < NumberOfBMEDevices; i++) {
            String ws = "temperature-from-BME280-on-pin-";
            ws.concat(String(BME280_SDO_Pins[i]));
            Publish_A_Reading(ws, Reading_Temperature[DeviceNumber]);
            DeviceNumber++;
          }

          for (int i = 0; i < NumberOfDHTDevices; i++) {
            String ws = "temperature-from-DHT22-on-pin-";
            ws.concat(String(DHT22_DAT_Pins[i]));
            Publish_A_Reading(ws, Reading_Temperature[DeviceNumber]);
            DeviceNumber++;
          }

          String ws = "temperature-from-DS3231";
          Publish_A_Reading(ws, Reading_Temperature[DeviceNumber]);

        }

        if ( Final_Temperature != ReadingUnavailable )
          Publish_A_Reading("temperature", Final_Temperature);


        // publish humidity readings

        if (!MQTT_REPORT_FINALIZED_NUMBERS_ONLY) {

          int DeviceNumber = 0;

          for (int i = 0; i < NumberOfBMEDevices; i++) {
            String ws = "humidity-BME280-on-pin-";
            ws.concat(String(BME280_SDO_Pins[i]));
            Publish_A_Reading(ws, Reading_Humidity[DeviceNumber]);
            DeviceNumber++;
          }

          for (int i = 0; i < NumberOfDHTDevices; i++) {
            String ws = "humidity-DHT22-on-pin-";
            ws.concat(String(DHT22_DAT_Pins[i]));
            Publish_A_Reading(ws, Reading_Humidity[DeviceNumber]);
            DeviceNumber++;
          }

        }

        if ( Final_Humidity != ReadingUnavailable )
          Publish_A_Reading("humidity", Final_Humidity);


        // publish presure readings

        if (!MQTT_REPORT_FINALIZED_NUMBERS_ONLY) {

          int DeviceNumber = 0;

          for (int i = 0; i < NumberOfBMEDevices; i++) {
            String ws = "presure-BME280-on-pin-";
            ws.concat(String(BME280_SDO_Pins[i]));
            Publish_A_Reading(ws, Reading_Pressure[DeviceNumber]);
            DeviceNumber++;
          }

        }


        if ( Final_Pressure != ReadingUnavailable )
          Publish_A_Reading("pressure", Final_Pressure);


        if (!MQTT_REPORT_FINALIZED_NUMBERS_ONLY) {

          if (PWS_ENABLED)
            Publish_A_Status("published-to-PWS", LastPWSResponse);

          if ((PWS_ENABLED) && (UNDRGRND_ENABLED))
            delay(100);  // if the last response also includes the post you need to increase this delay quite a bit

          if (UNDRGRND_ENABLED)
            Publish_A_Status("published-to-Underground", LastUndergroundResponse);

        }

        delay(500);  // this delay is needed to finish off the publishing

        MQTTclient.disconnect();

        if (DEBUG_ENABLED) {
          Serial.println("MQTT connection disconnected");
          Serial.println("");
        }


      } else {

        if (DEBUG_ENABLED)
          Serial.println("Based on the user settings, the above readings were not published to an MQTT server");

      }

    }

  }

}

//********************************************************************************************************************************************************************

String Publish_Readings_to_WeatherService(String host, String folder, String id, String password ) {

  // prepare post, examples:
  // https://pwsupdate.pwsweather.com/api/v1/submitwx?ID=xxxxxx&PASSWORD=xxxxxx&dateutc=2021-04-30+15:20:01&tempf=34.88&baromin=29.49&humidity=83&softwaretype=Whatever&action=updateraw
  // https://rtupdate.wunderground.com/weatherstation/updateweatherstation.php?ID=<DEVICE_ID>&PASSWORD=<DEVICE_KEY>&dateutc=now&tempf=99.99&baromin=99.99&humidity=99.99&softwaretype=Whatever&action=updateraw

  String post = "";

  post.concat("GET ");
  post.concat(folder);

  post.concat("ID=");
  post.concat(id);
  post.concat("&PASSWORD=");
  post.concat(password);

  post.concat("&dateutc=");
  post.concat(Reading_UTC_Date_and_Time);

  if ( Final_Temperature != ReadingUnavailable ) {
    float tempF = (Final_Temperature * 1.8) + 32;
    post.concat("&tempf=");
    post.concat(String(tempF));
  }

  if ( Final_Humidity != ReadingUnavailable ) {
    post.concat("&humidity=");
    post.concat(String(Final_Humidity));
  }

  if ( Final_Pressure != ReadingUnavailable ) {
    post.concat("&baromin=");
    // convert pressure from kPa to inches
    float preasureInches = Final_Pressure / 33.863886666667;
    post.concat(String(preasureInches));
  }

  post.concat("&softwaretype=");
  post.concat(SoftwareType);

  post.concat("&action=updateraw");

  post.concat(" HTTP/1.1");

  SecureWeatherClient.setInsecure();

  if (DEBUG_ENABLED) {
    Serial.print("Connecting to ");
    Serial.print(host);
  }

  if (SecureWeatherClient.connect(host.c_str(), 443)) {
    if (DEBUG_ENABLED) {
      Serial.println("...connection succeeded");
    }
  } else {
    if (DEBUG_ENABLED) {
      Serial.println("...connection failed");
      return "connection failed";
    }
  }

  if (DEBUG_ENABLED)
    Serial.println("Sending readings");

  SecureWeatherClient.println(post);
  SecureWeatherClient.println("Host: " + String(host));
  SecureWeatherClient.println("Content-Type : application/json");
  SecureWeatherClient.println("Content-Length : 0");
  SecureWeatherClient.println();

  int WaitLimit = 0;
  while ((!SecureWeatherClient.available()) && (WaitLimit < 250)) {
    delay(50); //
    WaitLimit++;
  }

  String Response = "";
  WaitLimit = 0;
  while ( (SecureWeatherClient.connected()) && (WaitLimit < 250) ) {
    String line = SecureWeatherClient.readStringUntil('\n');
    if (line == "\r") {
      // retrieved header lines can be ignored
      break;
    }
    WaitLimit++;
  }

  while (SecureWeatherClient.available()) {
    char c = SecureWeatherClient.read();
    if (isPrintable(c))
      Response += c;
  }

  if (DEBUG_ENABLED)
    Serial.println(Response);

  SecureWeatherClient.stop();

  // the Response is returned so it can be pubished via MQTT

  // if both the Post and the Response are returned a much larger delay is needed for MQTT to do its job
  // accordingly if you want the post and the reponse published, then you can uncommet this code and put a large delay in the MQTT Response process
  //
  // String PostAndResponse = "";
  // PostAndResponse = post;
  // PostAndResponse.concat("\n");
  // PostAndResponse.concat(Response);

  return Response;

}


//********************************************************************************************************************************************************************

void Publish_Readings_to_WeatherServices() {

  if ( ( Final_Temperature == ReadingUnavailable ) && ( Final_Humidity == ReadingUnavailable ) && ( Final_Pressure == ReadingUnavailable ) ) {

    if (DEBUG_ENABLED) {
      Serial.println("There are no readings to report");
      Serial.println("");
    }

  } else {

    if (PWS_ENABLED)
      LastPWSResponse = Publish_Readings_to_WeatherService(PWS_Host, PWS_Folder, PWS_STATION_ID, SECRET_PSW_API_KEY);

    if (UNDRGRND_ENABLED)
      LastUndergroundResponse = Publish_Readings_to_WeatherService(UNDRGRND_Host, UNDRGRND_Folder, UNDRGRND_STATION_ID, SECRET_UNDRGRND_STATION_KEY);

  }

}


void Take_All_Readings() {

  Toggle_Power_to_Sensors(true);

  delay(500);  // changed from 100 as one sensor might need more time to power up

  sprintf(Reading_UTC_Date_and_Time, "%4u-%02u-%02u+%02u:%02u:%02u", year(), month(), day(), hour(), minute(), second());

  if (DEBUG_ENABLED) {
    Serial.print("Reading UTC date and time: "); Serial.println(Reading_UTC_Date_and_Time);
    Serial.println("");
  };

  int ReadingIndex = 0;    // this is used as the index for the readings, it counts from 0 to NumberOfBMEDevices + NumberOfDHTDevices - 1

  // Get readings from BME280s
  for (int i = 0; i < NumberOfBMEDevices; i++) {
    Take_BME280_Readings(ReadingIndex);
    ReadingIndex++;
  }

  // Get reading from the DHT22
  for (int i = 0; i < NumberOfDHTDevices; i++) {
    Take_DHT22_Reading(ReadingIndex);
    ReadingIndex++;
  }

  Toggle_Power_to_Sensors(false);

  Take_DS3231_Reading(ReadingIndex);

}


void Finalize_Readings() {

  // create averages

  Final_Temperature = 0;
  Final_Humidity = 0;
  Final_Pressure = 0;

  int Valid_Temperature_Readings = 0;
  int Valid_Humidity_Readings = 0;
  int Valid_Pressure_Readings = 0;

  // accounts for the temperature readings from the BME820 and DHT22 devices plus the DS3231 (RTC)

  // remove the + 1 in the next line to remove the results of the DS3231
  for (int i = 0; i < (NumberOfBMEDevices + NumberOfDHTDevices + 1); i++) {

    if (!isnan(Reading_Temperature[i])) {
      Final_Temperature += Reading_Temperature[i];
      Valid_Temperature_Readings++;
    }

  };

  // accounts for the humidity readings from the BME820 and DHT22 devices
  for (int i = 0; i < (NumberOfBMEDevices + NumberOfDHTDevices); i++) {

    if (!isnan(Reading_Humidity[i])) {
      Final_Humidity += Reading_Humidity[i];
      Valid_Humidity_Readings++;
    }

  };

  // accounts for the pressure readings from the BME820 devices
  for (int i = 0; i < NumberOfBMEDevices; i++) {

    if (!isnan(Reading_Pressure[i])) {
      Final_Pressure += Reading_Pressure[i];
      Valid_Pressure_Readings++;
    }

  };

  if (Valid_Temperature_Readings > 0 )
    Final_Temperature = Final_Temperature / Valid_Temperature_Readings;
  else
    Final_Temperature = ReadingUnavailable;

  if (Valid_Humidity_Readings > 0 )
    Final_Humidity = Final_Humidity  / Valid_Humidity_Readings;
  else
    Final_Humidity = ReadingUnavailable;

  if (Valid_Pressure_Readings > 0 )
    Final_Pressure = Final_Pressure / Valid_Pressure_Readings;
  else
    Final_Pressure = ReadingUnavailable;

}

void Take_BME280_Readings(int ReadingIndex) {

  static int DeviceIndex = 0;

  pinMode(BME280_SDO_Pins[DeviceIndex], OUTPUT);         // sets the BME unit to work on 0x77 as opposed to 0x76 which is the default
  digitalWrite(BME280_SDO_Pins[DeviceIndex], HIGH);

  //bool BMEStartedOK = bme[DeviceIndex].begin(0x77);  // used when using default sda and scl pins

  TwoWire I2CBME = TwoWire(1);                           // this lets the BME devices run on alternate SDA and SCL pins
  I2CBME.begin(I2C_SDA, I2C_SCL, 100000);
  bool BMEStartedOK = bme[DeviceIndex].begin(0x77, &I2CBME);

  if  (BMEStartedOK) {

    Reading_Temperature[ReadingIndex] = bme[DeviceIndex].readTemperature();
    Reading_Humidity[ReadingIndex] = bme[DeviceIndex].readHumidity();
    Reading_Pressure[ReadingIndex] = bme[DeviceIndex].readPressure() / 100.0F;

    if (DEBUG_ENABLED) {

      Serial.print("Readings from the BME280 sensor connected to pin ");
      Serial.print(BME280_SDO_Pins[DeviceIndex]);
      Serial.println(": ");

      if (!isnan(Reading_Temperature[ReadingIndex])) {
        Serial.print(" Temperature:      \t  "); Serial.print(String(Reading_Temperature[ReadingIndex])); Serial.println(" C");
      };

      if (!isnan(Reading_Humidity[ReadingIndex])) {
        Serial.print(" Humidity:         \t  "); Serial.print(String(Reading_Humidity[ReadingIndex])); Serial.println(" % ");
      };

      if (!isnan(Reading_Pressure[ReadingIndex])) {
        Serial.print(" Pressure:         \t"); Serial.print(String(Reading_Pressure[ReadingIndex])); Serial.println(" hPa");
      };

      Serial.println("");

    }

  } else {

    if (DEBUG_ENABLED) {
      Serial.print("Could not find a valid BME280 sensor for the sensor connected to pin ");
      Serial.print(BME280_SDO_Pins[DeviceIndex]);
      Serial.println(", please check wiring!");

    }

  };

  digitalWrite(BME280_SDO_Pins[DeviceIndex], LOW);  // reset the BME device to work on 0x76, so the next device can use 0x77 instead

  DeviceIndex++;

}

void Take_DHT22_Reading(int ReadingIndex) {

  static int DeviceIndex = 0;

  //get readings from DHT22

  DHT dht(DHT22_DAT_Pins[DeviceIndex], DHTTYPE);
  dht.begin();
  delay(500);  // a delay is required, without it the readings don't work

  Reading_Temperature[ReadingIndex] =  dht.readTemperature(false);
  Reading_Humidity[ReadingIndex] = dht.readHumidity(false);

  if (DEBUG_ENABLED) {

    Serial.print("Readings from the DHT22 sensor connected to pin ");
    Serial.print(DHT22_DAT_Pins[DeviceIndex]);
    Serial.println(" : ");

    if (!isnan(Reading_Temperature[ReadingIndex])) {
      Serial.print(" Temperature :      \t  "); Serial.print(String(Reading_Temperature[ReadingIndex])); Serial.println(" C");
    };

    if (!isnan(Reading_Humidity[ReadingIndex])) {
      Serial.print(" Humidity :         \t  "); Serial.print(String(Reading_Humidity[ReadingIndex])); Serial.println(" % ");
    };

    Serial.println("");

  }

  DeviceIndex++;

}


void Take_DS3231_Reading(int ReadingIndex) {

  //get readings from DS3231


  int t = rtc.temperature(); //Get temperature
  float celsius = t / 4.0;
  Reading_Temperature[ReadingIndex] = celsius;

  if (DEBUG_ENABLED) {

    Serial.println("Reading from the DS3231 sensor : ");

    if (!isnan(Reading_Temperature[ReadingIndex])) {
      Serial.print(" Temperature :      \t  "); Serial.print(String(Reading_Temperature[ReadingIndex])); Serial.println(" C");
    };

    Serial.println("");

  }

}

//********************************************************************************************************************************************************************

void printDigits(int digits) {

  if (DEBUG_ENABLED) {
     if (digits < 10)
        Serial.print("0");
     Serial.print(digits);
     }
  
}

void printCurrentTime() {

  if (DEBUG_ENABLED) {
     Serial.print(year()); Serial.print("-"); printDigits(month()); Serial.print("-"); printDigits(day()); Serial.print(" ");
     printDigits(hour()); Serial.print(":"); printDigits(minute()); Serial.print(":"); printDigits(second()); Serial.println("");
  }

}

//********************************************************************************************************************************************************************


bool SetTimeFromNTPServer() {

  bool ReturnValue = false;

  // for some reason two calls to set the time using the ntpServer seems to result in a more accurate result
  configTime(0, 0, ntpServer);   // get the UTC time from an internet ntp server (try 1)
  delay(10);
  configTime(0, 0, ntpServer);   // get the UTC time from an internet ntp server (try 2)

  if (getLocalTime(&timeinfo)) {

    if (DEBUG_ENABLED) {
      Serial.println("Setting time from NTP server");
      Serial.print("NTP UTC server time: ");
      Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    }

    int y = timeinfo.tm_year + 1900;
    int mo = timeinfo.tm_mon + 1;
    int d = timeinfo.tm_mday;
    int h = timeinfo.tm_hour;
    int m = timeinfo.tm_min;
    int s = timeinfo.tm_sec;

    setTime(h, m, s, d, mo, y);

    ReturnValue = (rtc.set(now()) == 0);

  };

  if (DEBUG_ENABLED)
    if (ReturnValue)
      Serial.println("RTC set from NTP server");
    else
      Serial.println("Failed to set RTC from NTP server");

  return ReturnValue;

}

void Setup_Time() {

  bool NTPTimeSetOK = true;                                // will only be set to false if an NTP time set is requested, and that time set failed

  time_t rtcTime;
  rtcTime = rtc.get();

  if (rtcTime > 0)
    setTime(rtcTime);

  NumberOfRestartsSinceLastRTCUpdateFromAnNTPServer++;

  if ( (NumberOfRestartsSinceLastRTCUpdateFromAnNTPServer > NumberOfRestartsBeforeAnRTCUpdateFromAnNTPServerIsNeeded) || (year() < 2021) ) {
    NTPTimeSetOK = SetTimeFromNTPServer();
    if (NTPTimeSetOK)
      NumberOfRestartsSinceLastRTCUpdateFromAnNTPServer = 1;
  };

  if  ( NTPTimeSetOK) {

    // sanity check; this program was written in 2021 so year should be at least 2021

    if (year() >= 2021) {
      Time_OK = true;
    }
    else {
      Time_OK = false;
      if (DEBUG_ENABLED)
        Serial.print("Bad year: "); Serial.println(year());
    }

  } else {

    Time_OK = false;
    if (DEBUG_ENABLED)
      Serial.println("Bad NTP time check");

  };

  if (DEBUG_ENABLED) {
    Serial.print("System time: ");
    printCurrentTime();
    Serial.println("");
  };

}

//********************************************************************************************************************************************************************

void Setup_OTAUpdate() {

  ArduinoOTA.setHostname(OTA_HOST_NAME);
  ArduinoOTA.setPassword(SECRET_OTA_PASSWORD);

  ArduinoOTA
  .onStart([]() {

    OTAUpdateInProgress = true;

    String type;

    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    if (DEBUG_ENABLED)
      Serial.println("Start updating " + type);

  })

  .onEnd([]() {

    if (DEBUG_ENABLED)
      Serial.println("\nEnd");

  })

  .onProgress([](unsigned int progress, unsigned int total) {

    if (DEBUG_ENABLED)
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));

  })

  .onError([](ota_error_t error) {

    if (DEBUG_ENABLED)
    {

      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    }

  });

  ArduinoOTA.begin();

}

bool OTA_Enabled_RightNow() {

  // check 1 - are top of the hour updates only enabled, if yes is it the top of the hour?

  if (OTA_TOP_OF_THE_HOUR_ONLY)
    if (minute() == 0) {}
    else {

      if (DEBUG_ENABLED)
        Serial.println("OTA Update Window closed - only top of the hour updates allowed\n");

      return false;

    }

  // check 2 - are OTA updates enabled in the current hour?

  const String OTA_Enabled_Hours = OTA_OPEN_WINDOW_HOURS;

  if ( OTA_Enabled_Hours.length() == 24 )
  {

    bool returnvalue = ( String(OTA_Enabled_Hours.charAt(hour())) == "Y" );

    if (DEBUG_ENABLED)
      if (returnvalue)
        Serial.println("OTA Update Window opened ...\n");
      else
        Serial.println("OTA Update Window closed\n");

    return returnvalue;

  }
  else {

    if (DEBUG_ENABLED)
      Serial.println(" OTA_OPEN_WINDOW_HOURS is incorrectly configured, will assume OTA Window is open\n");

    return true;

  }

}

void  Check_For_OTA_Updates() {

  if (!OTA_Enabled_RightNow())
    return;

  Setup_OTAUpdate();

  long LoopUntil =  millis() + OTAWindowInSeconds * 1000;

  while ( (millis() < LoopUntil) || (OTAUpdateInProgress) )
    ArduinoOTA.handle();

  if (DEBUG_ENABLED) {

    Serial.println("OTA update window closed");
    Serial.println("");

  }

}

//********************************************************************************************************************************************************************

void RTC_Sleep() {

  // put the ESP32 into deepsleep mode
  // only to be awaken a the top of the next minute mark corresponding to the NumberOfMinutesBetweenReadings

  int nextWakeupMinute = ( minute() + NumberOfMinutesBetweenReadings ) % 60;

  // if ota top of the hour updates only then set the wakeup time to be the top of the next hour if it would otherwise happen after that time
  if (OTA_TOP_OF_THE_HOUR_ONLY)
    if ( nextWakeupMinute < NumberOfMinutesBetweenReadings )
      nextWakeupMinute = 0;

  //note below Alarm2 is used as the primary alarm as its fidelity is to the minutes only
  //           Alarm1 is used as the secondary  alarm as its fidelity is to the second


  // set primary alarm to wakeup at the top of the nextWakeupMinute exactly
  rtc.setAlarm(DS3232RTC::ALM2_MATCH_MINUTES, 0, nextWakeupMinute, 0, 0);
  rtc.alarm(DS3232RTC::ALARM_2);
  rtc.squareWave(DS3232RTC::SQWAVE_NONE);
  rtc.alarmInterrupt(DS3232RTC::ALARM_2, true);

  // set secondary alarm to wakeup at the nextWakeupMinut plus one second (used as a fallback to the primary alarm not working)
  rtc.setAlarm(DS3232RTC::ALM1_MATCH_MINUTES, 0, nextWakeupMinute, 1, 0);
  rtc.alarm(DS3232RTC::ALARM_1);
  rtc.squareWave(DS3232RTC::SQWAVE_NONE);
  rtc.alarmInterrupt(DS3232RTC::ALARM_1, true);

  pinMode(interruptPin, INPUT_PULLUP);
  rtc_gpio_pullup_en(interruptPin);
  rtc_gpio_pulldown_dis(interruptPin);
  esp_sleep_enable_ext0_wakeup(interruptPin, 0);

  esp_deep_sleep_start();

}

void Internal_Timer_Sleep() {

  // put the ESP32 into deepsleep mode
  // only to be awakened approx. the NumberOfMinutesBetweenReadings minutes from when this program was last started

  // top of the hour logic will not work here, as the time was not set correctly

  const int32_t MicroSecondsBetweenReadings  = NumberOfMinutesBetweenReadings * 60 * 1000000;
  int32_t TimeToSleep = MicroSecondsBetweenReadings - millis() * 1000;

  esp_sleep_enable_timer_wakeup(TimeToSleep);
  esp_deep_sleep_start();

}

void Go_to_Sleep() {

  if (DEBUG_ENABLED) {

    Serial.print("");

    Serial.print("Processing completed in ");
    float pt; pt = millis(); pt = pt / 1000;
    Serial.print(String(pt));
    Serial.println(" seconds");

    Serial.println("");
    Serial.print("Go to sleep based on the ");

  }

  if (Time_OK) {

    if (DEBUG_ENABLED) {
      Serial.println("RTC");
      Serial.println("");
    }

    RTC_Sleep();

  }

  else {

    if (DEBUG_ENABLED) {
      Serial.println("internal timer");
      Serial.println("");
    }

    Internal_Timer_Sleep();

  }

}

//********************************************************************************************************************************************************************

void Validations() {

  if (NUMBER_OF_MINUTES_BETWEEN_READINGS < 3)
    NumberOfMinutesBetweenReadings = 3;
  else if (NUMBER_OF_MINUTES_BETWEEN_READINGS > 60)
    NumberOfMinutesBetweenReadings = 60;
  else
    NumberOfMinutesBetweenReadings = NUMBER_OF_MINUTES_BETWEEN_READINGS;

  if (OTA_WINDOW_IN_SECONDS < 15)
    OTAWindowInSeconds = 15;
  else if (OTA_WINDOW_IN_SECONDS > 45)
    OTAWindowInSeconds = 45;
  else
    OTAWindowInSeconds = OTA_WINDOW_IN_SECONDS;

  // used to update the RTC from and NTP Server once a week
  NumberOfRestartsBeforeAnRTCUpdateFromAnNTPServerIsNeeded = (7 * 24 * 60 ) / NumberOfMinutesBetweenReadings;

  // on very first power-up force an update from the NTP server
  if (NumberOfRestartsSinceLastRTCUpdateFromAnNTPServer == 0)
    NumberOfRestartsSinceLastRTCUpdateFromAnNTPServer = NumberOfRestartsBeforeAnRTCUpdateFromAnNTPServerIsNeeded;

}

//********************************************************************************************************************************************************************

void setup() {

  Setup_Serial();

  Validations();

  Setup_RTC();

  Setup_Sensor_Power_Control();

  if (Setup_WiFi()) {

    Setup_Time();

    if (OTA_ENABLED)
      Check_For_OTA_Updates();

    Take_All_Readings();

    Finalize_Readings();

    Publish_Readings_to_WeatherServices();

    Publish_Readings_to_MQTTServer();

  }

  Go_to_Sleep();

}

void loop() {
}
