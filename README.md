# germin8

Boots an iPhone XS Max into [badapple](https://forgejo.toe.quest/kiki/badapple) Linux from a Raspberry Pi Pico 2 W, with no computer.

The Pico runs the usbliter8 bootrom exploit, then sends badapple's patched iBSS, device tree, trust cache, GPU firmware and a small m1n1 over USB. m1n1 loads the rest from the phone's NVMe. The images are built into the firmware.

Based on a Pico 2 W fork of **usbliter8**. The A13 and S4/S5 support was dropped, since only A12 images are built in.

What's different from usbliter8:

* Works on the Pico 2 W, including the on-board LED.
* Boots the phone after the exploit.
* You can run it again without unplugging the board. When it's done, press and let go of the BOOTSEL button.

The original authors say that even small code changes can make the exploit less reliable. If it fails more often than the original, set `LED_CYW43=0` in `CMakeLists.txt` to turn off the LED code and build again.

---

### Original Author
* **Author:** [Paradigm Shift](https://ps.tc/pages/blog-usbliter8.html) - user web.archive.org

---

### Freedom of Information & Disclosure

Neither Magnet Forensics nor any other corporation can stand in the way of disclosing vulnerabilities that affect millions of devices owned by ordinary citizens. Transparency and public awareness are fundamental to technical security and digital integrity.

---

> **SAY NO TO CENSORSHIP!**
Tethered bootrom exploit for the Apple A12. Upstream also supports S4/S5 and A13; this fork dropped them.

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

Build the firmware (see [Building](#building)). Then hold BOOTSEL while plugging in the Pico 2 W, and copy `build/germin8.uf2` to the drive that shows up. Or use [picotool](https://github.com/raspberrypi/picotool):

```
picotool load -x build/germin8.uf2
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

1. Fast blinking - starting (about 5 seconds), then again while sending the boot images
2. On - ready, or exploit running
3. Slow blinking - the phone is booting
4. Off - failed

The exploit takes 0.7 - 1.2 seconds to run, and sending the images about 2 minutes. When it's done (worked or failed), press and let go of BOOTSEL to restart the board and run it again. The exploit sometimes fails with "too many timeouts"; run it again. No need to unplug it. Restarting with `picotool` or unplugging the board also works.

### Booting

After the exploit the board sends iBSS, then the rest of the images in recovery mode, then `bootx`. m1n1 boots the newest `.bin` on the phone's FAT partition. Hold volume down for its boot menu. See badapple's README for setting up the NVMe.

If the phone is already in this iBSS's recovery mode, the board skips the exploit and the iBSS.

## Building

If you want to compile the firmware yourself, you need to get CMake, [Pico SDK](https://github.com/raspberrypi/pico-sdk), [picotool](https://github.com/raspberrypi/picotool) (depends on `libusb`) and [ARM toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads).

Due to the racy nature of the exploit, code changes (even seemingly unrelated) might decrease reliability. We believe this is due to RP2350 executing code from external QSPI flash. The actual instructions can go to a little cache, and then evictions from that cache break timings and then the entire thing breaks.

Here are the exact versions of Pico SDK and ARM toolchain that we used:

* Pico SDK - SDK Release 2.2.0 (commit `a1438dff1d38bd9c65dbd693f0e5db4b9ae91779`)
* ARM toolchain - 15.2.Rel1

```
cd germin8
cmake -S . -B build -DPICO_SDK_PATH=$PICO_SDK_PATH -DPICO_TOOLCHAIN_PATH=$PICO_TOOLCHAIN_PATH -DBADAPPLE_IBOOT=/path/to/badapple/build/iboot
cmake --build build
```

Change `$PICO_SDK_PATH` and `$PICO_TOOLCHAIN_PATH` to your paths. `BADAPPLE_IBOOT` is badapple's `build/iboot` after `make images` and `make bootbin`. The images are Apple's, so they are not in this repository. The board is always `pico2_w`, no need to set it. The Pico SDK needs the `lib/cyw43-driver` and `lib/tinyusb` submodules.

## Authors

* [@__gsch](https://x.com/__gsch) of Paradigm Shift - bug & exploitation
* [@hdesk@infosec.exchange](https://infosec.exchange/@hdesk/) of Paradigm Shift - exploitation
* **REDACTED** of Paradigm Shift - post-exploitation & development

## Credits

* **sekigon-gonnoc** - for the [PIO USB library](https://github.com/sekigon-gonnoc/Pico-PIO-USB)
