
# Smart Lock
This project project is an smart lock device targeting the ESP32, it features 3 types of input a 4x4 Keypad input, RFID Reader and Remote App Controller (using Secure WebSockets). Besides that, it features an 128x64 OLED screen to display the current state of the smart lock.
The actual lock is a 12v solenoid that is connected through a 5v relay.

This project is fully coded on embedded C.
This project also connect to a Progressive Web Application (PWA), you can find the code for that here: [GitHub Repo Smart Lock PWA](https://github.com/CarlosT25-png/smart-lock-web-ui)


## Screenshots


![GIF](https://github.com/CarlosT25-png/smart-lock/blob/main/docs/SmartLockVideo.gif?raw=true)
![GIF](https://github.com/CarlosT25-png/smart-lock/blob/main/docs/1.png?raw=true)
## Wiring Diagram

![GIF](https://github.com/CarlosT25-png/smart-lock/blob/main/docs/SmartLock_bb.jpg?raw=true)
## Components List
- 1x ESP32 (30 pin)
- 1x OLED 128x64 (SSD1306)
- 1x Keypad 4x4
- 1x RFID Reader (RC522)
- 1x Diode Rectifier (1N4007)
- 2x Lever Wire Connector 3-Port
- 1x 12v Electromagnetic Solenoid lock
- 1x 5V single channel relay
- 1x 12v Transformer with Female barrel jack 
- 22x Jumper Wires
- 18 AWG stranded wire (I used this for connecting the solenoid, 20 inches of this cable should work)


## Technical Brief

### Project Structure
The main.c file is where high level logic of this project lives. Here we control the different state of the app, print into the OLED Screen and send the different output signals.

Then, on the components folder, we have specialized libraries to control each peripheral (Keypad, rfid-rc522, solenoid-lock-12v, wifi-ws). 

### Serial Communication Protocols
This project use I2C and SPI protocols.

The I2C is mainly used for displaying information on the OLED screen, meanwhile, the SPI is used for sending/receiving information from the RFID Reader.

### Wifi Library and Secure WebSockets
In order to connect the smart lock to the outside world, we rely on a very basic implementation of websocket, where it leave an open communication between the server and the ESP32.

IMPORTANT NOTE: This approach is not secure for production application, this was only designed for educational purpose.
## Prerequisite

- ESP-IDF v6.0
## Installation

You can run this project by cloning the github repo and running the following commands

```bash
  idf.py build
  idf.py flash
```

Alternative, you can use the built-in icons on VS Code ESP-IDF Extension

## WIFI Setup + WebSockets

if you want to setup the wifi module with your Home Network, go the Wifi-ws library -> C File and make sure to set the wifi SSID and password of your home network.

If you want activate the PWA, make sure to also replace the URL at the end of the C File (wifi-ws) and also the root certificate.
## Authors

- [@CarlosT25-png](https://www.github.com/CarlosT25-png)


## License

[MIT](https://choosealicense.com/licenses/mit/)

