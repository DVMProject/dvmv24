# DVM-V24 USB V24 Adapter
No more cisco routers and WIC cards! Now you can connect your quantar directly to a linked network without stacks of extra hardware. The DVM-V24 connects Quantars and other V24-capable equipment to the [DVMDFSI network bridge application](https://github.com/DVMProject/dvmdfsi).

<img src="pics/front.jpg" width="256"><img src="pics/back.jpg" width="256"><img src="pics/quan.jpg" width="256">

## Hardware
DVM-V24 boards are available from the [W3AXL Online Store](https://store.w3axl.com/products/dvm-v24-usb-converter-for-v24-equipment). Purchasing through W3AXL supports development efforts for the DVMProject team and future hardware projects!

Schematics for the board are also included in the `hw` directory to build your own adapters.

## Firmware
Firmware is availble in this repo, under the `fw` directory. It's written in bare C, generated from STM32CubeMX. You will need an STLink programmer in order to flash the boards with the latest version of software.

### Building the latest firmware
We recommend building the firmware for the DVM-V24 on a linux-based machine, since it's much easier to set up a working ARM toolchain.

Once you have the ARM toolchain installed, simply running `make` will create binary & elf files in a `./build/` directory. These can then be flashed using the STLink using the `make flash` command.
