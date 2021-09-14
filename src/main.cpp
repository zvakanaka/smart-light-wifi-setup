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
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <FS.h>  // this needs to be first, or it all crashes and burns...
#include <PubSubClient.h>
#include <Servo.h>
#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager

// #define BLINK

int SERVO_PIN = 14;
Servo servo;  // create servo object to control a servo
// twelve servo objects can be created on most boards
int SERVO_MAX = 115;  // 180 is the absolute max limit
int SERVO_MIN = 25;   // 0 is the absolute low limit
int SERVO_MIDDLE = int(SERVO_MAX + SERVO_MIN) / 2;
int SERVO_DELAY = 500;

void startMqtt();
void checkMqtt();
int splitTopic(char* topic, char* tokens[], int tokensNumber);
void callback(char* topic, byte* payload, unsigned int length);

WiFiServer server(80);
WiFiClient espClient;
PubSubClient client(espClient);
bool mqttReady = false;

// Variable to store the HTTP request
String header;
// Auxiliar variables to store the current output state
String outputState = "off";
char mqtt_server[20] = "smartnest.cz\0";
char mqtt_port[11] = "1883\0";
char mqtt_username[20] = "\0";
char mqtt_password[64] = "\0";
char mqtt_client[64] = "\0";

// flag for saving data
bool shouldSaveConfig = false;

// callback notifying us of the need to save config
void saveConfigCallback() {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setup() {
  Serial.begin(9600);

#ifdef BLINK
  pinMode(LED_BUILTIN, OUTPUT);
#endif

  // clean FS, for testing
  // SPIFFS.format();

  servo.attach(SERVO_PIN); // attaches the servo on GPIO2 (D4 on a NodeMCU board) to the servo object
  servo.write(SERVO_MIDDLE); // tell servo to go to degree
  delay(SERVO_DELAY); // waits n ms for the servo to reach the position

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

          mqttReady = true;
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

  // Uncomment and run it once, if you want to erase all the stored information
  // wifiManager.resetSettings();

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
  wifiManager.autoConnect("AutoConnectAP");
  // or use this for auto generated name ESP + ChipID
  // wifiManager.autoConnect();

  // if you get here you have connected to the WiFi
  Serial.println("Connected.");

  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_username, custom_mqtt_username.getValue());
  strcpy(mqtt_password, custom_mqtt_password.getValue());
  strcpy(mqtt_client, custom_mqtt_client.getValue());

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

void loop() {
  WiFiClient webClient = server.available();  // Listen for incoming clients

  if (webClient) {                  // If a new client connects,
    Serial.println("New Client.");  // print a message out in the serial port
    String currentLine =
        "";  // make a String to hold incoming data from the client
    while (webClient.connected()) {  // loop while the client's connected
      if (webClient.available()) {  // if there's bytes to read from the client,
        char c = webClient.read();  // read a byte, then
        Serial.write(c);            // print it out the serial monitor
        header += c;
        if (c == '\n') {  // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a
          // row. that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200
            // OK) and a content-type so the client knows what's coming, then a
            // blank line:
            webClient.println("HTTP/1.1 200 OK");
            webClient.println("Content-type:text/html");
            webClient.println("Connection: close");
            webClient.println();

            // Display the HTML web page
            webClient.println("<!DOCTYPE html><html>");
            webClient.println(
                "<head><meta name=\"viewport\" content=\"width=device-width, "
                "initial-scale=1\">");
            webClient.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons
            // Feel free to change the background-color and font-size attributes
            // to fit your preferences
            webClient.println(
                "<style>html { font-family: Helvetica; display: inline-block; "
                "margin: 0px auto; text-align: center;}");
            webClient.println(
                ".button { background-color: #195B6A; border: none; color: "
                "white; padding: 16px 40px;");
            webClient.println(
                "text-decoration: none; font-size: 30px; margin: 2px; cursor: "
                "pointer;}");
            webClient.println(
                ".button2 {background-color: #77878A;}</style></head>");

            // Web Page Heading
            webClient.println("<body><h1>ESP8266 Web Server</h1>");

            // Display current state, and ON/OFF buttons for the defined GPIO
            webClient.println("<p>Output - State " + outputState + "</p>");
            // If the outputState is off, it displays the ON button
            if (outputState == "off") {
              webClient.println(
                  "<p><a href=\"/output/on\"><button "
                  "class=\"button\">ON</button></a></p>");
            } else {
              webClient.println(
                  "<p><a href=\"/output/off\"><button class=\"button "
                  "button2\">OFF</button></a></p>");
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

  if (mqttReady) {
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

      delay(0x7530);
    }
  }

  char topic[100];
  sprintf(topic, "%s/#", mqtt_client);
  client.subscribe(topic);

  sprintf(topic, "%s/report/online", mqtt_client);
  client.publish(topic, "true");
  delay(200);
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
      // for testing first
      servo.write(SERVO_MAX);  // tell servo to go to degree
      delay(SERVO_DELAY);      // waits n ms for the servo to reach the position
      servo.write(SERVO_MIDDLE);  // tell servo to go to degree
      delay(SERVO_DELAY);  // waits n ms for the servo to reach the position
      client.publish(myTopic, "ON");
    } else if (strcmp(message, "OFF") == 0) {
#ifdef BLINK
      digitalWrite(LED_BUILTIN, HIGH);
#endif
      servo.write(SERVO_MIN);  // tell servo to go to degree
      delay(SERVO_DELAY);      // waits n ms for the servo to reach the position
      servo.write(SERVO_MIDDLE);  // tell servo to go to degree
      delay(SERVO_DELAY);  // waits n ms for the servo to reach the position
      client.publish(myTopic, "OFF");
    }
  }
}