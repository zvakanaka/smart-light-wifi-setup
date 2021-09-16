# Smart Light Switch Flipper (with WiFi setup)
Configurable MQTT light switch flipper using ESP8266 board and SG90 servo.

## Customizable Settings (through web portal)
- MQTT Server/Broker (https://smartnest.cz is a free/easy one)
- MQTT Client (device id)
- MQTT Username
- MQTT Password
- MQTT Port

## Other Customizable Settings (through web server)
> NOTE: This is in beta and requires:
> 1) MQTT client (during the web portal step) to be left blank
> 2) Valid WiFi to be set up during the web portal step

Navigate to the IP address of the ESP8266 in a web browser to test and save these settings:
- Servo Max (the degree that the servo goes up to when turning light switch on)
- Servo Min (the degree the servo goes down to when turning light switch off)

## Instructions
1) Set up [PlatformIO](https://platformio.org/platformio-ide)
2) `git clone https://github.com/zvakanaka/smart-light-wifi-setup; cd smart-light-wifi-setup; code .`
3) PlatformIO Home -> Import Arduino Project -> Select Board 'WeMos D1 mini Lite (WEMOS) -> find project -> Import
4) `ctrl/cmd+shift+p` -> `Switch Project Environment` -> smart-light-wifi-setup
5) Build & Upload (`ctrl/cmd+shift+u`)

> Note: Pin 14 (for servo PWM) maps to D5 on the WeMos D1 mini Lite.

6) Set up an MQTT account (https://smartnest.cz is a free/easy one)
7) Connect servo and power
8) Use phone to connect to portal's WiFi (temporary WiFi access point that starts if setup hasn't run/WiFi can't connect)
9) Enter MQTT credentials into portal
10) Save and reboot

Problem? [File an issue](https://github.com/zvakanaka/smart-light-wifi-setup/issues/new)

## Thank You!
- [Thingiverse Print](https://www.thingiverse.com/thing:1156995)
- [Rui Santos](https://randomnerdtutorials.com/wifimanager-with-esp8266-autoconnect-custom-parameter-and-manage-your-ssid-and-password/)
- [Andres Sosa (aososam)](https://github.com/aososam/Smartnest/blob/master/Devices/light/light.ino)

[Old project](https://github.com/zvakanaka/light-switch-servo/)
