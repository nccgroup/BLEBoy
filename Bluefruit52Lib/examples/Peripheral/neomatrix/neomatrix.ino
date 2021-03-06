/*********************************************************************
 This is an example for our nRF52 based Bluefruit LE modules

 Pick one up today in the adafruit shop!

 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/

/* How to run this sketch
 *  - Connect the Neopixel FeatherWing and load this sketch to your Bluefruit52
 *  - Connect to the board using the Bluefruit Connect LE app
 *  - Send character(s) using BLEUART
 *  - Bluefruit will render the received character(s) on Neopixel FeatherWing
 *  
 *  Note: due to the font being larger than the 4x8 Neopixel Wing, you can
 *  only see part of the characters in some cases.
 *  Run the sketch with a larger Neopixel Matrix for a complete demo
 */

/* NOTE: This sketch required at least version 1.1.0 of Adafruit_Neopixel !!! */

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <bluefruit.h>

#define PIN             30   /* Pin used to drive the NeoPixels */

#define MATRIX_WIDTH    4
#define MATRIX_HEIGHT   8
#define MATRIX_LAYOUT   (NEO_MATRIX_TOP + NEO_MATRIX_RIGHT + NEO_MATRIX_COLUMNS + NEO_MATRIX_PROGRESSIVE)

// MATRIX DECLARATION:
// Parameter 1 = width of NeoPixel matrix
// Parameter 2 = height of matrix
// Parameter 3 = pin number (most are valid)
// Parameter 4 = matrix layout flags, add together as needed:
//   NEO_MATRIX_TOP, NEO_MATRIX_BOTTOM, NEO_MATRIX_LEFT, NEO_MATRIX_RIGHT:
//     Position of the FIRST LED in the matrix; pick two, e.g.
//     NEO_MATRIX_TOP + NEO_MATRIX_LEFT for the top-left corner.
//   NEO_MATRIX_ROWS, NEO_MATRIX_COLUMNS: LEDs are arranged in horizontal
//     rows or in vertical columns, respectively; pick one or the other.
//   NEO_MATRIX_PROGRESSIVE, NEO_MATRIX_ZIGZAG: all rows/columns proceed
//     in the same order, or alternate lines reverse direction; pick one.
//   See example below for these values in action.
// Parameter 5 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)

Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(MATRIX_WIDTH, MATRIX_HEIGHT, PIN,
                                               MATRIX_LAYOUT,
                                               NEO_GRB + NEO_KHZ800);
                                                 
// BLE Service
BLEDis  bledis;
BLEUart bleuart;

void setup()
{
  Serial.begin(115200);
  Serial.println(F("Adafruit Bluefruit NeoMatrix"));
  Serial.println(F("----------------------------"));

  Serial.println();
  Serial.println("Please connect using Bluefruit Connect LE application");
  
  // Config Neopixels Matrix
  matrix.begin();
  matrix.setTextWrap(false);
  matrix.setBrightness(40);
  matrix.setTextColor( matrix.Color(0, 0, 255) ); // Blue for Bluefruit

  // Init Bluefruit
  Bluefruit.begin();
  Bluefruit.setName("Bluefruit52");
  Bluefruit.setConnectCallback(connect_callback);

  // Configure and Start Device Information Service
  bledis.setManufacturer("Adafruit Industries");
  bledis.setModel("Bluefruit Feather52");
  bledis.begin();  

  // Configure and start BLE UART service
  bleuart.begin();

  // Set up the advertising packet
  setupAdv();

  // Start advertising
  Bluefruit.Advertising.start();
}

void setupAdv(void)
{  
  // Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  
  // Include bleuart 128-bit uuid
  Bluefruit.Advertising.addService(bleuart);

  // There is no room for Name in Advertising packet
  // Use Scan response for Name
  Bluefruit.ScanResponse.addName();
}

void connect_callback(void)
{
  Serial.println("Connected ! Please select 'Uart' tab and send any characters");
}


void loop()
{
  // Echo received data
  if ( Bluefruit.connected() && bleuart.notifyEnabled() )
  {
    if ( bleuart.available() )
    {
      char ch = (char) bleuart.read();

      matrix.fillScreen(0);
      matrix.setCursor(0, 0);
      matrix.print(ch);

      matrix.show();
      delay(100);
    }    
  }  
}

