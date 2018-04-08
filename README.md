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
1) Clone this repository
2) Open CCS

a) Import the BLE stack project from the TI SDK folder
   (<ti>/simplelink_cc13x0_sdk_1_60_00_21/examples/rtos/CC1350_LAUNCHXL/blestack/simple_peripheral/tirtos/ccs)

b) Import the cc1350-ota project (from this repository)

c) Build the "stack" project

d) Build the "app" project

3) OTA client

a) On Windows - build the gattclient C# client

b) On Linux: TODO: add steps

How to run
==========
1) Flash the projects onto the board (via Eclipse / Uniflash)
a) Flash the built "stack" project onto the board
b) Flash the built "app" project onto the board

2) Run the GATT client
   The GATT client accepts a path to a .json file, and transmits the application
   blob (metadata + code + data) onto the board via BLE.
   The JSON file (ota.json) can be located under the Debug/ directory of the app project.

a) On Windows: run the gattclient.exe <path to json>

b) On Linux: TODO: add steps

License
=======
BSD