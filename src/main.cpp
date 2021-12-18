/**
 * I could not have done this without the help of:
 *
 * Rui Santos
 * https://randomnerdtutorials.com/wifimanager-with-esp8266-autoconnect-custom-parameter-and-manage-your-ssid-and-password/
 *
 * Andres Sosa (aososam)
 * https://github.com/aososam/Smartnest/blob/master/Devices/light/light.ino
 *
 * As well as the numerous open source libraries and projects on the internet.
 *
 * Thank you.
 * */

#include <Arduino.h>
#include <ArduinoJson.h>  // https://github.com/bblanchon/ArduinoJson
#include <DNSServer.h>
#include <DoubleResetDetector.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <FS.h>  // this needs to be first, or it all crashes and burns...
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRutils.h>
#include <PubSubClient.h>
#include <Servo.h>
#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager

// #define BLINK
#define BLINK_IP_ADDRESS

#define IR

// Number of seconds after reset during which a
// subseqent reset will be considered a double reset.
#define DRD_TIMEOUT 10
// RTC Memory Address for the DoubleResetDetector to use
#define DRD_ADDRESS 0

int SERVO_PIN = 14;
Servo servo;  // create servo object to control a servo
// twelve servo objects can be created on most boards
int SERVO_MAX = 115;  // 180 is the absolute max limit
int SERVO_MIN = 25;   // 0 is the absolute low limit
int SERVO_MIDDLE = int(SERVO_MAX + SERVO_MIN) / 2;
int SERVO_DELAY = 500;

#ifdef IR
// THANK YOU: https://github.com/crankyoldgit/IRremoteESP8266
// Note: GPIO 16 won't work on the ESP8266 as it does not have interrupts.
const uint16_t kRecvPin = 4;  // GPIO4 is labelled D2 on the D1 Mini
IRrecv irrecv(kRecvPin);
decode_results results;
void checkIR();
#endif

void eraseSpiffsAndWifi();
void startMqtt();
void checkMqtt();
int splitTopic(char* topic, char* tokens[], int tokensNumber);
void callback(char* topic, byte* payload, unsigned int length);

DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);

WiFiServer server(80);
WiFiClient espClient;
PubSubClient client(espClient);
bool setupComplete = false;

// Variable to store the HTTP request
String header;

int webServoMax = SERVO_MAX;
int webServoMin = SERVO_MIN;

char mqtt_server[20] = "smartnest.cz\0";
char mqtt_port[11] = "1883\0";
char mqtt_username[20] = "\0";
char mqtt_password[64] = "\0";
char mqtt_client[64] = "\0";
char servo_max_char[5] = "115\0";
char servo_min_char[5] = "25\0";
char advanced_servo[7] = "true\0";

// flag for saving data
bool shouldSaveConfig = false;

// callback notifying us of the need to save config
void saveConfigCallback() {
  Serial.println("Should save config");
  shouldSaveConfig = true;
  drd.stop();
}

void setup() {
  Serial.begin(9600);
  delay(100);
#ifdef IR
  irrecv.enableIRIn();  // start the receiver
  Serial.print("IRrecvDemo is now running and waiting for IR message on Pin ");
  Serial.println(kRecvPin);
#endif
  if (drd.detectDoubleReset()) {
    Serial.println("Double Reset Detected");
    eraseSpiffsAndWifi();
  }

  pinMode(LED_BUILTIN, OUTPUT);
  // turn light on board off
  digitalWrite(LED_BUILTIN, HIGH);

  servo.attach(SERVO_PIN);    // attaches the servo on GPIO2 (D4 on a NodeMCU
                              // board) to the servo object
  servo.write(SERVO_MIDDLE);  // tell servo to go to degree
  delay(SERVO_DELAY);         // waits n ms for the servo to reach the position

  // read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      // file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");
          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(mqtt_username, json["mqtt_username"]);
          strcpy(mqtt_password, json["mqtt_password"]);
          strcpy(mqtt_client, json["mqtt_client"]);
          Serial.print("CONTAINS KEY servo_max: ");
          Serial.println(json.containsKey("servo_max"));
          if (json.containsKey("servo_max") && strlen(json["servo_max"]) > 0) {
            strcpy(servo_max_char, json["servo_max"]);
            webServoMax = atoi(json["servo_max"]);
          }
          if (json.containsKey("servo_min") && strlen(json["servo_min"]) > 0) {
            strcpy(servo_min_char, json["servo_min"]);
            webServoMin = atoi(json["servo_min"]);
          }
          strcpy(advanced_servo, json["advanced_servo_config"]);
          if (strcmp(advanced_servo, "true") == 0) {
            Serial.println(
                "User desired advanced servo setup - Serving servo test "
                "page...");
          } else {
            setupComplete = true;
          }
        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  // end read

  WiFiManagerParameter custom_mqtt_server("server", "MQTT server", mqtt_server,
                                          20);
  WiFiManagerParameter custom_mqtt_port("port", "MQTT port", mqtt_port, 6);
  WiFiManagerParameter custom_mqtt_username("username", "MQTT username",
                                            mqtt_username, 20);
  WiFiManagerParameter custom_mqtt_password("password", "MQTT password",
                                            mqtt_password, 64);
  WiFiManagerParameter custom_mqtt_client("client", "MQTT client", mqtt_client,
                                          64);
  WiFiManagerParameter custom_servo_max("servo_max", "Servo Max",
                                        servo_max_char, 5);
  WiFiManagerParameter custom_servo_min("servo_min", "Servo Min",
                                        servo_min_char, 5);

  WiFiManagerParameter custom_advanced_servo_config(
      "advanced_servo_config",
      "Advanced Servo Config (requires going to IP address of this device)",
      advanced_servo, 7);
  // WiFiManager
  // Local intialization. Once its business is done, there is no need to keep it
  // around
  WiFiManager wifiManager;

  // set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  // set custom ip for portal
  // wifiManager.setAPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1),
  // IPAddress(255,255,255,0));

  // add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_username);
  wifiManager.addParameter(&custom_mqtt_password);
  wifiManager.addParameter(&custom_mqtt_client);
  wifiManager.addParameter(&custom_servo_max);
  wifiManager.addParameter(&custom_servo_min);
  wifiManager.addParameter(&custom_advanced_servo_config);

  wifiManager.setCustomHeadElement(
      "<style>"
      " input { margin-bottom: 1em;} "
      " label:first-letter { text-transform: uppercase } "
      "</style>"
      "<script>"
      "window.addEventListener('DOMContentLoaded', () => {"
      "Array.from(document.querySelectorAll('input')).forEach(input => {"
      "const label = document.createElement('label');"
      "label.textContent = input.placeholder;"
      "label.htmlFor = input.id;"
      "input.parentElement.insertBefore(label, input);"
      "})"
      "})"
      "</script>");

  // set minimu quality of signal so it ignores AP's under that quality
  // defaults to 8%
  // wifiManager.setMinimumSignalQuality();

  // sets timeout until configuration portal gets turned off
  // useful to make it all retry or go to sleep
  // in seconds
  // wifiManager.setTimeout(120);

  // fetches ssid and pass from eeprom and tries to connect
  // if it does not connect it starts an access point with the specified name
  // here  "AutoConnectAP"
  // and goes into a blocking loop awaiting configuration
  wifiManager.autoConnect("SmartLightAp");
  // or use this for auto generated name ESP + ChipID
  // wifiManager.autoConnect();

  // if you get here you have connected to the WiFi
  Serial.println("Connected.");

  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_username, custom_mqtt_username.getValue());
  strcpy(mqtt_password, custom_mqtt_password.getValue());
  strcpy(mqtt_client, custom_mqtt_client.getValue());
  strcpy(servo_max_char, custom_servo_max.getValue());
  strcpy(servo_min_char, custom_servo_min.getValue());
  strcpy(advanced_servo, custom_advanced_servo_config.getValue());

  // save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["mqtt_username"] = mqtt_username;
    json["mqtt_password"] =
        mqtt_password;                  // Password from Smartnest (or API key)
    json["mqtt_client"] = mqtt_client;  // Device Id from smartnest

    json["servo_max"] = servo_max_char;
    webServoMax = atoi(json["servo_max"]);
    json["servo_min"] = servo_min_char;
    webServoMin = atoi(json["servo_min"]);

    json["advanced_servo_config"] = advanced_servo;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    // end save
  }

  server.begin();
}

bool hasBlinkedIp = false;
bool hasPrintedServerReady = false;
bool showWebSavedInstructions = false;

void loop() {
  // setup is complete if the user has set "Advanced Servo Config" to false, or
  // if they left it true (default) hit "Save" in the servo config page
  if (!setupComplete) {
#ifdef BLINK_IP_ADDRESS
    if (!hasBlinkedIp) {
      // blink last digits of IP address
      Serial.println("Blinking last digits of IP address...");
      delay(1500);
      String lastDigits = String(WiFi.localIP()[3]);
      for (int i = 0; i < lastDigits.length(); i++) {
        int currentDigit = lastDigits.charAt(i) - '0';
        for (int j = 0; j < currentDigit; j++) {
          digitalWrite(LED_BUILTIN,
                       LOW);  // blink the light on the ESP8266 board
          delay(200);
          digitalWrite(LED_BUILTIN, HIGH);  // off
          delay(350);
        }
        delay(1500);
      }
      hasBlinkedIp = true;
      Serial.println("Done blinking IP address");
    }
#endif
    if (!hasPrintedServerReady) {
      Serial.print(
          "Waiting for client to connect to advanced servo config at ");
      // https://forum.arduino.cc/t/how-to-manipulate-ipaddress-variables-convert-to-string/222693/15
      String LocalIP = String() + WiFi.localIP()[0] + "." + WiFi.localIP()[1] +
                       "." + WiFi.localIP()[2] + "." + WiFi.localIP()[3];
      Serial.println(LocalIP);
      hasPrintedServerReady = true;
    }

    WiFiClient webClient = server.available();  // Listen for incoming clients
    if (webClient) {                            // If a new client connects,
      Serial.println("New Client.");  // print a message out in the serial port
      Serial.print("Web servo max: ");
      Serial.println(webServoMax);
      String currentLine =
          "";  // make a String to hold incoming data from the client
      while (webClient.connected()) {  // loop while the client's connected
        if (webClient
                .available()) {  // if there's bytes to read from the client,
          char c = webClient.read();  // read a byte, then
          Serial.write(c);            // print it out the serial monitor
          header += c;
          if (c == '\n') {  // if the byte is a newline character
            // if the current line is blank, you got two newline characters in a
            // row. that's the end of the client HTTP request, so send a
            // response:
            if (currentLine.length() == 0) {
              // HTTP headers always start with a response code (e.g. HTTP/1.1
              // 200 OK) and a content-type so the client knows what's coming,
              // then a blank line:
              webClient.println("HTTP/1.1 200 OK");
              webClient.println("Content-type:text/html");
              webClient.println("Connection: close");
              webClient.println();

              // test servo
              if (header.indexOf("GET /output/on/") >= 0) {
                String saughtHeaderChunk = "GET /output/on/";
                Serial.println(saughtHeaderChunk.length());
                String value = header.substring(saughtHeaderChunk.length());
                Serial.print("Testing servo on: ");
                Serial.println(value);
                servo.write(value.toInt());  // tell servo to go to degree
                delay(SERVO_DELAY);  // waits n ms for the servo to reach the
                                     // position
                servo.write(SERVO_MIDDLE);  // tell servo to go to degree
                delay(SERVO_DELAY);  // waits n ms for the servo to reach the
                                     // position

                webServoMax = value.toInt();
              } else if (header.indexOf("GET /output/off") >= 0) {
                String saughtHeaderChunk = "GET /output/off/";
                Serial.println(saughtHeaderChunk.length());
                String value = header.substring(saughtHeaderChunk.length());
                Serial.print("Testing servo off: ");
                Serial.println(value);
                servo.write(value.toInt());  // tell servo to go to degree
                delay(SERVO_DELAY);  // waits n ms for the servo to reach the
                                     // position
                servo.write(SERVO_MIDDLE);  // tell servo to go to degree
                delay(SERVO_DELAY);  // waits n ms for the servo to reach the
                                     // position

                webServoMin = value.toInt();
              } else if (header.indexOf("GET /output/save") >= 0) {
                // save the custom parameters to FS
                Serial.println("saving config");
                DynamicJsonBuffer jsonBuffer;
                JsonObject& json = jsonBuffer.createObject();
                json["mqtt_server"] = mqtt_server;
                json["mqtt_port"] = mqtt_port;
                json["mqtt_username"] = mqtt_username;
                json["mqtt_password"] =
                    mqtt_password;  // Password from Smartnest (or API key)
                json["mqtt_client"] = mqtt_client;  // Device Id from smartnest

                json["servo_max"] = String(webServoMax);
                json["servo_min"] = String(webServoMin);
                json["advanced_servo_config"] = "false";

                File configFile = SPIFFS.open("/config.json", "w");
                if (!configFile) {
                  Serial.println("failed to open config file for writing");
                }

                json.printTo(Serial);
                json.printTo(configFile);
                configFile.close();
                showWebSavedInstructions = true;
                // end save
              } else if (header.indexOf("GET /output/reset") >= 0) {
                Serial.println("Reformatting SPIFFS...");
                SPIFFS.format();
              }

              // Display the HTML web page
              webClient.println("<!DOCTYPE html><html>");
              webClient.println(
                  "<head><meta name=\"viewport\" content=\"width=device-width, "
                  "initial-scale=1\">");

              webClient.println("<link rel=\"icon\" href=\"data:,\">");
              webClient.println(
                  "<style>html { font-family: Helvetica; display: "
                  "inline-block; "
                  "margin: 0px auto; text-align: center;}");
              webClient.println(
                  ".button { background-color: #195B6A; border: none; color: "
                  "white; padding: 16px 40px;");
              webClient.println(
                  "text-decoration: none; font-size: 30px; margin: 2px; "
                  "cursor: "
                  "pointer;}");
              webClient.println(
                  ".button3 {background-color: red;} "
                  ".button2 {background-color: #77878A;}</style></head>");
              webClient.println("<html><body>");
              webClient.println("<h1>Light Switch Servo Adjustment</h1>");

              if (!showWebSavedInstructions) {
                webClient.print(
                    "<p><br><label for='servo-max'>Servo Max</label><br><input "
                    "type='number' name='servo-max' value='");
                webClient.print(webServoMax);
                webClient.print(
                    "'><br><br>"
                    "<button "
                    "class=\"button\"  onclick='window.location.href = "
                    "`/output/on/"
                    "${document.querySelector(\"[name=\\\"servo-max\\\"]\")."
                    "value}`'>ON</button></p>"
                    "<p><br><label for='servo-min'>Servo Min</label><br><input "
                    "type='number' name='servo-min' value='");
                webClient.print(webServoMin);
                webClient.println(
                    "'><br><br>"
                    "<button class=\"button "
                    "button2\" onclick='window.location.href = "
                    "`/output/off/"
                    "${document.querySelector(\"[name=\\\"servo-min\\\"]\")."
                    "value}`'>OFF</button></p>");
                webClient.println(
                    "<p><br><br>"
                    "<button class=\"button "
                    "button3\" onclick='window.location.href = "
                    "`/output/save`'>SAVE</button></p>");
              } else {
                webClient.println(
                    "<p><br><br>"
                    "Settings saved. <br>Reboot by unplugging the board. "
                    "<br>Double-press the reset button to remove all "
                    "settings including WiFi.</p>");
              }

              webClient.println("</body></html>");

              // The HTTP response ends with another blank line
              webClient.println();
              // Break out of the while loop
              break;
            } else {  // if you got a newline, then clear currentLine
              currentLine = "";
            }
          } else if (c != '\r') {  // if you got anything else but a carriage
                                   // return character,
            currentLine += c;      // add it to the end of the currentLine
          }
        }
      }
      // Clear the header variable
      header = "";
      // Close the connection
      webClient.stop();
      Serial.println("Client disconnected.");
      Serial.println("");
    }
  } else {
#ifdef IR
    checkIR();
#endif
    client.loop();
    checkMqtt();
  }
}

void startMqtt() {
  client.setServer(mqtt_server, atoi(mqtt_port));
  client.setCallback(callback);

  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");

    if (client.connect(mqtt_client, mqtt_username, mqtt_password)) {
      Serial.println("connected");
    } else {
      if (client.state() == 5) {
        Serial.println("Connection not allowed by broker, possible reasons:");
        Serial.println(
            "- Device is already online. Wait some seconds until it appears "
            "offline for the broker");
        Serial.println("- Wrong Username or password. Check credentials");
        Serial.println(
            "- Client Id does not belong to this username, verify ClientId");

      } else {
        Serial.println("Not possible to connect to Broker Error code:");
        Serial.print(client.state());
      }

      // check to see if button held down and reset SPIFFS, otherwise retry
      // connecting to MQTT in 30 seconds
      for (int i = 0; i < 300; i++) {
        delay(100);
      }
    }
  }

  char topic[100];
  sprintf(topic, "%s/#", mqtt_client);
  client.subscribe(topic);

  sprintf(topic, "%s/report/online", mqtt_client);
  client.publish(topic, "true");
  delay(200);
}

void eraseSpiffsAndWifi() {
  Serial.println("Reformatting SPIFFS...");
  // clean FS, for testing
  SPIFFS.format();

  WiFiManager wifiManager;
  wifiManager.resetSettings();

  Serial.println("Restarting board..");
  ESP.restart();
}

int splitTopic(char* topic, char* tokens[], int tokensNumber) {
  const char s[2] = "/";
  int pos = 0;
  tokens[0] = strtok(topic, s);

  while (pos < tokensNumber - 1 && tokens[pos] != NULL) {
    pos++;
    tokens[pos] = strtok(NULL, s);
  }

  return pos;
}

void checkMqtt() {
  if (!client.connected()) {
    startMqtt();
  }
}

void callback(char* topic, byte* payload,
              unsigned int length) {  // a new message has been received
  Serial.print("Topic:");
  Serial.println(topic);
  int tokensNumber = 10;
  char* tokens[tokensNumber];
  char message[length + 1];
  splitTopic(topic, tokens, tokensNumber);
  sprintf(message, "%c", (char)payload[0]);
  for (int i = 1; i < length; i++) {
    sprintf(message, "%s%c", message, (char)payload[i]);
  }
  Serial.print("Message:");
  Serial.println(message);

  //------------------ACTIONS HERE---------------------------------
  char myTopic[100];

  if (strcmp(tokens[1], "directive") == 0 &&
      strcmp(tokens[2], "powerState") == 0) {
    sprintf(myTopic, "%s/report/powerState", mqtt_client);

    if (strcmp(message, "ON") == 0) {
#ifdef BLINK
      digitalWrite(LED_BUILTIN, LOW);  // blink the light on the ESP8266 board
#endif
      servo.write(webServoMax);  // tell servo to go to degree
      delay(SERVO_DELAY);  // waits n ms for the servo to reach the position
      servo.write(SERVO_MIDDLE);  // tell servo to go to degree
      delay(SERVO_DELAY);  // waits n ms for the servo to reach the position
      client.publish(myTopic, "ON");
    } else if (strcmp(message, "OFF") == 0) {
#ifdef BLINK
      digitalWrite(LED_BUILTIN, HIGH);
#endif
      servo.write(webServoMin);  // tell servo to go to degree
      delay(SERVO_DELAY);  // waits n ms for the servo to reach the position
      servo.write(SERVO_MIDDLE);  // tell servo to go to degree
      delay(SERVO_DELAY);  // waits n ms for the servo to reach the position
      client.publish(myTopic, "OFF");
    }
  }
}

void checkIR() {
  // Thank you:
  // https://github.com/crankyoldgit/IRremoteESP8266/blob/master/examples/IRrecvDemo/IRrecvDemo.ino
  if (irrecv.decode(&results)) {
    Serial.print("IR ");
    Serial.print(results.bits);
    Serial.print(" bits");
    Serial.print(": 0x");
    serialPrintUint64(results.value, HEX);
    Serial.println("");

    switch (results.value) {
      case 0xFFA25D:  // generic NEC remote top left
      case 0xBD0AF5:  // dser up-cup
      case 0xBDD02F:  // dser up
      case 0xBDB04F:  // dser OK
        Serial.println("IR: ON");
        // uncomment the next line if you want to test using the light on the
        // board (for off, uncomment the one in the next case too)
        // digitalWrite(LED_BUILTIN, LOW);
        servo.write(webServoMax);  // tell servo to go to degree
        delay(SERVO_DELAY);  // waits n ms for the servo to reach the position
        servo.write(SERVO_MIDDLE);  // tell servo to go to degree
        delay(SERVO_DELAY);  // waits n ms for the servo to reach the position
        break;
      case 0xFFE21D:  // generic NEC remote top right
      case 0xBD807F:  // dser power
      case 0xBD8A75:  // dser down-cup
      case 0xBDF00F:  // dser down
        Serial.println("IR: OFF");
        // digitalWrite(LED_BUILTIN, HIGH);
        servo.write(webServoMin);  // tell servo to go to degree
        delay(SERVO_DELAY);  // waits n ms for the servo to reach the position
        servo.write(SERVO_MIDDLE);  // tell servo to go to degree
        delay(SERVO_DELAY);  // waits n ms for the servo to reach the position
        break;
    }
    irrecv.resume();
  }
}