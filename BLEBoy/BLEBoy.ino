#include <Adafruit_seesaw.h>
#include <uECC.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <bluefruit.h>
#include <stdio.h>
#include <fcntl.h>    /* For O_RDWR */
#include <unistd.h>   /* For open(), creat() */
#include <string.h>
#include <MenuSystem.h>
#include <math.h>


 

//DO NOT USE PIN 28! PIN 28 is used to read random values
//for RNG function, must not be connected to anything.
#define BUTTON_A 31
#define BUTTON_B 30
#define BUTTON_C 27
#define LED 17
//#define BUTTON_UP 2
//#define BUTTON_DOWN 3
//#define BUTTON_RIGHT 4
//#define BUTTON_LEFT 5
//#define BUTTON_5 28


#define NUM_ECC_DIGITS 32


#define BUTTON_RIGHT 6
#define BUTTON_DOWN  7
#define BUTTON_LEFT  9
#define BUTTON_UP    10
#define BUTTON_SEL   14
uint32_t button_mask = (1 << BUTTON_RIGHT) | (1 << BUTTON_DOWN) | 
                (1 << BUTTON_LEFT) | (1 << BUTTON_UP) | (1 << BUTTON_SEL);

#define IRQ_PIN   27


Adafruit_SSD1306 display = Adafruit_SSD1306();
//Adafruit_SSD1306 ssd1306 = display;
Adafruit_seesaw ss;


#if (SSD1306_LCDHEIGHT != 32)
 #error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif



/******** CONFIG VALUES (Change if needed)********/
char* bleDeviceName = "BLEBoy";
/*******************************/

/*** OTHER BLE CONFIGURATIONS ***/
bool advertising = false;
bool bleConnected = false;
int oob = 0;
int bond = 1;
int mitm = 1;
int lesc = 1;
int keypress = 1;
int io = 0;
void beginAdvertising();
void endAdvertising();
void terminateAllConnections();
/********************************/

/******** MENU CONFIGURATION *********/
#define fontX 5
#define fontY 11
#define MAX_DEPTH 4
#define textScale 1

void copyCurrentAddrToAddrString(uint8_t* addr);

int test=55;
int ledCtrl=LOW;
int advertiseControl = 0;
bool passkeyTriggered = false;
bool passkeyDismissed = false;

bool lescStatus;
bool bondedStatus;
uint8_t modelStatus;
uint8_t* currentAddr;
bool connectedStatus;
uint8_t secModeStatus;
uint8_t secLevelStatus;
char* addrString = (char*)malloc(18*sizeof(char));//hex string separated by : (ie 00:11:22:33:44:55)

void setBonding(MenuComponent* p_menu_component);
void setLed(MenuComponent* p_menu_component);
void setAdv(MenuComponent* p_menu_component);
void terminateConnections(MenuComponent* p_menu_component);
void updateStatus();
void setStatus(MenuComponent* p_menu_component);
void setLesc(MenuComponent* p_menu_component);
void setKeypress(MenuComponent* p_menu_component);
void setIO(MenuComponent* p_menu_component);
void setOOB(MenuComponent* p_menu_component);
void setMitm(MenuComponent* p_menu_component);
void clearBonds(MenuComponent* p_menu_component);
void on_item1_selected(MenuComponent* p_menu_component);
void genOOBData(MenuComponent* p_menu_component);



class CustomRenderer : public MenuComponentRenderer {
public:
    const int MAX_VIEWABLE = 2; // number of items that can be viewed at once
    //2 allows one of the displayed items to line wrap
    void render(Menu const& menu) const {
      display.clearDisplay();
      display.setCursor(0,0);
      //display.print("\nCurrent menu name: ");
      if(strlen(menu.get_name()) != 0){
        display.print(menu.get_name());
        display.println(":");
      }
      else{
        display.println("   BLEBoy   ");
        display.println("Press [select] to");
        display.println("to the menu");
      }
      String buffer;
      int start = 0;
      for (int i = 0; i < menu.get_num_components(); ++i) {
          MenuComponent const* cp_m_comp = menu.get_menu_component(i);
          
    
          if (cp_m_comp->is_current()){
            start = i;
          }
      }
      int group;
      if ( start == 0){
            group = 0;
          }
      else{
        group = floor(start/MAX_VIEWABLE);
      }
      int curGroup = 0;
      for (int i = 0; i < menu.get_num_components(); ++i) {
          MenuComponent const* cp_m_comp = menu.get_menu_component(i);
          if ( i == 0){
            curGroup = 0;
          }
          else{
            curGroup = floor(i/MAX_VIEWABLE);
          }
          if (curGroup != group){
            continue;
          }
    
          if (cp_m_comp->is_current()){
              display.print(">");
          }
            display.print(" ");
            cp_m_comp->render(*this);
            display.println("");
          
      }
      display.display();
    }

    void render_menu_item(MenuItem const& menu_item) const {
        
        display.print(menu_item.get_name());
        display.display();
    }

    void render_config_menu_item(ConfigMenuItem const& menu_item) const {
        
        display.print(menu_item.get_name());
        display.print(":");
        display.print(menu_item.get_value());
        display.display();
    }

    void render_back_menu_item(BackMenuItem const& menu_item) const {
        display.print(menu_item.get_name());
        display.display();
    }

    void render_numeric_menu_item(NumericMenuItem const& menu_item) const {
      String buffer;

      buffer = menu_item.get_name();
      buffer += menu_item.has_focus() ? '<' : '=';
      buffer += menu_item.get_formatted_value();
  
      if (menu_item.has_focus())
          buffer += '>';
  
      display.print(buffer);
      display.display();
    }


    void render_menu(Menu const& menu) const {
        display.print(menu.get_name());
        display.display();
    }
};





// Menu variables
CustomRenderer my_renderer;
MenuSystem ms(my_renderer);
Menu mm("Main Menu");

ConfigMenuItem muLed("Off","LED",&setLed);
ConfigMenuItem muAdv("Off","Advertising",&setAdv);

MenuItem mu_terminate("Terminate Connections",&terminateConnections);

Menu muStatus("Status");
//TODO need to do an update check each loop and update these config values (for status)
ConfigMenuItem muStatus_miAddr("00:00:00:00:00:00", "Addr", &setStatus);
ConfigMenuItem muStatus_miConn("False", "Connected", &setStatus);
ConfigMenuItem muStatus_miLesc("False", "LESC", &setStatus);
ConfigMenuItem muStatus_miAM("None", "Asc.Model", &setStatus);
ConfigMenuItem muStatus_miBond("False", "Bonded", &setStatus);
ConfigMenuItem muStatus_miSecMode("None", "SecMode", &setStatus);
ConfigMenuItem muStatus_miSecLevel("None", "SecLevel", &setStatus);


Menu muSettings("Settings");
ConfigMenuItem muSettings_miBond("1","Bonding",&setBonding);
ConfigMenuItem muSettings_miLesc("1","LESC",&setLesc);
ConfigMenuItem muSettings_miMitm("1","MitM",&setMitm);
ConfigMenuItem muSettings_miKeypress("1","Keypress",&setKeypress);
ConfigMenuItem muSettings_miIO("DisplayOnly","IO",&setIO);
ConfigMenuItem muSettings_miOOB("0","OOB",&setOOB);
MenuItem muSettings_miClearBonds("Clear Bonds",&clearBonds);
MenuItem mu_genoob("GenOOBData",&genOOBData);




/******************************************************************/

/****************** MENU EVENTS *************************/

void setBonding(MenuComponent* p_menu_component) {
    char* temp = (char*)malloc(2*sizeof(char));
    if (bond == 0){
      bond = 1;
      strncpy(temp,"1",2);
      *(temp+2) = '\0'; 
    }
    else{
      bond = 0;
      strncpy(temp,"0",2);
      *(temp+2) = '\0'; 
    }
    updateSecurityParameters();
    muSettings_miBond.set_value(temp);
}

void setLed(MenuComponent* p_menu_component){
    char* temp = (char*)malloc(4*sizeof(char));
    if (ledCtrl == HIGH){
      ledCtrl = LOW;
      strncpy(temp,"Off",4);
      *(temp+4) = '\0'; 
    }
    else{
      ledCtrl = HIGH;
      strncpy(temp,"On",3);
      *(temp+3) = '\0'; 
    }
    muLed.set_value(temp);
}
void setAdv(MenuComponent* p_menu_component){
  char* temp = (char*)malloc(4*sizeof(char));
  if (advertising == false){
    advertising = true;
    strncpy(temp,"On",3);
    *(temp+3) = '\0'; 
    beginAdvertising();
  }
  else{
    advertising = false;
    strncpy(temp,"Off",4);
    *(temp+4) = '\0'; 
    endAdvertising();
  }
  muAdv.set_value(temp);
}
void terminateConnections(MenuComponent* p_menu_component){
  terminateAllConnections();
}

void setLesc(MenuComponent* p_menu_component){
  char* temp = (char*)malloc(2*sizeof(char));
  if (lesc == 0){
    lesc = 1;
    strncpy(temp,"1",2);
    *(temp+2) = '\0'; 
  }
  else{
    lesc = 0;
    strncpy(temp,"0",2);
    *(temp+2) = '\0'; 
  }
  updateSecurityParameters();
  muSettings_miLesc.set_value(temp);
}

void setMitm(MenuComponent* p_menu_component){
  char* temp = (char*)malloc(2*sizeof(char));
  if (mitm == 0){
    mitm = 1;
    strncpy(temp,"1",2);
    *(temp+2) = '\0'; 
  }
  else{
    mitm = 0;
    strncpy(temp,"0",2);
    *(temp+2) = '\0'; 
  }
  updateSecurityParameters();
  muSettings_miMitm.set_value(temp);
}
void setKeypress(MenuComponent* p_menu_component){
  char* temp = (char*)malloc(2*sizeof(char));
  if (keypress == 0){
    keypress = 1;
    strncpy(temp,"1",2);
    *(temp+2) = '\0'; 
  }
  else{
    keypress = 0;
    strncpy(temp,"0",2);
    *(temp+2) = '\0'; 
  }
  updateSecurityParameters();
  muSettings_miKeypress.set_value(temp);
}
void setIO(MenuComponent* p_menu_component){
  char* temp = (char*)malloc(16*sizeof(char));
  /*,VALUE("DisplayOnly",0,setIO0,noEvent)
  ,VALUE("DisplayYesNo",1,setIO1,noEvent)
  ,VALUE("KeyboardOnly",2,setIO2,noEvent)
  ,VALUE("NoInputNoOutput",3,setIO3,noEvent)
  ,VALUE("KeyboardDisplay",4,setIO4,noEvent)
  */
  switch(io){
    case 0:
      io = 1;
      strncpy(temp,"DisplayYesNo",13);
      *(temp+13) = '\0';
      break;
    case 1:
      io = 2;
      strncpy(temp,"KeyboardOnly",13);
      *(temp+13) = '\0';
      break;
    case 2:
      io = 3;
      strncpy(temp,"NoInputNoOutput",16);
      *(temp+16) = '\0';
      break;
    case 3:
      io = 4;
      strncpy(temp,"KeyboardDisplay",16);
      *(temp+16) = '\0';
      break;
    case 4:
      io = 0;
      strncpy(temp,"DisplayOnly",12);
      *(temp+12) = '\0';
      break;
  }
  updateSecurityParameters();
  muSettings_miIO.set_value(temp);
}
void setOOB(MenuComponent* p_menu_component){
  char* temp = (char*)malloc(2*sizeof(char));
  if (oob == 0){
    oob = 1;
    strncpy(temp,"1",2);
    *(temp+2) = '\0'; 
  }
  else{
    oob = 0;
    strncpy(temp,"0",2);
    *(temp+2) = '\0'; 
  }
  updateSecurityParameters();
  muSettings_miOOB.set_value(temp);
}

void clearBonds(MenuComponent* p_menu_component){
  Bluefruit.clearBonds();
}

void setStatus(MenuComponent* p_menu_component){
  updateStatus();
}
void updateStatus(){
  connectedStatus = Bluefruit.connected();
  bondedStatus = Bluefruit.isBonded();
  if( connectedStatus ){
    uint16_t handle = Bluefruit.connHandle();
    secModeStatus = Bluefruit.getConnectionSecurityMode(handle);
    secLevelStatus = Bluefruit.getConnectionSecurityLevel(handle);
    char temp[2];
    itoa(secModeStatus, temp, 10);
    muStatus_miSecMode.set_value(temp);
    itoa(secLevelStatus, temp, 10);
    muStatus_miSecLevel.set_value(temp);
  }
  else{
    muStatus_miSecMode.set_value("None");
    muStatus_miSecLevel.set_value("None");
  }
  currentAddr = Bluefruit.getAddr();
  copyCurrentAddrToAddrString(currentAddr);

  muStatus_miAddr.set_value(addrString);
  if(connectedStatus){
    muStatus_miConn.set_value("True");
  }
  else{
    muStatus_miConn.set_value("False");
  }
  if(bondedStatus){
    muStatus_miBond.set_value("True");
  }
  else{
    muStatus_miBond.set_value("False");
  }
}
void genOOBData(MenuComponent* p_menu_component){
  uint16_t connHandle = Bluefruit.getConnHandle();
  //calling insecure OOB data generation so we have data ready
  Bluefruit.generateOOBData(connHandle);

  // need to choose Legacy vs SC (up SC, down Legacy)
  display.clearDisplay();
  display.setCursor(0,0);
  //display.display();
  display.println("Generated OOB Data.");
  display.println("Select a key type");
  //display.println(":");
  display.println("<Up> SC to Serial");
  display.println("<Down> Legacy to Serial");
  display.display();
  const int ADDRESS_DATA_TYPE = 0x1B;
  const int LE_ROLE_DATA_TYPE = 0x1C;
  const int APPEARANCE_DATA_TYPE = 0x19;
  const int LOCAL_NAME_DATA_TYPE = 0x09;
  const int SECURITY_KEY_DATA_TYPE = 0x10;
  const int SECURITY_KEY_SIZE = 16;
  const int ADDRESS_SIZE = 6;
  const int LESC_CONFIRM_TYPE = 0x22;
  const int LESC_CONFIRM_SIZE = 16;
  const int LESC_RANDOM_TYPE = 0x23;
  const int LESC_RANDOM_SIZE = 16;
  const int ADDRESS_TYPE_PUBLIC = 0x00;
  const int ADDRESS_TYPE_RANDOM = 0x01;

  uint8_t* r;
  uint8_t* c;
  uint8_t* key;
  char* periph_name;
  uint8_t* addr;
  int nameLen;
  int index = 0;
  int buffLen = 0;
  //Block until user exits. 
  while(true){
          if(!digitalRead(IRQ_PIN)){
            uint32_t buttons = ss.digitalReadBulk(button_mask);
            if(! (buttons & (1 << BUTTON_DOWN))){
              //Legacy
              key = Bluefruit.getLegacyOOBKey();
              periph_name = Bluefruit.getName();
              addr = Bluefruit.getAddr();
              nameLen = strlen(periph_name);
              /*
              * addrSize (1) + addrDataType (1) + addr (6) + addrType (1) + tkSize (1)+
              * tkDataType (1) + tk (16) + 
              * nameLength (1) + nameDataType (1) + name (sizeof name)
              */
              buffLen = (1+1+6+1+1+1+16+1+1+nameLen);
              uint8_t oobNFCPayload[buffLen];
              index = 0;
              
              oobNFCPayload[index] = ADDRESS_SIZE;
              index++;
              oobNFCPayload[index] = ADDRESS_DATA_TYPE;
              index++;
              for( int i = 0; i < ADDRESS_SIZE; i++){
              oobNFCPayload[index] = addr[i];
              index++;
              }
              //TODO: we configured the device to use the public address type,
              //later we can make this a dynamic option.
              oobNFCPayload[index] = ADDRESS_TYPE_PUBLIC;
              index++;
              
              oobNFCPayload[index] = SECURITY_KEY_SIZE;
              index++;
              oobNFCPayload[index] = SECURITY_KEY_DATA_TYPE;
              index++;
              for( int i = 0; i < SECURITY_KEY_SIZE; i++){
              oobNFCPayload[index] = key[i];
              index++;
              }
              
              oobNFCPayload[index] = nameLen;
              index++;
              oobNFCPayload[index] = LOCAL_NAME_DATA_TYPE;
              index++;
              for( int i = 0; i < nameLen; i++){
              oobNFCPayload[index] = (uint8_t) periph_name[i];
              index++;
              }
              
              Serial.println("Writing NFC Payload");
              Serial.print("Len: ");
              Serial.println(buffLen);
              Serial.write(oobNFCPayload, buffLen);
              Serial.println("");
              Serial.println("Finished write");

              Serial.println("Printing params:");
              
              Serial.print("Name: ");
              Serial.println(periph_name);
              Serial.print("Addr: ");
              char* tempAddr = (char*)malloc(7);
              int b = sprintf(tempAddr, "%02x:%02x:%02x:%02x:%02x:%02x", addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
              Serial.println(tempAddr);
              Serial.print("Key: ");
              Serial.println(uint8ToHex(key, SECURITY_KEY_SIZE));
            }
            if(! (buttons & (1 << BUTTON_UP))){
              // SC
             r = Bluefruit.getSCOOBRandom();
             c = Bluefruit.getSCOOBConfirm();
             periph_name = Bluefruit.getName();
             addr = Bluefruit.getAddr();
             nameLen = strlen(periph_name);
             /*
              * addrSize (1) + addrDataType (1) + addr (6) + addrType (1) + confirmSize (1)+
              * confirmDataType (1) + confirm (16) + randomSize(1) + randomDataType(1) + 
              * random (16) + nameLength (1) + nameDataType (1) + name (sizeof name)
              */
              buffLen = (1+1+1+6+1+1+16+1+1+16+1+1+nameLen);
              uint8_t oobNFCPayload[buffLen];
              index = 0;
              
              oobNFCPayload[index] = ADDRESS_SIZE;
              index++;
              oobNFCPayload[index] = ADDRESS_DATA_TYPE;
              index++;
              for( int i = 0; i < ADDRESS_SIZE; i++){
              oobNFCPayload[index] = addr[i];
              index++;
              }
              //TODO: we configured the device to use the public address type,
              //later we can make this a dynamic option.
              oobNFCPayload[index] = ADDRESS_TYPE_PUBLIC;
              index++;
              
              oobNFCPayload[index] = LESC_CONFIRM_SIZE;
              index++;
              oobNFCPayload[index] = LESC_CONFIRM_TYPE;
              index++;
              for( int i = 0; i < LESC_CONFIRM_SIZE; i++){
              oobNFCPayload[index] = c[i];
              index++;
              }
              
              oobNFCPayload[index] = LESC_RANDOM_SIZE;
              index++;
              oobNFCPayload[index] = LESC_RANDOM_TYPE;
              index++;
              for( int i = 0; i < LESC_RANDOM_SIZE; i++){
              oobNFCPayload[index] = r[i];
              index++;
              }
              
              oobNFCPayload[index] = nameLen;
              index++;
              oobNFCPayload[index] = LOCAL_NAME_DATA_TYPE;
              index++;
              for( int i = 0; i < nameLen; i++){
              oobNFCPayload[index] = (uint8_t) periph_name[i];
              index++;
              }
              Serial.println("Writing NFC Payload");
              Serial.print("Len: ");
              Serial.println(buffLen);
              Serial.write(oobNFCPayload, buffLen);
              Serial.println("");
              Serial.println("Finished write");

              Serial.println("Printing params:");
              
              Serial.print("Name: ");
              Serial.println(periph_name);
              Serial.print("Addr: ");
              char* tempAddr = (char*)malloc(7);
              int b = sprintf(tempAddr, "%02x:%02x:%02x:%02x:%02x:%02x", addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
              Serial.println(tempAddr);
              Serial.print("Confirm: ");
              Serial.println(uint8ToHex(c, LESC_CONFIRM_SIZE));
              Serial.print("Random: ");
              Serial.println(uint8ToHex(r, LESC_RANDOM_SIZE));
            }
            if(! (buttons & (1 << BUTTON_LEFT))){
              break;
            }
          }
  }
  passkeyDismissed = true;

}
/******************************************/


/************** UTIL FUNCTIONS **************/

void updateSecurityParameters(){
  Bluefruit.updateSecParams(bond, mitm, lesc, keypress, io, oob);
}

void copyCurrentAddrToAddrString(uint8_t* addr){
  int i;
  //backwards to fix endianness
  int b = sprintf(addrString, "%02x:%02x:%02x:%02x:%02x:%02x", addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
}

char* uint8ToHex(uint8_t* str, int size){
  char* output;
  output = (char*)malloc((size * 2) + 1);
  char* ptr;
  ptr =output;
  int i;

  for (i = 0; i < size; i++)
  {
      ptr += sprintf (ptr, "%02X", str[i]);
  }
  return output;
}
/************** SETUP FUNCTIONS ***************/

void bleConnectCallback() {
  bleConnected = true;
  Serial.println("Successful connection.");
}

void bleDisconnectCallback(uint8_t reason) {
  bleConnected = false;
  Serial.print("Disconnect. Reason: ");
  Serial.println(reason, DEC);
}

//Using serial read/write for peripheral passkey entry.
//Should explore adding a keypad for future iterations
// Notifications we send are only start, digit in, and end. we do not support character clear or deletion.
void blePasskeyEntryCallback(uint16_t connHandle, bool sendKeypressNotifications, uint8_t key_type, uint8_t* passkey){
     unsigned long startTime = millis();
     unsigned long currentTime;
     int timeout = 1000 * 30; //30 second timeout
     int incomingByte;
     int inputCounter = 0;
     passkeyTriggered = true;
     Bluefruit.sendKeypressNotification(connHandle, BLE_GAP_KP_NOT_TYPE_PASSKEY_START);
     display.clearDisplay();
      display.setCursor(0,0);
      display.println("Enter passkey");
      display.println("using Serial1 conn");
      display.display();
     Serial.println("<UserInputRequired>Please enter passkey displayed on Central device");
     Serial.print("Timeout = ");
     Serial.print(timeout/1000,DEC);
     Serial.println(" seconds");
     Serial.print(">");
     //While loop configured for timeout (default 30 seconds) and a max passkey of 6 digits. Only
     //accepts numeric ascii values (0-9) and ignores all other input.
     while(true){
      if(Serial.available() > 0){
        incomingByte = Serial.read();
        //input should only allow numbers, dec 48-57 (inclusive)
        if(incomingByte > 47 && incomingByte < 58){
          passkey[inputCounter] = incomingByte;
          inputCounter++;
          if(sendKeypressNotifications){
            Bluefruit.sendKeypressNotification(connHandle,  BLE_GAP_KP_NOT_TYPE_PASSKEY_DIGIT_IN);
          }
        }
      }
      if(inputCounter > 5){
          break;
        }
        currentTime = millis();
        if((currentTime - startTime) >= timeout){
          break;
        }
     }
     if(sendKeypressNotifications){
      Bluefruit.sendKeypressNotification(connHandle,  BLE_GAP_KP_NOT_TYPE_PASSKEY_END);
     }
     Serial.print("User supplied passkey:");
     Serial.println((char*)passkey);
     passkeyDismissed = true;
  
}

void blePasskeyDisplayCallback(int match_request, const uint8_t* passkey, int* result) {
  passkeyTriggered = true;
  Serial.println("Passkey display callback triggered");
  Serial.print("Match request: ");
  Serial.println(match_request, DEC);
  
  display.clearDisplay();
  display.setCursor(0,0);
  display.display();
  display.println("PASSKEY:");
  display.println((char*)passkey);
  
  if(match_request == 0){
    display.println("Press up or down to continue.");
    display.display();
    *result = 1;
    while(true){
            if(!digitalRead(IRQ_PIN)){
              uint32_t buttons = ss.digitalReadBulk(button_mask);
              if(! (buttons & (1 << BUTTON_DOWN))){
                //*result = 0;
                break;
              }
              if(! (buttons & (1 << BUTTON_UP))){
                //*result = 1;
                break;
              }
            }
    }
    passkeyDismissed = true;
  }
  if(match_request == 1){
    display.println("<Up> if match.");
    display.println("<Down> if no match.");
    display.display();
    
    //Block until we get confirmation from the user
    //This implements the user input requirement for numeric comparison.
    //When match_request == 1, we must report if there is a match
    while(true){
            if(!digitalRead(IRQ_PIN)){
              uint32_t buttons = ss.digitalReadBulk(button_mask);
              if(! (buttons & (1 << BUTTON_DOWN))){
                *result = 0;
                break;
              }
              if(! (buttons & (1 << BUTTON_UP))){
                *result = 1;
                break;
              }
            }
    }
    passkeyDismissed = true;
    
     
  }
  Serial.println("Done printing passkey display");
}

void setupAdvertising(){
  // Advertise as BLE only and general discoverable
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  
  // Add the TX Power to the advertising packet
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addName();
 
  // Include bleuart 128-bit uuid in the advertising packet
  //Bluefruit.Advertising.addService(bleuart);
 
  // There is no room for Name in Advertising packet
  // Use Scan response to store it instead
  Bluefruit.ScanResponse.addName();
}


//RNG function pulled from micro-ecc example ecc_test.ino
//https://github.com/kmackay/micro-ecc/blob/master/examples/ecc_test/ecc_test.ino
//Not cryptographically sound, but we don't really care since this
//is merely a test device. 
static int RNG(uint8_t *dest, unsigned size) {
  // Use the least-significant bits from the ADC for an unconnected pin (or connected to a source of 
  // random noise). This can take a long time to generate random data if the result of analogRead(0) 
  // doesn't change very frequently.
  while (size) {
    uint8_t val = 0;
    for (unsigned i = 0; i < 8; ++i) {
      int init = analogRead(28);
      int count = 0;
      while (analogRead(28) == init) {
        ++count;
      }
      
      if (count == 0) {
         val = (val << 1) | (init & 0x01);
      } else {
         val = (val << 1) | (count & 0x01);
      }
    }
    *dest = val;
    ++dest;
    --size;
  }
  // NOTE: it would be a good idea to hash the resulting random data using SHA-256 or similar.
  return 1;
}

//copy of above function that accepts oob uint8_t* buffer for input of random data
void oob_rng(uint8_t **dest, unsigned size){
  uint8_t* ptr;
  ptr = *dest;
  while (size) {
    uint8_t val = 0;
    for (unsigned i = 0; i < 8; ++i) {
      int init = analogRead(28);
      int count = 0;
      while (analogRead(28) == init) {
        ++count;
      }
      
      if (count == 0) {
         val = (val << 1) | (init & 0x01);
      } else {
         val = (val << 1) | (count & 0x01);
      }
    }
    *ptr = val;
    ++ptr;
    --size;
  }
}


void setupDHKeys(){
  uint8_t* secret;
  uint8_t* pubkey;
  int keyresult;
  secret = (uint8_t*)calloc(NUM_ECC_DIGITS+4, sizeof(uint8_t));
  pubkey = (uint8_t*)calloc((NUM_ECC_DIGITS*2)+4, sizeof(uint8_t));//cover x and y portion of public key
  uECC_set_rng(&RNG);
  Serial.println("Starting DH Key creation.");
  const struct uECC_Curve_t * curve = uECC_secp256r1();

  Serial.println("Initialized curve");
  int z = uECC_make_key(pubkey, secret, curve);
  Serial.print("Key creation return: ");
  Serial.println(z,DEC);
  Serial.println("Created keys");
  Bluefruit.updateDHKeys(pubkey, secret);
}

void setupCharacteristics() {
  
  BLEService sec1 = BLEService(0x1100);
  
  BLECharacteristic sec1Level1Read = BLECharacteristic(0x1111); 
  BLECharacteristic sec1Level2Read = BLECharacteristic(0x2111);
  BLECharacteristic sec1Level3Read = BLECharacteristic(0x3111);
  BLECharacteristic sec1Level4Read = BLECharacteristic(0x4111);

  BLECharacteristic sec1Level1Write = BLECharacteristic(0x1121); 
  BLECharacteristic sec1Level2Write = BLECharacteristic(0x2121);
  BLECharacteristic sec1Level3Write = BLECharacteristic(0x3121);
  BLECharacteristic sec1Level4Write = BLECharacteristic(0x3121);

  sec1.begin();
  
  sec1Level1Read.setProperties(CHR_PROPS_READ);
  sec1Level1Read.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  sec1Level1Read.setStringDescriptor("Sec1Level1");
  sec1Level1Read.begin();
  sec1Level1Read.write("Sec1Level1Char");

  sec1Level2Read.setProperties(CHR_PROPS_READ);
  sec1Level2Read.setPermission(SECMODE_ENC_NO_MITM, SECMODE_NO_ACCESS);
  sec1Level2Read.setStringDescriptor("Sec1Level2");
  sec1Level2Read.begin();
  sec1Level2Read.write("Sec1Level2Char");
  
  sec1Level3Read.setProperties(CHR_PROPS_READ);
  sec1Level3Read.setPermission(SECMODE_ENC_WITH_MITM, SECMODE_NO_ACCESS);
  sec1Level3Read.setStringDescriptor("Sec1Level3");
  sec1Level3Read.begin();
  sec1Level3Read.write("Sec1Level3Char");
  
  sec1Level4Read.setProperties(CHR_PROPS_READ);
  sec1Level4Read.setPermission(SECMODE_ENC_WITH_MITM_SC, SECMODE_NO_ACCESS);
  sec1Level4Read.setStringDescriptor("Sec1Level4");
  sec1Level4Read.begin();
  sec1Level4Read.write("Sec1Level4Char");

  
}


void setup() {  
  Serial.begin(9600);
  Serial.println("Begin BLEBoy setup.");
  
  /***** BUTTON SETUP ****/
  pinMode(BUTTON_UP, INPUT);
  pinMode(BUTTON_DOWN, INPUT);
  pinMode(BUTTON_RIGHT, INPUT);
  pinMode(BUTTON_LEFT, INPUT);
  //pinMode(BUTTON_5, INPUT);
  //pinMode(BUTTON_6, INPUT);
  if(!ss.begin(0x49)){
    Serial.println("ERROR!");
    while(1);
  }
  else{
    Serial.println("seesaw started");
    Serial.print("version: ");
    Serial.println(ss.getVersion(), HEX);
  }
  ss.pinModeBulk(button_mask, INPUT_PULLUP);
  ss.setGPIOInterrupts(button_mask, 1);
  pinMode(IRQ_PIN, INPUT);
  /***********************/

  /****** OLED Init ******/
  Serial.println("Buttons initialized.");
  Serial.println("Beginning OLED initialization.");
  lescStatus = Bluefruit.isLesc();
  bondedStatus = Bluefruit.isBonded();
  modelStatus = Bluefruit.getPairingAssociationModel();
  
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
  
  // Show image buffer on the display hardware.
  // Since the buffer is intialized with an Adafruit splashscreen
  // internally, this will display the splashscreen.
  display.display();
  delay(1000);
 
  // Clear the buffer.
  display.clearDisplay();
  display.display();
  /*
  Serial.println("Initializing OLED buttons.");
  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);*/

  
  // text display tests
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.display();

  /*******************************/

  /************ Menu Init **********/
  Serial.println("Initializing menu.");
  //nav.idleTask=idle;//point a function to be used when menu is suspended
  SPI.begin();
  display.begin();
  display.clearDisplay();
  display.println(F("BLEBoy"));
  display.display(); 
  delay(2000);
  display.clearDisplay();
  
  // Menu setup
  ms.get_root_menu().add_menu(&mm);

  mm.add_item(&muLed);
  mm.add_item(&muAdv);
  mm.add_item(&mu_terminate);
  mm.add_item(&mu_genoob);
  mm.add_menu(&muStatus);
  muStatus.add_item(&muStatus_miAddr);
  muStatus.add_item(&muStatus_miConn);
  //muStatus.add_item(&muStatus_miLesc);
  //muStatus.add_item(&muStatus_miAM);
  muStatus.add_item(&muStatus_miBond);
  muStatus.add_item(&muStatus_miSecMode);
  muStatus.add_item(&muStatus_miSecLevel);
  mm.add_menu(&muSettings);
  muSettings.add_item(&muSettings_miBond);
  muSettings.add_item(&muSettings_miLesc);
  muSettings.add_item(&muSettings_miMitm);
  muSettings.add_item(&muSettings_miKeypress);
  muSettings.add_item(&muSettings_miIO);
  muSettings.add_item(&muSettings_miOOB);
  muSettings.add_item(&muSettings_miClearBonds);
  
  /******************************************/


  /************ BLE Init ************/
  Serial.println("Setting BLE device name.");
  Bluefruit.setName(bleDeviceName);
  
  Serial.println("Initializing BLE.");
  Bluefruit.begin();

  Serial.println("Initializing BLE connection callbacks.");
  Bluefruit.setConnectCallback(bleConnectCallback);
  Bluefruit.setDisconnectCallback(bleDisconnectCallback);
  Bluefruit.setPasskeyDisplayCallback(blePasskeyDisplayCallback);
  Bluefruit.setPasskeyEntryCallback(blePasskeyEntryCallback);
  Bluefruit.setRNGCallback(oob_rng);

  Serial.println("Configure BLE services and properties.");
  setupCharacteristics();
  setupAdvertising();
  updateSecurityParameters();
  setupDHKeys();
  uint16_t connHandle = Bluefruit.getConnHandle();
  //calling insecure OOB data generation so we have data ready
  Bluefruit.generateOOBData(connHandle);
  updateStatus();
  Serial.print("addrString: ");
  Serial.println(addrString);
  /*********************************/
  Serial.println("Initialization complete.");
  
  ms.display();
}

/******************************************************************/

/************ DEVICE ACTIONS **************************************/

void beginAdvertising() {
  Serial.println("Start advertising.");
  Bluefruit.Advertising.start();
  Serial.println("Advertising.");
}

void endAdvertising() {
  Serial.println("Stop advertising.");
  Bluefruit.Advertising.stop();
  Serial.println("Done advertising.");
}

void terminateAllConnections() {
  Serial.println("Terminating all BLE connections.");
  Bluefruit.disconnect();
  Serial.println("Connections terminated.");
}
/******************************************************************/
void updateMenu(){
  //OLED set up
  display.display();    
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  ms.display();
  //Serial.println("");
}
int last_x = 0, last_y = 0;
void loop() {
  bool change=false;
//upCmd = down menu item
//downCmd = up menu item
/* Can enable buttons on OLED Featherwing, but for now I'm relying on Joy Featherwing
  if (! digitalRead(BUTTON_A)){
    delay(100);
      while(digitalRead(BUTTON_A) == LOW);
      nav.doNav(downCmd);
      if(passkeyTriggered){
        passkeyDismissed = true;
      }
  }
  if (! digitalRead(BUTTON_B)){
    delay(100);
      while(digitalRead(BUTTON_B) == LOW);
      nav.doNav(enterCmd);
      if(passkeyTriggered){
        passkeyDismissed = true;
      }
  }
  if (! digitalRead(BUTTON_C)){
    delay(100);
      while(digitalRead(BUTTON_C) == LOW);
      nav.doNav(upCmd);
      if(passkeyTriggered){
        passkeyDismissed = true;
      }
  }*/
  //JoyWing Joystick
  //only allowing up and down movement for now
  int x = ss.analogRead(2);
  int y = ss.analogRead(3);
  /*   Joystick Values
   *        0
   *        |
   * 0------|-------1023   y
   *        |
   *       1023
   *        x
   *       
   *       Center Point (where we don't register a move)
   *       1024/2 and we'll add a tolerance of 12
   *       500 - 524 , 500 - 524
   *       
   *       \     |     /  x = 1023 - y (eq1)
             \   |   /
               \ | /
          -------|--------   y
               / | \
             /   |   \
           /     |     \
   *             x        x = y (eq2)
   *       
   *       take y coordinate and calculate x on each line
   *       if y read > 512 then mid dir = right, else mid dir = left
   *       if x read (from joystick) is greater than eq1 x and less than eq 2 x, then it's right
   *        else if x read < eq1 and > eq2, then it's left
   *        else if x read < eq1 x then use up
   *        else if x read > eq2 x then use down
   *       
   *       
   */
  if ( ((abs(x - last_x) > 50)  ||  (abs(y - last_y) > 50))) {
  //if ( (abs(x - last_x) > 50) ) {
    //Serial.print(x); Serial.print(", "); Serial.println(y);
    int eq1 = 1023 - y;
    int eq2 = y;
    if ( x > eq1 && x < eq2 ){
      if ( y >= 524 && y <= 1024){
        //move right
        Serial.println("Analog right pressed");
        //nav.doNav(enterCmd);
        ms.select();
        change=true;
        if(passkeyTriggered){
          passkeyDismissed = true;
        }
      }
      
    }
    else if ( x < eq1 && x > eq2 ){
      //determine if left or right move
      if ( y >= 0 && y <= 500){
        //move left 
        ms.back();
        change=true;
        if(passkeyTriggered){
          passkeyDismissed = true;
        }
      }
      
    }
    else if ( x <= eq1 && x >= 0 && x <= 500){
      //move up
      ms.prev();
      change=true;
      if(passkeyTriggered){
        passkeyDismissed = true;
      }
    }
    else if ( x >= eq2 && x >= 524 && x <= 1024) {
      //move down
      ms.next();
      change=true;
      if(passkeyTriggered){
        passkeyDismissed = true;
      }
    }
    delay(10);
    last_x = x;
    last_y = y;
  }
  //JoyWing Momentary Switches
  // pin 27 on JoyWing must have option interrupt shorted
  if(!digitalRead(IRQ_PIN)){
    uint32_t buttons = ss.digitalReadBulk(button_mask);
    if (! (buttons & (1 << BUTTON_RIGHT))) {
      delay(10);
      ms.select();
      change=true;
      if(passkeyTriggered){
        passkeyDismissed = true;
      }
    }
    if (! (buttons & (1 << BUTTON_DOWN))) {
      delay(10);
      ms.next();
      change=true;
      if(passkeyTriggered){
        passkeyDismissed = true;
      }
    }
    if (! (buttons & (1 << BUTTON_LEFT))) {
      delay(10);
      ms.back();
      change=true;
      if(passkeyTriggered){
        passkeyDismissed = true;
      }
    }
    if (! (buttons & (1 << BUTTON_UP))) {
      delay(10);
      ms.prev();
      change=true;
      if(passkeyTriggered){
        passkeyDismissed = true;
      }
    }
    if (! (buttons & (1 << BUTTON_SEL))) {
      delay(10);
    }
  }
  
  //Clears passkey display after button has
  //been pressed. Removes artifacts of the
  //passkey display.
  if(passkeyTriggered && passkeyDismissed){
    passkeyTriggered = false;
    passkeyDismissed = false;
    updateStatus();
    ms.display();
  }

  if(change){
    updateStatus();
    ms.display();
    digitalWrite(LED_BUILTIN,ledCtrl);
  }
  

  //This delay may need to be shortened
  delay(100);
}
