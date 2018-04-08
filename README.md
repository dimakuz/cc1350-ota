# OTA Firmware Update (over BLE)
This project demonstrates how to update the code of a running application on the CC1350 Texas Instruments board
via Bluetooth Low Energy (BLE).

The project consists of:
* A sample application (cc1350-ota).
* GATT client - a utility used to send the updated app over BLE.

Prerequisites
=============
* CC1350 Texas Instruments board
* Code Composer Studio (CCS)
* simplelink_cc13x0_sdk

How to build
============
1. Clone this repository
1. Open CCS
   1. Import the BLE stack project from the TI SDK folder
      (`<ti>/simplelink_cc13x0_sdk_1_60_00_21/examples/rtos/CC1350_LAUNCHXL/blestack/simple_peripheral/tirtos/ccs`)
   1. Import the cc1350-ota project (from this repository)
   1. Build the `stack` project
   1. Build the `app` project
1. OTA client
   1. On Windows - build the gattclient C# client
   1. On Linux:
      1. Install dependencies: `sudo dnf install python3-pyelftools bluez`


How to run
==========
1. Flash the projects onto the board (via Eclipse / Uniflash)
   1. Flash the built "stack" project onto the board
   1. Flash the built "app" project onto the board
1. Run the GATT client
   The GATT client accepts a path to a .json file, and transmits the application
   blob (metadata + code + data) onto the board via BLE.
   The JSON file (`ota.json`) can be located under the `Debug/` directory of the app project.

   1. On Windows: run the `gattclient.exe <path to json>`
   1. On Linux:
      1. Convert the JSON artifact into a series of blobs: `./prepare_blobs.py Debug/ota.json`. This will create $PWD/ota_blobs directory.
      1. Discover the MAC address of your CC1350 board, easy way to do this is with `sudo hcitool lescan -i hciXX`
      1. Push the OTA blobs to the board: `./push_ota.sh BLE_MAC_ADDR DIR_WITH_BLOBS`

License
=======
BSD
