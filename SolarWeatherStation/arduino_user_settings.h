
#define DEBUG_ENABLED                        true                          // set to true to see displays in the Serial Monitor

#define POWER_SENSOR_CONTROLLER_PIN          26                            // used to power up and down the sensors



#define NUMBER_OF_BME280_DEVICES             2                             // Number of BME280 devices used 

#define BME280_SDA_PIN                       32                            // SDA pin for the DS3231 RTC 

#define BME280_SCL_PIN                       33                            // SCL pin for the DS3231 RTC

#define BME_SDO_PIN_NUMBERS                  {18, 25}                      // ESP32 numbers pins connecting to BME280 SDO pins; there should be an ESP32 pin for each BME280 device



#define NUMBER_OF_DHT22_DEVICES              2                             // Number of DHT22 devices used 

#define DHT_DAT_PIN_NUMBERS                  {14, 16}                      // ESP32 numbers pins connecting to DHT22 DAT pins; there should be an ESP32 pin for each DHT22 device



#define OTA_ENABLED                         true                            // Set to true to enable Over the Air (OTA) Updates; OTA updates are performed prior to taking readings / publishing results

#define  OTA_OPEN_WINDOW_HOURS              "YYYNNNNNNNNNYYYYYYYYYYYY"      // Set to all "Y"s to allow OTA updates to run at any hour 
//                                           012345678901234567890123          Set a particular value to "N" to disable OTA through out that hour
//                                           !           !          !          Hours are based on UTC time, so please consider the offset for your time zone when updating the above values
//                                           V           V          V          Above values are what I use in Ottawa, Ontario, Canada (-4 from UTC) to allow OTA updates between 8:00am and 10:59pm inclusive
//                                      UTC: Midnight    Noon       23hrs      (No use chewing up battery power for OTA updates which will never be made outside those hours)
//                                                                         

#define OTA_TOP_OF_THE_HOUR_ONLY            true                            // Set to true to enable Over the Air (OTA) Updates only to run at the top of the hour

#define OTA_WINDOW_IN_SECONDS               15                              // Window of opportunity for OTA updates at start-up (minimum is 15 seconds, maximum is 45 seconds)
 
#define OTA_HOST_NAME                       "ESP32WeatherStation"           // OTA host name


#define NUMBER_OF_MINUTES_BETWEEN_READINGS  5                               // how often the program should publish readings (minimum is 3, maximum is 60)


#define MQTT_ENABLED                        true                            // will results be published to the MQTT server

#define MQTT_CLIENT_NAME                    "WeatherStation"                // MQTTT this client name

#define MQTT_REPORT_FINALIZED_NUMBERS_ONLY  false                           // set to true to report detailed readings along with finalized ones

#define MQTT_SERVER_ADDESS                  "192.168.1.21"                  // MQTTT server address

#define MQTT_SERVER_PORT                    1883                            // MQTT port

#define MQTT_TOPIC                          "weather"                       // MQTT Topic


   
#define PWS_ENABLED                         true                            // will readings be published to pwsweather.com
   
#define PWS_STATION_ID                      "***************"               // PWSWeather Station ID; see arduino_user_secrets file for API Key



#define UNDRGRND_ENABLED                    true                             // will readings be published to weatherunderground.com

#define UNDRGRND_STATION_ID                 "*********"                     // Underground Weather Station ID; see arduino_user_secrets file for Station Key
  


#define NTP_SERVER_POOL                     "pool.ntp.org"                  // NTP pool

#define SERIAL_PORT_SPEED                   115200                          // Serial monitor baud rate

#define INTERRUPT_PIN                       GPIO_NUM_4                      // Connect this pin to the RTC SQW pin to let the RTC wake the ESP32
