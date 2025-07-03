# ESPToolbox, handy web utilities for ESP8266 and ESP32 microcontrollers

This is an Arduino sketch created in order to test few items on that microcontrollers using a simple web interface

Current utilities:
 - Blink any pin
 - I2C scanner on any pins
 - WiFi network scanner (take in mind if your ESP has 5GHz support)

It's configurable, you can use it as an AP or connected to your home Wireless



## Compilation

You can compile the sketch using Arduino IDE with ESP8266 or ESP32 support.

This sketch depends on following libreries:
 - uEspConfigLib by Naguissa - Available on Library Manager. It will pull uEEPROMLib as dependency
 - uTimerLib by Naguissa - Available on Library Manager
 - Wire - Available on ESP Arduino core



## Sketch upload

This sketch uses LittleFS in order to store configurations. You need to set a minimal SPIFFS space when uploading. It only needs few kbytes.


## Usage ##

Once uploaded it will be a new WiFi open network available, called ESPToolbox

Connect to that AP and navigate to http://192.168.4.1



## Who do I talk to? ##

 * [Naguissa](https://github.com/Naguissa)
 * English: https://www.foroelectro.net/english-f34/toolbox-arduino-sketch-for-esp8266-and-esp32-t542.html
 * Spanish: https://www.foroelectro.net/electronica-digital-microcontroladores-f8/sketch-arduino-de-caja-de-herramientas-web-para-es-t541.html


## Contribute ##

Any code contribution, report or comment are always welcome. Don't hesitate to use GitHub for that.


 * You can sponsor this project using GitHub's Sponsor button: https://github.com/Naguissa/ESPToolbox
 * You can make a donation via PayPal: https://paypal.me/foroelectro


Thanks for your support.


Contributors hall of fame: https://www.foroelectro.net/hall-of-fame-f32/contributors-contribuyentes-t271.html
