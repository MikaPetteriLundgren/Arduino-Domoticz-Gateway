Arduino-Domoticz-Gateway
=================

Arduino based Domoticz gateway receives messages from Arduino temp and temp + hum loggers via 433MHz RF link.
Received data is parsed and send to MQTT server, using MQTT protocol, running on a Raspberry Pi via ethernet.
The Domoticz gateway also measures temperature from DS18B20 digital temperature sensor. Measured temperature values are sent to
the Domoticz via the MQTT protocol. The MQTT server delivers data to Domoticz server running on a same Raspberry Pi.

More information about Arduino based temp and temp + hum loggers can be found via following links:

[Temp logger](https://github.com/MikaPetteriLundgren/Arduino-Temp-Logger)

[Temp and hum logger](https://github.com/MikaPetteriLundgren/Arduino-Temp-Hum-Logger)
 
Arduino-Domoticz-Gateway sketch will need following HW and SW libraries to work:

**HW**

* Arduino and ethernet shield
* DS18B20 temperature sensor
* RX433 RF receiver

**Libraries**

* DallasTemperature for DS18B20 temp sensor
* OneWire for communication with DS18B20 sensor
* SPI for ethernet shield
* Ethernet for ethernet communication
* PubSubClient for MQTT communication
* Time and TimeAlarms for alarm functionality
* VirtualWire for RF communication

**HW connections**

Temperature readings are read from DS18B20 digital temperature sensor via OneWire bus.
TX433N RF receiver is connected to the Arduino via single GPIO pin.

This repository includes also breadboard and schematics pictures in PDF and Fritzing formats.

**Functionality of the sketch**

Sketch constantly checks has it has received a message via 433MHz RF link from the Arduino based temp or temp + hum loggers. 
If the message has been received, temperature or temperature and humidity values are parsed from the message.

Domoticz gateway also measures temperature itself by using DS18B20 digital temperature sensor. Measurement interval is handled with
timer function which will trigger after measurement interval has been elapsed.

The parsed and measured values are added to the MQTT payload (payload differs depends on a type of a sensor (temp vs temp + hum)). 
The created MQTT payload is sent to MQTT server.

If MQTT connection has been disconnected 5 times for some reason, an ethernet connection will be initialized again.

It's possible to print amount of free RAM memory of Arduino via serial port by uncommenting `#define RAM_DEBUG` line

Note! It's possible to add several Arduino based temp and temp + hum loggers to the system without modifying code of the 
Arduino Domoticz Gateway by using unique MQTT topicIDs.