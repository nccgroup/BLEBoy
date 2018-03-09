/**************************************************************************/
/*!
    @file     bluefruit.cpp
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

#include "bluefruit.h"
#include "AdaCallback.h"
#include <Nffs.h>
#include <uECC.h>

#define SVC_CONTEXT_FLAG                 (BLE_GATTS_SYS_ATTR_FLAG_SYS_SRVCS | BLE_GATTS_SYS_ATTR_FLAG_USR_SRVCS)

#define CFG_BLE_TX_POWER_LEVEL           0
#define CFG_DEFAULT_NAME                 "Bluefruit52"
#define CFG_ADV_BLINKY_INTERVAL          500

#define CFG_BLE_TASK_STACKSIZE          (512*3)
#define CFG_SOC_TASK_STACKSIZE          (200)

#define CFG_BOND_NFFS_DIR                "/adafruit/bond"
//Filename = BDADDR or Filename = BDADDR_cccd
#define BOND_FILENAME                    CFG_BOND_NFFS_DIR "/%12s"
#define BOND_FILENAME_LEN                (sizeof(CFG_BOND_NFFS_DIR) + 18)

AdafruitBluefruit Bluefruit;

/*------------------------------------------------------------------*/
/* PROTOTYPTES
 *------------------------------------------------------------------*/
extern "C"
{
  void hal_flash_event_cb(uint32_t event) ATTR_WEAK;
}

void adafruit_ble_task(void* arg);
void adafruit_soc_task(void* arg);

/*------------------------------------------------------------------*/
/* INTERNAL FUNCTION
 *------------------------------------------------------------------*/
static void bluefruit_blinky_cb( TimerHandle_t xTimer )
{
  (void) xTimer;
  ledToggle(LED_BLUE);
}

/**
 * Constructor
 */
AdafruitBluefruit::AdafruitBluefruit(void)
  : Central()
{
  _prph_enabled    = true;
  _central_enabled = false;

  _ble_event_sem    = NULL;
  _soc_event_sem    = NULL;

  _led_blink_th     = NULL;
  _led_conn         = true;

  _tx_power  = 0;

  strcpy(_name, CFG_DEFAULT_NAME);

  _conn_hdl  = BLE_CONN_HANDLE_INVALID;
  _bonded    = false;

   _ppcp_min_conn = BLE_GAP_CONN_MIN_INTERVAL_DFLT;
   _ppcp_max_conn = BLE_GAP_CONN_MAX_INTERVAL_DFLT;
   _conn_interval = 0;


  _auth_type = BLE_GAP_AUTH_KEY_TYPE_NONE;
  varclr(_pin);


  varclr(&_bond_data);
  _bond_data.own_enc.master_id.ediv = 0xFFFF; // invalid value for ediv

  _sec_param = (ble_gap_sec_params_t)
              {
                .bond         = 1,
                .mitm         = 0,
                .lesc         = 0,
                .keypress     = 0,
                .io_caps      = BLE_GAP_IO_CAPS_NONE,
                .oob          = 0,
                .min_key_size = 7,
                .max_key_size = 16,
                .kdist_own    = { .enc = 1, .id = 1},
                .kdist_peer   = { .enc = 1, .id = 1},
              };           
  _connect_cb     = NULL;
  _discconnect_cb = NULL;
  _passkey_display_cb = NULL;
  _passkey_entry_cb = NULL;
  _pairingAssociationModel = BLE_ASSOCIATION_JUSTWORKS;

  _rng_cb = NULL;

  varclr(legacy_oob_key);
  
}

void AdafruitBluefruit::updateSecParams(int bond, int mitm, int lesc, int keypress, int io_caps, int oob){
  _sec_param = (ble_gap_sec_params_t)
              {
                .bond         = bond,
                .mitm         = mitm,
                .lesc         = lesc,
                .keypress     = keypress,
                .io_caps      = io_caps,
                .oob          = oob,
                .min_key_size = 7,
                .max_key_size = 16,
                .kdist_own    = { .enc = 1, .id = 1},
                .kdist_peer   = { .enc = 1, .id = 1},
              };

  Serial.print("Set bond: ");
  Serial.println(_sec_param.bond,DEC);
  Serial.print("Set mitm: ");
  Serial.println(_sec_param.mitm,DEC);
  Serial.print("Set lesc: ");
  Serial.println(_sec_param.lesc,DEC);
  Serial.print("Set keypress: ");
  Serial.println(_sec_param.keypress,DEC);
  Serial.print("Set io: ");
  Serial.println(_sec_param.io_caps,DEC);
  Serial.print("Set oob: ");
  Serial.println(_sec_param.oob,DEC);
}

void AdafruitBluefruit::updateDHKeys(uint8_t *pubkey, uint8_t *secret){
  Serial.println("In updateDHKeys.");
  memcpy(pk.pk, pubkey, BLE_GAP_LESC_P256_PK_LEN);
  pk.pk[BLE_GAP_LESC_P256_PK_LEN] = '\0';
  Serial.println("Copied public key");
  memcpy(sk.sk, secret, BLE_GAP_LESC_P256_PK_LEN);
  sk.sk[32] = '\0';
  Serial.println("Copied secret key.");
}

//This function will only be called in the stack upon bluefruit init or a disconnection event
//Users will be able to set their own key using updateLegacyOOBKey
//Note: pk has to be set with updateDHKeys before this can be called
void AdafruitBluefruit::generateOOBData(uint16_t conn_handle){  
  //user can supply connection handle (if device connected),
  //else use BLE_CONN_HANDLE_INVALID (default value)
  int r = sd_ble_gap_lesc_oob_data_get(conn_handle, &pk, &p_oobd_own);

  //we really should be using a crypto secure random method...
  legacy_oob_key = (uint8_t*)calloc(16,sizeof(uint8_t));
  if(_rng_cb){
    unsigned size = 16;
    _rng_cb(&legacy_oob_key, size);
  }
  else{
    //fallback to known insecure random
    //Yes, this is terrible practice, but this is meant for training, not meant to be secure
    //The version of NRF52 nordic SDK used here doesn't yet support nrf_drv_rng_bytes_available,
    //and we don't want to try the PIN reading method without the user supplying a free pin (also not a good secure method.)

    //DO NOT USE THIS FOR SECURE APPLICATIONS.
    int i;
    for(i = 0; i < 16; i++){
      legacy_oob_key[i] = (uint8_t) random(1000); //random number between 0 and 1000....it hurts....
    }

  }
}
void AdafruitBluefruit::updateSCOOBKey(){
  int r = sd_ble_gap_lesc_oob_data_get(_conn_hdl, &pk, &p_oobd_own);
}
//can be used by user to set legacy oob key if they want.
void AdafruitBluefruit::updateLegacyOOBKey(uint8_t *key){
  memcpy(legacy_oob_key, key, 16);
}

uint8_t* AdafruitBluefruit::getLegacyOOBKey(){
  return legacy_oob_key;
}

ble_gap_lesc_oob_data_t AdafruitBluefruit::getSCOOBKey(){
  return p_oobd_own;
}

uint8_t* AdafruitBluefruit::getSCOOBRandom(){
  return p_oobd_own.r;
}

uint8_t* AdafruitBluefruit::getSCOOBConfirm(){
  return p_oobd_own.c;
}

 uint16_t AdafruitBluefruit::getConnHandle(){
  return _conn_hdl;
 }

err_t AdafruitBluefruit::begin(bool prph_enable, bool central_enable)
{
  _prph_enabled    = prph_enable;
  _central_enabled = central_enable;

  // Configure Clock
  nrf_clock_lf_cfg_t clock_cfg =
  {
#if defined( USE_LFXO )
      // LFXO
      .source        = NRF_CLOCK_LF_SRC_XTAL,
      .rc_ctiv       = 0,
      .rc_temp_ctiv  = 0,
      .xtal_accuracy = NRF_CLOCK_LF_XTAL_ACCURACY_20_PPM
#else
      // LFRC
      .source        = NRF_CLOCK_LF_SRC_RC,
      .rc_ctiv       = 16,
      .rc_temp_ctiv  = 2,
      .xtal_accuracy = NRF_CLOCK_LF_XTAL_ACCURACY_20_PPM
#endif
  };

  VERIFY_STATUS( sd_softdevice_enable(&clock_cfg, NULL) );

  // Configure BLE params & ATTR Size
  ble_enable_params_t params =
  {
      .common_enable_params = { .vs_uuid_count = BLE_VENDOR_UUID_MAX },
      .gap_enable_params = {
          .periph_conn_count  = (uint8_t) (_prph_enabled    ? 1 : 0),
          .central_conn_count = (uint8_t) (_central_enabled ? BLE_CENTRAL_MAX_CONN : 0),
          .central_sec_count  = (uint8_t) (_central_enabled ? BLE_CENTRAL_MAX_SECURE_CONN : 0),
      },
      .gatts_enable_params = {
          .service_changed = 1,
          .attr_tab_size   = BLE_GATTS_ATTR_TABLE_SIZE
      }
  };

  extern uint32_t __data_start__[]; // defined in linker
  uint32_t app_ram_base = (uint32_t) __data_start__;

  VERIFY_STATUS( sd_ble_enable(&params, &app_ram_base) );

  /*------------- Configure GAP  -------------*/

  // Peripheral Preferred Connection Parameters
  ble_gap_conn_params_t   gap_conn_params =
  {
      .min_conn_interval = _ppcp_min_conn, // in 1.25ms unit
      .max_conn_interval = _ppcp_max_conn, // in 1.25ms unit
      .slave_latency     = BLE_GAP_CONN_SLAVE_LATENCY,
      .conn_sup_timeout  = BLE_GAP_CONN_SUPERVISION_TIMEOUT_MS / 10 // in 10ms unit
  };

  VERIFY_STATUS( sd_ble_gap_ppcp_set(&gap_conn_params) );

  // Default device name
  ble_gap_conn_sec_mode_t sec_mode = BLE_SECMODE_OPEN;
  VERIFY_STATUS ( sd_ble_gap_device_name_set(&sec_mode, (uint8_t const *) _name, strlen(_name)) );

  VERIFY_STATUS( sd_ble_gap_appearance_set(BLE_APPEARANCE_UNKNOWN) );
  VERIFY_STATUS( sd_ble_gap_tx_power_set( CFG_BLE_TX_POWER_LEVEL ) );

  /*------------- DFU OTA as built-in service -------------*/
  // TT: Disabling this. This is the Nordic NRF52 DFU service
  // that is accessible by any device connected to us. Seems way too dangerous to expose
  // without a specific use-case
  //_dfu_svc.begin();

  if (_central_enabled)  Central.begin(); // Init Central

  // Create RTOS Semaphore & Task for BLE Event
  _ble_event_sem = xSemaphoreCreateBinary();
  VERIFY(_ble_event_sem, NRF_ERROR_NO_MEM);

  TaskHandle_t ble_task_hdl;
  xTaskCreate( adafruit_ble_task, "SD BLE", CFG_BLE_TASK_STACKSIZE, NULL, TASK_PRIO_HIGH, &ble_task_hdl);

  // Create RTOS Semaphore & Task for SOC Event
  _soc_event_sem = xSemaphoreCreateBinary();
  VERIFY(_soc_event_sem, NRF_ERROR_NO_MEM);

  TaskHandle_t soc_task_hdl;
  xTaskCreate( adafruit_soc_task, "SD SOC", CFG_SOC_TASK_STACKSIZE, NULL, TASK_PRIO_HIGH, &soc_task_hdl);

  ada_callback_init();


  NVIC_SetPriority(SD_EVT_IRQn, 6);
  NVIC_EnableIRQ(SD_EVT_IRQn);

  // Create Timer for led advertising blinky
  _led_blink_th = xTimerCreate(NULL, ms2tick(CFG_ADV_BLINKY_INTERVAL), true, NULL, bluefruit_blinky_cb);

  // Initialize nffs for bonding (it is safe to call nffs_pkg_init() multiple time)
  Nffs.begin();
  (void) Nffs.mkdir_p(CFG_BOND_NFFS_DIR);

  sd_ble_gap_address_get(&_addr);

  return ERROR_NONE;
}

bool AdafruitBluefruit::setConnInterval(uint16_t min, uint16_t max)
{
  _ppcp_min_conn = min;
  _ppcp_max_conn = max;

  ble_gap_conn_params_t   gap_conn_params =
  {
      .min_conn_interval = _ppcp_min_conn, // in 1.25ms unit
      .max_conn_interval = _ppcp_max_conn, // in 1.25ms unit
      .slave_latency     = BLE_GAP_CONN_SLAVE_LATENCY,
      .conn_sup_timeout  = BLE_GAP_CONN_SUPERVISION_TIMEOUT_MS / 10 // in 10ms unit
  };

  VERIFY_STATUS( sd_ble_gap_ppcp_set(&gap_conn_params), false);

  return true;
}

bool AdafruitBluefruit::setConnIntervalMS(uint16_t min_ms, uint16_t max_ms)
{
  return setConnInterval( MS100TO125(min_ms), MS100TO125(max_ms) );
}


void AdafruitBluefruit::setName(const char* str)
{
  strncpy(_name, str, 32);
}

char* AdafruitBluefruit::getName(void)
{
  return _name;
}

bool AdafruitBluefruit::setTxPower(int8_t power)
{
  // Check if TX Power is valid value
  const int8_t accepted[] = { -40, -30, -20, -16, -12, -8, -4, 0, 4 };

  uint32_t i;
  for (i=0; i<sizeof(accepted); i++)
  {
    if (accepted[i] == power) break;
  }
  VERIFY(i < sizeof(accepted));

  // Apply
  VERIFY_STATUS( sd_ble_gap_tx_power_set(power), false);
  _tx_power = power;

  return true;
}

int8_t AdafruitBluefruit::getTxPower(void)
{
  return _tx_power;
}

bool   AdafruitBluefruit::isLesc(void){
  return _lescPairing;
}

bool   AdafruitBluefruit::isBonded(void){
  return _bonded;
}

uint8_t AdafruitBluefruit::getPairingAssociationModel(void){
  return _pairingAssociationModel;
}

uint8_t* AdafruitBluefruit::getAddr(void){
  return _addr.addr;

}
void AdafruitBluefruit::autoConnLed(bool enabled)
{
  _led_conn = enabled;
}

void AdafruitBluefruit::setConnLedInterval(uint32_t ms)
{
  BaseType_t active = xTimerIsTimerActive(_led_blink_th);
  xTimerChangePeriod(_led_blink_th, ms2tick(ms), 0);

  // Change period of inactive timer will also start it !!
  if ( !active ) xTimerStop(_led_blink_th, 0);
}

bool AdafruitBluefruit::connected(void)
{
  return ( _conn_hdl != BLE_CONN_HANDLE_INVALID );
}

void AdafruitBluefruit::disconnect(void)
{
  // disconnect if connected
  if ( connected() )
  {
    sd_ble_gap_disconnect(_conn_hdl, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
  }

}

void AdafruitBluefruit::setConnectCallback   ( connect_callback_t fp )
{
  _connect_cb = fp;
}

void AdafruitBluefruit::setDisconnectCallback( disconnect_callback_t fp )
{
  _discconnect_cb = fp;
}

void AdafruitBluefruit::setPasskeyDisplayCallback( passkey_display_callback_t fp )
{
  _passkey_display_cb = fp;
}

void AdafruitBluefruit::setPasskeyEntryCallback( passkey_entry_callback_t fp )
{
  _passkey_entry_cb = fp;
}

void AdafruitBluefruit::setRNGCallback( rng_callback_t fp )
{
  _rng_cb = fp;
}

uint16_t AdafruitBluefruit::connHandle(void)
{
  return _conn_hdl;
}

bool AdafruitBluefruit::connPaired(void)
{
  return _bonded;
}

uint16_t AdafruitBluefruit::connInterval(void)
{
  return _conn_interval;
}

ble_gap_addr_t AdafruitBluefruit::peerAddr(void)
{
  return _peer_addr;
}

uint8_t AdafruitBluefruit::getConnectionSecurityMode(uint16_t connHandle){
  int r = sd_ble_gap_conn_sec_get(connHandle, &connection_security);
  return connection_security.sec_mode.sm;
}

uint8_t AdafruitBluefruit::getConnectionSecurityLevel(uint16_t connHandle){
  int r = sd_ble_gap_conn_sec_get(connHandle, &connection_security);
  return connection_security.sec_mode.lv;
}


bool AdafruitBluefruit::setPIN(const char* pin)
{
  VERIFY ( strlen(pin) == BLE_GAP_PASSKEY_LEN );

  //_auth_type = BLE_GAP_AUTH_KEY_TYPE_PASSKEY;
  memcpy(_pin, pin, BLE_GAP_PASSKEY_LEN);

// Config Static Passkey
//  ble_opt_t opt
//	uint8_t passkey[] = STATIC_PASSKEY;
//	m_static_pin_option.gap.passkey.p_passkey = passkey;
//err_code = sd_ble_opt_set(BLE_GAP_OPT_PASSKEY, &m_static_pin_option);

  return true;
}


void AdafruitBluefruit::sendKeypressNotification(uint16_t connHandle, uint8_t notificationType){
  Serial.println("Sending keypress notification");
  int r = sd_ble_gap_keypress_notify(connHandle, notificationType);
  Serial.print("Notification response: ");
  Serial.println(r);
}

/*------------------------------------------------------------------*/
/* Private Methods
 *------------------------------------------------------------------*/
void AdafruitBluefruit::startConnLed(void)
{
  if (_led_conn) xTimerStart(_led_blink_th, 0);
}

void AdafruitBluefruit::stopConnLed(void)
{
  xTimerStop(_led_blink_th, 0);
}

/*------------------------------------------------------------------*/
/* Thread & SoftDevice Event handler
 *------------------------------------------------------------------*/
void SD_EVT_IRQHandler(void)
{
  // Notify both BLE & SOC Task
  xSemaphoreGiveFromISR(Bluefruit._soc_event_sem, NULL);
  xSemaphoreGiveFromISR(Bluefruit._ble_event_sem, NULL);
}

/**
 * Handle SOC event such as FLASH opertion
 */
void adafruit_soc_task(void* arg)
{
  (void) arg;

  while (1)
  {
    if ( xSemaphoreTake(Bluefruit._soc_event_sem, portMAX_DELAY) )
    {
      uint32_t soc_evt;
      uint32_t err = ERROR_NONE;

      // until no more pending events
      while ( NRF_ERROR_NOT_FOUND != err )
      {
        err = sd_evt_get(&soc_evt);

        if (ERROR_NONE == err)
        {
          switch (soc_evt)
          {
            case NRF_EVT_FLASH_OPERATION_SUCCESS:
            case NRF_EVT_FLASH_OPERATION_ERROR:
              if (hal_flash_event_cb) hal_flash_event_cb(soc_evt);
            break;

            default: break;
          }
        }
      }
    }
  }
}

/*------------------------------------------------------------------*/
/* BLE Event handler
 *------------------------------------------------------------------*/
void adafruit_ble_task(void* arg)
{
  (void) arg;

  enum { BLE_STACK_EVT_MSG_BUF_SIZE = (sizeof(ble_evt_t) + (GATT_MTU_SIZE_DEFAULT)) };

  while (1)
  {
    if ( xSemaphoreTake(Bluefruit._ble_event_sem, portMAX_DELAY) )
    {
      uint32_t err = ERROR_NONE;

      // Until no pending events
      while( NRF_ERROR_NOT_FOUND != err )
      {
        uint32_t ev_buf[BLE_STACK_EVT_MSG_BUF_SIZE/4 + 4];
        uint16_t ev_len = sizeof(ev_buf);

        // Get BLE Event
        err = sd_ble_evt_get((uint8_t*)ev_buf, &ev_len);

        // Handle valid event, ignore error
        if( ERROR_NONE == err)
        {
          Bluefruit._ble_handler( (ble_evt_t*) ev_buf );
        }
      }
    }
  }
}

void AdafruitBluefruit::_ble_handler(ble_evt_t* evt)
{
  LOG_LV1(BLE, dbg_ble_event_str(evt->header.evt_id));

  // conn handle has fixed offset regardless of event type
  const uint16_t evt_conn_hdl = evt->evt.common_evt.conn_handle;

  // GAP handler
  Gap._eventHandler(evt);

  /*------------- BLE Peripheral Events -------------*/
  /* Only handle Peripheral events with matched connection handle
   * or a few special one
   * - Connected event
   * - Advertising timeout (could be connected and advertising at the same time)
   */
  if ( evt_conn_hdl       == _conn_hdl             ||
       evt->header.evt_id == BLE_GAP_EVT_CONNECTED ||
       evt->header.evt_id == BLE_GAP_EVT_TIMEOUT )
  {
    switch ( evt->header.evt_id  )
    {
      case BLE_GAP_EVT_CONNECTED:
      {
        ble_gap_evt_connected_t* para = &evt->evt.gap_evt.params.connected;
        //TODO: Do we need to convert addresses if we have a non-static (ie private)
        // address?
        //peer_addr = evt->evt.gap_evt.params.connected.peer_addr;

        if (para->role == BLE_GAP_ROLE_PERIPH)
        {
          stopConnLed();
          if (_led_conn) ledOn(LED_BLUE);

          _conn_hdl      = evt->evt.gap_evt.conn_handle;
          _conn_interval = para->conn_params.min_conn_interval;
          _peer_addr     = para->peer_addr;

          // Connection interval set by Central is out of preferred range
          // Try to negotiate with Central using our preferred values
          if ( !is_within(_ppcp_min_conn, para->conn_params.min_conn_interval, _ppcp_max_conn) )
          {
            // Null, value is set by sd_ble_gap_ppcp_set will be used
            VERIFY_STATUS( sd_ble_gap_conn_param_update(_conn_hdl, NULL), );
          }

          if ( _connect_cb ) ada_callback(NULL, _connect_cb);
        }
      }
      break;

      case BLE_GAP_EVT_CONN_PARAM_UPDATE:
      {
        // Connection Parameter after negotiating with Central
        // min conn = max conn = actual used interval
        ble_gap_conn_params_t* param = &evt->evt.gap_evt.params.conn_param_update.conn_params;
        _conn_interval = param->min_conn_interval;
      }
      break;

      case BLE_GAP_EVT_DISCONNECTED:
        if (_led_conn)  ledOff(LED_BLUE);

        // Save all configured cccd
        if (_bonded) _saveBondedCCCD();

        _conn_hdl = BLE_CONN_HANDLE_INVALID;
        _bonded   = false;
        _lescPairing = false;
        _sendDHKey = false;
        varclr(&_peer_addr);
        varclr(&pk_peer);
        varclr(&dhk);
        varclr(&_peer_sec_param);
        varclr(legacy_oob_key);
        _pairingAssociationModel = BLE_ASSOCIATION_JUSTWORKS;
        

        if ( _discconnect_cb ) _discconnect_cb(evt->evt.gap_evt.params.disconnected.reason);

        Advertising.start();
      break;

      case BLE_GAP_EVT_TIMEOUT:
        if (evt->evt.gap_evt.params.timeout.src == BLE_GAP_TIMEOUT_SRC_ADVERTISING)
        {
          // TODO advanced advertising
          // Restart Advertising
          Advertising.start();
        }
      break;

      case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
      {
        // Pairing in progress
        varclr(&_bond_data);
        _bond_data.own_enc.master_id.ediv = 0xFFFF; // invalid value for ediv

        /* Step 1: Pairing/Bonding
         * - Central supplies its parameters
         * - We replies with our security parameters
         */
        _peer_sec_param = evt->evt.gap_evt.params.sec_params_request.peer_params;
        COMMENT_OUT(
            // Change security parameter according to authentication type
            if ( _auth_type == BLE_GAP_AUTH_KEY_TYPE_PASSKEY)
            {
              sec_para.mitm    = 1;
              sec_para.io_caps = BLE_GAP_IO_CAPS_DISPLAY_ONLY;
            }
        )

        ble_gap_sec_keyset_t keyset =
        {
            .keys_own = {
                .p_enc_key  = &_bond_data.own_enc,
                .p_id_key   = NULL,
                .p_sign_key = NULL,
                .p_pk       = &pk
            },

            .keys_peer = {
                .p_enc_key  = &_bond_data.peer_enc,
                .p_id_key   = &_bond_data.peer_id,
                .p_sign_key = NULL,
                .p_pk       = &pk_peer
            }
        };

        uint32_t replyResult = sd_ble_gap_sec_params_reply(evt->evt.gap_evt.conn_handle, BLE_GAP_SEC_STATUS_SUCCESS, &_sec_param, &keyset);
        Serial.print("BLE GAP SEC PARAMS REPLY RESULT: ");
        Serial.println(replyResult,DEC);
        VERIFY_STATUS(replyResult, RETURN_VOID);
        //VERIFY_STATUS(sd_ble_gap_sec_params_reply(evt->evt.gap_evt.conn_handle, BLE_GAP_SEC_STATUS_SUCCESS, &_sec_param, &keyset), RETURN_VOID);
      }
      break;

      case BLE_GAP_EVT_SEC_INFO_REQUEST:
      {
        Serial.println("BLE GAP SEC INFO REQUEST");
        // Reconnection. If bonded previously, Central will ask for stored keys.
        // return security information. Otherwise NULL
        ble_gap_evt_sec_info_request_t* sec_request = (ble_gap_evt_sec_info_request_t*) &evt->evt.gap_evt.params.sec_info_request;
        

        uint8_t ediv[5];
        ediv[0] = sec_request->master_id.ediv & 0x00FFU;
        ediv[1] = (sec_request->master_id.ediv& 0xFF00U) >> 8U;
        
        if ( _loadBondKeys(_peer_addr.addr) )
        {
          Serial.println("Found stored key");
          sd_ble_gap_sec_info_reply(evt->evt.gap_evt.conn_handle, &_bond_data.own_enc.enc_info, &_bond_data.peer_id.id_info, NULL);
        } else
        {
          Serial.println("Stored key not found");
          sd_ble_gap_sec_info_reply(evt->evt.gap_evt.conn_handle, NULL, NULL, NULL);
        }
      }
      break;

      case BLE_GAP_EVT_AUTH_KEY_REQUEST:
      {
        Serial.println("Got BLE GAP EVT AUTH KEY REQUEST");
        // HANDLES LESC PASSKEY ENTRY (PERIPHERAL ENTERS KEY) or Legacy passkey entry (peripheral enters key) or Legacy OOB
        //call user-defined blePasskeyEntryCallback(uint_16_t connHandle, bool sendKeypressNotifications, 
        //                                          uint8_t key_type, uint8_t* passkey)
        //  - if peer and self sec_params have keypress set to one, send keypress notifications per keypress
        //  - Send passkey to peer (sd_ble_gap_auth_key_reply)
        // Notes: connHandle allows user to send keypress notifications to a device. passKey is a passed by reference
        //

        if(evt->evt.gap_evt.params.auth_key_request.key_type == BLE_GAP_AUTH_KEY_TYPE_PASSKEY){
          uint8_t* passkey;
          bool sendKeypressNotifications = false;
          uint8_t key_type;
          passkey = (uint8_t*)calloc(7, sizeof(uint8_t));

          Serial.println("PASSKEY");

          _pairingAssociationModel = BLE_ASSOCIATION_PASSKEYENTRY;
          if(_sec_param.keypress == 1 && _peer_sec_param.keypress == 1){
            sendKeypressNotifications = true;
          }

          if ( _passkey_entry_cb ) {
            _passkey_entry_cb(evt->evt.gap_evt.conn_handle, sendKeypressNotifications, 
                              evt->evt.gap_evt.params.auth_key_request.key_type, passkey);
            Serial.print("Passskey from passkey entry callback:");
            Serial.println((char*)passkey);
          }

          int r = sd_ble_gap_auth_key_reply(evt->evt.gap_evt.conn_handle, evt->evt.gap_evt.params.auth_key_request.key_type,
                                            passkey);
          Serial.print("Auth key reply result:");
          Serial.println(r,DEC);

          _sendDHKey = true;
        }
        else if(evt->evt.gap_evt.params.auth_key_request.key_type == BLE_GAP_AUTH_KEY_TYPE_OOB){
          Serial.println("LEGACY OOB");
          _pairingAssociationModel = BLE_ASSOCIATION_OOB;
          int r = sd_ble_gap_auth_key_reply(evt->evt.gap_evt.conn_handle, evt->evt.gap_evt.params.auth_key_request.key_type,
                                            legacy_oob_key);
          Serial.print("Auth key reply result:");
          Serial.println(r,DEC);
          //By this point, it's assumed that the TK has been set. 

        }
      }
      break;

      case BLE_GAP_EVT_PASSKEY_DISPLAY:
      {
        Serial.println("Got GAP EVT PASSKEY DISPLAY");
        ble_gap_evt_passkey_display_t const* passkey_display = &evt->evt.gap_evt.params.passkey_display;
        //Forcing 6 character passkey and stripping excess data attached during LESC pairing
        uint8_t passkey[7];
        memcpy(passkey, passkey_display->passkey, 6 * sizeof(char));
        passkey[6] = '\0';
        int r = 0;
        if ( _passkey_display_cb ) {
          _pairingAssociationModel = BLE_ASSOCIATION_NUMERICCOMPARISON;
          _passkey_display_cb(passkey_display->match_request, passkey, &r);
        }
        
        //if using numeric comparison - neither host sets oob, atleast one host sets mitm,
        //both hosts set sc, and io is ((initiatior - display yesno && responder - display yesno) ||
        // (initiatior - keyboard display && responder - display yesno) || 
        // (initiatior - display yesno && responder - keyboard display) ||
        // (initiator - keyboard display && responder - keyboard display))
        // Currently we assume that this device is the responder since we are only supporting the peripheral
        // role in this case.
        if ((!_sec_param.oob && !_peer_sec_param.oob) && (_sec_param.mitm || _peer_sec_param.mitm) &&
            (_sec_param.lesc && _peer_sec_param.lesc) && ((_peer_sec_param.io_caps == BLE_GAP_IO_CAPS_DISPLAY_YESNO &&
                                                           _sec_param.io_caps == BLE_GAP_IO_CAPS_DISPLAY_YESNO) ||
                                                          (_peer_sec_param.io_caps == BLE_GAP_IO_CAPS_KEYBOARD_DISPLAY &&
                                                           _sec_param.io_caps == BLE_GAP_IO_CAPS_DISPLAY_YESNO) ||
                                                          (_peer_sec_param.io_caps == BLE_GAP_IO_CAPS_DISPLAY_YESNO &&
                                                           _sec_param.io_caps == BLE_GAP_IO_CAPS_KEYBOARD_DISPLAY) ||
                                                          (_peer_sec_param.io_caps == BLE_GAP_IO_CAPS_KEYBOARD_DISPLAY &&
                                                           _sec_param.io_caps == BLE_GAP_IO_CAPS_KEYBOARD_DISPLAY)
                                                         )
            )
        {
          //Note: According to the 5.0 spec, there are cases where the responder can automatically
          //respond, but I'm not going to implement that for simplicity (page 2016).
          if(r == 1){
            Serial.println("Numeric comparison confirm reply, sending null passkey reply.");
            r = sd_ble_gap_auth_key_reply(evt->evt.gap_evt.conn_handle, BLE_GAP_AUTH_KEY_TYPE_PASSKEY,NULL);
            Serial.print("Passkey reply result: ");
            Serial.println(r,DEC);
          }else{
            Serial.println("Numeric comparison reject reply, sending null passkey reply.");
            r = sd_ble_gap_auth_key_reply(evt->evt.gap_evt.conn_handle, BLE_GAP_AUTH_KEY_TYPE_NONE,NULL);
            Serial.print("Passkey reply result: ");
            Serial.println(r,DEC);
          }
        }else{//do standard passkey entry
           _pairingAssociationModel = BLE_ASSOCIATION_PASSKEYENTRY;
          //2 different flows, one requires the peripheral to supply the passkey and
          //the other does not. Initiator is required to input passkey when KeyboardOnly IO is used.
          if(_sec_param.io_caps == BLE_GAP_IO_CAPS_KEYBOARD_ONLY){
            Serial.println("KeyboardOnly set, sending passkey");
            r = sd_ble_gap_auth_key_reply(evt->evt.gap_evt.conn_handle, BLE_GAP_AUTH_KEY_TYPE_PASSKEY, passkey_display->passkey);
            Serial.print("Passkey reply result: ");
            Serial.println(r,DEC);
          }
          else{
            //Tell stack to send DHKey when we get the BLE_GAP_EVT_LESC_DHKEY_REQUEST
            //http://infocenter.nordicsemi.com/index.jsp?topic=%2Fcom.nordic.infocenter.sdk%2Fdita%2Fsdk%2Fnrf5_sdk.html
            //For some reason, when the peripheral doesn't need to send a passkey response, the
            //DHKEY request event doesn't happen until after the passkey display event. This flag should remedy it.
            _sendDHKey = true;
          }
        }
        if (_lescPairing){
        
          Serial.println("Using LESC pairing, attempting to send lesc dhkey reply.");
          

          r = sd_ble_gap_lesc_dhkey_reply(evt->evt.gap_evt.conn_handle, &dhk);
          Serial.print("DHKey reply result: ");
          Serial.println(r,DEC);

        }
      }
      break;

      case BLE_GAP_EVT_LESC_DHKEY_REQUEST:
      {
        Serial.println("Got BLE GAP EVT LESC DHKEY REQUEST.");

        if(evt->evt.gap_evt.params.lesc_dhkey_request.oobd_req == 1){
          Serial.println("LESC OOB PAIRING BEING USED!");
          _pairingAssociationModel = BLE_ASSOCIATION_OOB;
          //for now we don't support setting the peer's oob data. We are hardcoding their data to null
          int r = sd_ble_gap_lesc_oob_data_set(evt->evt.gap_evt.conn_handle,&p_oobd_own,NULL);
          Serial.print("Set OOB data Result: ");
          Serial.println(r,DEC);

          _sendDHKey = true;

        }

        
        Serial.println("Beginning DH key calculation.");

        const struct uECC_Curve_t * curve = uECC_secp256r1();
        int r = uECC_shared_secret(evt->evt.gap_evt.params.lesc_dhkey_request.p_pk_peer->pk, sk.sk, dhk.key, curve);
        
        dhk.key[32] = '\0';


        Serial.print("DH key calculation result: ");
        Serial.println(r,DEC);
        _lescPairing = true;

        //in passkey entry (peripheral displays), this event happens after the
        //passkey display and we need to send teh dhkey back
        //as soon as this is received in this scenario. 
        //http://infocenter.nordicsemi.com/index.jsp?topic=%2Fcom.nordic.infocenter.sdk%2Fdita%2Fsdk%2Fnrf5_sdk.html
        if(_sendDHKey){
          r = sd_ble_gap_lesc_dhkey_reply(evt->evt.gap_evt.conn_handle, &dhk);
          Serial.print("DHKey reply result: ");
          Serial.println(r,DEC);
          _sendDHKey = false;
        }
      }
      break;

      case BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST:
      {
        Serial.println("Got BLE GAP EVT CONN PARAM UPDATE REQUEST");

      }
      break;

      case BLE_GAP_EVT_CONN_SEC_UPDATE:
      {
        Serial.println("Received GAP EVT SEC UPDATE.");
        // Connection is secured aka Paired
        COMMENT_OUT( ble_gap_conn_sec_t* conn_sec = (ble_gap_conn_sec_t*) &evt->evt.gap_evt.params.conn_sec_update.conn_sec; )

          // Previously bonded --> secure by re-connection process
          // --> Load & Set Sys Attr (Apply Service Context)
          // Else Init Sys Attr
        _loadBondedCCCD(_peer_addr.addr);
        _bonded = true;
      }
      break;

      case BLE_GAP_EVT_AUTH_STATUS:
      {
        Serial.println("RECEIVED BLE GAP EVT AUTH STATUS");
        // Bonding process completed
        ble_gap_evt_auth_status_t* status = &evt->evt.gap_evt.params.auth_status;

        // Pairing/Bonding succeeded --> save encryption keys
        if (BLE_GAP_SEC_STATUS_SUCCESS == status->auth_status)
        {
          Serial.println("Successful pairing/bonding.");
          _saveBondKeys();
          _bonded = true;
        }else
        {
          Serial.print("Unsucessful pairing/bonding attempt.");
          Serial.println(status->auth_status,DEC);
          PRINT_HEX(status->auth_status);
        }
      }
      break;

      case BLE_GATTS_EVT_SYS_ATTR_MISSING:
        sd_ble_gatts_sys_attr_set(_conn_hdl, NULL, 0, 0);
      break;

      default: 
        Serial.print("Uknown BLE packet type: ");
        Serial.println(evt->header.evt_id,DEC);
        
        if(evt->header.evt_id == BLE_GATTS_EVT_WRITE){
          Serial.println("BLE GATTS EVT WRITE");
          Serial.print("Operation: ");
          Serial.println(evt->evt.gatts_evt.params.write.op, DEC);
          Serial.print("Handle: ");
          Serial.println(evt->evt.gatts_evt.params.write.handle, DEC);
          Serial.print("Data: ");
          Serial.println((char*)evt->evt.gatts_evt.params.write.data);
          }
      break;
    }
  }

  // Central Event Handler
  if (_central_enabled) Central._event_handler(evt);

  // Discovery Event Handler
  if ( Discovery.begun() ) Discovery._event_handler(evt);

  // GATTs characteristics event handler
  Gatt._eventHandler(evt);
}
/*------------------------------------------------------------------*/
/* Bonds
 *------------------------------------------------------------------*/
bool AdafruitBluefruit::requestPairing(void)
{
  // skip if already bonded
  if (_bonded) return true;

  VERIFY_STATUS( sd_ble_gap_authenticate(_conn_hdl, &_sec_param ), false);
  uint32_t start = millis();

  // timeout in 30 seconds
  while ( !_bonded && (start + 30000 > millis()) )
  {
    yield();
  }

  return _bonded;
}

void AdafruitBluefruit::clearBonds(void)
{
  // Detele bonds dir
  Nffs.remove(CFG_BOND_NFFS_DIR);

  // Create an empty one
  Nffs.mkdir_p(CFG_BOND_NFFS_DIR);
}

bool AdafruitBluefruit::_saveBondKeys(void)
{

  char filename[BOND_FILENAME_LEN];
  //sprintf(filename, BOND_FILENAME, _bond_data.own_enc.master_id.ediv);
  char* addr = _convertBytesToHexString(_peer_addr.addr, 6);
  sprintf(filename, BOND_FILENAME, addr);
  Serial.print("Saving to file: ");
  Serial.println(filename);
  return Nffs.writeFile(filename, &_bond_data, sizeof(_bond_data));
}

//bool AdafruitBluefruit::_loadBondKeys(uint16_t ediv)
bool AdafruitBluefruit::_loadBondKeys(uint8_t* addr)
{
  char filename[BOND_FILENAME_LEN];
  //sprintf(filename, BOND_FILENAME, ediv);
  char* stringAddr = _convertBytesToHexString(addr, 6);
  sprintf(filename, BOND_FILENAME, stringAddr);
  Serial.print("Attempting to load keys from: ");
    Serial.println(filename);
  bool result = Nffs.readFile(filename, &_bond_data, sizeof(_bond_data)) > 0;

  if ( result ) {
    LOG_LV1(BOND, "Load Keys from %s", filename);
  }

  return result;
}


//void AdafruitBluefruit::_loadBondedCCCD(uint16_t ediv)
void AdafruitBluefruit::_loadBondedCCCD(uint8_t* addr)
{
  bool loaded = false;

  char filename[BOND_FILENAME_LEN];
  char* stringAddr = _convertBytesToHexString(addr, 6);
  Serial.println("In load bonded cccd");
  //sprintf(filename, BOND_FILENAME "_cccd", ediv);
  sprintf(filename, BOND_FILENAME "_cccd", stringAddr);
  Serial.print("File name: ");
  Serial.println(filename);

  NffsFile file(filename, FS_ACCESS_READ);

  if ( file.exists() )
  {
    uint16_t len = file.size();
    uint8_t* sys_attr = (uint8_t*) rtos_malloc( len );

    if (sys_attr)
    {
      if ( file.read(sys_attr, len ) )
      {
        if (ERROR_NONE == sd_ble_gatts_sys_attr_set(_conn_hdl, sys_attr, len, SVC_CONTEXT_FLAG) )
        {
          loaded = true;
          Serial.println("Loaded successfully");
          LOG_LV1(BOND, "Load CCCD from %s", filename);
        }
      }

      rtos_free(sys_attr);
    }
  }

  file.close();

  if ( !loaded )
  {
    Serial.println("Stored CCCD not found for device, writing default values");
    sd_ble_gatts_sys_attr_set(_conn_hdl, NULL, 0, 0);
  }
  Serial.println("Finished loading CCCD");
}

void AdafruitBluefruit::_saveBondedCCCD(void)
{
  //if ( _bond_data.own_enc.master_id.ediv == 0xFFFF ) return;

  uint16_t len=0;
  sd_ble_gatts_sys_attr_get(_conn_hdl, NULL, &len, SVC_CONTEXT_FLAG);

  uint8_t* sys_attr = (uint8_t*) rtos_malloc( len );
  VERIFY( sys_attr, );

  if ( ERROR_NONE == sd_ble_gatts_sys_attr_get(_conn_hdl, sys_attr, &len, SVC_CONTEXT_FLAG) )
  {
    // save to file
    char filename[BOND_FILENAME_LEN];
    //sprintf(filename, BOND_FILENAME "_cccd", _bond_data.own_enc.master_id.ediv);
    char* addr = _convertBytesToHexString(_peer_addr.addr, 6);
    sprintf(filename, BOND_FILENAME "_cccd", addr);

    Nffs.writeFile(filename, sys_attr, len);

  }

  rtos_free(sys_attr);
}

char* AdafruitBluefruit::_convertBytesToHexString(uint8_t* string, int size)
{
  //char output[size + 1];
  //size*2 to cover 0xFF (1 byte) to FF (2 byte) conversion
  char* output = (char*)calloc((size*2)+1, sizeof(char));
  char *ptr = &output[0];
  int i;

  for (i = 0; i < size; i++)
  {
      ptr += sprintf(ptr, "%02X", string[i]);
  }
  return output;
}