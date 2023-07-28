# OpenThread MQTT-SN Publish Example

## Building for nRF52840 Dongle

$ cd ot-nrf52xx
$ ./script/build nrf52840 USB_trans -DOT_BOOTLOADER=USB -DOT_MQTT=ON -DOT_JOINER=ON -DOT_RCP_RESTORATION_MAX_COUNT=0 -DOT_LOG_LEVEL=WARN -DOT_UPTIME=ENABLED^C
$ arm-none-eabi-objcopy -O ihex build/bin/ot-cli-ftd-mqttsn-publish build/bin/ot-cli-ftd-mqttsn-publish.hex

Then program the hex file into the dongle and monitor the serial debug output

## Forming the OTBR network

(TODO: Need to add more detail here)

- Form the network based on the master network key and channel defined in the main.c file
- Run up the OTBR as per the instructions
- Run up a UDP6 build of the MQTT-SNGateway
