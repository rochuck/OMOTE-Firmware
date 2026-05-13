/*
  Before changing anything in this file, consider to copy file "secrets_override_example.h" to file "secrets_override.h" and to do
  your changes there. Doing so, you will
  - keep your credentials secret
  - most likely never have conflicts with new versions of this file
*/
#define WIFI_SSID "YourWifiSSID"         // override it in file "secrets_override.h"
#define WIFI_PASSWORD "YourWifiPassword" // override it in file "secrets_override.h"

#define MQTT_SERVER "IPAddressOfYourBroker" // override it in file "secrets_override.h"
#define MQTT_SERVER_PORT 1883               // override it in file "secrets_override.h"
#define MQTT_USER ""                        // override it in file "secrets_override.h"
#define MQTT_PASS ""                        // override it in file "secrets_override.h"
#define MQTT_CLIENTNAME "OMOTE"             // override it in file "secrets_override.h"

// new websocket code
#define WS_URL "ws://HA_IP:8123/api/websocket" // override it in file "secrets_override.h"
#define WS_TOKEN "your ha token"               // override it in file "secrets_override.h"

// --- Kodi JSON-RPC -------------------------------------------------------
// Kodi must have "Allow remote control via HTTP" enabled in
//   Settings -> Services -> Control
// If you set a username/password there, fill them in below; otherwise leave blank.
#define KODI_HOST "192.168.1.50" // override in secrets_override.h
#define KODI_PORT 8080            // override in secrets_override.h
#define KODI_USER ""              // override in secrets_override.h (blank = no auth)
#define KODI_PASS ""              // override in secrets_override.h

// --- Lyrion Music Server (LMS, formerly Logitech Media Server) ---------
// Default web port is 9000. Auth is rarely enabled on LMS; if you have it,
// extend the HAL to send Basic auth (mirrors the Kodi HAL pattern).
// LYRION_PLAYER_NAME: optional. If non-empty, the scene starts with the player
// whose name matches; otherwise the first discovered player wins. Either way
// CHUP/CHDOW cycle through all discovered players at runtime.
#define LYRION_HOST "192.168.1.50" // override in secrets_override.h
#define LYRION_PORT 9000           // override in secrets_override.h
#define LYRION_PLAYER_NAME ""       // override in secrets_override.h (blank = use first discovered)

// --- Apple TV Companion Protocol ----------------------------------------
// To get credentials, on a Mac with pyatv installed:
//   atvremote --id <ATV_ID> --protocol companion pair
//   atvremote --id <ATV_ID> --protocol companion credentials
// Credentials format: ltpk_hex:ltsk_hex:atv_id:client_id
#define COMPANION_ATV_HOST "192.168.1.100"   // override in secrets_override.h
#define COMPANION_ATV_PORT 49152              // override in secrets_override.h
#define COMPANION_CREDENTIALS "placeholder:placeholder:placeholder:placeholder"  // override in secrets_override.h

// --- include override settings from seperate file
// ---------------------------------------------------------------------------------------------------------------
#if __has_include("secrets_override.h")
#include "secrets_override.h"
#endif
