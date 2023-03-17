# BLE HID Caliper

Based on https://github.com/SiliconLabs/bluetooth_applications/tree/master/bluetooth_hid_keyboard

## Einrichten

1. Create a new 'Bluetooth - SoC Empty' project for your device.
2. Install the following software components:
   1. Platform -> Driver -> Simple Button (instance: button)
   2. Platform -> Driver -> PWM (instance: COOLEPWMLED)
   3. Platform -> Driver -> SPIDRV USART (instance: spifahrer)
   4. Platform -> Peripheral -> GPIO Init (instance: gpioInstanz)
   5. Services -> IO Stream -> IO Stream: USART
   6. Application -> Utility -> Log
3. Import the attached gatt_configuration.btconf file in the GATT Configurator.
4. Copy the attached app.c file into your project (overwriting the existing one).
6. Build and flash the project to your device.
7. Enjoy

## Benutzung

1. Board mit dem Messschieber verbinden
2. Messschieber mit dem GATT Client(PC, Smartphone, etz.) verbinden
3. Messschieber auf 0Zoll stellen
4. Mit dem Knopf lassen sich nun Daten versenden!

## Verwendete Gecko SDK Version ##

v3.1.2
