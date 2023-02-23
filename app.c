/***************************************************************************//**
 * @file
 * @brief Core application logic.
 *******************************************************************************
 * # License
 * <b>Copyright 2021 Silicon Laboratories Inc. www.silabs.com</b>
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
#include "app_assert.h"
#include "app_log.h"
#include "sl_bluetooth.h"
#include "gatt_db.h"
#include "app.h"
#include "sl_simple_button_instances.h"
#include "sl_simple_led_instances.h"

//my own global vars

#define MODIFIER_INDEX 0
#define DATA_INDEX 2

static uint8_t input_report_data[] = {0, 0, 0, 0, 0, 0, 0, 0};
static uint8_t actual_key, modifier;

static uint8_t notification_enabled = 0;
static uint8_t connection_handle = 0xff;

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;

static bool report_button_flag = false;

static bool button_state_tmp = false;

// Updates the Report Button characteristic.
//static sl_status_t update_report_button_characteristic(void);
// Sends notification of the Report Button characteristic.
//static sl_status_t send_report_button_notification(void);

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
  // Make sure there will be no button events before the boot event.
  //sl_button_disable(SL_SIMPLE_BUTTON_INSTANCE(0));
  //sl_led_turn_on(SL_SIMPLE_LED_INSTANCE(0));

  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application init code here!                         //
  // This is called once during start-up.                                    //
  /////////////////////////////////////////////////////////////////////////////
  //GPIO_PinModeSet(gpioPortB,0,gpioModeWiredAndPullUp,0);
  //sl_button_enable(SL_SIMPLE_BUTTON_INSTANCE(0));
}

/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
SL_WEAK void app_process_action(void)
{
  // Check if there was a report button interaction.
  //if (report_button_flag) {
  //  sl_status_t sc;

   // report_button_flag = false; // Reset flag

   // sc = update_report_button_characteristic();
   // app_log_status_error(sc);

   // if (sc == SL_STATUS_OK) {
   //   sc = send_report_button_notification();
   //   app_log_status_error(sc);
   // }
  //}

  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application code here!                              //
  // This is called infinitely.                                              //
  // Do not call blocking functions from here!                               //
  /////////////////////////////////////////////////////////////////////////////
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
      //sc = sl_bt_advertiser_start(
      //  advertising_set_handle,
      //  sl_bt_advertiser_general_discoverable,
      //  sl_bt_advertiser_connectable_scannable);
      sc = sl_bt_legacy_advertiser_generate_data(
          advertising_set_handle,
          sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);
      sc = sl_bt_legacy_advertiser_start(
          advertising_set_handle,
          sl_bt_legacy_advertiser_connectable);
      app_assert_status(sc);
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

      if ((notification_enabled == 1) && (connection_handle != 0xff))
      {
          memset(input_report_data, 0, sizeof(input_report_data));

          input_report_data[MODIFIER_INDEX] = modifier;
          input_report_data[DATA_INDEX] = actual_key;

          sc = sl_bt_gatt_server_send_notification(connection_handle, gattdb_report, 8, input_report_data);
          app_assert_status(sc);

          app_log("Key report was sent\r\n");
      }
      break;

    case sl_bt_evt_gatt_server_characteristic_status_id:

      if (evt->data.evt_gatt_server_characteristic_status.characteristic == gattdb_report) {
          // client characteristic configuration changed by remote GATT client
          if (evt->data.evt_gatt_server_characteristic_status.status_flags == gatt_server_client_config) {
              if (evt->data.evt_gatt_server_characteristic_status.client_config_flags == gatt_notification) {
                notification_enabled = 1;
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
      sc = sl_bt_legacy_advertiser_generate_data(
          advertising_set_handle,
          sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);
      sc = sl_bt_legacy_advertiser_start(
          advertising_set_handle,
          sl_bt_legacy_advertiser_connectable);
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

/***************************************************************************//**
 * Simple Button
 * Button state changed callback
 * @param[in] handle Button event handle
 ******************************************************************************/
void sl_button_on_change(const sl_button_t *handle)
{
  if (SL_SIMPLE_BUTTON_INSTANCE(0) == handle) {
    if(sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED) {
        //app_log("Button pushed - callback\r\n");
        actual_key = 0x1d;
        sl_led_turn_on(SL_SIMPLE_LED_INSTANCE(0));
    }
    else {
        // Button released
        actual_key = 0;
        sl_led_turn_off(SL_SIMPLE_LED_INSTANCE(0));
    }
    report_button_flag = true;
  }
  modifier = 0;
  sl_status_t sc = sl_bt_external_signal(1); // trigger sending key
  app_assert_status(sc);
}

/***************************************************************************//**
 * Updates the Report Button characteristic.
 *
 * Checks the current button state and then writes it into the local GATT table.
 ******************************************************************************/
/*
static sl_status_t update_report_button_characteristic(void)
{
  sl_status_t sc;
  uint8_t data_send;

  switch (sl_button_get_state(SL_SIMPLE_BUTTON_INSTANCE(0))) {
    case SL_SIMPLE_BUTTON_PRESSED:
      data_send = (uint8_t)SL_SIMPLE_BUTTON_PRESSED;
      break;

    case SL_SIMPLE_BUTTON_RELEASED:
      data_send = (uint8_t)SL_SIMPLE_BUTTON_RELEASED;
      break;

    default:
      // Invalid button state
      return SL_STATUS_FAIL; // Invalid button state
  }

  // Write attribute in the local GATT database.
  //sc = sl_bt_gatt_server_write_attribute_value(gattdb_report_button,
  //                                             0,
  //                                             sizeof(data_send),
  //                                             &data_send);
  if (sc == SL_STATUS_OK) {
    app_log_info("Attribute written: 0x%02x", (int)data_send);
  }

  return sc;
}
*/

/***************************************************************************//**
 * Sends notification of the Report Button characteristic.
 *
 * Reads the current button state from the local GATT database and sends it as a
 * notification.
 ******************************************************************************/
/*
static sl_status_t send_report_button_notification(void)
{
  sl_status_t sc;
  uint8_t data_send;
  size_t data_len;

  // Read report button characteristic stored in local GATT database.
  //sc = sl_bt_gatt_server_read_attribute_value(gattdb_report_button,
  //                                            0,
  //                                            sizeof(data_send),
  //                                            &data_len,
  //                                            &data_send);
  if (sc != SL_STATUS_OK) {
    return sc;
  }

  // Send characteristic notification.
  //sc = sl_bt_gatt_server_notify_all(gattdb_report_button,
  //                                  sizeof(data_send),
  //                                  &data_send);
  if (sc == SL_STATUS_OK) {
    app_log_append(" Notification sent: 0x%02x\n", (int)data_send);
  }
  return sc;
}
*/
