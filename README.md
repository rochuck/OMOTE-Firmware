# OMOTE - Open Universal Remote - Firmware

This is my fork of the OMOTE code.
My notes are here first....

## OTA (Over-the-Air) Updates

OTA is enabled for the `esp32-s3-Rev5andHigher` environment. After flashing once via USB with the new partition table, subsequent firmware updates can be pushed wirelessly.

The device runs an HTTP server on port 3232. Your machine POSTs the firmware binary to it — all connections are host → device, so it works across VLANs as long as your machine can reach the IoT VLAN (no return connection required).

**To push an OTA update:** select the `esp32-s3-Rev5andHigher-ota` environment in PlatformIO and run **Upload**. The device must be awake and on WiFi. In VSCode this environment appears at the bottom of the Project Tasks panel — scroll down if you don't see it. You can also use the CLI:

```
pio run -e esp32-s3-Rev5andHigher-ota -t upload
```

Or with curl directly:

```
curl -F "firmware=@.pio/build/esp32-s3-Rev5andHigher/firmware.bin" http://OMOTE.local:3232/update
```

If `OMOTE.local` doesn't resolve, replace `upload_port` in `platformio.ini` (or the URL above) with the device's IP address, visible in the serial console at startup.

**To disable OTA** (e.g. if the firmware grows too large for the 6 MB slot):

1. In [platformio.ini](platformio.ini), under `[env:esp32-s3-Rev5andHigher]`:
   - Change `board_build.partitions` back to `noota_16MB_custom.csv`
   - In `build_unflags`: remove `-D ENABLE_OTA=0`
   - In `build_flags`: change `-D ENABLE_OTA=1` to `-D ENABLE_OTA=0`
2. Flash via USB — this restores the single 12.5 MB app slot.

No other source files need touching; all OTA code is `#if (ENABLE_OTA == 1)` gated in `hardware/ESP32/ota_hal_esp32.cpp`.

## Apple TV Companion Protocol (App Launcher)

The `esp32-s3-Rev5andHigher` environment includes a C++ port of the Apple TV [Companion protocol](https://github.com/postlund/pyatv/tree/master/pyatv/protocols/companion), the same protocol used by the iOS TV Remote app. It lets the remote launch specific apps by bundle ID with a single button press.

### One-time pairing

Pairing uses SRP (done once on your Mac via pyatv — no PIN entry needed on the remote after this):

```bash
pip install pyatv
atvremote scan                               # find your Apple TV's device ID
atvremote --id <ATV_ID> --protocol companion pair
```

At the end of pairing, pyatv prints a line like:
```
You may now use these credentials: <ltpk>:<ltsk>:<atv_id>:<client_id>
```

Copy that credentials string into `src/secrets_override.h`:

```cpp
#define COMPANION_ATV_HOST  "192.168.x.x"   // your Apple TV's IP
#define COMPANION_ATV_PORT  49152
#define COMPANION_CREDENTIALS "ltpk:ltsk:atv_id:client_id"
```

The IP and port can also be found by running `atvremote scan`.

### Using app launch commands

In any scene's key bindings or start sequence, use the pre-registered commands:

```cpp
executeCommand(COMPANION_LAUNCH_NETFLIX);
executeCommand(COMPANION_LAUNCH_YOUTUBE);
executeCommand(COMPANION_LAUNCH_DISNEYPLUS);
executeCommand(COMPANION_LAUNCH_APPLETV_PLUS);
executeCommand(COMPANION_LAUNCH_PRIMEVIDEO);
executeCommand(COMPANION_LAUNCH_HBO_MAX);
executeCommand(COMPANION_LAUNCH_HULU);
executeCommand(COMPANION_LAUNCH_SPOTIFY);
executeCommand(COMPANION_LAUNCH_PLEX);
// Launch any app by bundle ID:
executeCommand(COMPANION_LAUNCH_CUSTOM, "com.example.myapp");
```

To find a bundle ID for any installed app:
```bash
atvremote --id <ATV_ID> --protocol companion app_list
```

The connection is maintained in a FreeRTOS background task (core 0). After the first button press the task connects, authenticates, and establishes a session (~500–800 ms), then launches the app. Subsequent presses on a live session are near-instant.

### Disabling Companion

The feature is `#if (ENABLE_COMPANION == 1)` gated. It is off by default (`ENABLE_COMPANION=0` in `[env]`) and enabled only in `[env:esp32-s3-Rev5andHigher]`. To disable it there, change `-D ENABLE_COMPANION=1` to `-D ENABLE_COMPANION=0` in [platformio.ini](platformio.ini) — no other source files need touching.

---

GUIS - I've used Squareline Studio for some of this stuff, and cherry-picked the output code
It is a completely manual process, unlike some others I've worked on.

I have backing art on some screens, Squareline Studio generated the code, but the asset files I've used come from the online [image converter](https://lvgl.io/tools/imageconverter)

Even so I've had to modify the code to match the Apple logo asset.  This is probably due to version
mismatches. LVGL in this build is 8.4.0


# ---- original readme follows ---

## Overview

This is the ESP32 Arduino based firmware for the OMOTE - Open Universal Remote.

To run this firmware, you have two options
*  run it on the [OMOTE ESP32 Hardware](https://github.com/OMOTE-Community/OMOTE-Hardware/)
*  run it in the simulator on Linux, macOs or Windows

### The state of this project

The software can be adjusted to your needs. You can add your own amplifier, TV and media player. Smart home devices can be controlled with MQTT. The software is an example made up of:
* a TV and an amplifier controlled with infrared
* a Fire TV media player controlled with BLE (bluetooth keyboard)
* some smart home devices controlled with MQTT
* an IR receiver for decoding the IR codes from your remote

Please see the [wiki on how to understand and modify the firmware.](https://github.com/OMOTE-Community/OMOTE-Firmware/wiki/How-to-understand-and-modify-the-firmware)

You need to have PlatformIO running, and you need to know how to compile and flash your own firmware with PlatformIO. There is no prebuilt firmware.

The remote can be charged and programmed via its USB-C port. Open the PlatformIO project to compile and upload the code to the ESP32.

As a long term goal, maybe a prebuilt firmware will be published, where you can configure your OMOTE via a web interface.

### LVGL GUI simulator for Windows, Linux, and macOS

A simulator for running the LVGL UI on your local Windows, Linux, or macOS machine is available.

You can run the simulator in Visual Studio Code with PlatformIO. No need for any other compiler or development environment (no Visual Studio needed as often done in other LVGL simulators).
<div align="center">
  <img src="images/WindowsSimulator.gif" width="60%">
</div>

For details, please see the [wiki for the software simulator for fast creating and testing of LVGL GUIs.](https://github.com/OMOTE-Community/OMOTE-Firmware/wiki/Software-simulator-for-fast-creating-and-testing-of-LVGL-GUIs)

### To-dos for software

Long term goals (not yet scheduled)
- [ ] Add an interface for graphically editing the configuration
- [ ] Store the configuration in Flash (e.g. as a editable json file)

See the [open issues](https://github.com/OMOTE-Community/OMOTE-Firmware/issues) and [discussions](https://github.com/OMOTE-Community/OMOTE-Firmware/discussions) for a full list of proposed features (and known issues).

## Contributing

If you have a suggestion for an improvement, please fork the repo and create a pull request. You can also simply open an issue or - for more general feature requests - head over to the [discussions](https://github.com/OMOTE-Community/OMOTE-Firmware/discussions).

## License

Distributed under the GPL v3 License. See [LICENSE](https://github.com/OMOTE-Community/OMOTE-Firmware/blob/main/LICENSE) for more information.

## Contact

[![OMOTE Discord](https://discordapp.com/api/guilds/1138116475559882852/widget.png?style=banner2 "OMOTE Discord")][link1]

Join the OMOTE Discord: [https://discord.gg/5PnYFAsKsG](https://discord.gg/5PnYFAsKsG)

[link1]: https://discord.gg/5PnYFAsKsG
