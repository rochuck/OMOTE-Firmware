#pragma once
#include <list>
#include <lvgl.h>
#include <string>

#include "applicationInternal/hardware/IRremoteProtocols.h"
#include "applicationInternal/hardware/arduinoLayer.h"

// --- hardware general -------------------------------------------------------
void
init_hardware_general(void);

// --- preferences ------------------------------------------------------------
void
init_preferences(void);
void
save_preferences(void);
std::string
get_activeScene();
void
set_activeScene(std::string anActiveScene);
std::string
get_activeGUIname();
void
set_activeGUIname(std::string anActiveGUIname);
int
get_activeGUIlist();
void
set_activeGUIlist(int anActiveGUIlist);
int
get_lastActiveGUIlistIndex();
void
set_lastActiveGUIlistIndex(int aGUIlistIndex);

// --- user led ---------------------------------------------------------------
void
init_userled(void);
void
update_userled();

// --- battery ----------------------------------------------------------------
void
init_battery(void);
void
get_battery_status(int* battery_voltage, int* battery_percentage, bool* battery_ischarging);

// --- sleep / IMU ------------------------------------------------------------
void
init_sleep();
void
init_IMU();
void
check_activity();
void
setLastActivityTimestamp();
uint32_t
get_sleepTimeout();
void
set_sleepTimeout(uint32_t aSleepTimeout);
bool
get_wakeupByIMUEnabled();
void
set_wakeupByIMUEnabled(bool aWakeupByIMUEnabled);
uint8_t
get_motionThreshold();
void
set_motionThreshold(uint8_t aMotionThreshold);

// --- keypad -----------------------------------------------------------------

// This is defined in the hardware presenter.
// the keypad driver in the command handler populates these to fake
// injection into the lvgl keypad driver.
extern uint8_t     queued_key;         // From your software callback
extern uint8_t     queued_key_timeout; // Timeout for the key release
extern lv_indev_t* indev_keypad;       // The keypad driver needed for button nav
void
           init_keys(void);
const char NO_KEY = '\0';
// -- has to be exactly the same structure as in the hardware implementations, because only a pointer is passed
const uint8_t keypadROWS = 5; // five rows
const uint8_t keypadCOLS = 5; // five columns
enum keypad_rawKeyStates { IDLE_RAW, PRESSED_RAW, RELEASED_RAW };
struct rawKey {
    unsigned long       timestampReceived;
    char                keyChar;
    keypad_rawKeyStates rawKeyState;
};
// --
extern rawKey rawKeys[][keypadCOLS];
void
getKeys(rawKey (*rawKeys)[keypadCOLS], unsigned long currentMillis);
#if (OMOTE_HARDWARE_REV >= 5)
void
update_keyboardBrightness(void);
uint8_t
get_keyboardBrightness();
void
set_keyboardBrightness(uint8_t aKeyboardBrightness);
#endif

// --- SD card ----------------------------------------------------------------
#if (OMOTE_HARDWARE_REV >= 5)
void
init_SD_card(void);
#endif

// --- IR sender --------------------------------------------------------------
void
init_infraredSender(void);
void
sendIRcode(int protocol, std::list<std::string> commandPayloads, std::string additionalPayload);

// --- IR receiver ------------------------------------------------------------
void
start_infraredReceiver(void);
void
shutdown_infraredReceiver(void);
void
infraredReceiver_loop(void);
bool
get_irReceiverEnabled();
void
set_irReceiverEnabled(bool aIrReceiverEnabled);

// --- BLE keyboard -----------------------------------------------------------
#if (ENABLE_KEYBOARD_BLE == 1)
void
init_keyboardBLE();
// used by "device_keyboard_ble.cpp", "sleep.cpp"
typedef uint8_t      MediaKeyReport[2];
const uint8_t        BLE_KEY_UP_ARROW             = 0xDA;
const uint8_t        BLE_KEY_DOWN_ARROW           = 0xD9;
const uint8_t        BLE_KEY_RIGHT_ARROW          = 0xD7;
const uint8_t        BLE_KEY_LEFT_ARROW           = 0xD8;
const uint8_t        BLE_KEY_RETURN               = 0xB0;
const uint8_t        BLE_KEY_ESC                  = 0xB1;
const uint8_t        BLE_KEY_BACKSPACE            = 0xB2;
const MediaKeyReport BLE_KEY_MEDIA_WWW_BACK       = {0, 32};
const MediaKeyReport BLE_KEY_MEDIA_WWW_HOME       = {128, 0};
const MediaKeyReport BLE_KEY_MEDIA_PREVIOUS_TRACK = {2, 0};
const MediaKeyReport BLE_KEY_MEDIA_REWIND         = {0, 128};
const MediaKeyReport BLE_KEY_MEDIA_PLAY_PAUSE     = {8, 0};
const MediaKeyReport BLE_KEY_MEDIA_FASTFORWARD    = {0, 2};
const MediaKeyReport BLE_KEY_MEDIA_NEXT_TRACK     = {1, 0};
const MediaKeyReport BLE_KEY_MEDIA_MUTE           = {16, 0};
const MediaKeyReport BLE_KEY_MEDIA_VOLUME_UP      = {32, 0};
const MediaKeyReport BLE_KEY_MEDIA_VOLUME_DOWN    = {64, 0};
void
keyboardBLE_startAdvertisingForAll();
void
keyboardBLE_startAdvertisingWithWhitelist(std::string peersAllowed);
void
keyboardBLE_startAdvertisingDirected(std::string peerAddress, bool isRandomAddress);
void
keyboardBLE_stopAdvertising();
void
keyboardBLE_printConnectedClients();
void
keyboardBLE_disconnectAllClients();
void
keyboardBLE_printBonds();
std::string
keyboardBLE_getBonds();
std::string
keyboardBLE_getLocalAddress();
void
keyboardBLE_deleteBonds();
bool
keyboardBLE_forceConnectionToAddress(std::string peerAddress);
bool
keyboardBLE_isAdvertising();
bool
keyboardBLE_isConnected();
void
keyboardBLE_shutdown();
void
keyboardBLE_write(uint8_t c);
void
keyboardBLE_longpress(uint8_t c);
void
keyboardBLE_home();
void
keyboardBLE_sendString(const std::string& s);
void
consumerControlBLE_write(const MediaKeyReport value);
void
consumerControlBLE_longpress(const MediaKeyReport value);
#endif

// --- tft --------------------------------------------------------------------
void
update_backlightBrightness(void);
uint8_t
get_backlightBrightness();
void
set_backlightBrightness(uint8_t aBacklightBrightness);

// --- lvgl -------------------------------------------------------------------
void
init_lvgl_hardware();

// --- WiFi / MQTT ------------------------------------------------------------
#if (ENABLE_WIFI_AND_MQTT == 1)
void
init_mqtt(void);
// used by "commandHandler.cpp", "sleep.cpp"
bool
getIsWifiConnected();
void
mqtt_loop();
bool
publishMQTTMessage(const char* topic, const char* payload);
void
wifi_shutdown();
#endif

#if (ENABLE_WEBSOCKET == 1)
void
init_websocket(void);
bool
send_websocket_message(const char* topic, const char* payload);
bool
websocket_sub(const char* entity_list);
#endif

// --- OTA --------------------------------------------------------------------
#if (ENABLE_OTA == 1)
typedef void (*tOtaStartCallback)(void);
typedef void (*tOtaProgressCallback)(int pct);
void
set_ota_start_cb(tOtaStartCallback cb);
void
set_ota_progress_cb(tOtaProgressCallback cb);
void
init_ota(void);
void
ota_loop(void);
#endif

// --- memory usage -----------------------------------------------------------
void
get_heapUsage(unsigned long* heapSize, unsigned long* freeHeap, unsigned long* maxAllocHeap, unsigned long* minFreeHeap);

// --- Apple TV Companion protocol --------------------------------------------
#if (ENABLE_COMPANION == 1)
#include <string>
void
init_companion_HAL(void);
bool
companion_launchApp_HAL(const std::string& bundleID);
bool
companion_isConnected_HAL(void);
void
companion_shutdown_HAL(void);
#endif

// --- Kodi JSON-RPC ----------------------------------------------------------
#if (ENABLE_KODI == 1)
#include <string>
void
init_kodi_HAL(void);
bool
kodi_sendRpc_HAL(const std::string& method, const std::string& params_json);
#endif

// --- Lyrion Music Server ----------------------------------------------------
#if (ENABLE_LYRION == 1)
#include <lvgl.h>
#include <string>
#include <vector>
struct LyrionStatus {
    std::string player_name;
    std::string title;
    std::string artist;
    std::string album;
    std::string track_id;
    float       elapsed_s  = 0.0f; // current play position, seconds
    float       duration_s = 0.0f; // total track length, seconds (0 = unknown, e.g. stream)
    int         volume     = -1;   // 0-100, negative = unknown
    bool        is_playing = false;
    bool        is_powered = false;
    bool        is_muted   = false; // true when the player is muted
    bool        valid      = false;
};
// What a browse row represents — drives the GUI's select behaviour so it does
// not need to special-case each level.
enum LyrionItemType {
    LIT_FOLDER = 0, // generic drill (root / library menu / favorites folder / search group)
    LIT_ARTIST,     // drill -> albums by this artist
    LIT_ALBUM,      // drill -> tracks on this album
    LIT_GENRE,      // drill -> artists in this genre
    LIT_TRACK,      // play this track
    LIT_PLAYLIST,   // load this playlist
    LIT_FAVORITE,   // play this favorite (audio leaf)
    LIT_URL,        // play this url (Radio Paradise)
    LIT_PLAY_ALL,   // play the current container (album / artist)
    LIT_APP,        // an LMS app/radio plugin root (id = its query cmd, e.g. "radioparadise")
    LIT_APP_FOLDER, // a drillable menu node inside an app (drill via app_cmd + id)
    LIT_APP_AUDIO,  // a playable leaf inside an app (play via app_cmd + id)
};
// One row in a browse list. `id` is a favorites item_id, playlist_id, or
// library id (artist/album/genre/track); `url` is set only for hardcoded
// direct-stream entries (Radio Paradise). `hasitems`/`isaudio` are still set
// from the favorites response; `type` is the GUI's source of truth.
struct LyrionBrowseItem {
    std::string    title; // display text (already sanitized to ASCII)
    std::string    id;    // favorites item_id / playlist_id / library id
    std::string    url;   // direct stream URL for hardcoded entries
    bool           hasitems = false;
    bool           isaudio  = false;
    LyrionItemType type     = LIT_FOLDER;
};
void                init_lyrion_HAL(void);
bool                lyrion_discoverPlayers_HAL(void);
bool                lyrion_cyclePlayer_HAL(int direction);
bool                lyrion_sendCommand_HAL(const std::string& command_array_json);
bool                lyrion_powerToggle_HAL(void);
bool                lyrion_powerOffAll_HAL(void);
bool                lyrion_pollStatus_HAL(LyrionStatus* out);
const lv_img_dsc_t* lyrion_fetchArt_HAL(const std::string& track_id);
void                lyrion_releaseArt_HAL(void);
// Library browse (Favorites + Playlists). List calls fill `out`; play calls
// target the currently-selected player.
bool                lyrion_browseFavorites_HAL(const std::string& item_id, std::vector<LyrionBrowseItem>* out);
bool                lyrion_browsePlaylists_HAL(std::vector<LyrionBrowseItem>* out);
bool                lyrion_playFavorite_HAL(const std::string& item_id);
bool                lyrion_playPlaylist_HAL(const std::string& playlist_id);
bool                lyrion_playUrl_HAL(const std::string& url, const std::string& title);
// Full library browse + search. List calls fill `out` (paginated via
// start/count) and report the total match count in `*total`. Optional filter
// args are applied only when non-empty. Play uses an LMS playlistcontrol
// selector ("album_id:5", "artist_id:9", "track_id:42") on the current player.
bool                lyrion_browseArtists_HAL(const std::string& genre_id, const std::string& search,
                                             int start, int count, std::vector<LyrionBrowseItem>* out, int* total);
bool                lyrion_browseAlbums_HAL(const std::string& artist_id, const std::string& genre_id,
                                            const std::string& search, int start, int count,
                                            std::vector<LyrionBrowseItem>* out, int* total);
bool                lyrion_browseGenres_HAL(int start, int count, std::vector<LyrionBrowseItem>* out, int* total);
bool                lyrion_browseTracks_HAL(const std::string& album_id, const std::string& artist_id,
                                            const std::string& search, int start, int count,
                                            std::vector<LyrionBrowseItem>* out, int* total);
bool                lyrion_searchCounts_HAL(const std::string& term, int* artists, int* albums, int* tracks);
bool                lyrion_playSelector_HAL(const std::string& selector);
// LMS apps / radio plugins (the XMLBrowser/OPML "items" interface used by the
// web UI and mobile apps). Launching a stream this way runs it through its
// plugin, so per-track metadata + cover art arrive — unlike playing a bare
// stream URL. browseApps lists the installed app/radio plugins (id = each
// plugin's query cmd). browseAppItems drills one app's menu: `app_cmd` is that
// query cmd; `item_id` is empty for the app's top menu, or a node id to drill
// in. playAppItem plays a leaf on the current player.
bool                lyrion_browseApps_HAL(std::vector<LyrionBrowseItem>* out);
bool                lyrion_browseAppItems_HAL(const std::string& app_cmd, const std::string& item_id,
                                              std::vector<LyrionBrowseItem>* out);
bool                lyrion_playAppItem_HAL(const std::string& app_cmd, const std::string& item_id);
#endif
