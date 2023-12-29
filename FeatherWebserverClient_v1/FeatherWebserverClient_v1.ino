#define DEBUG true // set to true for debug output, false for no debug ouput
#define Serial \
  if (DEBUG)   \
  Serial

boolean isTimerIntervalDone(); // test
int makeGetRequestToHagelschutzAPI();
void printResponseToConsole(int response);
void handleLEDsAccordingToTheResponse(int response);
void handleBlinds(int response);
bool isHailAlert(int response);
void handleBlinds(bool = false);

/*
GET /api/v1/devices/{sn}/poll
https://meteo.netitservices.com/api/v1/devices/<deviceId>/poll?hwtypeId=<HID>

Gives detailed information about the current hail status and software version
This endpoint is the successor to the /{sn}/poll endpoint. It gives more detailed information, for instance separated wind and hail alarm levels and signals, wind speeds, and so on.

VKF Signalboxes poll every 2 minutes / 120 seconds to get the latest alarm information.

Important: An API Client must not poll more frequently than 120 seconds since this has no advantage but just produces unnecessary traffic. Clients that do not follow this rule will get blocked at some point.

currentState can have 3 values:
0 --> no alarm
1 --> alarm (wind and hail combined)
2 --> test alarm (wind and hail combined)

hailState can have 3 values:
0 --> no alarm
1 --> alarm (hail)
2 --> test alarm (hail)

windState can have multiple values:
0 --> no alarm
1-4 --> wind alarm level 1-4
5-8 --> wind test alarm for wind level 1-4 (e.g. 5 --> test alarm wind level 1)

newProgVer is only used by VKF Signalboxes and should be ignored by API clients.

For any non-VKF Signalbox the hwtypeId is mandatory and the version is unused
*/

// Fill in the credentials you received from VKF
#define deviceId "" // The ID of the "Hagelschutzbox" (Hail protection box)
#define hwtypeId "" // The hwtypeId you got from VKF

// Example
// #define deviceId  "11TTA815ZE6C"
// #define hwtypeId  "88"

#define URL_BASE_HAGELSCHUTZ_API "meteo.netitservices.com" // Response: A json { currentState: <VAL>, newProgVer: <VAL> } currentState: 0, NO Hail currentState : 1, Hail currentState : 2, Hail state triggered by Test - alarm. newProgVer: This field can be safely ignored
char server[] = URL_BASE_HAGELSCHUTZ_API;

#define REQUEST_TO_HAGELSCHUTZ_API "/api/v1/devices/" deviceId "/poll?hwtypeId=" hwtypeId

#define NoHailAlarm 0
#define HailAlarm 1
#define HailAlarmTest 2
#define ErrorHailAPI 3

/* Relais start */
/*
  The Invio 868 (or Invio 915) is used for this project.
  If you you just need to pull up the blinds one relay is enough.
  With 2 Relais you can also decends the blinds...
*/

#define realyPullBlindsUpPin 6
// #define relayDecendBlinds
/*Relais end*/

/*LED Start*/

#define ledGreenPowerOnPin A4
#define ledGreenConnectionOkPin A3
#define ledRedErrorPin A2
#define ledYellowHailAlarmPin A1
/*LED End*/

/*Timer start*/
// Generally, you should use "unsigned long" for variables that hold time
// The value will quickly become too large for an int to store
unsigned long previousMillis = 120000; // will store when the last interavl was in milliseconds
unsigned long lastPullUpBlindsEvent = 0;

// constants won't change:
const long interval = 120000; // interval in milliseconds // 120000 = 2 Minuten
/*Timer end*/

#include <SPI.h>
#include <Ethernet2.h>
// ArduinoJson - arduinojson.org
// Copyright Benoit Blanchon 2014-2018
// MIT License
#include <ArduinoJson.h>

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = {0x38, 0xAC, 0xA8, 0x15, 0xF8, 0x6C};

// Set the static IP address to use if the DHCP fails to assign
IPAddress ip(192, 168, 1, 177);

// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 80 is default for HTTP):
EthernetClient client;

void setup()
{
  // Initialize Serial port
  Serial.begin(9600);
  /*Setup LEDs*/
  pinMode(ledGreenPowerOnPin, OUTPUT);
  pinMode(ledGreenConnectionOkPin, OUTPUT);
  pinMode(ledRedErrorPin, OUTPUT);
  pinMode(ledYellowHailAlarmPin, OUTPUT);

  // Test all LEDs
  digitalWrite(ledGreenPowerOnPin, HIGH);
  digitalWrite(ledGreenConnectionOkPin, HIGH);
  digitalWrite(ledRedErrorPin, HIGH);
  digitalWrite(ledYellowHailAlarmPin, HIGH);
  delay(1000);
  digitalWrite(ledGreenConnectionOkPin, LOW);
  digitalWrite(ledRedErrorPin, LOW);
  digitalWrite(ledYellowHailAlarmPin, LOW);

  /*Setup relais*/
  pinMode(realyPullBlindsUpPin, OUTPUT);
  digitalWrite(realyPullBlindsUpPin, LOW);
  // Initialize Ethernet library
  if (Ethernet.begin(mac) == 0)
  {
    Serial.println("Failed to configure Ethernet using DHCP");
    // try to congifure using IP address instead of DHCP:
    Ethernet.begin(mac, ip);
  }
  delay(1000);
  Serial.println(("Connecting to local network..."));
}

void loop()
{
  if (isTimerIntervalDone())
  {
    Serial.println("");
    Serial.println("******************************************************");
    Serial.println("Time Intervall");

    int apiResponse = makeGetRequestToHagelschutzAPI();
    handleLEDsAccordingToTheResponse(apiResponse);
    if (DEBUG)
      printResponseToConsole(apiResponse);
    handleBlinds(isHailAlert(apiResponse));
    Serial.println("Wait for 2 minutes until the next request is sent.");
  }
  handleBlinds();
  delay(1000); //Wait for 1000 ms = 1 s
}

boolean isTimerIntervalDone()
{
  if (millis() - previousMillis > interval)
  {
    previousMillis = millis();
    return true;
  }
  else
  {
    return false;
  }
}

int makeGetRequestToHagelschutzAPI()
{
  EthernetClient client;
  client.setTimeout(10000);
  // Connect to HTTP server
  if (!client.connect(server, 80))
  {
    Serial.println(("Connection failed"));
    return ErrorHailAPI;
  }

  Serial.println(("Connected!"));
  Serial.println((URL_BASE_HAGELSCHUTZ_API));

  Serial.println((REQUEST_TO_HAGELSCHUTZ_API));
  // Send HTTP request
  client.println("GET " REQUEST_TO_HAGELSCHUTZ_API " HTTP/1.1");
  client.println("Host: " URL_BASE_HAGELSCHUTZ_API);
  client.println("Connection: close");
  if (client.println() == 0)
  {
    Serial.println(F("Failed to send request"));
    return ErrorHailAPI;
  }

  // Check HTTP status
  char status[32] = {0};
  client.readBytesUntil('\r', status, sizeof(status));
  if (strcmp(status, "HTTP/1.1 200 OK") != 0)
  {
    Serial.println(F("Unexpected response: "));
    Serial.println(status);
    return ErrorHailAPI;
  }

  // Skip HTTP headers
  char endOfHeaders[] = "\r\n\r\n";
  if (!client.find(endOfHeaders))
  {
    Serial.println(F("Invalid response"));
    return ErrorHailAPI;
  }

  // Go to https://arduinojson.org/v6/assistant/ to calculate the e.g. <96> value for the given JSON
  //{"currentState":0,"newProgVer":0,"hailState":0}
  // Stream& input;
  StaticJsonDocument<96> doc;

  DeserializationError error = deserializeJson(doc, client);

  // Disconnect
  client.stop();

  if (error)
  {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return ErrorHailAPI;
  }

  int currentState = doc["currentState"]; // 0
  // int newProgVer = doc["newProgVer"];    // 0
  // int hailState = doc["hailState"];      // 0

  return currentState;
}

void printResponseToConsole(int response)
{
  /*
  currentState  : 0, NO Hail
  currentState  : 1, Hail
  currentState  : 2, Hail state triggered by Test - alarm
  response      : 3 couldn't make get request
  */
  if (response == NoHailAlarm)
  {
    Serial.println(("Response: NO Hail Alarm"));
  }
  else if (response == HailAlarm)
  {
    Serial.println(("Response: Hail Alarm!"));
  }
  else if (response == HailAlarmTest)
  {
    Serial.println(("Response: Hail Test Alarm!"));
  }
  else if (response == ErrorHailAPI)
  {
    Serial.println(("Response: Failed Request"));
  }
}
void handleLEDsAccordingToTheResponse(int response)
{
  // Turn LEDs off
  digitalWrite(ledGreenConnectionOkPin, LOW);
  digitalWrite(ledRedErrorPin, LOW);
  digitalWrite(ledYellowHailAlarmPin, LOW);

  if (response == NoHailAlarm)
  {
    digitalWrite(ledGreenConnectionOkPin, HIGH);
  }
  else if (response == HailAlarm)
  {
    digitalWrite(ledGreenConnectionOkPin, HIGH);
    digitalWrite(ledYellowHailAlarmPin, HIGH);
  }
  else if (response == HailAlarmTest)
  {
    digitalWrite(ledGreenConnectionOkPin, HIGH);
    digitalWrite(ledYellowHailAlarmPin, HIGH);
  }
  else if (response == ErrorHailAPI)
  {
    digitalWrite(ledRedErrorPin, HIGH);
  }
}
bool isHailAlert(int response)
{
  /*
  currentState: 0, NO Hail
  currentState : 1, Hail
  currentState : 2, Hail state triggered by Test - alarm
  */
  if (response == NoHailAlarm)
  {
    Serial.println(("Blinds: No hail alert -> Do nothing"));
    return false;
  }
  else if (response == HailAlarm)
  {
    Serial.println(("Blinds: Hail Alert -> Pull up!"));
    return true;
  }
  else if (response == HailAlarmTest)
  {
    Serial.println(("Blinds: Hail alert TEST -> Pull up!"));
    return true;
  }
  else if (response == ErrorHailAPI)
  {
    Serial.println(("Blinds: ERROR -> Do nothing"));
    return false;
  }
  else
  {
    return false;
  }
}

void handleBlinds(bool isHailAlert)
{
  if (isHailAlert)
  {
    lastPullUpBlindsEvent = millis();
    digitalWrite(realyPullBlindsUpPin, HIGH);
    Serial.println(F("handleBlinds: Pulling blinds up! (Turned on relay for approx. 30 seconds)"));
  }
  else if (millis() - lastPullUpBlindsEvent > 30000)
  {
    // 30000 ms -> 30 seconds
    if (digitalRead(realyPullBlindsUpPin) == HIGH)
      Serial.println(F("handleBlinds: Done with pulling up the blinds! (Turning off the relay"));
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