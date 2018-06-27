/*
POLL REQUEST:
To fetch the signal for a registered endpoint/location the client does an HTTP GET request with the
following format:
http://meteo.netitservices.com/devservice/poll?deviceId=<DID>&hwtypeId=<HID>&version=<VER>&firstpoll=true
REQUIRED PARAMETERS:
deviceId: 12 byte long identifier unique for each registered device
hwtypeId: Identifier (integer) distinguishing the type of the device
NOTE: Both these parameters do not change among requests from the
same endpoint and shall be provided by the VKF administrator.
OPTIONAL PARAMETERS (useful for monitoring):
version: Software version of polling endpoint
firstpoll: (=true) Used only during the first request to the server after a long
period of inactivity or a system reset. Omit completely on following
requests.
POLL RESPONSE:
On success: A json { currentState: <VAL>, newProgVer: <VAL> }
currentState: 0, NO Hail
currentState: 1, Hail
currentState: 2, Hail state triggered by Test-alarm
newProgVer: This field can be safely ignored
NOTE: You're encouraged to treat the currentState values as “zero” and
“non-zero” and to not differentiate between the two Hail cases.
On failure: Some HTTP Status Error & message

ERROR REPORT:
To report an error to the server a registered client does an HTTP POST request with the following
format:
http://meteo.netitservices.com/devservice/errlog?deviceId=<DID>&hwtypeId=<HID>

URL PARAMETERS: see POLL REQUEST. Both parameters are required.
BODY: A json { „errlog“ : „your error payload goes here“ }
Important: Make sure you have set Content-Type key at headers to
application/json.
*/

//Enter your Credential in the ApiKeys.h file

#include "ApiKeys.h"

//Write the text below into the ApiKey.h file
/*
#pragma once
//Fill in the credentials you received from VKF WITH " at the beginning and " at the end
//#define deviceId  "YourID"			//The ID of the "Hagelschutzbox"
//#define hwtypeId  "YourHwTypeID"		//Definded by the VKF, see documentaion  
*/

#define URL_BASE_HAGELSCHUTZ_API "meteo.netitservices.com"
#define URL_FOR_GET_REQUEST_TO_THE_HAGELSCHUTZ_API "meteo.netitservices.com/devservice/poll/"  //Respondse: A json { currentState: <VAL>, newProgVer: <VAL> } currentState: 0, NO Hail currentState : 1, Hail currentState : 2, Hail state triggered by Test - alarm. newProgVer: This field can be safely ignored

//#define deviceId  ""	//The ID of the "Hagelschutzbox"
//#define hwtypeId  ""				//Definded by the VKF, see documentaion 

#define REQUEST_TO_HAGELSCHUTZ_API "?deviceId=" deviceId "&hwtypeId=" hwtypeId

#define NoHailAlarm		0
#define HailAlarm		1
#define HailAlarmTest	2
#define ErrorHailAPI	3

 
/*Timer start*/ 
// Generally, you should use "unsigned long" for variables that hold time
// The value will quickly become too large for an int to store 
unsigned long previousMillis = 0;        // will store when the last interavl was in milliseconds

// constants won't change:
const long interval = 120000;           // interval in milliseconds // 120000 = 2 Minuten
/*Timer end*/
  
// ArduinoJson - arduinojson.org
// Copyright Benoit Blanchon 2014-2018
// MIT License 

#include <ArduinoJson.h>
#include <Ethernet.h>
#include <SPI.h>

	void setup() {
		// Initialize Serial port
		Serial.begin(9600); 
		 
		//Turn off the RF Module
		pinMode(8, INPUT_PULLUP);

		// Initialize Ethernet library
		byte mac[] = { 0x38, 0xAC, 0xA8, 0x15, 0xF8, 0x6C };
		if (!Ethernet.begin(mac)) {
			Serial.println(F("Failed to configure Ethernet")); 
		}
		delay(1000); 
		Serial.println(F("Connecting...")); 
	}

	void loop() { 
		if (isTimerIntervalDone()) {	
			int apiResondse = makeGetRequestToHagelschutzAPI();
			printResponseToConsole(apiResondse);
			handleBlinds(apiResondse);  
		}
	}

	boolean isTimerIntervalDone() {
		if (millis() - previousMillis > interval) {
			previousMillis = millis();
			return true; 
		}
		else {
			return false;
		} 
	}

	int makeGetRequestToHagelschutzAPI() {
		EthernetClient client;
		client.setTimeout(10000);
		// Connect to HTTP server
		if (!client.connect( URL_FOR_GET_REQUEST_TO_THE_HAGELSCHUTZ_API, 80)) {
			Serial.println(F("Connection failed"));
			return 3;
		}

		Serial.println(F("Connected!"));

		// Send HTTP request
		client.println("GET" REQUEST_TO_HAGELSCHUTZ_API " HTTP/1.0");
		client.println(F("Host: " URL_BASE_HAGELSCHUTZ_API));
		client.println(F("Connection: close"));
		if (client.println() == 0) {
			Serial.println(F("Failed to send request"));
			return 3;
		}

		 
		// Check HTTP status

		char status[32] = { 0 };
		client.readBytesUntil('\r', status, sizeof(status)); 
		if (strcmp(status, "HTTP/1.1 200 OK") != 0) { 
			Serial.print(F("Unexpected response: "));
			Serial.println(status);
			return 3;
		}
		 

		// Skip HTTP headers
		char endOfHeaders[] = "\r\n\r\n";
		if (!client.find(endOfHeaders)) {
			Serial.println(F("Invalid response"));
			return 3;
		}

		// Allocate JsonBuffer
		// Use arduinojson.org/assistant to compute the capacity.
		const size_t capacity = JSON_OBJECT_SIZE(3) + JSON_ARRAY_SIZE(2) + 60;
		DynamicJsonBuffer jsonBuffer(capacity);

		// Parse JSON object
		JsonObject& root = jsonBuffer.parseObject(client);
		if (!root.success()) {
			Serial.println(F("Parsing failed!"));
			return 3;
		}

		// Extract values
		Serial.println(F("Response:"));
		int currentState = root["currentState"];
		// Disconnect
		client.stop();  
		Serial.println(currentState);
		return currentState;
	}

	void printResponseToConsole(int response) {
		/*
		currentState	: 0, NO Hail
		currentState	: 1, Hail
		currentState	: 2, Hail state triggered by Test - alarm
		response		: 3 couldn't make get request
		*/
		if (response == NoHailAlarm) {
			Serial.println(F("Respondse: NO Hail Alarm"));
			 
		} 
		else if (response == HailAlarm) {
			Serial.println(F("Respondse: Hail Alarm!"));
			 
		} 
		else if (response == HailAlarmTest) {
			Serial.println(F("Respondse: Hail Test Alarm!"));
			 
		}
		else if (response == ErrorHailAPI) {
			Serial.println(F("Respondse: Failed GET Request"));
			 
		} 
	}

	void handleBlinds(int response) {
		/*
		currentState: 0, NO Hail
		currentState : 1, Hail
		currentState : 2, Hail state triggered by Test - alarm
		*/
		if (response == NoHailAlarm) {
			Serial.println(F("Blinds: No hail alert -> Do nothing")); 
		}
		else if (response == HailAlarm) {
			Serial.println(F("Blinds: Hail Alert -> Pull up!")); 
		}
		else if (response == HailAlarmTest) {
			Serial.println(F("Blinds: Hail alert TEST -> Pull up!")); 
		}
		else if (response == ErrorHailAPI) {
			Serial.println(F("Blinds: ERROR Do nothing")); 
		} 
	}
	 
	// See also
	// --------
	//
	// The website arduinojson.org contains the documentation for all the functions
	// used above. It also includes an FAQ that will help you solve any
	// serialization  problem.
	// Please check it out at: https://arduinojson.org/
	//
	// The book "Mastering ArduinoJson" contains a tutorial on deserialization
	// showing how to parse the response from Yahoo Weather. In the last chapter,
	// it shows how to parse the huge documents from OpenWeatherMap
	// and Weather Underground.
	// Please check it out at: https://arduinojson.org/book/
  