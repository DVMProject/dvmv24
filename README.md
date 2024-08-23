
# DVM-V24 USB V24 Adapter
No more cisco routers and WIC cards! Now you can connect your quantar directly to a linked network without stacks of extra hardware. The DVM-V24 connects Quantars and other V24-capable equipment to the [DVM Host (dvmhost) software](https://github.com/DVMProject/dvmhost).

<img src="pics/front.jpg" width="256"><img src="pics/back.jpg" width="256"><img src="pics/quan.jpg" width="256">

## Hardware
DVM-V24 boards are available from the [W3AXL Online Store](https://store.w3axl.com/products/dvm-v24-usb-converter-for-v24-equipment). Purchasing through W3AXL supports development efforts for the DVMProject team and future hardware projects!

Schematics for the board are also included in the `hw` directory to build your own adapters.

### The `CLKSEL` Jumper

This jumper connects the serial clock line to the `RXCLK` pin. Currently this jumper must be in place for the V24 adapter to work properly. In the future, the board will support external clocking, but for now the firmware only supports generating clocks for both TX & RX.

## Firmware
Firmware is availble in this repo, under the `fw` directory. It's written in bare C, generated from STM32CubeMX. You will need an STLink programmer in order to flash the boards with the latest version of software.

### Building the latest firmware
We recommend building the firmware for the DVM-V24 on a linux-based machine, since it's much easier to set up a working ARM toolchain.

Once you have the ARM toolchain installed, simply running `make` will create binary & elf files in a `./build/` directory. These can then be flashed using the STLink using the `make flash` command.

## Quick Start

### Quantar V24 Connection
The DVM-V24 RJ45 is pinned out so that a straight-through RJ45 cable (i.e. any normal, non-crossover ethernet cable) can be used to connect to the Quantar's front wireline RJ45 connectors.
#### Pinout
<img src="pics/pinout.png" width="320">

#### Repeater Programming
The following are the relevant fields that need to be configured in Quantar RSS:
|Field|Value  |
|--|--|
| Wireline Operation | 4 WIRE FULL DUPLEX |
| Astro To Wireline | ENABLED |
| Wireline Interface | V.24 ONLY |
| External Transmit Clock | ENABLED |
| RT/RT Configuration | DISABLED |

#### `dvmhost` Configuration
`dvmhost` must be configured to operate in dfsi mode. Under `modem`, ensure the following parameters are set:
```yaml
modem:
    protocol:
        type: "uart"
        mode: "dfsi"
        uart:
            port: /dev/ttyACM0 # change to your v24 board's serial port
            speed: 115200
    dfsi:
        rtrt: true
        diu: true
        jitter: 200
```

### CCGW V24 Connection
We are still investigating compatibility with the CCGW's V24 port. In theory, it should be possible, however early tests have shown the CCGW in Quantar compatibility mode does not properly mirror the Quantar's V24 port behavior.

## Support

Support is available on our Discord server - there are many users and project members online who can help with any issues you may encounter. [Join here!](https://discord.gg/3pBe8xgrEz)
