# BLEBoy Basics

The goal of this training module is to introduce users to the BLEBoy device, explain how to modify the GAP security parameters and show how it affects BLE pairing, and identify how different pairing modes affect access to characterisitics with various security properties.

## Requirements
* A functioning BLEBoy device.
* An Android device compatible with Bluetooth 4.0.
* nRF Connect app installed on the Android device (https://play.google.com/store/apps/details?id=no.nordicsemi.android.mcp&hl=en).
* A computer connected to the BLEBoy device over serial.
  * ADB installed to pull a file off of the Android device.
  * The latest Wireshark installed to view Bluetooth traffic logs pulled from the Android device

## BLEBoy Menu
The following is the BLEBoy menu format and descriptions of each item:

* LED (toggle) - Turn on/off the on-board LED
* Advertising (toggle) - Turn on/off advertising (used so central devices can find and connect)
* Terminate Connections (toggle) - Terminate any existing connections
* GenOOBData (sub-menu) - Generate OOB data and send over serial (for NFC OOB pairing)
  * Pressing UP - Send LESC formatted OOB data over serial.
  * Pressing DOWN - Send LE Legacy formatted OOB data over Serial.
* Status (sub-page) - Status page containing information about the peripheral and the active connection.
  * Addr - Current address of BLEBoy.
  * Connected - Displays whether BLEBoy is connected to a central device.
  * Bonded - Displays whether BLEBoy is bonded to connected device.
  * SecMode - Displays the security mode of the current connection.
  * SecLevel - Displays the security level of the current connection.
* Settings (sub-menu) - Settings that allow configure each GAP security parameter on the device. These are used when sending a pairing response to a central device that has requested pairing and used to determing which pairing mode and association model is used.
  * Bonding - Toggle the Bonding flag. Used to request bonding.
  * LESC - Toggle the LESC flag. Used to request LESC pairing.
  * MitM - Toggle MitM flag. Used to request pairing method with MitM protections.
  * Keypress - Toggle Keypress flag. Used to request Keypress notifications if using the Keypass Entry association model.
  * IO - Cycle through IO Capabilities. Used to set the IO capabilities of BLEBoy, used when determining pairing association model to use.
  * OOB - Toggle OOB flag. Used to tell central device during pairing that BLEBoy has OOB data to use for pairing.
    * If pairing with OOB, only have this selected if using LE Legacy pairing. BLEBoy cannot receive OOB data from central device, so if using LESC, do not set this flag.
  * Clear Bonds - Action. Clear all saved bonds.

# How to View BLE Traffic Using Android
This section walks the user through enabling Bluetooth traffic logging on an Android device and using ADB to pull the log for viewing in Wireshark.

1. On the Android device, open the Developer options menu from the Settings page.
2. Turn on the "Enable Bluetooth HCI snoop log" option.
3. On BLEBoy, toggle the advertising option (a blue light should start blinking).
4. On the BLEBoy, navigate to the Status page and take note of the address.
5. From the Android device, open the nRF Connect application and select the "SCANNER" tab.
6. Press the "SCAN" button (should be in the upper-right hand corner of the app).
7. Wait until you see the address of the BLEBoy.
8. Select "CONNECT" on the BLEBoy menu item and wait for the devices to connect (the blue LED on the BLEBoy should be solid).
9. In the new tab in the nRF Connect app, click the "X" to disconnect.
10. From your computer that is connected to the Android device, run
    adb shell "cat /etc/bluetooth/bt_stack.conf"
11. From the previous step, find the line that begins with "BtSnoopFileName=". The following file path is the location of the Bluetooth btsnoop file.
12. Use ADB to pull the file from the location found in step 11. In some cases, some devices store this file in a place that requires root permissions to access.
13. Open the file from step 12 in Wireshark and notice all of the BLE traffic.

When viewing the btsnoop file, there is a ton of traffic with several different protocols. The following is a general breakdown of the traffic types:
* HCI_CMD - These are commands sent to the Bluetooth controller from the host layers of the stack.
* HCI_EVT - These are events received by the host layers of the stack from the Bluetooth controller. Many commands have a corresponding event that indicates the status of that command.
* SMP - Contain security manager packets, such as pairing requests and responses.
* ATT - Attribute Protocol packets. These will contain actual data transferred between devices.

In general, when viewing data exchanged between devices, we only care about ATT/GATT traffic. If we are looking to see what kind of information is passed between the host portion of the BLE stack to the controller portion of the BLE stack, then HCI traffic is relevant.

## Pairing with BLEBoy

In order to force a central device (the Android phone) and BLEBoy to use a certain pairing mode and association model, we will need to modify the pairing response parameters sent by BLEBoy. To do this, we can modify the options in the Settings menu on the BLEBoy in order to force a particular pairing to be used. First we will need to know the pairing request parameters sent by the central device, assuming we can't change these parameters.

1. On the Android device, open the Developer options menu from the Settings page.
2. Turn on the "Enable Bluetooth HCI snoop log" option.
3. On BLEBoy, toggle the advertising option (a blue light should start blinking).
4. On the BLEBoy, navigate to the Status page and take note of the address.
5. On the BLEBoy Settings page, set:
   * Bonding: 1
   * LESC: 0
   * MitM: 0
   * Keypress: 0
   * IO: NoInputOutput
   * OOB: 0
   These settings will force JustWorks pairing to be used.
6. Start BLEBoy advertising.
7. From the Android Settings menu, select Bluetooth.
8. Select BLEBoy from the list (or the address from step 4 if the name is not displaying).
9. Wait for the devices to finish connecting and pairing.
10. Using the same process from the previous section, pull the Bluetooth btsnoop file and open it in Wireshark.
11. Walk through the file until you find a packet with the SMP protocol. This should be a pairing request sent by the central device (Android).
12. In the pairing request, view the Security Manager Protocol values parsed by Wireshark and take note of the: IO Capability, OOB flag, the bits set in the AuthReq value (keypress, MitM, SC, Bonding), the initiator key distribution, and the responder key distribution.

Using these values, we can now modify BLEBoy's security settings to force a particular pairing method to be used, see BLUETOOTH SPECIFICATION Version 5.0 | Vol 3, Part H, Tables 2.6, 2.7, and 2.8. pages 2,312 - 2,314.

**Note** In order to utilize all pairing methods, ensure you have a serial connection open with BLEBoy by connecting a USB cable to the micro USB connection on the BLEBoy NRF52 Featherboard. In cases where the Passkey Entry pairing method is used and requires the peripheral to input a value, the serial connection is currently used to input the value to the BLEBoy.

**Note about OOB pairing** More information about OOB NFC pairing can be found in the "NFC Out-of-band Pairing with BLEBoy" tutorial.
 
## BLE Characteristic Security

As described in the Introduction training, BLE Attributes have attribute permissions that define how attributes can be accessed and what conditions have to be met in order to allow access. The access conditions are based on whether the requesting device is using encryption, has authorization, and/or is authenticated. These conditions are met by having a BLE connection with a minimum Security Mode and Level, which is determined by the pairing method and association model used by the two BLE devices (see BLUETOOTH SPECIFICATION Version 5.0 | Vol 3, Part H, Sections 2.3.5.2 - 2.3.5.6.5. Pages 2,315 - 2,325.).

BLEBoy contains a single GATT Service with 4 GATT Characteristics. Each characteristic contains a value (string), allows GATT clients to read the value, requires Security Mode 1, and each has a different required Security Level (1-4). To access each characteristic, the BLE devices must have a connection with Security Mode 1 and a Security Level that is equal to or greater than the required Security Level. That is to say, a characteristic that requires Security Mode 1 Level 2 can be accessed by a device connection with Security Mode 2, 3, or 4.

For this exercise, modify the BLEBoy settings such that a pairing method and association model is used between the Android device and BLEBoy to allow access to all 4 characteristics in the custom service (called "Uknown Service" in nRF Connect). Try to pair with each pairing method and association model. To learn about how to use OOB pairing with BLEBoy, see the Training module "NFC Out-of-band Pairing".
