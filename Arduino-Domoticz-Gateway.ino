/*Arduino based Domoticz gateway receives messages from Arduino temp and temp + hum loggers via 433MHz RF link.
  Received data is parsed and send to MQTT server, using MQTT protocol, running on a Raspberry Pi via ethernet.
  The Domoticz gateway also measures temperature from DS18B20 digital temperature sensor. Measured temperature values are send to
  the Domoticz via the MQTT protocol. The MQTT server delivers data to Domoticz server running on a same Raspberry Pi.
 */

#include <VirtualWire.h>
#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Time.h>
#include <TimeAlarms.h>

// Variables used to store received data
char *topicID; // MQTT topicID
char *temp; // Received temperature data will be stored to this variable
char *hum; // Received humidity data will be stored to this variable
char *dtype; // Received dtype (device type) will be stored to this variable
int deviceType = 0; // deviceType is a int value converted from *type pointer variable

// HW settings
const int RxLed = 13; // RxLed will be lit when RX433N received valid data
const int RxPin = 2; // Data pin of RX433 is connected to this pin

// Network settings
byte mac[] = { 0x90, 0xA5, 0xDA, 0x0D, 0xCC, 0x55 }; // this must be unique
EthernetClient ethernetClient;
int mqttConnectionFails = 0; // If MQTT connection is disconnected for some reason, this variable is increment by 1

//MQTT configuration
#define  DEVICE_ID  "Uno"
#define MQTT_SERVER "192.168.1.2" // IP address of MQTT server. CHANGE THIS TO CORRECT ONE!

//MQTT initialisation
PubSubClient mqttClient(MQTT_SERVER, 1883, sensorCallback, ethernetClient);
char clientID[50];
char topic[50];
char msg[80];

//Temperature measurement configuration
const unsigned int tempMeasInterval = 1800; // Set temperature measurement interval in seconds. Default value 1800s (30min)
float temperature = 0; // Measured temperature value is stored to this variable

// OneWire and Dallas temperature sensors library are initialised
#define ONE_WIRE_BUS 7 // OneWire data wire is plugged into pin 7 on the Arduino. Parasite powering scheme is used.
OneWire oneWire(ONE_WIRE_BUS); // Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
DallasTemperature sensors(&oneWire); // Pass our oneWire reference to Dallas Temperature.

//#define RAM_DEBUG // Comment this line out if there is no need to print amount of free RAM memory via serial port

void setup()
{
  Serial.begin(9600); // Start serial port

  pinMode(RxLed, OUTPUT); //RxLed is set to output
  vw_set_rx_pin(RxPin); // IO is set for data receiving
  vw_setup(2000); // Rx433N bits per second rate is set
  vw_rx_start(); // Start the receiver PLL running

  // SD Card SPI CS signal configured to output and set to high state. SD Card and ethernet controller shares same SPI bus
  pinMode(4,OUTPUT);
  digitalWrite(4,HIGH);

  // Start Ethernet on Arduino
  startEthernet();

  //Create MQTT client String
  String clientIDStr = "Arduino-";
  clientIDStr.concat(DEVICE_ID);
  clientIDStr.toCharArray(clientID, clientIDStr.length()+1);

  //MQTT connection is established
  mqttClient.connect(clientID) ? Serial.println("MQTT client connected") : Serial.println("MQTT client connection failed..."); //condition ? valueIfTrue : valueIfFalse - This is a Ternary operator

  // Start up the Dallas temperature library
  sensors.begin();

  // Timers are initialised
  Alarm.timerRepeat(tempMeasInterval, tempFunction);  // Temperature measurement and delivery function is run every 30min

  Serial.println(F("Setup done"));
}

void loop()
{

	uint8_t buf[VW_MAX_MESSAGE_LEN];
	uint8_t buflen = VW_MAX_MESSAGE_LEN;

    // If there are incoming bytes available from the server, read them and print them out.
	// This is for debugging purposes only. Not needed with current sketch.
	/*
    while(ethernetClient.available())
    {
    	Serial.print(F("Following data read from the server: "));
    	char c = ethernetClient.read();
    	Serial.print(c);
    	Serial.println();
    }*/

    // Receive messages from Temperature subsystem sent via RX433N transmitter

    if (vw_get_message(buf, &buflen)) // Non-blocking
		{
			digitalWrite(RxLed, HIGH); // Flash a LED to show received good message

		// If message with a good checksum received, print it.
		Serial.print(F("Received data: "));
		for (int i = 0; i < buflen; i++)
		{
			Serial.print((char)buf[i]);
		}

		Serial.println();
		digitalWrite(RxLed, LOW);

		char *rest; // help variable

		Serial.println("Starting parsing received data...");

		//  First strtok iteration. Parses topicID out from the received data.
		topicID = strtok_r((char*)buf,":",&rest);
		Serial.print("topicID = ");
		Serial.println(topicID);

		//  Second strtok iteration. Parses dtype out from the received data.
		dtype = strtok_r(NULL,":",&rest);
		Serial.print("dtype = ");
		Serial.println(dtype);
		deviceType = atoi (dtype); // dtype is converted to int in order to use it with switch structure below and other functions

		//  Third strtok iteration. Parses temperature out from the received data.
		temp = strtok_r(NULL,":",&rest);
		Serial.print("temp = ");
		Serial.println(temp);

		if (deviceType == 82) // Humidity to be parsed only from combined temp & hum sensors
		{
			//  Fourth strtok iteration. Parses humidity out from the received data.
			hum = strtok_r(NULL,":",&rest);
			Serial.print("hum = ");
			Serial.println(hum);
		}

		Serial.println("Received data parsed...");

		//Send data to MQTT broker running on a Raspberry Pi
		sendMQTTPayload(createMQTTPayload());

		#if defined RAM_DEBUG
		Serial.print(F("Amount of free RAM memory: "));
		Serial.print(memoryFree()); // Prints the amount of free RAM memory
		Serial.println(F(" / 2048 bytes")); //ATmega328 has 2kB of RAM memory
		#endif
		}

    Alarm.delay(0); //Timers are only checks and their functions called when you use this delay function. You can pass 0 for minimal delay.

    mqttClient.loop(); //This should be called regularly to allow the MQTT client to process incoming messages and maintain its connection to the server.
}


float tempReading() // Function tempReading reads temperature from DS18B20 sensor and returns it
{
  sensors.requestTemperatures(); // Send the command to get temperatures
  return (float) sensors.getTempCByIndex(0); // Return measured temperature value
}

void tempFunction() // Function tempFunction reads temperature from DS18B20 sensor and sends it to Domoticz via MQTT protocol
{
  temperature = tempReading(); // Current temperature is measured

  // Read temperature is printed
  Serial.print(F("Temperature: "));
  Serial.print(temperature);
  Serial.println(F("DegC"));

  topicID = "Meriroom";
  dtype = "80";
  deviceType = atoi (dtype); // dtype is converted to int in order to use it with switch structure below and other functions

  //Measured temperature (float) is converted to char array
  char charVal[10];	//Temporarily holds data for dtostrf function
  dtostrf(temperature, 4, 1, charVal);  //4 is minimum width, 1 is precision; float value is copied into buff
  temp = charVal;

  //Send data to MQTT broker running on a Raspberry Pi
  sendMQTTPayload(createMQTTPayload());
}

String createMQTTPayload() //Create MQTT message payload. Returns created message as a String.
{

	String dataMsg = "{\"dunit\":1,";
	dataMsg.concat(F("\"dtype\":"));

	switch (deviceType)
	  {
		case 82: // Combined temperature and humidity sensor
	    	dataMsg.concat(dtype);
	    	dataMsg.concat(F(",\"dsubtype\":1,"));
	    	dataMsg.concat(F("\"svalue\":\""));
	    	dataMsg.concat(temp);
	    	dataMsg.concat(F(";"));
	    	dataMsg.concat(hum);
	    	dataMsg.concat(F(";0"));
	    	break;

		case 80: // Temperature sensor
			dataMsg.concat(dtype);
	    	dataMsg.concat(F(",\"dsubtype\":1,"));
	    	dataMsg.concat(F("\"svalue\":\""));
	    	dataMsg.concat(temp);
	    	break;

		default: // If dtype is unknown, then default case to be used. Default case is a temperature sensor.
			Serial.println("Unknown dtype received. Default procedure to be done.");
			dataMsg.concat(dtype);
	    	dataMsg.concat(F(",\"dsubtype\":1,"));
	    	dataMsg.concat(F("\"svalue\":\""));
	    	dataMsg.concat(temp);
	    	break;
	  }

	dataMsg.concat(F("\"}"));
	return dataMsg;
}

void sendMQTTPayload(String payload) // Sends MQTT payload to the Mosquitto server running on a Raspberry Pi.
// Mosquitto server deliveres data to Domoticz server running on a same Raspberry Pi
{
    //If connection to MQTT broker is disconnected. Connect and subscribe again
    if (!mqttClient.connected())
    {
    	(mqttClient.connect(clientID)) ? Serial.println(F("MQTT client connected")) : Serial.println(F("MQTT client connection failed...")); //condition ? valueIfTrue : valueIfFalse - This is a Ternary operator

    	mqttConnectionFails +=1; // // If MQTT connection is disconnected for some reason, this variable is increment by 1

    	// Ethernet connection to be disconnected and initialized again if MQTT connection has been disconnected 5 times
    	if (mqttConnectionFails >= 5)
    	{
    		Serial.println(F("Ethernet connection to be initialized again!"));
    		mqttConnectionFails = 0;
    		startEthernet();
    	}
    }

    // Create MQTT topic and convert it to char array
	String topicStr = "/actions/domoticz/";
	topicStr.concat(topicID);
	topicStr.toCharArray(topic, topicStr.length()+1);

	// Convert payload to char array
	payload.toCharArray(msg, payload.length()+1);

	//Publish payload to MQTT broker
	if (mqttClient.publish(topic, msg))
	{
		Serial.print("Following data published to MQTT broker: ");
		Serial.print(topic);
		Serial.print(" ");
		Serial.println(payload);
		Serial.println();
	}
	else
		Serial.println(F("Publishing to MQTT broker failed..."));

}

void startEthernet()
{
  ethernetClient.stop();

  Serial.println(F("Connecting Arduino to network..."));

  delay(1000);

  // Connect to network and obtain an IP address using DHCP
  if (Ethernet.begin(mac) == 0)
  {
    Serial.println(F("DHCP Failed, reset Arduino to try again\n"));
  }
  else
  {
	  Serial.println(F("Arduino connected to network using DHCP"));
	  Serial.print(F("IP address: "));
	  Serial.println(Ethernet.localIP()); // print your local IP address:
  }
  delay(1000);
}

// variables created by the build process when compiling the sketch. Used in memoryFree function
extern int __bss_end;
extern void *__brkval;

int memoryFree() //Function to return the amount of free RAM
{
  int freeValue;
  if((int)__brkval == 0)
  {
    freeValue = ((int)&freeValue) - ((int)&__bss_end);
  }
  else
  {
    freeValue = ((int)&freeValue) - ((int)__brkval);
  }
  return freeValue;
}

/* Handles message arrived on subscribed topic(s).
   In this sketch we do not subscribe to any topics, so this is a dummy method*/
void sensorCallback(char* topic, byte* payload, unsigned int length) { }
