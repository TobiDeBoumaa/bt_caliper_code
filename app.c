/***************************************************************************//**
 * @file
 * @brief Core application logic.
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
#include "em_common.h"
//#include "em_gpio.h"
#include "app_assert.h"
#include "sl_bluetooth.h"
#include "gatt_db.h"
#include "app.h"

#include "app_log.h"
#include "sl_simple_button_instances.h"
#include "sl_simple_led_instances.h"
#include "sl_spidrv_instances.h"

#define KEY_ARRAY_SIZE 25
#define MODIFIER_INDEX 0
#define DATA_INDEX 2

#define SHIFT_KEY_CODE 0x02

static uint8_t input_report_data[] = {0, 0, 0, 0, 0, 0, 0, 0};
static uint8_t actual_key, modifier;
static uint8_t counter=0;
static uint8_t bufferedKeyLen=0;
static uint8_t bufferedKeys[10]={0,0,0,0x36,0,0,0x10,0x10};

static uint8_t notification_enabled = 0;
static uint8_t connection_handle = 0xff;

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;

static const uint8_t reduced_key_array[] =
{
  0x04,   /* a */
  0x05,   /* b */
  0x06,   /* c */
  0x07,   /* d */
  0x08,   /* e */
  0x09,   /* f */
  0x0a,   /* g */
  0x0b,   /* h */
  0x0c,   /* i */
  0x0d,   /* j */
  0x0e,   /* k */
  0x0f,   /* l */
  0x10,   /* m */
  0x11,   /* n */
  0x12,   /* o */
  0x13,   /* p */
  0x14,   /* q */
  0x15,   /* r */
  0x16,   /* s */
  0x17,   /* t */
  0x18,   /* u */
  0x19,   /* v */
  0x1a,   /* w */
  0x1b,   /* x */
  0x1c,   /* y */
  0x1d,   /* z */
};

static const uint8_t key_Numbers[] =
{
  0x27,   /* 0 */
  0x1E,   /* 1 */
  0x1F,   /* 2 */
  0x20,   /* 3 */
  0x21,   /* 4 */
  0x22,   /* 5 */
  0x23,   /* 6 */
  0x24,   /* 7 */
  0x25,   /* 8 */
  0x26,   /* 9 */
};

uint16_t spi_rxbuffer[2]={0};
int32_t lastValue=0;
bool isInInch = false;
/*************************************************************************//**
 * Uart Callback
 *****************************************************************************/
void TransferComplete(SPIDRV_Handle_t handle, Ecode_t transferStatus, int itemsTransferred)
{
  if (transferStatus == ECODE_EMDRV_SPIDRV_OK) {
      uint32_t combiBuffer = spi_rxbuffer[0] + (spi_rxbuffer[1]<<12); // tranmition contains 2 12 bit frames
      lastValue =  combiBuffer & 0x0FFFFF;  // last 24bit contain data
      lastValue *= combiBuffer & 0x100000 ? -1:1;  // bi 25 determins if it is a negativ number
      isInInch = combiBuffer & 0x800000;
  }
  SPIDRV_SReceive(sl_spidrv_usart_spifahrer_handle, &spi_rxbuffer, 3, TransferComplete, 0 );
}

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
  sl_status_t sc;
  //sl_uartdrv_init_instances(); // done automaticlly
  //sc = sl_uartdrv_set_default(&sl_uartdrv_usart_caliprUART_handle);
  //app_assert_status(sc);
  sl_spidrv_usart_spifahrer_handle->peripheral.usartPort->CTRL |= USART_CTRL_CSINV_ENABLE;
  //GPIO->P[gpioPortC].CTRL
  //GPIO_PinModeSet(gpioPortC,1,gpioModeInputPull,1);




  sc = SPIDRV_SReceive(sl_spidrv_usart_spifahrer_handle, &spi_rxbuffer, 3, TransferComplete, 0 );
  app_assert_status(sc);
}

/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
SL_WEAK void app_process_action(void)
{

}

/**************************************************************************//**
 * Bluetooth stack event handler.
 * This overrides the dummy weak implementation.
 *
 * @param[in] evt Event coming from the Bluetooth stack.
 *****************************************************************************/
void sl_bt_on_event(sl_bt_msg_t *evt)
{
  sl_status_t sc;
  bd_addr address;
  uint8_t address_type;
  uint8_t system_id[8];

  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:

      // Extract unique ID from BT Address.
      sc = sl_bt_system_get_identity_address(&address, &address_type);
      app_assert_status(sc);

      // Pad and reverse unique ID to get System ID.
      system_id[0] = address.addr[5];
      system_id[1] = address.addr[4];
      system_id[2] = address.addr[3];
      system_id[3] = 0xFF;
      system_id[4] = 0xFE;
      system_id[5] = address.addr[2];
      system_id[6] = address.addr[1];
      system_id[7] = address.addr[0];

      sc = sl_bt_gatt_server_write_attribute_value(gattdb_system_id,
                                                   0,
                                                   sizeof(system_id),
                                                   system_id);
      app_assert_status(sc);

      // Create an advertising set.
      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      app_assert_status(sc);

      // Set advertising interval to 100ms.
      sc = sl_bt_advertiser_set_timing(
        advertising_set_handle,
        160, // min. adv. interval (milliseconds * 1.6)
        160, // max. adv. interval (milliseconds * 1.6)
        0,   // adv. duration
        0);  // max. num. adv. events
      app_assert_status(sc);
      // Start general advertising and enable connections.
      sc = sl_bt_advertiser_start(
        advertising_set_handle,
        advertiser_general_discoverable,
        advertiser_connectable_scannable);
      app_assert_status(sc);
      //sl_led_turn_on(&sl_led_btnled);
      break;

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:

      app_log("Connection opened\r\n");

      notification_enabled = 0;
      connection_handle = evt->data.evt_connection_opened.connection;

      sl_bt_sm_increase_security(evt->data.evt_connection_opened.connection);
      break;

    case sl_bt_evt_sm_bonded_id:

      app_log("Successful bonding\r\n");
      break;

    case  sl_bt_evt_sm_bonding_failed_id:

      app_log("Bonding failed, reason: 0x%2X\r\n", evt->data.evt_sm_bonding_failed.reason);
      /* Previous bond is broken, delete it and close connection, host must retry at least once */
      sl_bt_sm_delete_bondings();
      break;

    case  sl_bt_evt_system_external_signal_id:
      while(--bufferedKeyLen) {
      if ((notification_enabled == 1) && (connection_handle != 0xff))
      {
          memset(input_report_data, 0, sizeof(input_report_data));

          input_report_data[MODIFIER_INDEX] = modifier;
          input_report_data[DATA_INDEX] = bufferedKeys[sizeof(bufferedKeys)-bufferedKeyLen];

          while(sl_bt_gatt_server_send_notification(connection_handle, gattdb_report, 8, input_report_data));
          //app_assert_status(sc);

          //send 0
          memset(input_report_data, 0, sizeof(input_report_data));

          while(sl_bt_gatt_server_send_notification(connection_handle, gattdb_report, 8, input_report_data));

          app_log("Key report was sent\r\n");
      }
      }
      break;

    case sl_bt_evt_gatt_server_characteristic_status_id:

      if (evt->data.evt_gatt_server_characteristic_status.characteristic == gattdb_report) {
          // client characteristic configuration changed by remote GATT client
          if (evt->data.evt_gatt_server_characteristic_status.status_flags == gatt_server_client_config) {
              if (evt->data.evt_gatt_server_characteristic_status.client_config_flags == gatt_notification) {
                notification_enabled = 1;
                //sl_led_turn_off(&sl_led_btnled);
              }
              else {
                  notification_enabled = 0;
                }
              }
            }

      break;

    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:

      notification_enabled = 0;
      connection_handle = 0xff;

      // Restart advertising after client has disconnected.
      sc = sl_bt_advertiser_start(
        advertising_set_handle,
        advertiser_general_discoverable,
        advertiser_connectable_scannable);
      app_assert_status(sc);
      break;

    ///////////////////////////////////////////////////////////////////////////
    // Add additional event handlers here as your application requires!      //
    ///////////////////////////////////////////////////////////////////////////

    // -------------------------------
    // Default event handler.
    default:
      break;
  }
}

void sl_button_on_change(const sl_button_t *handle)
{
  if(&sl_button_button == handle){
      if (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED){
          //sl_led_turn_on(&sl_led_btnled);
          app_log("Button pushed - callback\r\n");
      }
      else{
          if(isInInch == false){
            uint8_t bufferedKeysSOLL[10]={0,0,0,0x36,0,0,0x10,0x10,0x28};
            memcpy(bufferedKeys, bufferedKeysSOLL, sizeof(bufferedKeys));
            bufferedKeyLen = 10;
            bufferedKeys[5]= key_Numbers[lastValue%10];
            lastValue /= 10;
            bufferedKeys[4]= key_Numbers[lastValue%10];
            lastValue /= 10;
            bufferedKeys[2]= key_Numbers[lastValue%10];
            lastValue /= 10;
            if(lastValue){
                bufferedKeys[1]= key_Numbers[lastValue%10];
                lastValue /= 10;
            }
            if(lastValue<0){
              bufferedKeys[0]= 0x2D;
            }else {
              if(lastValue){
                  bufferedKeys[0]= key_Numbers[lastValue%10];
                  lastValue /= 10;
              }
            }
          } else {
              uint8_t bufferedKeysSOLL[10]={0x0C,0x11,0x06,0x0B,0x2C,0x05,0x15,0x12,0x0E,0x11};
              memcpy(bufferedKeys, bufferedKeysSOLL, sizeof(bufferedKeys));
              bufferedKeyLen = 10;
          }
          //sl_led_turn_off(&sl_led_btnled);
          app_log("Button released - callback \r\n");
          sl_bt_external_signal(1);
      }
  }
}
