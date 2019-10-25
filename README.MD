# BLEBoy

## Goal
Create a BLE peripheral that supports all BLE pairing methods that can
be used for training and testing, including out-of-band (OOB) pairing.
The peripheral must be configurable and
easy to re-create using consumer parts.

## About
BLEBoy is a BLE peripheral implemented using the Adafruit nRF52 Feather
board, which has been updated to support all BLE pairing methods
described in the Bluetooth specification (<https://www.bluetooth.com/specifications/bluetooth-core-specification>). BLEBoy allows on-the-fly GAP
security parameters to be updated (LESC, OOB, MitM, IO capabilities, etc.)
to allow users to experiment with the various pairing methods (and the
level of security provided by each).
It also supports the sending OOB data over serial to allow other tools
to construct BLE OOB NFC records for OOB pairing.

This tool can be used with the training modules found in the "Training"
folder to learn about BLE security.

**NOTE** This device is intended to be used for BLE security training. Do not use code in secure applications. There are several random number generators used in the code that are not considered to be cryptographically secure. Later versions of the NRF52 Nordic SDK provide secure RNG APIs that can be used for this purpose if needed.

## Requirements

### Required Hardware

* nRF52 Feather (<https://www.adafruit.com/product/3406>)
* OLED Featherwing (<https://www.adafruit.com/product/2900>)
* Joy Featherwing (<https://www.adafruit.com/product/3632>)
  * Be sure to short pin 27 (pad on bottom of board can be soldered to achieve this) so we can configure it as the interrupt pin. If another interrupt pin is desired, the BLEBoy code will need to be modified.
* Feather Tripler (<https://www.adafruit.com/product/3417>)
  * Not necessarily required, but it simplifies connecting the components
* Micro-USB cable for Feather (needed for uploading code and handling peripheral Passkey Entry association model)
* Access to soldering iron for assembling components (soldering headers)
* An Android phone that supports Bluetooth 4.0 and has Developer Options enabled (<https://developer.android.com/studio/debug/dev-options.html>)
  * This can also be done with a jailbroken iOS (version <10) device with BTCompanion installed from Cydia. For simplicity, we will only be covering Android.

Note: To handle OOB and Passkey Entry (where the passkey is entered on the peripheral) pairing association models, a Micro-USB cable will need to be connected to the nRF52 Feather and listen for serial communications (baud 9600). This same serial connection is used to print status and debug information.

### Optional Hardware

* For following the OOB NFC example
  * PN532 Breakout (https://www.adafruit.com/product/364)
  * FTDI cable (BLEBoy was tested using an Adafruit FTDI Friend - <https://www.adafruit.com/product/284>)
  * NTAG213 NFC Tags
* A LIPO battery to remove need for Micro-USB cable to power Feather
* A hardcase for the solution. The OLED is rather fragile and can easily crack under sharp pressure.

### Required Arduino Libraries

All libraries can be installed from their respective sources, except for arduino-menusystem and Bluefruit52Lib that have been modified. A changelog of the modified libraries can be found in *3rdPartyChanges.txt*.

Alternatively, users can use the libraries contained in the "libraries" folder of this project to ensure the same version of the libraries used during development is used.

* Adafruit GFX (<https://github.com/adafruit/Adafruit-GFX-Library>)
* Adafruit SSD1306 (<https://github.com/adafruit/Adafruit_SSD1306>)
* Adafruit nRF52
  * First, follow installation instructions at <https://learn.adafruit.com/bluefruit-nrf52-feather-learning-guide> (Debian instructions can be found below). **NOTE:** Be sure to install board version 0.6.0.
  * Next, follow instructions below "Installing Custom Bluefruit52Lib".
* Micro-ecc (<https://github.com/kmackay/micro-ecc>)
* Adafruit Seesaw (<https://github.com/adafruit/Adafruit_Seesaw>)
* arduino-menusystem (<https://github.com/jonblack/arduino-menusystem>)
  * Use modified version found in libraries folder.

## Hardware Setup

The following are setup instructions for constructing BLEBoy using the required hardware listed in the sections above:

1. Solder male headers on to each of the Feather and Featherwings.
2. On the Joy Featherwing, look at the bottom of the board and place solder on the Option Interrupt Pin pad in front on pin 27.
3. Solder female headers on to the Tripler.
4. Plug all 3 Feather boards on to the Tripler (any order can be used, but for simplicity I put the OLED at the top, nRF52 in the middle, and Joy at the bottom).


## Software Installation

To program the nRF52 Feather with the BLEBoy code, follow these steps:

* Install Arduino IDE.
* Install Adafruit nRF52 Feather board resources within Arduino IDE (see <https://learn.adafruit.com/bluefruit-nrf52-feather-learning-guide/arduino-bsp-setup>).
   * Developed and tested against nRF52 board version 0.6.0. Success with other versions may vary.
* Install 3rd party Arduino libraries (listed in Required Arduino Libraries section above).
* Replace Adafruit Bluetooth52Lib with the Bluetooth52Lib folder in this repository.
* Install modified arduino-menusystem library located in the libraries folder within this repository.
* Upload BLEBoy to nRF52 Feather.
* Set up serial connection to nRF52 Feather (9600 baud).
* (Optional) Set up PN532 for OOB NFC pairing

### Install Arduino with Adafruit nRF52 Resources on Debian

* Download latest Arduino IDE
* Follow instructions at <https://learn.adafruit.com/bluefruit-nrf52-feather-learning-guide/arduino-bsp-setup>
  * OSX and Linux: Be sure to install nrfutil Python wrapper (following instructions to install using BSP installation done by the Arduino IDE)
  * This page also contains a few troubleshooting steps if there are issues
* Follow instructions at <https://learn.adafruit.com/bluefruit-nrf52-feather-learning-guide/arduino-board-setup> to install cp260x driver.
* If on Linux, follow instructions at <http://playground.arduino.cc/linux/all#permission> to configure the correct permissions to access the serial device.


### Install Custom Bluefruit52Lib

* Copy the custom Bluefruit52Lib folder to (<version> will vary on installation):
  * OS X : ~/Library/Arduino15/packages/adafruit/hardware/nrf52/\<version\>/libraries
  * Linux : ~/.arduino15/packages/adafruit/hardware/nrf52/\<version\>/libraries
  * Windows: %APPDATA%\Local\Arduino15\packages\adafruit\hardware\nrf52\ \<version\> \libraries

### Install 3rd Party Libraries

* Install 3rd party libraries listed in Required Arduino Libraries
* Copy arduino-menusystem from BLEBoy libraries folder into your user Arduino library folder.

*OR*

* Copy folders in "libraries" to the user Arduino library folder
  * Commonly found in ~/Arduino/libraries or C:\Users\ \<user\> \Documents\Arduino\libraries
  
### (OPTIONAL) Installation for PN352 for OOB Pairing via NFC 

* Install Python nfcpy (<https://nfcpy.readthedocs.io/en/latest/>) and PySerial (<http://pyserial.readthedocs.io/en/latest/pyserial.html>)
* Connect the PN532 to an FTDI cable (tested with an Adafruit FTDI Friend), and connect it to your host
* Connect the BLEBoy device to the host
* Take note of the serial port for the previous two steps and update the OOB parsing Python script to use those ports.

