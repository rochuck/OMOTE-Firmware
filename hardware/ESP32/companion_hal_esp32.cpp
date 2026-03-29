/*
 * Apple TV Companion Protocol HAL for ESP32-S3
 *
 * Implements enough of the Companion protocol to launch apps by bundle ID.
 * Requires credentials obtained from pyatv:
 *   pip install pyatv
 *   atvremote --id <ATV_ID> --protocol companion pair
 *   atvremote --id <ATV_ID> --protocol companion credentials
 *
 * Store the credential string in secrets_override.h as COMPANION_CREDENTIALS.
 *
 * Protocol reference: https://github.com/postlund/pyatv
 */

#include "companion_hal_esp32.h"

#if (ENABLE_COMPANION == 1)

#include <Arduino.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <string.h>

// Rhys Weatherley's Arduino Crypto library
#include <ChaChaPoly.h>
#include <Curve25519.h>
#include <Ed25519.h>
#include <SHA512.h>

#include "applicationInternal/omote_log.h"
#include "secrets.h"

static const char* TAG = "COMPANION";

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

#define FRAME_HEADER_LEN    4
#define AUTH_TAG_LEN       16
#define RECV_BUF_SIZE    2048
#define CMD_QUEUE_LEN       4
#define CMD_BUNDLE_MAX    128
#define TASK_STACK_SIZE  9216

// Frame type bytes
#define FT_PV_START  5
#define FT_PV_NEXT   6
#define FT_E_OPACK   8

// TLV8 keys (HAP spec)
#define TLV_METHOD         0
#define TLV_IDENTIFIER     1
#define TLV_SALT           2
#define TLV_PUBLIC_KEY     3
#define TLV_PROOF          4
#define TLV_ENCRYPTED_DATA 5
#define TLV_SEQ_NO         6
#define TLV_ERROR          7
#define TLV_SIGNATURE     10

// ---------------------------------------------------------------------------
// Data types
// ---------------------------------------------------------------------------

struct CompanionCredentials {
    uint8_t ltpk[32];      // Apple TV Ed25519 long-term public key
    uint8_t ltsk[32];      // Our Ed25519 long-term secret key (seed)
    uint8_t atv_id[64];    // Apple TV identifier (raw bytes)
    size_t  atv_id_len;
    uint8_t client_id[64]; // Our identifier (raw bytes)
    size_t  client_id_len;
    bool    valid;
};

struct CompanionSession {
    uint8_t  output_key[32]; // ClientEncrypt-main
    uint8_t  input_key[32];  // ServerEncrypt-main
    uint64_t send_counter;
    uint64_t recv_counter;
    bool     encrypted;
    uint64_t sid;
    uint32_t xid;
};

enum CompanionState {
    ST_IDLE,
    ST_CONNECTING,
    ST_PAIR_VERIFY,
    ST_SESSION_SETUP,
    ST_READY,
    ST_ERROR
};

// ---------------------------------------------------------------------------
// Module-level state
// ---------------------------------------------------------------------------

static CompanionCredentials g_creds   = {};
static CompanionSession     g_session = {};
static WiFiClient           g_client;
static QueueHandle_t        g_cmd_queue = nullptr;
static volatile bool        g_connected = false;
static volatile bool        g_running   = false;

// ---------------------------------------------------------------------------
// Section 1: Hex / utility helpers
// ---------------------------------------------------------------------------

static int hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int hex_decode(const char* hex, uint8_t* out, size_t max_out) {
    size_t len = strlen(hex);
    if (len % 2 != 0) return -1;
    size_t bytes = len / 2;
    if (bytes > max_out) return -1;
    for (size_t i = 0; i < bytes; i++) {
        int hi = hex_nibble(hex[i * 2]);
        int lo = hex_nibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return -1;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return (int)bytes;
}

// ---------------------------------------------------------------------------
// Section 2: Credential parsing
// Format: ltpk_hex(64):ltsk_hex(64):atv_id:client_id
// atv_id may contain colons; split at first two ':', then last ':'
// ---------------------------------------------------------------------------

static bool parse_credentials(const char* cred_str, CompanionCredentials* out) {
    memset(out, 0, sizeof(*out));

    const char* p = cred_str;

    // ltpk: first 64 hex chars
    if (strlen(p) < 65) { omote_log_e("%s: credential too short for ltpk\r\n", TAG); return false; }
    if (p[64] != ':') { omote_log_e("%s: expected ':' after ltpk\r\n", TAG); return false; }
    if (hex_decode(p, out->ltpk, 32) != 32) { omote_log_e("%s: ltpk hex decode failed\r\n", TAG); return false; }
    p += 65; // skip ltpk + ':'

    // ltsk: next 64 hex chars
    if (strlen(p) < 65) { omote_log_e("%s: credential too short for ltsk\r\n", TAG); return false; }
    if (p[64] != ':') { omote_log_e("%s: expected ':' after ltsk\r\n", TAG); return false; }
    if (hex_decode(p, out->ltsk, 32) != 32) { omote_log_e("%s: ltsk hex decode failed\r\n", TAG); return false; }
    p += 65; // skip ltsk + ':'

    // remainder: atv_id:client_id  (split at last colon)
    const char* last_colon = strrchr(p, ':');
    if (!last_colon) { omote_log_e("%s: no separator between atv_id and client_id\r\n", TAG); return false; }

    size_t atv_len    = (size_t)(last_colon - p);
    size_t client_len = strlen(last_colon + 1);

    if (atv_len == 0 || atv_len >= sizeof(out->atv_id)) {
        omote_log_e("%s: atv_id length invalid (%u)\r\n", TAG, (unsigned)atv_len);
        return false;
    }
    if (client_len == 0 || client_len >= sizeof(out->client_id)) {
        omote_log_e("%s: client_id length invalid (%u)\r\n", TAG, (unsigned)client_len);
        return false;
    }

    memcpy(out->atv_id, p, atv_len);
    out->atv_id_len = atv_len;

    memcpy(out->client_id, last_colon + 1, client_len);
    out->client_id_len = client_len;

    out->valid = true;
    omote_log_d("%s: credentials parsed OK, atv_id='%.*s' client_id='%.*s'\r\n",
                TAG,
                (int)atv_len, out->atv_id,
                (int)client_len, out->client_id);
    return true;
}

// ---------------------------------------------------------------------------
// Section 3: HMAC-SHA512 and HKDF-SHA512 (RFC 5869)
// ---------------------------------------------------------------------------

#define SHA512_BLOCK_LEN  128
#define SHA512_HASH_LEN    64

static void hmac_sha512(const uint8_t* key, size_t key_len,
                        const uint8_t* msg, size_t msg_len,
                        uint8_t out[SHA512_HASH_LEN]) {
    uint8_t k_ipad[SHA512_BLOCK_LEN];
    uint8_t k_opad[SHA512_BLOCK_LEN];
    uint8_t tk[SHA512_HASH_LEN];

    // If key is longer than block size, hash it first
    if (key_len > SHA512_BLOCK_LEN) {
        SHA512 h;
        h.update(key, key_len);
        h.finalize(tk, SHA512_HASH_LEN);
        key     = tk;
        key_len = SHA512_HASH_LEN;
    }

    memset(k_ipad, 0x36, SHA512_BLOCK_LEN);
    memset(k_opad, 0x5c, SHA512_BLOCK_LEN);
    for (size_t i = 0; i < key_len; i++) {
        k_ipad[i] ^= key[i];
        k_opad[i] ^= key[i];
    }

    uint8_t inner[SHA512_HASH_LEN];
    {
        SHA512 h;
        h.update(k_ipad, SHA512_BLOCK_LEN);
        h.update(msg, msg_len);
        h.finalize(inner, SHA512_HASH_LEN);
    }
    {
        SHA512 h;
        h.update(k_opad, SHA512_BLOCK_LEN);
        h.update(inner, SHA512_HASH_LEN);
        h.finalize(out, SHA512_HASH_LEN);
    }
}

// HKDF-Extract: PRK = HMAC-SHA512(salt, ikm)
// If salt is nullptr/empty, uses 64 zero bytes per RFC 5869
static void hkdf_extract(const uint8_t* salt, size_t salt_len,
                         const uint8_t* ikm,  size_t ikm_len,
                         uint8_t prk[SHA512_HASH_LEN]) {
    static const uint8_t zero_salt[SHA512_HASH_LEN] = {0};
    if (!salt || salt_len == 0) {
        salt     = zero_salt;
        salt_len = SHA512_HASH_LEN;
    }
    hmac_sha512(salt, salt_len, ikm, ikm_len, prk);
}

// HKDF-Expand: OKM = T(1) || T(2) || ... truncated to okm_len
static void hkdf_expand(const uint8_t prk[SHA512_HASH_LEN],
                        const char*   info,
                        uint8_t*      okm,
                        size_t        okm_len) {
    size_t   info_len  = info ? strlen(info) : 0;
    size_t   n         = (okm_len + SHA512_HASH_LEN - 1) / SHA512_HASH_LEN;
    uint8_t  t[SHA512_HASH_LEN] = {0};
    size_t   t_len     = 0;
    size_t   offset    = 0;

    for (size_t i = 1; i <= n && offset < okm_len; i++) {
        uint8_t counter = (uint8_t)i;
        // HMAC(PRK, T(i-1) || info || counter)
        // We need to concatenate T, info, and counter for the HMAC input.
        // Build a temporary buffer: max T(64) + info + 1 byte counter
        size_t  buf_len = t_len + info_len + 1;
        uint8_t buf[SHA512_HASH_LEN + 256 + 1];
        if (buf_len > sizeof(buf)) break; // guard
        size_t pos = 0;
        memcpy(buf + pos, t, t_len);   pos += t_len;
        if (info_len) memcpy(buf + pos, info, info_len); pos += info_len;
        buf[pos++] = counter;
        hmac_sha512(prk, SHA512_HASH_LEN, buf, pos, t);
        t_len = SHA512_HASH_LEN;

        size_t copy = okm_len - offset;
        if (copy > SHA512_HASH_LEN) copy = SHA512_HASH_LEN;
        memcpy(okm + offset, t, copy);
        offset += copy;
    }
}

// Convenience: HKDF-SHA512(salt_str, info_str, ikm, ikm_len) → 32 bytes
static void hkdf_sha512_32(const char*   salt_str,
                           const char*   info_str,
                           const uint8_t* ikm,
                           size_t         ikm_len,
                           uint8_t        out[32]) {
    uint8_t prk[SHA512_HASH_LEN];
    if (salt_str && strlen(salt_str) > 0) {
        hkdf_extract((const uint8_t*)salt_str, strlen(salt_str), ikm, ikm_len, prk);
    } else {
        hkdf_extract(nullptr, 0, ikm, ikm_len, prk);
    }
    hkdf_expand(prk, info_str, out, 32);
}

// ---------------------------------------------------------------------------
// Section 4: TLV8 helpers
// ---------------------------------------------------------------------------

// Encode a single TLV8 item. Returns bytes written (handles chunking if len>255).
static size_t tlv8_encode_item(uint8_t* buf, size_t buf_size,
                                uint8_t tag, const uint8_t* data, size_t data_len) {
    size_t written = 0;
    size_t offset  = 0;
    do {
        size_t chunk = data_len - offset;
        if (chunk > 255) chunk = 255;
        if (written + 2 + chunk > buf_size) return 0; // overflow
        buf[written++] = tag;
        buf[written++] = (uint8_t)chunk;
        memcpy(buf + written, data + offset, chunk);
        written += chunk;
        offset  += chunk;
    } while (offset < data_len);
    return written;
}

// Find a TLV8 tag in buf and copy its concatenated value into out_buf.
// Returns the total value length, or 0 if not found / out_buf_size too small.
static size_t tlv8_find(const uint8_t* buf, size_t buf_len,
                        uint8_t tag,
                        uint8_t* out_buf, size_t out_buf_size) {
    size_t total  = 0;
    size_t i      = 0;
    bool   found  = false;

    while (i + 2 <= buf_len) {
        uint8_t t = buf[i];
        uint8_t l = buf[i + 1];
        i += 2;
        if (i + l > buf_len) break;
        if (t == tag) {
            found = true;
            if (total + l <= out_buf_size) {
                memcpy(out_buf + total, buf + i, l);
            }
            total += l;
        }
        i += l;
    }
    return found ? total : 0;
}

// ---------------------------------------------------------------------------
// Section 5: Minimal OPACK encoder
// ---------------------------------------------------------------------------
// We only need to encode the specific messages used in this protocol.
// All outgoing messages fit in simple, counted dicts.

struct OpackWriter {
    uint8_t* buf;
    size_t   cap;
    size_t   pos;
};

static bool ow_write(OpackWriter* w, const uint8_t* data, size_t len) {
    if (w->pos + len > w->cap) return false;
    memcpy(w->buf + w->pos, data, len);
    w->pos += len;
    return true;
}

static bool ow_byte(OpackWriter* w, uint8_t b) {
    return ow_write(w, &b, 1);
}

// Encode a string (length 0..255). Short form for len <= 32.
static bool ow_str(OpackWriter* w, const char* s) {
    size_t len = strlen(s);
    if (len <= 32) {
        if (!ow_byte(w, (uint8_t)(0x40 + len))) return false;
    } else if (len <= 255) {
        if (!ow_byte(w, 0x61)) return false;
        if (!ow_byte(w, (uint8_t)len)) return false;
    } else {
        return false; // not needed for our use case
    }
    return ow_write(w, (const uint8_t*)s, len);
}

// Encode a small non-negative integer
static bool ow_int(OpackWriter* w, uint32_t val) {
    if (val < 0x28) {
        return ow_byte(w, (uint8_t)(val + 8));
    } else if (val <= 0xFF) {
        return ow_byte(w, 0x30) && ow_byte(w, (uint8_t)val);
    } else if (val <= 0xFFFF) {
        uint8_t tmp[3] = {0x31, (uint8_t)(val & 0xFF), (uint8_t)((val >> 8) & 0xFF)};
        return ow_write(w, tmp, 3);
    } else {
        uint8_t tmp[5] = {0x32,
                          (uint8_t)(val & 0xFF),
                          (uint8_t)((val >> 8) & 0xFF),
                          (uint8_t)((val >> 16) & 0xFF),
                          (uint8_t)((val >> 24) & 0xFF)};
        return ow_write(w, tmp, 5);
    }
}

// Encode a float64 (double)
static bool ow_float64(OpackWriter* w, double val) {
    if (!ow_byte(w, 0x36)) return false;
    return ow_write(w, (const uint8_t*)&val, 8);
}

// Encode bytes blob (short form for len <= 32)
static bool ow_bytes(OpackWriter* w, const uint8_t* data, size_t len) {
    if (len <= 32) {
        if (!ow_byte(w, (uint8_t)(0x70 + len))) return false;
    } else if (len <= 255) {
        if (!ow_byte(w, 0x91)) return false;
        if (!ow_byte(w, (uint8_t)len)) return false;
    } else if (len <= 0xFFFF) {
        uint8_t hdr[3] = {0x92, (uint8_t)(len & 0xFF), (uint8_t)((len >> 8) & 0xFF)};
        if (!ow_write(w, hdr, 3)) return false;
    } else {
        return false;
    }
    return ow_write(w, data, len);
}

// Begin a dict with a known pair count (0..14)
static bool ow_dict_begin(OpackWriter* w, uint8_t pair_count) {
    if (pair_count > 14) return false;
    return ow_byte(w, (uint8_t)(0xE0 + pair_count));
}

// Begin a list with a known item count (0..14)
static bool ow_list_begin(OpackWriter* w, uint8_t item_count) {
    if (item_count > 14) return false;
    return ow_byte(w, (uint8_t)(0xD0 + item_count));
}

// ---------------------------------------------------------------------------
// Section 6: OPACK message builders
// ---------------------------------------------------------------------------

// Build: {_pd: <tlv8_bytes>, _auTy: 4}  (PairVerify step 1 payload)
static size_t opack_pv_start(uint8_t* buf, size_t cap,
                              const uint8_t* tlv8, size_t tlv8_len) {
    OpackWriter w = {buf, cap, 0};
    if (!ow_dict_begin(&w, 2))          return 0;
    if (!ow_str(&w, "_pd"))             return 0;
    if (!ow_bytes(&w, tlv8, tlv8_len))  return 0;
    if (!ow_str(&w, "_auTy"))           return 0;
    if (!ow_int(&w, 4))                 return 0;
    return w.pos;
}

// Build: {_pd: <tlv8_bytes>}  (PairVerify step 2 payload)
static size_t opack_pv_next(uint8_t* buf, size_t cap,
                             const uint8_t* tlv8, size_t tlv8_len) {
    OpackWriter w = {buf, cap, 0};
    if (!ow_dict_begin(&w, 1))          return 0;
    if (!ow_str(&w, "_pd"))             return 0;
    if (!ow_bytes(&w, tlv8, tlv8_len))  return 0;
    return w.pos;
}

// Build: {_i: "_systemInfo", _t: 2, _c: {_bf:0,_cf:512,_clFl:128}, _x: xid}
static size_t opack_systeminfo(uint8_t* buf, size_t cap, uint32_t xid) {
    OpackWriter w = {buf, cap, 0};
    if (!ow_dict_begin(&w, 4))          return 0;
    if (!ow_str(&w, "_i"))              return 0;
    if (!ow_str(&w, "_systemInfo"))     return 0;
    if (!ow_str(&w, "_t"))              return 0;
    if (!ow_int(&w, 2))                 return 0;
    if (!ow_str(&w, "_c"))              return 0;
    if (!ow_dict_begin(&w, 3))          return 0;
    if (!ow_str(&w, "_bf"))             return 0;
    if (!ow_int(&w, 0))                 return 0;
    if (!ow_str(&w, "_cf"))             return 0;
    if (!ow_int(&w, 512))               return 0;
    if (!ow_str(&w, "_clFl"))           return 0;
    if (!ow_int(&w, 128))               return 0;
    if (!ow_str(&w, "_x"))              return 0;
    if (!ow_int(&w, xid))               return 0;
    return w.pos;
}

// Build: {_i: "_touchStart", _t: 2, _c: {_height:1000.0,_tFl:0,_width:1000.0}, _x: xid}
static size_t opack_touchstart(uint8_t* buf, size_t cap, uint32_t xid) {
    OpackWriter w = {buf, cap, 0};
    if (!ow_dict_begin(&w, 4))          return 0;
    if (!ow_str(&w, "_i"))              return 0;
    if (!ow_str(&w, "_touchStart"))     return 0;
    if (!ow_str(&w, "_t"))              return 0;
    if (!ow_int(&w, 2))                 return 0;
    if (!ow_str(&w, "_c"))              return 0;
    if (!ow_dict_begin(&w, 3))          return 0;
    if (!ow_str(&w, "_height"))         return 0;
    if (!ow_float64(&w, 1000.0))        return 0;
    if (!ow_str(&w, "_tFl"))            return 0;
    if (!ow_int(&w, 0))                 return 0;
    if (!ow_str(&w, "_width"))          return 0;
    if (!ow_float64(&w, 1000.0))        return 0;
    if (!ow_str(&w, "_x"))              return 0;
    if (!ow_int(&w, xid))               return 0;
    return w.pos;
}

// Build: {_i: "_sessionStart", _t: 2, _c: {_srvT:"com.apple.tvremoteservices", _sid: local_sid}, _x: xid}
static size_t opack_sessionstart(uint8_t* buf, size_t cap, uint32_t local_sid, uint32_t xid) {
    OpackWriter w = {buf, cap, 0};
    if (!ow_dict_begin(&w, 4))                          return 0;
    if (!ow_str(&w, "_i"))                              return 0;
    if (!ow_str(&w, "_sessionStart"))                   return 0;
    if (!ow_str(&w, "_t"))                              return 0;
    if (!ow_int(&w, 2))                                 return 0;
    if (!ow_str(&w, "_c"))                              return 0;
    if (!ow_dict_begin(&w, 2))                          return 0;
    if (!ow_str(&w, "_srvT"))                           return 0;
    if (!ow_str(&w, "com.apple.tvremoteservices"))      return 0;
    if (!ow_str(&w, "_sid"))                            return 0;
    if (!ow_int(&w, local_sid))                         return 0;
    if (!ow_str(&w, "_x"))                              return 0;
    if (!ow_int(&w, xid))                               return 0;
    return w.pos;
}

// Build: {_i: "_tiStart", _t: 2, _c: {}, _x: xid}
static size_t opack_tistart(uint8_t* buf, size_t cap, uint32_t xid) {
    OpackWriter w = {buf, cap, 0};
    if (!ow_dict_begin(&w, 4))          return 0;
    if (!ow_str(&w, "_i"))              return 0;
    if (!ow_str(&w, "_tiStart"))        return 0;
    if (!ow_str(&w, "_t"))              return 0;
    if (!ow_int(&w, 2))                 return 0;
    if (!ow_str(&w, "_c"))              return 0;
    if (!ow_dict_begin(&w, 0))          return 0;
    if (!ow_str(&w, "_x"))              return 0;
    if (!ow_int(&w, xid))               return 0;
    return w.pos;
}

// Build: {_i: "_interest", _t: 1, _c: {_regEvents: [event_name]}, _x: xid}
static size_t opack_interest(uint8_t* buf, size_t cap, const char* event_name, uint32_t xid) {
    OpackWriter w = {buf, cap, 0};
    if (!ow_dict_begin(&w, 4))          return 0;
    if (!ow_str(&w, "_i"))              return 0;
    if (!ow_str(&w, "_interest"))       return 0;
    if (!ow_str(&w, "_t"))              return 0;
    if (!ow_int(&w, 1))                 return 0; // Event type
    if (!ow_str(&w, "_c"))              return 0;
    if (!ow_dict_begin(&w, 1))          return 0;
    if (!ow_str(&w, "_regEvents"))      return 0;
    if (!ow_list_begin(&w, 1))          return 0;
    if (!ow_str(&w, event_name))        return 0;
    if (!ow_str(&w, "_x"))              return 0;
    if (!ow_int(&w, xid))               return 0;
    return w.pos;
}

// Build: {_i: "_launchApp", _t: 2, _c: {_bundleID: bundle_id}, _x: xid}
static size_t opack_launchapp(uint8_t* buf, size_t cap,
                               const char* bundle_id, uint32_t xid) {
    OpackWriter w = {buf, cap, 0};
    if (!ow_dict_begin(&w, 4))          return 0;
    if (!ow_str(&w, "_i"))              return 0;
    if (!ow_str(&w, "_launchApp"))      return 0;
    if (!ow_str(&w, "_t"))              return 0;
    if (!ow_int(&w, 2))                 return 0;
    if (!ow_str(&w, "_c"))              return 0;
    if (!ow_dict_begin(&w, 1))          return 0;
    if (!ow_str(&w, "_bundleID"))       return 0;
    if (!ow_str(&w, bundle_id))         return 0;
    if (!ow_str(&w, "_x"))              return 0;
    if (!ow_int(&w, xid))               return 0;
    return w.pos;
}

// ---------------------------------------------------------------------------
// Section 7: Minimal OPACK decoder (find uint32 value by key name)
// Used only for extracting remote_sid from _sessionStart response.
// ---------------------------------------------------------------------------

// Forward declaration needed for mutual recursion between dicts and lists.
static bool opack_skip_value(const uint8_t* buf, size_t len, size_t& p);

static bool opack_skip_value(const uint8_t* buf, size_t len, size_t& p) {
    if (p >= len) return false;
    uint8_t b = buf[p];
    if (b == 0x01 || b == 0x02 || b == 0x04) { p++; return true; }  // bool/null
    if (b >= 0x08 && b <= 0x2F) { p++; return true; }               // small int
    if (b == 0x35) { p += 5; return true; }                          // float32
    if (b == 0x36) { p += 9; return true; }                          // float64
    if ((b & 0xF0) == 0x30) { size_t nb = 1u << (b & 0xF); p += 1 + nb; return true; } // sized int
    if (b >= 0x40 && b <= 0x60) { p += 1 + (b - 0x40); return true; } // short string
    if (b >= 0x61 && b <= 0x64) {
        size_t nb = b & 0x0F;
        if (p + 1 + nb > len) return false;
        size_t slen = 0;
        for (size_t i = 0; i < nb; i++) slen |= ((size_t)buf[p + 1 + i]) << (8 * i);
        p += 1 + nb + slen;
        return true;
    }
    if (b >= 0x70 && b <= 0x90) { p += 1 + (b - 0x70); return true; } // short bytes
    if (b >= 0x91 && b <= 0x94) {
        size_t nb = 1u << ((b & 0xF) - 1);
        if (p + 1 + nb > len) return false;
        size_t blen = 0;
        for (size_t i = 0; i < nb; i++) blen |= ((size_t)buf[p + 1 + i]) << (8 * i);
        p += 1 + nb + blen;
        return true;
    }
    if ((b & 0xF0) == 0xE0) {  // dict
        size_t cnt = b & 0x0F; p++;
        bool end = (cnt == 0xF);
        size_t n = end ? 0 : cnt;
        for (size_t i = 0; (end ? (p < len && buf[p] != 0x03) : (i < n)); i++) {
            if (!opack_skip_value(buf, len, p)) return false;
            if (!opack_skip_value(buf, len, p)) return false;
        }
        if (end && p < len) p++;
        return true;
    }
    if ((b & 0xF0) == 0xD0) {  // list
        size_t cnt = b & 0x0F; p++;
        bool end = (cnt == 0xF);
        size_t n = end ? 0 : cnt;
        for (size_t i = 0; (end ? (p < len && buf[p] != 0x03) : (i < n)); i++) {
            if (!opack_skip_value(buf, len, p)) return false;
        }
        if (end && p < len) p++;
        return true;
    }
    return false;
}

static bool opack_find_uint32(const uint8_t* buf, size_t len,
                               const char* key, uint32_t* value) {
    if (len < 1) return false;
    uint8_t first = buf[0];

    size_t  pair_count = 0;
    bool    endless    = false;
    size_t  pos        = 1;

    if ((first & 0xF0) == 0xE0) {
        pair_count = first & 0x0F;
        if (pair_count == 0xF) {
            endless    = true;
            pair_count = 0;
        }
    } else {
        return false; // not a dict
    }

    size_t key_len = strlen(key);

    for (size_t pair = 0; ; pair++) {
        if (!endless && pair >= pair_count) break;
        if (pos >= len) break;
        if (endless && buf[pos] == 0x03) break;

        // Read key
        uint8_t kb = buf[pos];
        const char* kdata = nullptr;
        size_t klen = 0;
        if (kb >= 0x40 && kb <= 0x60) {
            klen  = kb - 0x40;
            kdata = (const char*)(buf + pos + 1);
            pos  += 1 + klen;
        } else {
            // skip unknown key type and its value
            if (!opack_skip_value(buf, len, pos)) break;
            if (!opack_skip_value(buf, len, pos)) break;
            continue;
        }

        bool key_match = (klen == key_len && memcmp(kdata, key, key_len) == 0);

        if (!key_match) {
            if (!opack_skip_value(buf, len, pos)) break;
            continue;
        }

        // Read value (small or sized int → uint32)
        if (pos >= len) break;
        uint8_t vb = buf[pos];
        if (vb >= 0x08 && vb <= 0x2F) {
            *value = vb - 8;
            return true;
        }
        if ((vb & 0xF0) == 0x30) {
            size_t nb = 1 << (vb & 0xF);
            if (nb > 4 || pos + 1 + nb > len) break;
            uint32_t v = 0;
            for (size_t i = 0; i < nb; i++) v |= ((uint32_t)buf[pos + 1 + i]) << (8 * i);
            *value = v;
            return true;
        }
        break; // value not an integer
    }
    return false;
}

// ---------------------------------------------------------------------------
// Section 8: Wire framing
// ---------------------------------------------------------------------------

static bool send_frame(WiFiClient& c, uint8_t type,
                        const uint8_t* payload, size_t payload_len) {
    uint8_t hdr[FRAME_HEADER_LEN];
    hdr[0] = type;
    hdr[1] = (uint8_t)((payload_len >> 16) & 0xFF);
    hdr[2] = (uint8_t)((payload_len >>  8) & 0xFF);
    hdr[3] = (uint8_t)(payload_len & 0xFF);
    if (c.write(hdr, FRAME_HEADER_LEN) != FRAME_HEADER_LEN) return false;
    if (payload_len > 0) {
        if (c.write(payload, payload_len) != payload_len) return false;
    }
    return true;
}

static bool recv_exact(WiFiClient& c, uint8_t* buf, size_t n) {
    size_t got = 0;
    unsigned long deadline = millis() + 5000;
    while (got < n) {
        if (!c.connected()) return false;
        int avail = c.available();
        if (avail > 0) {
            size_t want = n - got;
            if ((size_t)avail < want) want = (size_t)avail;
            size_t r = c.read(buf + got, want);
            if (r > 0) got += r;
        } else {
            if (millis() > deadline) return false;
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }
    return true;
}

static bool recv_frame(WiFiClient& c, uint8_t* out_type,
                        uint8_t* buf, size_t buf_size, size_t* out_len) {
    uint8_t hdr[FRAME_HEADER_LEN];
    if (!recv_exact(c, hdr, FRAME_HEADER_LEN)) return false;

    *out_type = hdr[0];
    size_t payload_len = ((size_t)hdr[1] << 16) |
                         ((size_t)hdr[2] <<  8) |
                          (size_t)hdr[3];

    if (payload_len > buf_size) {
        omote_log_e("%s: frame payload %u exceeds buffer %u\r\n", TAG, (unsigned)payload_len, (unsigned)buf_size);
        return false;
    }
    if (!recv_exact(c, buf, payload_len)) return false;
    *out_len = payload_len;
    return true;
}

// ---------------------------------------------------------------------------
// Section 9: ChaCha20-Poly1305 wrappers
// ---------------------------------------------------------------------------

// Build 12-byte nonce: 4 zero bytes + 8-char ASCII label
static void make_pv_nonce(const char* label8, uint8_t nonce[12]) {
    memset(nonce, 0, 4);
    memcpy(nonce + 4, label8, 8);
}

// Build 12-byte nonce from little-endian counter
static void make_counter_nonce(uint64_t counter, uint8_t nonce[12]) {
    memset(nonce, 0, 12);
    for (int i = 0; i < 8; i++) {
        nonce[i] = (uint8_t)(counter & 0xFF);
        counter >>= 8;
    }
}

// Encrypt plaintext → ciphertext + 16-byte auth tag appended.
// aad is the 4-byte frame header when used for E_OPACK frames.
static bool chacha_encrypt(const uint8_t key[32], const uint8_t nonce[12],
                            const uint8_t* aad,  size_t aad_len,
                            const uint8_t* plain, size_t plain_len,
                            uint8_t* out, size_t* out_len) {
    if (plain_len + AUTH_TAG_LEN > *out_len) return false;
    ChaChaPoly cp;
    cp.setKey(key, 32);
    cp.setIV(nonce, 12);
    if (aad && aad_len > 0) cp.addAuthData(aad, aad_len);
    cp.encrypt(out, plain, plain_len);
    cp.computeTag(out + plain_len, AUTH_TAG_LEN);
    *out_len = plain_len + AUTH_TAG_LEN;
    return true;
}

// Decrypt ciphertext (with appended 16-byte auth tag) → plaintext.
static bool chacha_decrypt(const uint8_t key[32], const uint8_t nonce[12],
                            const uint8_t* aad,  size_t aad_len,
                            const uint8_t* in, size_t in_len,
                            uint8_t* out, size_t* out_len) {
    if (in_len < AUTH_TAG_LEN) return false;
    size_t plain_len = in_len - AUTH_TAG_LEN;
    if (plain_len > *out_len) return false;
    ChaChaPoly cp;
    cp.setKey(key, 32);
    cp.setIV(nonce, 12);
    if (aad && aad_len > 0) cp.addAuthData(aad, aad_len);
    cp.decrypt(out, in, plain_len);
    if (!cp.checkTag(in + plain_len, AUTH_TAG_LEN)) {
        omote_log_e("%s: ChachaPoly auth tag mismatch\r\n", TAG);
        return false;
    }
    *out_len = plain_len;
    return true;
}

// ---------------------------------------------------------------------------
// Section 10: Encrypted frame send/recv (post-PairVerify)
// ---------------------------------------------------------------------------

static bool send_encrypted(WiFiClient& c, uint8_t frame_type,
                             const uint8_t* payload, size_t payload_len) {
    g_session.send_counter++;

    uint8_t nonce[12];
    make_counter_nonce(g_session.send_counter, nonce);

    // Build frame header for AAD (type + length of encrypted payload)
    size_t enc_len = payload_len + AUTH_TAG_LEN;
    uint8_t hdr[FRAME_HEADER_LEN];
    hdr[0] = frame_type;
    hdr[1] = (uint8_t)((enc_len >> 16) & 0xFF);
    hdr[2] = (uint8_t)((enc_len >>  8) & 0xFF);
    hdr[3] = (uint8_t)(enc_len & 0xFF);

    uint8_t enc_buf[RECV_BUF_SIZE + AUTH_TAG_LEN];
    size_t  enc_out = sizeof(enc_buf);
    if (!chacha_encrypt(g_session.output_key, nonce,
                        hdr, FRAME_HEADER_LEN,
                        payload, payload_len,
                        enc_buf, &enc_out)) {
        omote_log_e("%s: encrypt failed\r\n", TAG);
        return false;
    }

    if (c.write(hdr, FRAME_HEADER_LEN) != FRAME_HEADER_LEN) return false;
    if (c.write(enc_buf, enc_out) != enc_out) return false;
    return true;
}

static bool recv_decrypted(WiFiClient& c,
                            uint8_t* out_type, uint8_t* buf, size_t buf_size,
                            size_t* out_len) {
    uint8_t hdr[FRAME_HEADER_LEN];
    if (!recv_exact(c, hdr, FRAME_HEADER_LEN)) return false;

    *out_type = hdr[0];
    size_t enc_len = ((size_t)hdr[1] << 16) |
                     ((size_t)hdr[2] <<  8) |
                      (size_t)hdr[3];

    if (enc_len > buf_size + AUTH_TAG_LEN) {
        omote_log_e("%s: encrypted payload %u too large\r\n", TAG, (unsigned)enc_len);
        return false;
    }

    uint8_t enc_buf[RECV_BUF_SIZE + AUTH_TAG_LEN];
    if (!recv_exact(c, enc_buf, enc_len)) return false;

    g_session.recv_counter++;
    uint8_t nonce[12];
    make_counter_nonce(g_session.recv_counter, nonce);

    *out_len = buf_size;
    if (!chacha_decrypt(g_session.input_key, nonce,
                        hdr, FRAME_HEADER_LEN,
                        enc_buf, enc_len,
                        buf, out_len)) {
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Section 11: PairVerify
// ---------------------------------------------------------------------------

static bool run_pair_verify(WiFiClient& c, const CompanionCredentials& creds) {
    omote_log_d("%s: starting PairVerify\r\n", TAG);

    // Generate ephemeral X25519 keypair.
    // dh1(k, f): k = private key output, f = public key output.
    uint8_t local_priv[32], local_pub[32];
    Curve25519::dh1(local_priv, local_pub);

    // --- Step 1: Send PV_Start ---
    uint8_t tlv8_buf[64];
    size_t  tlv8_len = 0;
    {
        uint8_t seq[1] = {0x01};
        tlv8_len  = tlv8_encode_item(tlv8_buf,            64, TLV_SEQ_NO,    seq,      1);
        tlv8_len += tlv8_encode_item(tlv8_buf + tlv8_len, 64 - tlv8_len,
                                     TLV_PUBLIC_KEY, local_pub, 32);
    }

    uint8_t opack_buf[256];
    size_t  opack_len = opack_pv_start(opack_buf, sizeof(opack_buf), tlv8_buf, tlv8_len);
    if (!opack_len) { omote_log_e("%s: PV_Start build failed\r\n", TAG); return false; }
    if (!send_frame(c, FT_PV_START, opack_buf, opack_len)) {
        omote_log_e("%s: PV_Start send failed\r\n", TAG); return false;
    }

    // --- Step 2: Receive PV_Next response ---
    uint8_t recv_type;
    uint8_t recv_buf[RECV_BUF_SIZE];
    size_t  recv_len;
    if (!recv_frame(c, &recv_type, recv_buf, sizeof(recv_buf), &recv_len)) {
        omote_log_e("%s: PV_Next recv failed\r\n", TAG); return false;
    }
    if (recv_type != FT_PV_NEXT) {
        omote_log_e("%s: expected FT_PV_NEXT (6), got %u\r\n", TAG, recv_type); return false;
    }

    // Parse _pd field from OPACK (find bytes blob after "_pd" key)
    // Quick scan: look for 0x43 '_' 'p' 'd' pattern in the OPACK dict
    // We'll do a minimal parse: find the bytes blob that follows "_pd"
    const char*  pd_key = "_pd";
    uint8_t      pd_buf[512];
    size_t       pd_len = 0;
    {
        // Walk OPACK dict manually to find "_pd" value (a bytes blob)
        if (recv_len < 1) return false;
        uint8_t first = recv_buf[0];
        if ((first & 0xF0) != 0xE0) { omote_log_e("%s: PV_Next not dict\r\n", TAG); return false; }
        size_t pairs = first & 0x0F;
        size_t pos = 1;
        for (size_t p = 0; p < pairs && pos < recv_len; p++) {
            uint8_t kb = recv_buf[pos];
            if (kb >= 0x40 && kb <= 0x60) {
                size_t kl = kb - 0x40;
                if (pos + 1 + kl > recv_len) break;
                bool match = (kl == 3 && memcmp(recv_buf + pos + 1, "_pd", 3) == 0);
                pos += 1 + kl;
                if (pos >= recv_len) break;
                // Read value (bytes blob)
                uint8_t vb = recv_buf[pos];
                size_t  vl = 0;
                size_t  voff;
                if (vb >= 0x70 && vb <= 0x90) {
                    vl   = vb - 0x70;
                    voff = pos + 1;
                } else if (vb == 0x91) {
                    vl   = recv_buf[pos + 1];
                    voff = pos + 2;
                } else if (vb == 0x92) {
                    vl   = (size_t)recv_buf[pos + 1] | ((size_t)recv_buf[pos + 2] << 8);
                    voff = pos + 3;
                } else {
                    break;
                }
                if (match) {
                    if (vl > sizeof(pd_buf)) { omote_log_e("%s: _pd too large\r\n", TAG); return false; }
                    memcpy(pd_buf, recv_buf + voff, vl);
                    pd_len = vl;
                }
                pos = voff + vl;
            } else {
                break;
            }
        }
    }
    if (pd_len == 0) { omote_log_e("%s: _pd not found in PV_Next\r\n", TAG); return false; }

    // Extract server public key and encrypted data from TLV8
    uint8_t server_pub[32];
    uint8_t enc_data[256];
    size_t  server_pub_len = tlv8_find(pd_buf, pd_len, TLV_PUBLIC_KEY,     server_pub, sizeof(server_pub));
    size_t  enc_data_len   = tlv8_find(pd_buf, pd_len, TLV_ENCRYPTED_DATA, enc_data,   sizeof(enc_data));

    if (server_pub_len != 32) { omote_log_e("%s: bad server pub key length %u\r\n", TAG, (unsigned)server_pub_len); return false; }
    if (enc_data_len == 0)    { omote_log_e("%s: no encrypted data in TLV8\r\n", TAG); return false; }

    // --- Step 3: X25519 shared secret ---
    // dh2(k, f): k starts as our private key and is overwritten with the shared secret.
    Curve25519::dh2(local_priv, server_pub);
    // local_priv now holds the shared secret.

    // --- Step 4: Derive session key and decrypt ---
    uint8_t session_key[32];
    hkdf_sha512_32("Pair-Verify-Encrypt-Salt",
                   "Pair-Verify-Encrypt-Info",
                   local_priv, 32, session_key);

    uint8_t nonce_02[12];
    make_pv_nonce("PV-Msg02", nonce_02);

    uint8_t decrypted[256];
    size_t  dec_len = sizeof(decrypted);
    if (!chacha_decrypt(session_key, nonce_02,
                        nullptr, 0,
                        enc_data, enc_data_len,
                        decrypted, &dec_len)) {
        omote_log_e("%s: PV-Msg02 decrypt failed\r\n", TAG); return false;
    }

    // Extract identifier and signature from decrypted TLV8
    uint8_t dev_identifier[64];
    uint8_t dev_signature[64];
    size_t  id_len  = tlv8_find(decrypted, dec_len, TLV_IDENTIFIER, dev_identifier, sizeof(dev_identifier));
    size_t  sig_len = tlv8_find(decrypted, dec_len, TLV_SIGNATURE,  dev_signature,  sizeof(dev_signature));

    if (id_len == 0)  { omote_log_e("%s: no identifier in decrypted TLV8\r\n", TAG); return false; }
    if (sig_len != 64){ omote_log_e("%s: bad signature length %u\r\n", TAG, (unsigned)sig_len); return false; }

    // --- Step 5: Verify Ed25519 signature ---
    // info = local_pub + atv_id + server_pub
    uint8_t sig_info[32 + 64 + 32];
    size_t  si = 0;
    memcpy(sig_info + si, local_pub, 32);               si += 32;
    memcpy(sig_info + si, creds.atv_id, creds.atv_id_len); si += creds.atv_id_len;
    memcpy(sig_info + si, server_pub, 32);              si += 32;

    if (!Ed25519::verify(dev_signature, creds.ltpk, sig_info, si)) {
        omote_log_e("%s: Ed25519 verify failed\r\n", TAG); return false;
    }
    omote_log_d("%s: Ed25519 verify OK\r\n", TAG);

    // --- Step 6: Sign our response ---
    // device_info = local_pub + client_id + server_pub
    uint8_t my_info[32 + 64 + 32];
    size_t  mi = 0;
    memcpy(my_info + mi, local_pub, 32);                      mi += 32;
    memcpy(my_info + mi, creds.client_id, creds.client_id_len); mi += creds.client_id_len;
    memcpy(my_info + mi, server_pub, 32);                     mi += 32;

    // Ed25519::sign(sig, privateKey_seed, publicKey, msg, msgLen)
    // We need to derive the public key from the seed first.
    uint8_t my_pub[32];
    Ed25519::derivePublicKey(my_pub, creds.ltsk);

    uint8_t my_sig[64];
    Ed25519::sign(my_sig, creds.ltsk, my_pub, my_info, mi);

    // Build response TLV8: {Identifier, Signature}
    uint8_t resp_tlv[128];
    size_t  resp_tlv_len = 0;
    resp_tlv_len  = tlv8_encode_item(resp_tlv, sizeof(resp_tlv),
                                     TLV_IDENTIFIER, creds.client_id, creds.client_id_len);
    resp_tlv_len += tlv8_encode_item(resp_tlv + resp_tlv_len, sizeof(resp_tlv) - resp_tlv_len,
                                     TLV_SIGNATURE, my_sig, 64);

    // Encrypt with PV-Msg03 nonce
    uint8_t nonce_03[12];
    make_pv_nonce("PV-Msg03", nonce_03);

    uint8_t resp_enc[256];
    size_t  resp_enc_len = sizeof(resp_enc);
    if (!chacha_encrypt(session_key, nonce_03,
                        nullptr, 0,
                        resp_tlv, resp_tlv_len,
                        resp_enc, &resp_enc_len)) {
        omote_log_e("%s: PV-Msg03 encrypt failed\r\n", TAG); return false;
    }

    // --- Step 7: Send PV_Next ---
    {
        uint8_t seq3[1] = {0x03};
        uint8_t resp_pd[256];
        size_t  resp_pd_len = 0;
        resp_pd_len  = tlv8_encode_item(resp_pd, sizeof(resp_pd), TLV_SEQ_NO, seq3, 1);
        resp_pd_len += tlv8_encode_item(resp_pd + resp_pd_len, sizeof(resp_pd) - resp_pd_len,
                                        TLV_ENCRYPTED_DATA, resp_enc, resp_enc_len);

        uint8_t out_opack[512];
        size_t  out_len = opack_pv_next(out_opack, sizeof(out_opack), resp_pd, resp_pd_len);
        if (!out_len) { omote_log_e("%s: PV_Next build failed\r\n", TAG); return false; }
        if (!send_frame(c, FT_PV_NEXT, out_opack, out_len)) {
            omote_log_e("%s: PV_Next send failed\r\n", TAG); return false;
        }
    }

    // Receive acknowledgment (ignore content, just verify it arrives)
    if (!recv_frame(c, &recv_type, recv_buf, sizeof(recv_buf), &recv_len)) {
        omote_log_e("%s: PV ack recv failed\r\n", TAG); return false;
    }

    // --- Step 8: Derive session encryption keys ---
    hkdf_sha512_32("", "ClientEncrypt-main", local_priv, 32, g_session.output_key);
    hkdf_sha512_32("", "ServerEncrypt-main", local_priv, 32, g_session.input_key);

    g_session.encrypted     = true;
    g_session.send_counter  = 0;
    g_session.recv_counter  = 0;

    omote_log_i("%s: PairVerify OK\r\n", TAG);
    return true;
}

// ---------------------------------------------------------------------------
// Section 12: Session setup
// ---------------------------------------------------------------------------

static bool send_recv_opack(WiFiClient& c, const uint8_t* payload, size_t len) {
    if (!send_encrypted(c, FT_E_OPACK, payload, len)) return false;

    uint8_t rtype;
    uint8_t rbuf[RECV_BUF_SIZE];
    size_t  rlen;
    if (!recv_decrypted(c, &rtype, rbuf, sizeof(rbuf), &rlen)) return false;
    // Response received; we ignore content for most setup commands
    return true;
}

static bool run_session_setup(WiFiClient& c) {
    omote_log_d("%s: starting session setup\r\n", TAG);

    uint8_t buf[RECV_BUF_SIZE];
    size_t  len;

    // _systemInfo
    len = opack_systeminfo(buf, sizeof(buf), g_session.xid++);
    if (!len || !send_recv_opack(c, buf, len)) {
        omote_log_e("%s: _systemInfo failed\r\n", TAG); return false;
    }

    // _touchStart
    len = opack_touchstart(buf, sizeof(buf), g_session.xid++);
    if (!len || !send_recv_opack(c, buf, len)) {
        omote_log_e("%s: _touchStart failed\r\n", TAG); return false;
    }

    // _sessionStart — need to capture response for remote_sid
    uint32_t local_sid = (uint32_t)(esp_random());
    len = opack_sessionstart(buf, sizeof(buf), local_sid, g_session.xid++);
    if (!len || !send_encrypted(c, FT_E_OPACK, buf, len)) {
        omote_log_e("%s: _sessionStart send failed\r\n", TAG); return false;
    }
    {
        uint8_t rtype;
        uint8_t rbuf[RECV_BUF_SIZE];
        size_t  rlen;
        if (!recv_decrypted(c, &rtype, rbuf, sizeof(rbuf), &rlen)) {
            omote_log_e("%s: _sessionStart recv failed\r\n", TAG); return false;
        }
        // Try to find _sid in response _c sub-dict.
        // Response is: {_t:3, _x:xid, _c: {_sid: remote_sid}, ...}
        // We do a simple scan for "_sid" integer value
        uint32_t remote_sid = 0;
        if (opack_find_uint32(rbuf, rlen, "_sid", &remote_sid)) {
            g_session.sid = ((uint64_t)remote_sid << 32) | (uint64_t)local_sid;
            omote_log_d("%s: session SID=0x%llX\r\n", TAG, (unsigned long long)g_session.sid);
        } else {
            omote_log_w("%s: could not parse remote_sid, proceeding anyway\r\n", TAG);
        }
    }

    // _tiStart (text input start)
    len = opack_tistart(buf, sizeof(buf), g_session.xid++);
    if (!len || !send_recv_opack(c, buf, len)) {
        omote_log_e("%s: _tiStart failed\r\n", TAG); return false;
    }

    // Subscribe to _iMC event (send only, no response needed)
    len = opack_interest(buf, sizeof(buf), "_iMC", g_session.xid++);
    if (!len || !send_encrypted(c, FT_E_OPACK, buf, len)) {
        omote_log_e("%s: _iMC subscribe failed\r\n", TAG); return false;
    }

    omote_log_i("%s: session setup complete\r\n", TAG);
    return true;
}

// ---------------------------------------------------------------------------
// Section 13: FreeRTOS background task
// ---------------------------------------------------------------------------

static void companion_task(void* pvParameters) {
    (void)pvParameters;

    CompanionState state     = ST_IDLE;
    uint32_t       retry_ms  = 0;

    while (g_running) {
        switch (state) {

        case ST_IDLE:
            if (WiFi.isConnected()) {
                omote_log_d("%s: WiFi connected, will connect to ATV\r\n", TAG);
                state = ST_CONNECTING;
            } else {
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            break;

        case ST_CONNECTING:
            omote_log_d("%s: connecting to %s:%d\r\n", TAG, COMPANION_ATV_HOST, COMPANION_ATV_PORT);
            if (g_client.connect(COMPANION_ATV_HOST, COMPANION_ATV_PORT)) {
                g_client.setTimeout(5000);
                omote_log_i("%s: TCP connected\r\n", TAG);
                state = ST_PAIR_VERIFY;
            } else {
                omote_log_w("%s: TCP connect failed, retry in 10s\r\n", TAG);
                vTaskDelay(pdMS_TO_TICKS(10000));
            }
            break;

        case ST_PAIR_VERIFY:
            memset(&g_session, 0, sizeof(g_session));
            g_session.xid = (uint32_t)(esp_random() & 0xFFFF);
            if (run_pair_verify(g_client, g_creds)) {
                state = ST_SESSION_SETUP;
            } else {
                omote_log_e("%s: PairVerify failed, reconnecting\r\n", TAG);
                g_client.stop();
                vTaskDelay(pdMS_TO_TICKS(5000));
                state = ST_CONNECTING;
            }
            break;

        case ST_SESSION_SETUP:
            if (run_session_setup(g_client)) {
                g_connected = true;
                state = ST_READY;
                omote_log_i("%s: READY\r\n", TAG);
            } else {
                omote_log_e("%s: session setup failed, reconnecting\r\n", TAG);
                g_client.stop();
                g_connected = false;
                vTaskDelay(pdMS_TO_TICKS(5000));
                state = ST_CONNECTING;
            }
            break;

        case ST_READY: {
            char bundle_id[CMD_BUNDLE_MAX];
            // Drain command queue
            while (xQueueReceive(g_cmd_queue, bundle_id, 0) == pdTRUE) {
                omote_log_i("%s: launching app '%s'\r\n", TAG, bundle_id);
                uint8_t buf[512];
                size_t  len = opack_launchapp(buf, sizeof(buf), bundle_id, g_session.xid++);
                if (len) {
                    if (!send_encrypted(g_client, FT_E_OPACK, buf, len)) {
                        omote_log_e("%s: launchApp send failed\r\n", TAG);
                        g_connected = false;
                        g_client.stop();
                        state = ST_CONNECTING;
                        break;
                    }
                    // Read and discard response
                    uint8_t rtype;
                    uint8_t rbuf[RECV_BUF_SIZE];
                    size_t  rlen;
                    recv_decrypted(g_client, &rtype, rbuf, sizeof(rbuf), &rlen);
                }
            }
            if (state != ST_READY) break;

            // Check socket health
            if (!g_client.connected()) {
                omote_log_w("%s: socket dropped\r\n", TAG);
                g_connected = false;
                state = ST_ERROR;
                break;
            }
            // Drain any unsolicited incoming frames (events etc.)
            while (g_client.available() > FRAME_HEADER_LEN) {
                uint8_t rtype;
                uint8_t rbuf[RECV_BUF_SIZE];
                size_t  rlen;
                if (!recv_decrypted(g_client, &rtype, rbuf, sizeof(rbuf), &rlen)) {
                    g_connected = false;
                    state = ST_ERROR;
                    break;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(50));
            break;
        }

        case ST_ERROR:
            omote_log_w("%s: error state, waiting 15s before reconnect\r\n", TAG);
            g_client.stop();
            g_connected = false;
            g_session.encrypted = false;
            vTaskDelay(pdMS_TO_TICKS(15000));
            state = ST_IDLE;
            break;
        }
    }

    vTaskDelete(nullptr);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void init_companion_HAL(void) {
    if (!parse_credentials(COMPANION_CREDENTIALS, &g_creds)) {
        omote_log_e("%s: invalid credentials, companion disabled\r\n", TAG);
        return;
    }

    g_cmd_queue = xQueueCreate(CMD_QUEUE_LEN, CMD_BUNDLE_MAX);
    if (!g_cmd_queue) {
        omote_log_e("%s: queue create failed\r\n", TAG);
        return;
    }

    g_running = true;
    xTaskCreatePinnedToCore(companion_task, "companion", TASK_STACK_SIZE,
                            nullptr, 1, nullptr, 0 /* core 0 */);
    omote_log_i("%s: initialized\r\n", TAG);
}

bool companion_launchApp_HAL(const std::string& bundleID) {
    if (!g_cmd_queue) return false;
    char buf[CMD_BUNDLE_MAX];
    strncpy(buf, bundleID.c_str(), CMD_BUNDLE_MAX - 1);
    buf[CMD_BUNDLE_MAX - 1] = '\0';
    return xQueueSend(g_cmd_queue, buf, 0) == pdTRUE;
}

bool companion_isConnected_HAL(void) {
    return g_connected;
}

#endif // ENABLE_COMPANION
