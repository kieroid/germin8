# USBLITER8

Copy of the **usbliter8** source code.

---

### About this fork

This fork only works on the **Raspberry Pi Pico 2 W**. All other boards (and RP2040) were removed. The exploit code (`exploit.c`) is the same as the original.

What's different:

* Works on the Pico 2 W, including the on-board LED.
* You can run the exploit again without unplugging the board. When it's done, press and let go of the BOOTSEL button. The board restarts and is ready for the next device.

The original authors say that even small code changes can make the exploit less reliable. If it fails more often than the original, set `LED_CYW43=0` in `CMakeLists.txt` to turn off the LED code and build again.

---

### Original Author
* **Author:** [Paradigm Shift](https://ps.tc/pages/blog-usbliter8.html) - user web.archive.org

---

### Freedom of Information & Disclosure

Neither Magnet Forensics nor any other corporation can stand in the way of disclosing vulnerabilities that affect millions of devices owned by ordinary citizens. Transparency and public awareness are fundamental to technical security and digital integrity.

---

> **SAY NO TO CENSORSHIP!**
Tethered bootrom exploit for Apple A12, S4/S5 & A13 SoCs (A12X/Z can theoretically be supported as well, but it's not implemented yet).

## Bug & exploit write-up

Available in [our blogpost](https://ps.tc/pages/blog-usbliter8.html).

## Usage

### Hardware requirements

The exploit abuses a very low level bug of the USB controller. This means that default Mac/PC USB stack can't normally reach it. So instead we use Raspberry Pi's RP2350-based microcontroller boards.

This fork runs on the **Raspberry Pi Pico 2 W**. It has no USB-A port, so cut a Lightning to USB-A cable and solder the wires to the board:

| Lightning cable wire | Pico 2 W pin |
|---|---|
| D+ (usually green) | GP12 (pin 16) |
| D- (usually white) | GP13 (pin 17) |
| GND (usually black) | GND (pin 18) |
| 5V (usually red) | VBUS (pin 40) |

The 5V wire is needed. The exploit won't work without it.

Wire colors are not the same on every cable, so check them with a multimeter. Do NOT use USB-C cables, they often have a very different pinout. Keep the cable (with the Lightning end) short.

Power the Pico 2 W from its micro-USB port. The VBUS pin passes that 5V to the Lightning cable. The micro-USB port also shows the log.

### Flashing firmware

Build the firmware (see [Building](#building)). Then hold BOOTSEL while plugging in the Pico 2 W, and copy `build/usbliter8.uf2` to the drive that shows up. Or use [picotool](https://github.com/raspberrypi/picotool):

```
picotool load -x build/usbliter8.uf2
```

### Exploiting

1. Enter [DFU mode](https://theapplewiki.com/wiki/DFU_Mode#A11_and_newer_devices_without_clickable_home_buttons_(iPhone_8_and_above,_iPad_Pro_2018_and_above,_iPad_Mini_2021,_iPad_Air_2020_and_above,_iPad_10th_generation)). Do this while device is connected to your Mac/PC
    * Do NOT enter DFU by breaking LLB - this will not work

2. Unplug it from Mac/PC and replug it into your Pico 2 W

3. In a few seconds, the exploit will finish

There are 2 ways you can watch the process:

1. Pico 2 W appears as a virtual COM-port - the exploit logs will be printed there

2. Via on-board LED

LED:

1. Fast blinking - booting (takes about 5 seconds)
2. On - ready, or exploit running
3. Slow blinking - exploit worked!
4. Off - exploit failed

The exploit takes 0.7 - 1.2 seconds to run. When it's done (worked or failed), press and let go of BOOTSEL to restart the board and run it again. No need to unplug it. Restarting with `picotool` or unplugging the board also works.

### After exploiting

Replug the device back to your Mac/PC.

In USB serial number you shall see PWND string in the end, for instance:

```
CPID:8020 CPRV:11 CPFM:03 SCEP:01 BDID:0E ECID:XXXXXXXXXXXXXXXX IBFL:3C SRTG:[iBoot-3865.0.0.4.7] PWND:[usbliter8]
```

There is a Python control tool (`usbliter8ctl`) in the repo which allows you to demote production mode or boot raw iBoot ("raw" as in decrypted and free of any container).

The control tool depends on `pyusb` that you can get from **pip**.

```
➜  usbliter8 git:(main) ✗ ./usbliter8ctl
usage: usbliter8ctl [-h] {boot,demote} ...

Love is Control

positional arguments:
  {boot,demote}
    boot         boot raw iBoot
    demote       demote production mode

options:
  -h, --help     show this help message and exit
```

## Building

If you want to compile the firmware yourself, you need to get CMake, [Pico SDK](https://github.com/raspberrypi/pico-sdk), [picotool](https://github.com/raspberrypi/picotool) (depends on `libusb`) and [ARM toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads).

Due to the racy nature of the exploit, code changes (even seemingly unrelated) might decrease reliability. We believe this is due to RP2350 executing code from external QSPI flash. The actual instructions can go to a little cache, and then evictions from that cache break timings and then the entire thing breaks.

Here are the exact versions of Pico SDK and ARM toolchain that we used:

* Pico SDK - SDK Release 2.2.0 (commit `a1438dff1d38bd9c65dbd693f0e5db4b9ae91779`)
* ARM toolchain - 15.2.Rel1

```
cd usbliter8
cmake -S . -B build -DPICO_SDK_PATH=$PICO_SDK_PATH -DPICO_TOOLCHAIN_PATH=$PICO_TOOLCHAIN_PATH
cmake --build build
```

Change `$PICO_SDK_PATH` and `$PICO_TOOLCHAIN_PATH` to your paths. The board is always `pico2_w`, no need to set it. The Pico SDK needs the `lib/cyw43-driver` and `lib/tinyusb` submodules.

## Authors

* [@__gsch](https://x.com/__gsch) of Paradigm Shift - bug & exploitation
* [@hdesk@infosec.exchange](https://infosec.exchange/@hdesk/) of Paradigm Shift - exploitation
* **REDACTED** of Paradigm Shift - post-exploitation & development

## Credits

* **sekigon-gonnoc** - for the [PIO USB library](https://github.com/sekigon-gonnoc/Pico-PIO-USB)
