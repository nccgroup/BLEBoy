/**************************************************************************/
/*!
    @file     bluefruit.h
    @author   hathach

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2016, Adafruit Industries (adafruit.com)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/**************************************************************************/
#ifndef BLUEFRUIT_H_
#define BLUEFRUIT_H_

#include <Arduino.h>
#include "bluefruit_common.h"
#include <uECC.h>

// Note chaning these parameters will affect APP_RAM_BASE
// --> need to update RAM in feather52_s132.ld linker
#define BLE_GATTS_ATTR_TABLE_SIZE        0x0B00
#define BLE_VENDOR_UUID_MAX              10
#define BLE_PRPH_MAX_CONN                1
#define BLE_CENTRAL_MAX_CONN             4
#define BLE_CENTRAL_MAX_SECURE_CONN      1 // should be enough

//Defining pairing association models to track which was used
#define BLE_ASSOCIATION_JUSTWORKS 1
#define BLE_ASSOCIATION_NUMERICCOMPARISON 2
#define BLE_ASSOCIATION_PASSKEYENTRY 3
#define BLE_ASSOCIATION_OOB 4

#include "BLEUuid.h"
#include "BLEAdvertising.h"
#include "BLECharacteristic.h"
#include "BLEService.h"

#include "BLECentral.h"
#include "BLEClientCharacteristic.h"
#include "BLEClientService.h"
#include "BLEDiscovery.h"
#include "BLEGap.h"
#include "BLEGatt.h"

// Services
#include "services/BLEDis.h"
#include "services/BLEDfu.h"
#include "services/BLEUart.h"
#include "services/BLEBas.h"
#include "services/BLEBeacon.h"
#include "services/BLEHidGeneric.h"
#include "services/BLEHidAdafruit.h"
#include "services/BLEMidi.h"

#include "clients/BLEAncs.h"
#include "clients/BLEClientUart.h"
#include "clients/BLEClientDis.h"

#define BLE_MAX_DATA_PER_MTU  (GATT_MTU_SIZE_DEFAULT - 3)
#define BLE_GAP_LESC_P256_SK_LEN 32
typedef struct
    {
      uint8_t   sk[BLE_GAP_LESC_P256_SK_LEN];        /**< LE Secure Connections Elliptic Curve Diffie-Hellman P-256 Private Key in little-endian. */
    } ble_gap_lesc_p256_sk_t;

extern "C"
{
  void SD_EVT_IRQHandler(void);
}

class AdafruitBluefruit
{
  public:
    typedef void (*connect_callback_t   ) (void);
    typedef void (*disconnect_callback_t) (uint8_t reason);
    //*result is a pass by reference variable that allows us to know
    //if a numeric comparison match is accepted or rejected. 
    typedef void (*passkey_display_callback_t) (int match_request, const uint8_t* passkey, int* result);
    //*passkey is a pass by reference variable used to store user-supplied passkey (displayed on central device)
    typedef void (*passkey_entry_callback_t) (uint16_t connHandle, bool sendKeypressNotifications, uint8_t key_type, uint8_t* passkey);

    typedef void (*rng_callback_t) (uint8_t** buffer, unsigned size);

    // Constructor
    AdafruitBluefruit(void);

    err_t begin(bool prph_enable = true, bool central_enable = false);

    /*------------------------------------------------------------------*/
    /* Lower Level Classes (Bluefruit.Advertising.*, etc.)
     *------------------------------------------------------------------*/
    BLEAdvertising Advertising;
    BLEAdvertising ScanResponse;
    BLECentral     Central;
    BLEDiscovery   Discovery;

    BLEGap         Gap;
    BLEGatt        Gatt;

    /*------------------------------------------------------------------*/
    /* General Purpose Functions
     *------------------------------------------------------------------*/
    void   autoConnLed         (bool enabled);
    void   setConnLedInterval  (uint32_t ms);
    void   startConnLed        (void);
    void   stopConnLed         (void);
    void   setName             (const char* str);
    char*  getName             (void);
    bool   setTxPower          (int8_t power);
    int8_t getTxPower          (void);
    bool   isLesc              (void);
    bool   isBonded              (void);
    uint8_t getPairingAssociationModel (void);
    uint8_t*  getAddr(void);
    uint16_t getConnHandle(void);


    /*------------------------------------------------------------------*/
    /* GAP, Connections and Bonding
     *------------------------------------------------------------------*/
    bool     connected         (void);
    void     disconnect        (void);

    bool     setConnInterval   (uint16_t min, uint16_t max);
    bool     setConnIntervalMS (uint16_t min_ms, uint16_t max_ms);

    void updateSecParams(int bond, int mitm, int lesc, int keypress, int io_caps, int oob);
    void updateDHKeys(uint8_t *pubkey, uint8_t *secret);
    void updateLegacyOOBKey(uint8_t *key);
    void updateSCOOBKey();
    void generateOOBData(uint16_t conn_handle);

    uint8_t* getLegacyOOBKey();
    ble_gap_lesc_oob_data_t getSCOOBKey();
    uint8_t* getSCOOBRandom();
    uint8_t* getSCOOBConfirm();

    void sendKeypressNotification(uint16_t connHandle, uint8_t notificationType);

    uint16_t connHandle        (void);
    bool     connPaired        (void);
    uint16_t connInterval      (void);

    uint8_t getConnectionSecurityMode(uint16_t connHandle);
    uint8_t getConnectionSecurityLevel(uint16_t connHandle);


    bool     requestPairing    (void);
    void     clearBonds        (void);

    ble_gap_addr_t peerAddr(void);

    /*------------------------------------------------------------------*/
    /* Callbacks
     *------------------------------------------------------------------*/
    void setConnectCallback   ( connect_callback_t    fp);
    void setDisconnectCallback ( disconnect_callback_t fp);
    void setPasskeyDisplayCallback (passkey_display_callback_t fp);
    void setPasskeyEntryCallback (passkey_entry_callback_t fp);

    void setRNGCallback (rng_callback_t fp);

    bool setPIN(const char* pin); 

  private:
    /*------------- BLE para -------------*/
    bool _prph_enabled;
    bool _central_enabled;

    // Peripheral Preferred Connection Parameters (PPCP)
    uint16_t _ppcp_min_conn;
    uint16_t _ppcp_max_conn;

    // Actual connection interval in use
    uint16_t _conn_interval;

    int8_t _tx_power;
    char _name[32+1];

    SemaphoreHandle_t _ble_event_sem;
    SemaphoreHandle_t _soc_event_sem;

    TimerHandle_t _led_blink_th;
    bool _led_conn;

    BLEDfu _dfu_svc;

    uint16_t _conn_hdl;
    //static __ALIGN(4) 
    ble_gap_lesc_p256_pk_t pk;
    
    ble_gap_lesc_p256_sk_t sk;
    ble_gap_lesc_p256_pk_t pk_peer;
    
    ble_gap_lesc_dhkey_t dhk;

    /*
        ble_gap_addr_t  addr
            uint8_t     addr_type
            uint8_t     addr [6]
        uint8_t     r [16]
        uint8_t     c [16]
    */
    ble_gap_lesc_oob_data_t p_oobd_own;
    ble_gap_lesc_oob_data_t p_oobd_peer;
    uint8_t* legacy_oob_key;

    ble_gap_conn_sec_t connection_security;


    bool _lescPairing = false;
    bool     _bonded;
    bool _sendDHKey = false;
    uint8_t _pairingAssociationModel;

public: // TODO temporary for bledfu to load bonding data
    struct
    {
      // Keys
      ble_gap_enc_key_t own_enc;
      ble_gap_enc_key_t peer_enc;
      ble_gap_id_key_t  peer_id;
    } _bond_data;

private:
    //ble_gap_addr_t peer_addr;
    ble_gap_sec_params_t _sec_param;
    ble_gap_sec_params_t _peer_sec_param;
    ble_gap_addr_t    _peer_addr;
    ble_gap_addr_t    _addr;

    uint8_t _auth_type;
    char _pin[BLE_GAP_PASSKEY_LEN];


    /*------------- Callbacks -------------*/
    connect_callback_t    _connect_cb;
    disconnect_callback_t _discconnect_cb;
    passkey_display_callback_t _passkey_display_cb;
    passkey_entry_callback_t _passkey_entry_cb;

    rng_callback_t _rng_cb;

    bool _addToAdv(bool scan_resp, uint8_t type, const void* data, uint8_t len);

    bool _saveBondKeys(void);
    bool _loadBondKeys(uint8_t* addr);

    char* _convertBytesToHexString(uint8_t* string, int size);

    void _saveBondedCCCD(void);
    void _loadBondedCCCD(uint8_t* addr);

    void _ble_handler(ble_evt_t* evt);

    friend void SD_EVT_IRQHandler(void);
    friend void adafruit_ble_task(void* arg);
    friend void adafruit_soc_task(void* arg);
    friend class BLECentral;
};

extern AdafruitBluefruit Bluefruit;

#endif
