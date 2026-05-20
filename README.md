# OMOTE - Open Universal Remote - Firmware

This is my fork of the OMOTE code.
My notes are here first....

## Platform Port: espressif32 6.10.0 → 7.0.0

The firmware has been ported to **espressif32 7.0.0** (Arduino ESP32 3.x / ESP-IDF 5.3.x) with NimBLE-Arduino 2.0.x. 

**Key changes:**
- LEDC PWM API updated (GPIO-based instead of channel-based)
- NimBLE 1.4.x → 2.0.x with automatic bond format migration on first boot
- WebSocket client return types fixed for IDF 5.x

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

## IR Blaster (network IR forwarding)

The remote's onboard IR LED has limited range and angle. The [OMOTE-Blaster](https://github.com/rochuck/OMOTE-Blaster) is a small mains-powered ESP8266 device that sits in line-of-sight of your equipment and sends IR on the remote's behalf. The remote keeps doing all the UI / scene / code-table work; the blaster is a dumb sender. When the blaster is reachable, IR commands are forwarded to it over the network; otherwise the remote falls back to its local LED automatically — no configuration switch required.

This lives in `applicationInternal/blasterClient.cpp` and is gated by `ENABLE_WIFI_AND_MQTT`.

### How it works

Every IR send already funnels through one bottleneck — the `IR:` case in `commandHandler.cpp`. There, if `blaster_isEnabled() && blaster_isAvailable()`, the command is POSTed to the blaster; on any failure it transparently drives the local IR LED instead.

The blaster is discovered automatically:

1. **User override** — a host/IP saved in NVS (`blaster`/`host`), if set.
2. **Cached IP** — the last address that handshaked successfully (`blaster`/`last_ip`).
3. **mDNS** — browse for `_omote-blaster._tcp`.

A `GET /status` handshake confirms identity (the body must contain `omote-blaster`) before the address is used. Discovery runs off the main task so it never blocks the GUI, and re-checks roughly every 60 s while the blaster is down.

### Protocol

All requests are JSON over HTTP to port 80 on the blaster.

**`POST /send`** — fire an IR command:

```jsonc
{
  "protocol": 4,            // IRremoteProtocols.h enum value (must match byte-for-byte)
  "data": "0x77E1FA80",     // hex code; may be "data:nbits:repeat"
  "nbits": 32,              // optional; omitted = blaster's per-protocol default
  "repeat": 1,              // optional; omitted = default
  "scene": "Apple TV",      // optional; active scene name (for the blaster's display)
  "name": "ATV PLAY"        // optional; human-readable label of this command
}
```

**`POST /scene`** — push the active scene when it changes, even when no IR is sent (e.g. switching to "Off"). Sent automatically from the scene-change path so the blaster's display stays current:

```json
{ "scene": "Off" }
```

**`GET /status`** — health + display state: `{ ok, service, version, uptime, rssi, ip, scene, lastCommand, lastCommandAgo }`.

`scene` and `name` are optional and additive — older blaster builds simply ignore them. They exist to drive a (planned) status display on the blaster showing the current scene, last command, IP, and uptime.

### Human-readable command names

`scene` comes for free from `get_activeScene()`. The command `name` is supplied per command via the optional third argument to `makeCommandData()`:

```cpp
register_command(&APPLETV_PLAY,
                 makeCommandData(IR, {std::to_string(IR_PROTOCOL_NEC), "0x77E1FA80"}, "ATV PLAY"));
```

Names are kept terse as `<DEVICE> <ACTION>` so they fit a small display (e.g. `ATV PLAY`, `TV HDMI5`, `AMP VOL+`). The argument is optional — commands left without a name still send fine; the blaster falls back to showing the raw code. Only IR-protocol commands reach the blaster, so only the IR device files (`devices/.../device_*`) carry names.

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

## Kodi (JSON-RPC)

The Kodi scene controls a [Kodi](https://kodi.tv) media player over HTTP using the [JSON-RPC API](https://kodi.wiki/view/JSON-RPC_API). No pairing, no IR, no special hardware — just WiFi.

### Enable Kodi's HTTP control

In Kodi, go to **Settings → Services → Control** and enable:

- **Allow remote control via HTTP** → **On**
- **Port** → `8080` (default)
- **Username** / **Password** → optional, but recommended on a shared network

If the menu items are missing, switch the settings level to **Advanced** (cog icon at the bottom).

### Find your Kodi host's IP

From the Kodi machine: **Settings → System Info → Network** shows the IP. Or check your router's DHCP table.

### Configure the remote

Add to `src/secrets_override.h` (create it from `secrets_override_example.h` if it doesn't exist):

```cpp
#undef KODI_HOST
#undef KODI_PORT
#undef KODI_USER
#undef KODI_PASS

#define KODI_HOST "192.168.x.x"   // your Kodi machine's IP
#define KODI_PORT 8080            // matches the Kodi setting above
#define KODI_USER ""              // leave blank if no auth
#define KODI_PASS ""
```

### Test it

Once flashed and on WiFi, the Kodi scene's D-pad / OK / Back / Home / transport / volume keys map to Kodi's `Input.*` and `Input.ExecuteAction` JSON-RPC methods. You can also fire commands from any scene:

```cpp
executeCommand(KODI_PLAY_PAUSE);
executeCommand(KODI_HOME);
executeCommand(KODI_VOLUME_UP);

// Custom action - pass any Kodi action string:
// https://kodi.wiki/view/Action_IDs
executeCommand(KODI_ACTION_CUSTOM, "{\"action\":\"osd\"}");
executeCommand(KODI_ACTION_CUSTOM, "{\"action\":\"fullscreen\"}");
```

### Verify from the command line

To sanity-check that Kodi's HTTP API is reachable before pointing the remote at it:

```bash
curl -s -u USER:PASS \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","method":"Input.Home","id":1}' \
  http://192.168.x.x:8080/jsonrpc
```

If you get back `{"id":1,"jsonrpc":"2.0","result":"OK"}` and Kodi jumps to the home screen, you're set.

### Disabling Kodi

The feature is `#if (ENABLE_KODI == 1)` gated. It is enabled by default in `[env]`. To disable it, change `-D ENABLE_KODI=1` to `-D ENABLE_KODI=0` in [platformio.ini](platformio.ini) — no other source files need touching.

## Home Assistant WebSocket Integration

The `esp32-s3-Rev5andHigher` environment includes native WebSocket support for Home Assistant, allowing direct control of lights and other entities without MQTT. This uses Home Assistant's native WebSocket API for real-time, low-latency communication.

### Configuration

Set the Home Assistant WebSocket URL in `src/secrets_override.h`:

```cpp
#define WS_URL "ws://192.168.x.x:8123/api/websocket"  // Your Home Assistant IP and port
```

Port 8123 is Home Assistant's default WebSocket port. If your Home Assistant is behind a reverse proxy or on a different port, adjust accordingly.

### Adding Entities

Entities (lights, etc.) are configured in [`src/devices/misc/device_smarthome/gui_smarthome.cpp`](src/devices/misc/device_smarthome/gui_smarthome.cpp). The entity map defines which Home Assistant entities are controllable from the remote:

```cpp
void init_entity_maps(void) {
    entity_item* e;
    ADD_ENTITY("friendly_name", "light.your_entity_id", "Display Label");
}
```

Add entries for each light you want to control. The entity ID can be found in Home Assistant's UI under **Settings → Devices & Services → Entities**.

### Features

- **Toggle Lights**: Turn lights on/off via touch/button in the Smart Home scene
- **Brightness Control**: Adjust brightness with a slider for dimmable lights
- **Real-time State**: Light states are updated when changed elsewhere in Home Assistant
- **No Authentication Required**: Uses Home Assistant's local network; configure access control via firewall

### Disabling WebSocket

The feature is `#if (ENABLE_WEBSOCKET == 1)` gated. It is enabled by default in `[env:esp32-s3-Rev5andHigher]`. To disable it, change `-D ENABLE_WEBSOCKET=1` to `-D ENABLE_WEBSOCKET=0` in [platformio.ini](platformio.ini) — this frees ~10 KB of firmware space if needed.

## Lyrion (LMS) Now-Playing Scene

The Lyrion scene controls one or more players attached to a [Lyrion Music Server](https://lyrion.org) (formerly Logitech Media Server / squeezebox server) over its JSON-RPC API. No pairing — just WiFi and the server's IP.

### Configure the server address

Set the LMS host and port in `src/secrets_override.h` (port 9000 is the LMS default):

```cpp
#undef LYRION_HOST
#undef LYRION_PORT

#define LYRION_HOST "192.168.x.x"   // your LMS machine's IP
#define LYRION_PORT 9000
```

Players (squeezelite instances, picoreplayer boxes, Squeezebox hardware, etc.) are discovered automatically from the server every time you open the scene — no per-player config needed. Newly-attached players show up without rebooting the remote.

### Using the scene

Pick **Lyrion** from the scene-selection grid. The now-playing tab shows:

- **Top bar:** power indicator (green = on, gray = off) on the left, current player name and volume in the centre, play/pause indicator on the right.
- **Album art** (200×200) fetched from `/music/current/cover.png`, with the title / artist / album overlaid on a translucent band at the bottom. A ♪ glyph is shown when no art is available.
- **Progress bar** with elapsed / remaining times. For streams with unknown duration, only elapsed is shown.

Status is polled every 500 ms, so play/pause, track changes, and player switches feel immediate.

### Key bindings

| Key | Short press | Long press |
|---|---|---|
| PLAY | Play / pause | — |
| STOP | Stop | Power toggle (current player) |
| REWI / FORW | Previous / next track (repeats) | — |
| VOL+ / VOL− | Volume ±5 (repeats) | — |
| MUTE | Mute toggle | — |
| CH▲ / CH▼ | Switch to next / previous player | — |

D-pad and OK are intentionally unbound for now — they're reserved for a future library-browse tab so the scene-level keymap won't need a tab-aware override later.

When you leave the Lyrion scene (pick another scene, or the all-off scene runs), all known players are powered off.

### Disabling Lyrion

The feature is `#if (ENABLE_LYRION == 1)` gated. It is enabled by default — change `-D ENABLE_LYRION=1` to `-D ENABLE_LYRION=0` in [platformio.ini](platformio.ini) to remove the scene and HAL.

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
