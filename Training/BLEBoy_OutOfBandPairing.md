# NFC Out-of-band Pairing with BLEBoy

A common method for performing out-of-band (OOB) pairing is to use NFC. The Bluetooth NFC Specification can be found at https://www.bluetooth.org/DocMan/handlers/DownloadDoc.ashx?doc_id=264234 .

BLEBoy has been developed to provide OOB data over a serial connection using the "GenOOBData" menu option. The data that is returned is the body of the NFC tag payload that should be written to the target NFC device with the record type name "application/vnd.bluetooth.le.oob". By default, when an Android reads an NFC tag of this type, it will attempt to carry out Bluetooth pairing with the device address contained in the NFC tag. The pairing method (Legacy or LESC) is dependent on the type of data in the tag. If the tag contains only a TK, then LE Legacy pairing is used. If the tag contains a 128-bit random and 128-bit confirm values, then LESC is used.

This tutorial will use the BLEBoy, BLEBoy_parseOOBFromSerial_writeToNTAG213.py script (located in the Scripts folder), a PN532 NFC reader/writer, and an NTAG213 NFC tag with the Android phone to conduct OOB pairing (using both LE Legacy and LESC pairing methods).


## Requirements

* BLEBoy
* Android device that supports NFC and Bluetooth 4.0
* A PN532 NFC reader/writer (another NFC writer may be used, but the script used in this training may need to be modified)
* An FTDI cable to connect the PN532 to your computer (such as an Adafruit FTDI Friend)
* An NTAG213 NFC tag for writing the data to
* The BLEBoy_parseOOBFromSerial_writeToNTAG213.py Python 2.7 script in the in the Scripts folder.  
* A host with a serial connection to the BLEBoy and the following Python libraries installed:
  * PySerial
  * nfcpy

## Steps

1. Connect the BLEBoy to your computer and take note of the port (on linux this is something like /dev/ttyUSBX and on Windows COMX).
2. Connect the PN532 to your FTDI cable.
3. Connect the FTDI cable to your computer and take note of the port (on linux this is something like /dev/ttyUSBX and on Windows COMX).
4. Edit the BLEBoy_parseOOBFromSerial_writeToNTAG213.py script so that the PN532 and BLEBoy serial ports used match the ones from steps 1 and 3.
5. Run the BLEBoy_parseOOBFromSerial_writeToNTAG213.py script (may require elevated privileges depending on your user's permissions).
6. When instructed, open the GenOOBData menu on the BLEBoy device.
	* If you want to use LESC pairing, press up.
	* If you want to use LE Legacy pairing, press down.
8. The script will now parse the OOB data. When instructed, place the NTAG213 tag over the PN532 and hold it.
9. When the script says "Tag written", remove the tag from the PN532.
10. On the BLEBoy, open the Settings menu.
    * If using LESC OOB pairing, set:
        * OOB: 0 (In LESC OOB, BLEBoy only supports supplying OOB data to another device and cannot accept OOB data. Since we are only sending our portion of the OOB data, this flag is left as 0)
        * LESC: 1
    * If using LE Legacy OOB pairing, set:
        * OOB: 1 (In LE Legacy, the OOB data exchanged is a shared key: TK. Since both device have the value, OOB is set to 1)
        * LESC: 0
12. Enable BLEBoy advertising.
13. Open the Android device.
14. Place the NFC tag near the NFC reader on the Android device (usually somewhere on the back) and wait for the dialog on the screen.
15. When the Android device asks if you want to pair with the Bluetooth device, click yes.
16. Wait for the connection and pairing process to finish.