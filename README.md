# Smart Light Switch Flipper (configure with WiFi portal)
Configurable MQTT light switch flipper using ESP8266 board and SG90 servo (and optional IR remote).

YouTube Video:  
[![YouTube Video](https://user-images.githubusercontent.com/8365885/134785502-4038ff19-f12d-4d6f-86a2-9883e69f9636.gif)](https://www.youtube.com/watch?v=q2pruaEamRM)

## Customizable Settings (through web portal)
- MQTT Server/Broker (https://smartnest.cz is a free/easy one)
- MQTT Client (device id)
- MQTT Username
- MQTT Password
- MQTT Port
- Servo Max
- Servo Min
- Advanced Servo Config (`true` starts a web server for servo testing - [details below](#other-customizable-settings-through-web-server))

## Other Customizable Settings (through web server)
> NOTE: This requires:
> 1) Advanced Servo Config setting to be previously set to 'true'
> 2) Valid WiFi to be set up during the web portal step

Navigate to the IP address of the ESP8266 in a web browser to test and save these settings:
- Servo Max (the degree that the servo goes up to when turning light switch on)
- Servo Min (the degree the servo goes down to when turning light switch off)

> NOTE: You can find the last digits of the IP address by counting the blinks after it starts up (the server won't be ready until it's done blinking).

Pressing "Save" on the web page prevents it from starting up again (until reset).

## Build and Flash Instructions
1) Set up [PlatformIO](https://platformio.org/platformio-ide)
2) `$ git clone https://github.com/zvakanaka/smart-light-wifi-setup; cd smart-light-wifi-setup; code .`
3) PlatformIO Home -> Import Arduino Project -> Select Board 'WeMos D1 mini Lite (WEMOS) -> find project -> Import
4) `ctrl/cmd+shift+p` -> `Switch Project Environment` -> smart-light-wifi-setup
5) Build & Upload (`ctrl/cmd+shift+u`)

## Setup Instructions
> Note: If your ESP8266 is not already flashed, follow instructions [above](#build-and-flash-instructions).
1) Set up an MQTT account (https://smartnest.cz is a free/easy one)
2) Connect servo and power
> Note: Pin 14 (for servo PWM) maps to D5 on the WeMos D1 mini Lite.
3) Use phone to connect to portal's WiFi (temporary WiFi access point that starts if setup hasn't run, WiFi can't connect, or you [reset all settings](#reset-all-settings))
4) Enter MQTT credentials into portal
5) If you set "Advanced Servo Config" to 'true', [connect to the web server](#other-customizable-settings-through-web-server) and save after adjusting servos right
6) Save and use your MQTT app to use the switch!

## Reset All Settings
[Press](https://github.com/datacute/DoubleResetDetector/) the reset button on the board to clear WiFi credentials and ALL settings (MQTT and servo max/min). This will also cause the board to reboot.
> ⚠ Warning: Do not press the reset button at all to reboot the board unless you want the settings to be reset.

Problem? [File an issue](https://github.com/zvakanaka/smart-light-wifi-setup/issues/new)

## Thank You!
- [Thingiverse Print](https://www.thingiverse.com/thing:1156995)
- [Rui Santos](https://randomnerdtutorials.com/wifimanager-with-esp8266-autoconnect-custom-parameter-and-manage-your-ssid-and-password/)
- [Andres Sosa (aososam)](https://github.com/aososam/Smartnest/blob/master/Devices/light/light.ino)

Libraries
- [bblanchon/ArduinoJson@5.13.4](https://github.com/bblanchon/ArduinoJson)
- [tzapu/WiFiManager@^0.16.0](https://github.com/tzapu/WiFiManager)
- [knolleary/PubSubClient@^2.8](https://github.com/knolleary/pubsubclient)
- [datacute/DoubleResetDetector@^1.0.3](https://github.com/datacute/DoubleResetDetector)
- [crankyoldgit/IRremoteESP8266@^2.8.0](https://github.com/crankyoldgit/IRremoteESP8266)

## Old project
This started as [light-switch-servo](https://github.com/zvakanaka/light-switch-servo/), which required manually setting configuration (WiFi credentials, MQTT, servo min/max) in the code. There is also code in there if you want to run this on a Raspberry Pi, use MicroPython on the ESP8266, or use it with an infrared remote.
