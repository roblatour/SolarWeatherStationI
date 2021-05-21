/*

  The following are example commands for use on a Mosquitto MQTT server to subscribe to publications from the Weather Station:
  
    mosquitto_sub -v -t weather/temperature
    mosquitto_sub -v -t weather/pressure
    mosquitto_sub -v -t weather/humidity
    mosquitto_sub -v -t weather/temperature -t weather/pressure -t weather/humidity
  
  
  A simpler way to subscribe to everything is:
  
    mosquitto_sub -v -t "#"
  
  
  If you would like a time stamp added to the output, tack on the following:
  
    | xargs -d$'\n' -L1 bash -c 'date "+%Y-%m-%d %T.%3N $0"'
  
  for example:

    mosquitto_sub -v -t "#" | xargs -d$'\n' -L1 bash -c 'date "+%Y-%m-%d %T.%3N $0"'
    
    mosquitto_sub -v -t weather/temperature -t weather/pressure -t weather/humidity | xargs -d$'\n' -L1 bash -c 'date "+%Y-%m-%d %T.%3N $0"'

  
  To stop monitoring:

    Ctrl-C
    
  
*/
