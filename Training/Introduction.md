# Introduction

## BLE Overview

### BLE Roles

There are 2 main roles defined in the Bluetooth Specification:
* Central - The central device is the connection initiator. Commonly, in the case where you have a BLE enabled accessory, your mobile device is the central device. 
* Peripheral - The peripheral device is the device that broadcasts advertising packets that contain information about itself. The peripheral device commonly hosts a GATT server that can be queried by any central device after establishing a connection.


### ATT and GATT

The Attribute Protocol (ATT) allows a device to expose a set of attributes that have associated attribute values (BLUETOOTH SPECIFICATION Version 5.0 | Vol 3, Part F page 2712). Each attribute has a type (defined by UUID), and handle that it can be referenced by, and value. Attributes also have a set of attribute permissions (see Attribute Permissions for more details) that define the operation the attribute exposes (read, write, indicate, notify) and the level of security to access the attribute, which are defined and accessed only by a higher layer. 

The Generic Attribute Profile (GATT) simply defines how attributes are accessed using ATT. GATT provides an abstraction that organizes attributes into several entities: services, characteristics, and descriptors. A device contains a set of services that contain one or more characteristics and/or references to other services. Each characterisitic contains a value and optionally can contain information about the value, referred to as a descriptor.

* Service - A service represents a particular function or feature on a device. For example, a medical device may contain a health service that contains information about the user's health (heartbeat, pulse, temperature, etc.).
* Characteristic - A characteristic represents a value in the service. For example, a medical device with a health service may have a heartbeat characteristic, whose value is the user's current heartbeat. Each characteristic has a property exposed that describes how it can be used (read, write, indicate, notify). 
* Descriptor - A descriptor represents information about its parent characteristic or holds configuration data. Descriptors may contain the characteristic name, information about the characteristic value such as a measurement type, or configuration for notify or indicate characteristics. For instance, a medical device with a temperature characteristic may have a descriptor that contains the measurement type for the characteristic value (Fahrenheit or Celsius). Another example may be a temperature characteristic that can be configured to notify the user each time the temperature changes, a descriptor will be present that allows the user to enable/disable the notify functionality.


GATT defines a number of different operations that can be used to access attributes, some of the most common include:
* Read - Read characteristic value.
* Write - Write characteristic value.
* Notify - Characteristics that notify central devices send a notify packet (if the notify option is enabled), when a specific event is triggered on the peripheral. The central device recieves the notification with the data, but does not acknowledge the event. Many notifications can be sent per connection interval.
* Indicate - Characteristics that notify central devices send an indicate packet (if the indicate option is enabled), when a specific event is triggered on the peripheral. The central device must send an application-layer acknowledgement to the peripheral. Only one indication can be sent per connection interval, making this a poor choice if high throughput is necessary.

The above GATT operations required the device receiving the packet to send a response, with the exception of the indicate packet. There are many other
GATT operations that are available, but less common. See BLUETOOTH SPECIFICATION Version 5.0 | Vol 3, Part G for more information.

### Attribute Permissions
Attributes have a set permission values that define the type of access available to the attribute and required conditions in order to access it.

Access types:
* Readable
* Writeable
* Readable and Writeable

Conditions:
* No encryption required
* Encryption required
* No authentication required
* Authentication required
* No authorization required
* Authorization required

An attribute can use a combination of the permissions above, such as an attribute that is readable if encryption is used (no authentication required). See BLUETOOTH SPECIFICATION Version 5.0 | Vol 3, Part F Section 3.2.5 for more information.



## Pairing and Bonding

Pairing is the process where two connected BLE devices exchange information to calculate a shared key that is then used to encrypt the BLE connection between devices. The Bluetooth specification defines 2 pairing methods:
* LE Legacy - Pairing described in the Bluetooth 4.0 Specification. This pairing method uses a 128-bit temporary key (TK) known by each device and a 128-bit random value generated by each device to generate a short term key (STK). The STK is then used to encrypt the BLE connection. Then the central device uses the STK to generate a long term key (LTK) and transmits it over the encrypted connection (if negotiated during the initial pairing request).
* LE Secure Connections (LESC) - Pairing method introduced in the Bluetooth 4.2 Specification that uses Elliptic Curve Diffie-Hellman (ECDH) to improve upon security flaws in the LE Legacy pairing method. In LESC the BLE devices exchange their ECDH public keys and generate a shared Diffie-Hellman key (DHK). The peripheral sends a calculated commitment value (referred to as Cb in the Bluetooth 5.0 Specification) to the central device and the devices exchange their nonces. After the central device successfully validates the peripherals commitment value, the exchanged values are then used with the ECDH public keys and the DHK to calculate an LTK. This process is slightly different for OOB pairing, more information can be found below.

### Association Models

Association models are a set of procedures used by BLE devices
(in Legacy this is the TK, in LESC this is the confirm value) to exchange specific information in order to complete the BLE pairing procedure. These association models range from using a static value as the TK to using a out-of-band (OOB) method of exchanging the values to calculate an LTK. The following are the defined association models for each pairing method:

* LE Legacy
  * JustWorks - Both devices assume 128-bit 0 value for TK, no user interaction is required. 
  * Passkey Entry - Once device displays a random value between 000,000 and 999,999. The user then must input the displayed number on the other device. The TK is the 128-bit value that is shared.
  * OOB - Both devices exchange the 128-bit TK using an undefined method. Commonly NFC is used.
* LESC - Each association model is used after the initial ECDH handshake is complete and ECDH public keys are exchanged.
  * JustWorks - Each device generates a 128-bit nonce and the peripheral sends a calculated commitment value, which is calculated and validated by the central device. If successful, the devices generate an LTK.
  * Passkey Entry - The user either enters a 6 digit passkey on each device or one device displays a key and the user enters it on the other. This short key (one bit at a time) is then used with a random nonce to calculate a commitment value, which is then exchanged between the devices. The devices then exchange nonces and validate the exchanged commitments. This process is repeated for each bit of the passkey. If the process is successful, an LTK is calculated.
  * Numeric Comparison - Each device generates a 128-bit nonce and the peripheral sends a calculated commitment value, which is calculated and validated by the central device. If successful, each device then calculates a 6 digit number that is displayed and requires the user to confirm the value displayed on each device is the same. If confirmed, an LTK is calculated.
  * OOB - Either device exchanges a 128-bit confirm and 128-bit random value to the other using an out-of-band method. These values are used to calculate a commitment value that is used by the other device to confirm the pairing and establish an LTK.

For message sequence charts that depict how each association model is implemented, see BLUETOOTH SPECIFICATION Version 5.0 | Vol 3, Part H, Sections 2.3.5.2 - 2.3.5.6.5 pages 2,315 - 2,325.

Association models are negotiated by BLE devices during the initial pairing process. After a BLE connection is established between a central and peripheral, the central can send a pairing request to the peripheral. The pairing request contains the following values that are used to determine the pairing method to be used:
* LESC - A flag to indicate whether the device supports LESC.
* MitM - A flag to indicate whether the device is requesting man-in-the-middle protection.
* IO Capability - A value that indicates the input/output capabilities of the device:
  * NoInputNoOutput - No input or output capabilities.
  * DisplayOnly - Only has a display.
  * DisplayYesNo - Has display and ability for user to supply inputs (minimum to give yes or no input).
  * KeyboardOnly - Has user input capability, but no output.
  * KeyboardDisplay - Has user input capability and output.
* OOB - A flag to indicate whether the device has received OOB data for pairing.


For diagrams that explain how these values are used to determine the association model used, see BLUETOOTH SPECIFICATION Version 5.0 | Vol 3, Part H, Tables 2.6, 2.7, and 2.8. pages 2,312 - 2,314


### Security Mode and Level

The Bluetooth Specification defines 2 security modes and a set of security levels for each mode, as defined by the Generic Access Profile that MUST be implemented on every BLE device. These security modes and levels define the level of security a connection must have in order to access entities that require a specific level of security (see Attribute Permissions). The specification defines the following (found in BLUETOOTH SPECIFICATION Version 5.0 | Vol 3, Part C page 2,068):

* Security Mode 1
  * Level 1 - No security (no pairing required)
  * Level 2 - Unauthenticated pairing, encryption required
  * Level 3 - Authenticated pairing, encryption required
  * Level 4 - LESC pairing, encryption required
* Security Mode 2 - Connections sign data using a key exchanged during the pairing process
  * Level 1 - Unauthenticated pairing, data signing required
  * Level 2 - Authenticated pairing, data signing required

See table 2.8 in BLUETOOTH SPECIFICATION Version 5.0 | Vol 3, Part H pages 2,313 - 2,314 to see which association models provide authenticated vs unauthenticated pairing.

## GATT Security

### LE Legacy Pairing Cracking

A major BLE vulnerability disclosed in the Bluetooth 4.0 specification (the first Bluetooth specification that includes BLE) is that the BLE pairing method (referred to now as LE Legacy) is vulnerable to cracking the pairing encryption. The attack requires an attacker to sniff the initial pairing of 2 BLE devices, but works against the LE Legacy JustWorks and Passkey Entry association models. For more information about the attack, see Mike Ryan's slides from his BlackHat 2013 talk "Bluetooth Smart: The Good, The Bad, The Ugly... and The Fix" (http://lacklustre.net/bluetooth/). To exploit this vulnerability an attacker can use easily obtainable equipment such as an Ubertooth (https://greatscottgadgets.com/ubertoothone/) to sniff the BLE traffic and Mike Ryan's CrackLE tool (https://github.com/mikeryan/crackle) to quickly crack the STK used for the encryption and decrypt all sniffed packets. This can even include the LTK (if exchanged) for persistence.

To prevent this attack from affecting users, it is recommended to enforce Security Mode 1, Level 4 for any GATT characteristics that will be used to transfer sensitive information. This will prevent users from accessing sensitive information over a connection that is being monitored by an attacker that can decrypt and modify traffic.


