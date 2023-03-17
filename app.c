#include "em_common.h"
//#include "em_gpio.h"
#include "app.h"
#include "app_assert.h"
#include "gatt_db.h"
#include "sl_bluetooth.h"

#include "app_log.h"
#include "sl_simple_button_instances.h"
#include "sl_simple_led_instances.h"
#include "sl_spidrv_instances.h"

#define KEY_ARRAY_SIZE 25
#define MODIFIER_INDEX 0
#define DATA_INDEX 2

#define MAXM_MESSAGE_LEN 20

static uint8_t input_report_data[] = {0, 0, 0, 0, 0, 0, 0, 0};
static uint8_t modifier;
static uint8_t bufferedKeyLen = 0;
static uint8_t bufferedKeys[MAXM_MESSAGE_LEN] = {0, 0, 0,    0x36,
                                                 0, 0, 0x10, 0x10};

static uint8_t notification_enabled = 0;
static uint8_t connection_handle = 0xff;

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;

enum reduced_key_de{A= 0x04, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Z, Y};
enum special_key_codes{KEY_ENTER= 0x28, KEY_SPACE=0x2C, KEY_COMMA=0x36, KEY_MINUS=0x38};


static const uint8_t key_Numbers[] = {
    0x27, /* 0 */
    0x1E, /* 1 */
    0x1F, /* 2 */
    0x20, /* 3 */
    0x21, /* 4 */
    0x22, /* 5 */
    0x23, /* 6 */
    0x24, /* 7 */
    0x25, /* 8 */
    0x26, /* 9 */
};

uint16_t spi_rxbuffer[3] = {0};

typedef struct {
  uint32_t caliperValue : 20;
  uint32_t isNegativ : 1;
  uint32_t reserved : 2;
  uint32_t isInch : 1;
} readFIFO;

/**************************************************************************/
/**
* Application Init.
*****************************************************************************/
SL_WEAK void app_init(void) {
  // run once
  sl_spidrv_usart_spifahrer_handle->peripheral.usartPort->CTRL |=
      USART_CTRL_CSINV_ENABLE;
}

/**************************************************************************/
/**
* Application Process Action.
*****************************************************************************/
SL_WEAK void app_process_action(void) {}

/**************************************************************************/
/**
* Bluetooth stack event handler.
* This overrides the dummy weak implementation.
*
* @param[in] evt Event coming from the Bluetooth stack.
*****************************************************************************/
void sl_bt_on_event(sl_bt_msg_t *evt) {
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

    sc = sl_bt_gatt_server_write_attribute_value(gattdb_system_id, 0,
                                                 sizeof(system_id), system_id);
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
    sc = sl_bt_advertiser_start(advertising_set_handle,
                                advertiser_general_discoverable,
                                advertiser_connectable_scannable);
    app_assert_status(sc);
    // sl_led_turn_on(&sl_led_btnled);
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

  case sl_bt_evt_sm_bonding_failed_id:

    app_log("Bonding failed, reason: 0x%2X\r\n",
            evt->data.evt_sm_bonding_failed.reason);
    /* Previous bond is broken, delete it and close connection, host must retry
     * at least once */
    sl_bt_sm_delete_bondings();
    break;

  case sl_bt_evt_system_external_signal_id:
    if ((notification_enabled == 1) && (connection_handle != 0xff)) {
      uint8_t messageLength = bufferedKeyLen;
      while (--bufferedKeyLen) {
        memset(input_report_data, 0, sizeof(input_report_data));

        input_report_data[MODIFIER_INDEX] = modifier;
        input_report_data[DATA_INDEX] =
            bufferedKeys[messageLength - bufferedKeyLen];

        while (sl_bt_gatt_server_send_notification(
            connection_handle, gattdb_report, 8, input_report_data))
          ;
        // app_assert_status(sc);

        // send 0
        memset(input_report_data, 0, sizeof(input_report_data));

        while (sl_bt_gatt_server_send_notification(
            connection_handle, gattdb_report, 8, input_report_data))
          ;

        app_log("Key report was sent\r\n");
      }
    }
    break;

  case sl_bt_evt_gatt_server_characteristic_status_id:

    if (evt->data.evt_gatt_server_characteristic_status.characteristic ==
        gattdb_report) {
      // client characteristic configuration changed by remote GATT client
      if (evt->data.evt_gatt_server_characteristic_status.status_flags ==
          gatt_server_client_config) {
        if (evt->data.evt_gatt_server_characteristic_status
                .client_config_flags == gatt_notification) {
          notification_enabled = 1;
          // sl_led_turn_off(&sl_led_btnled);
        } else {
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
    sc = sl_bt_advertiser_start(advertising_set_handle,
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

void sl_button_on_change(const sl_button_t *handle) {
  if (&sl_button_button == handle) {
    if (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED) {
      //sl_led_turn_on(&sl_led_btnled);
      app_log("Button pushed - callback\r\n");
    } else {
        volatile uint64_t timeoutCounter =0;
    REDOREAD:;
      sl_status_t sc =  SPIDRV_SReceiveB(sl_spidrv_usart_spifahrer_handle, &spi_rxbuffer, 4, 210);
      if(sc == ECODE_EMDRV_SPIDRV_TIMEOUT){
        timeoutCounter++;
        sl_sleeptimer_delay_millisecond(20);
        goto REDOREAD;
      }
      uint32_t combiBuffer = spi_rxbuffer[0] + (spi_rxbuffer[1] << 12); // tranmition contains 2 12 bit frames
      readFIFO *messWert = (readFIFO *)&combiBuffer;
      if (messWert->reserved || (messWert->caliperValue > 15500))
        goto REDOREAD;
      uint32_t writeValue = messWert->caliperValue;
      if (!messWert->isInch) {
        uint8_t bufferedKeysSOLL[12] = {0, 0, 0,    0,    0,    KEY_COMMA,
                                        0, 0, KEY_SPACE, M, M, KEY_ENTER};
        memcpy(bufferedKeys, bufferedKeysSOLL, sizeof(bufferedKeysSOLL));
        bufferedKeyLen = 12;
        bufferedKeys[7] = key_Numbers[writeValue % 10];
        writeValue /= 10;
        bufferedKeys[6] = key_Numbers[writeValue % 10];
        writeValue /= 10;
        bufferedKeys[4] = key_Numbers[writeValue % 10];
        writeValue /= 10;
        if (writeValue) {
          bufferedKeys[3] = key_Numbers[writeValue % 10];
          writeValue /= 10;
        }
        if (writeValue) {
          bufferedKeys[2] = key_Numbers[writeValue % 10];
          writeValue /= 10;
        }
        if (messWert->isNegativ) {
          bufferedKeys[1] = 0x38;
        } else {
          if (writeValue) {
            bufferedKeys[1] = key_Numbers[writeValue % 10];
            writeValue /= 10;
          }
        }
      } else {
        // setup for inch
        uint8_t bufferedKeysSOLL[14] = {0, 0,    0, KEY_COMMA, 0, 0, 0,
                                        0, KEY_SPACE, Z, O, L, L, KEY_ENTER};
        memcpy(bufferedKeys, bufferedKeysSOLL, sizeof(bufferedKeysSOLL));
        bufferedKeyLen = 14;
        if (writeValue & 1)
          bufferedKeys[7] = key_Numbers[5];
        writeValue /= 2;
        bufferedKeys[6] = key_Numbers[writeValue % 10];
        writeValue /= 10;
        bufferedKeys[5] = key_Numbers[writeValue % 10];
        writeValue /= 10;
        bufferedKeys[4] = key_Numbers[writeValue % 10];
        writeValue /= 10;
        bufferedKeys[2] = key_Numbers[writeValue % 10];
        writeValue /= 10;
        if (messWert->isNegativ) {
          bufferedKeys[1] = 0x38;
        } else {
          if (writeValue) {
            bufferedKeys[1] = key_Numbers[writeValue % 10];
            writeValue /= 10;
          }
        }
      }
      //sl_led_turn_off(&sl_led_btnled);
      app_log("Button released - callback \r\n");
      sl_bt_external_signal(1);
    }
  }
}
