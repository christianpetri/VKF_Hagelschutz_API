#define DEBUG true  //set to true for debug output, false for no debug ouput
#define Serial if(DEBUG)Serial

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
NOTE: You're encouraged to treat the currentState values as �zero� and
�non-zero� and to not differentiate between the two Hail cases.
On failure: Some HTTP Status Error & message

ERROR REPORT:
To report an error to the server a registered client does an HTTP POST request with the following
format:
http://meteo.netitservices.com/devservice/errlog?deviceId=<DID>&hwtypeId=<HID>

URL PARAMETERS: see POLL REQUEST. Both parameters are required.
BODY: A json { �errlog� : �your error payload goes here� }
Important: Make sure you have set Content-Type key at headers to
application/json.
*/
  
//Fill in the credentials you received from VKF 
#define deviceId  ""    // The ID of the "Hagelschutzbox" (Hail protection box)
#define hwtypeId  ""    // The hwtypeId you got from VKF
 
//Example
//#define deviceId  "11TTA815ZE6C"   
//#define hwtypeId  "88"

#define URL_BASE_HAGELSCHUTZ_API "meteo.netitservices.com"  //Respondse: A json { currentState: <VAL>, newProgVer: <VAL> } currentState: 0, NO Hail currentState : 1, Hail currentState : 2, Hail state triggered by Test - alarm. newProgVer: This field can be safely ignored
char server[] = URL_BASE_HAGELSCHUTZ_API;

#define REQUEST_TO_HAGELSCHUTZ_API "/devservice/poll/?deviceId=" deviceId "&hwtypeId=" hwtypeId

#define NoHailAlarm    0
#define HailAlarm     1
#define HailAlarmTest 2
#define ErrorHailAPI  3

boolean isPullUpBlindsEvent = false;

/* Relais start */
/*
  The Ivio 868 (or Invio 915) is used for this project.
  If you you just need to pull up the blinds one relay is enough.
  With 2 Relais you can also decends the blinds...
*/


#define realyPullBlindsUpPin 6
//#define relayDecendBlinds 
/*Relais end*/

/*LED Start*/

#define ledGreenPowerOnPin      A4
#define ledGreenConnectionOkPin A3
#define ledRedErrorPin          A2
#define ledYellowHailAlarmPin   A1
/*LED End*/

/*Timer start*/ 
// Generally, you should use "unsigned long" for variables that hold time
// The value will quickly become too large for an int to store 
unsigned long previousMillis = 120000;        // will store when the last interavl was in milliseconds
unsigned long lastPullUpBlindsEvent = 0;

// constants won't change:
const long interval = 120000;           // interval in milliseconds // 120000 = 2 Minuten
/*Timer end*/


#include <SPI.h>
#include <Ethernet2.h>
// ArduinoJson - arduinojson.org
// Copyright Benoit Blanchon 2014-2018
// MIT License 
#include <ArduinoJson.h> 

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = { 0x38, 0xAC, 0xA8, 0x15, 0xF8, 0x6C };
  
// Set the static IP address to use if the DHCP fails to assign
IPAddress ip(192, 168, 1, 177);

// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 80 is default for HTTP):
EthernetClient client;

  void setup() {
    // Initialize Serial port
    Serial.begin(9600);  
    /*Setup LEDs*/
    pinMode(ledGreenPowerOnPin      , OUTPUT); 
    pinMode(ledGreenConnectionOkPin , OUTPUT);
    pinMode(ledRedErrorPin          , OUTPUT);
    pinMode(ledYellowHailAlarmPin   , OUTPUT);

    //Test all LEDs
    digitalWrite(ledGreenPowerOnPin     , HIGH);
    digitalWrite(ledGreenConnectionOkPin, HIGH);
    digitalWrite(ledRedErrorPin         , HIGH);
    digitalWrite(ledYellowHailAlarmPin  , HIGH);
    delay(1000);
    digitalWrite(ledGreenConnectionOkPin, LOW);
    digitalWrite(ledRedErrorPin         , LOW);
    digitalWrite(ledYellowHailAlarmPin  , LOW);
    
     /*Setup relais*/
    pinMode(realyPullBlindsUpPin, OUTPUT);
    digitalWrite(realyPullBlindsUpPin, LOW); 
    // Initialize Ethernet library    
    if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP"); 
    // try to congifure using IP address instead of DHCP:
    Ethernet.begin(mac, ip);
  }
    delay(1000); 
    Serial.println(("Connecting to local network...")); 
  }

  void loop() { 
    if (isTimerIntervalDone()) {  
      Serial.println("");
      Serial.println("******************************************************");
      Serial.println("Time Intervall");
      
      int apiResonse = makeGetRequestToHagelschutzAPI();
      printResponseToConsole(apiResonse);
      handleBlinds(apiResonse);
      handleLEDsAccoringToTheResponse(apiReson se);
      Serial.println("Wait for 2 minutes until the next request is sent.");       
    } 
    pullBlindsUp();
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
    if (!client.connect( server , 80)) {
      Serial.println(("Connection failed"));
      return ErrorHailAPI;
    }

    Serial.println(("Connected!"));

    // Send HTTP request
    client.println("GET " REQUEST_TO_HAGELSCHUTZ_API " HTTP/1.0");
    client.println("Host: " URL_BASE_HAGELSCHUTZ_API);
    client.println("Connection: close");
     if (client.println() == 0) {
    Serial.println(F("Failed to send request"));
    return ErrorHailAPI;
  }

  // Check HTTP status
  char status[32] = {0};
  client.readBytesUntil('\r', status, sizeof(status));
  if (strcmp(status, "HTTP/1.1 200 OK") != 0) {
    Serial.println(F("Unexpected response: "));
    Serial.println(status);
    return ErrorHailAPI;
  }

  // Skip HTTP headers
  char endOfHeaders[] = "\r\n\r\n";
  if (!client.find(endOfHeaders)) {
    Serial.println(F("Invalid response"));
    return ErrorHailAPI;
  }


    const size_t bufferSize = JSON_OBJECT_SIZE(2) + 40;
    DynamicJsonBuffer jsonBuffer(bufferSize);
    
    //const char* json = "{\"currentState\":0,\"newProgVer\":1}";
    
    JsonObject& root = jsonBuffer.parseObject(client);

   // Extract values
   int currentState = root["currentState"]; // 0
   int newProgVer = root["newProgVer"]; // 1 
 
    if (!root.success()) {
      Serial.println(F("Parsing failed!"));
      return ErrorHailAPI;
    } 
    //root.printTo(Serial);  // DEBUG must be set to true
    Serial.println();
    // Disconnect
    client.stop();
    return currentState; 
  }

  void printResponseToConsole(int response) {
    /*
    currentState  : 0, NO Hail
    currentState  : 1, Hail
    currentState  : 2, Hail state triggered by Test - alarm
    response      : 3 couldn't make get request
    */
    if (response == NoHailAlarm) {
      Serial.println(("Response: NO Hail Alarm")); 
    } 
    else if (response == HailAlarm) {
      Serial.println(("Respondse: Hail Alarm!")); 
    } 
    else if (response == HailAlarmTest) {
      Serial.println(("Respondse: Hail Test Alarm!"));
       
    }
    else if (response == ErrorHailAPI) {
      Serial.println(("Respondse: Failed Request")); 
    } 
  }
  void handleLEDsAccoringToTheResponse(int response) { 
    //Turn LEDs off 
    digitalWrite(ledGreenConnectionOkPin, LOW);
    digitalWrite(ledRedErrorPin, LOW);
    digitalWrite(ledYellowHailAlarmPin, LOW);
    
    if (response == NoHailAlarm) { 
      digitalWrite(ledGreenConnectionOkPin, HIGH);
    } 
    else if (response == HailAlarm) { 
      digitalWrite(ledGreenConnectionOkPin, HIGH);
      digitalWrite(ledYellowHailAlarmPin, HIGH);
    } 
    else if (response == HailAlarmTest) { 
      digitalWrite(ledGreenConnectionOkPin, HIGH);
      digitalWrite(ledYellowHailAlarmPin, HIGH);
       
    }
    else if (response == ErrorHailAPI) { 
      digitalWrite(ledRedErrorPin, HIGH);
    } 
  }
  void handleBlinds(int response) {
    /*
    currentState: 0, NO Hail
    currentState : 1, Hail
    currentState : 2, Hail state triggered by Test - alarm
    */
    if (response == NoHailAlarm) {
      Serial.println(("Blinds: No hail alert -> Do nothing")); 
    }
    else if (response == HailAlarm) {
      Serial.println(("Blinds: Hail Alert -> Pull up!")); 
      isPullUpBlindsEvent = true;
    }
    else if (response == HailAlarmTest) {
      Serial.println(("Blinds: Hail alert TEST -> Pull up!")); 
      isPullUpBlindsEvent = true;
    }
    else if (response == ErrorHailAPI) {
      Serial.println(("Blinds: ERROR -> Do nothing")); 
    } 
  }

 void pullBlindsUp() { 
  if (isPullUpBlindsEvent) {
    isPullUpBlindsEvent = false;
    lastPullUpBlindsEvent = millis();
    digitalWrite(realyPullBlindsUpPin, HIGH); 
    Serial.println("Pulling up!"  ); 
  } 

  if(millis()-lastPullUpBlindsEvent >  30000){
    digitalWrite(realyPullBlindsUpPin, LOW);
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
  
