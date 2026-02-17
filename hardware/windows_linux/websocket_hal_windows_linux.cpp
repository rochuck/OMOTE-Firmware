
// Minimal POSIX-based WebSocket client for desktop (macOS / Linux)
#include "websocket_hal_windows_linux.h"
#include "secrets.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

static int               ws_sock = -1;
static std::thread       ws_thread;
static std::mutex        ws_send_mutex;
static std::atomic<bool> ws_running(false);
static std::atomic<bool> ws_connected(false);

static tAnnounceWebsocketMessage_cb thisAnnounceWebsocketMessage_cb = NULL;

static std::string
host_from_url(const std::string& url, std::string& path, int& port) {
    // Expect ws://host[:port]/path
    std::string       s      = url;
    const std::string prefix = "ws://";
    size_t            pos    = 0;
    if (s.rfind(prefix, 0) == 0) pos = prefix.size();
    size_t      slash    = s.find('/', pos);
    std::string hostport = (slash == std::string::npos) ? s.substr(pos) : s.substr(pos, slash - pos);
    path                 = (slash == std::string::npos) ? std::string("/") : s.substr(slash);
    size_t colon         = hostport.find(':');
    if (colon != std::string::npos) {
        port = std::stoi(hostport.substr(colon + 1));
        return hostport.substr(0, colon);
    }
    port = 80;
    return hostport;
}

static std::string
base64_encode(const std::vector<uint8_t>& data) {
    static const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string        out;
    size_t             i = 0;
    for (; i + 2 < data.size(); i += 3) {
        uint32_t val = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
        out.push_back(table[(val >> 18) & 0x3F]);
        out.push_back(table[(val >> 12) & 0x3F]);
        out.push_back(table[(val >> 6) & 0x3F]);
        out.push_back(table[val & 0x3F]);
    }
    if (i < data.size()) {
        uint32_t val = data[i] << 16;
        out.push_back(table[(val >> 18) & 0x3F]);
        if (i + 1 < data.size()) val |= data[i + 1] << 8;
        out.push_back(table[(val >> 12) & 0x3F]);
        if (i + 1 < data.size()) {
            out.push_back(table[(val >> 6) & 0x3F]);
            out.push_back('=');
        } else {
            out.push_back('=');
            out.push_back('=');
        }
    }
    return out;
}

static bool
tcp_connect(const std::string& host, int port, int& out_sock) {
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    char portstr[16];
    snprintf(portstr, sizeof(portstr), "%d", port);
    if (getaddrinfo(host.c_str(), portstr, &hints, &res) != 0) return false;
    int s = -1;
    for (struct addrinfo* p = res; p != nullptr; p = p->ai_next) {
        s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (s == -1) continue;
        if (connect(s, p->ai_addr, p->ai_addrlen) == 0) break;
        close(s);
        s = -1;
    }
    freeaddrinfo(res);
    if (s == -1) return false;
    out_sock = s;
    return true;
}

static bool
send_all(int sock, const void* data, size_t len) {
    const uint8_t* p = (const uint8_t*) data;
    while (len > 0) {
        ssize_t n = ::send(sock, p, len, 0);
        if (n <= 0) return false;
        p += n;
        len -= n;
    }
    return true;
}

static ssize_t
recv_all(int sock, void* buf, size_t len) {
    uint8_t* p   = (uint8_t*) buf;
    size_t   got = 0;
    while (got < len) {
        ssize_t n = ::recv(sock, p + got, len - got, 0);
        if (n <= 0) return -1;
        got += n;
    }
    return (ssize_t) got;
}

static void
reader_thread_func() {
    while (ws_running) {
        // read 2-byte header
        uint8_t hdr[2];
        ssize_t r = ::recv(ws_sock, hdr, 2, 0);
        if (r <= 0) break;
        bool     fin         = (hdr[0] & 0x80) != 0;
        uint8_t  opcode      = hdr[0] & 0x0F;
        bool     mask        = (hdr[1] & 0x80) != 0;
        uint64_t payload_len = hdr[1] & 0x7F;
        if (payload_len == 126) {
            uint8_t ext[2];
            if (recv_all(ws_sock, ext, 2) < 0) break;
            payload_len = (ext[0] << 8) | ext[1];
        } else if (payload_len == 127) {
            uint8_t ext[8];
            if (recv_all(ws_sock, ext, 8) < 0) break;
            payload_len = 0;
            for (int i = 0; i < 8; i++) payload_len = (payload_len << 8) | ext[i];
        }
        uint8_t mask_key[4] = {0, 0, 0, 0};
        if (mask) {
            if (recv_all(ws_sock, mask_key, 4) < 0) break;
        }
        std::vector<uint8_t> payload(payload_len);
        if (payload_len > 0) {
            if (recv_all(ws_sock, payload.data(), payload_len) < 0) break;
            if (mask)
                for (uint64_t i = 0; i < payload_len; i++) payload[i] ^= mask_key[i % 4];
        }
        if (opcode == 0x1) { // text
            std::string msg((char*) payload.data(), payload.size());
            if (thisAnnounceWebsocketMessage_cb) thisAnnounceWebsocketMessage_cb(msg);
        } else if (opcode == 0x8) { // close
            break;
        }
    }
    ws_connected = false;
}

void
set_announceWebsocketMessage_cb_HAL(tAnnounceWebsocketMessage_cb cb) {
    thisAnnounceWebsocketMessage_cb = cb;
}

void
init_websocket_HAL(void) {
    if (ws_running) return;
    std::string path;
    int         port;
    std::string host = host_from_url(WS_URL, path, port);
    int         s    = -1;
    if (!tcp_connect(host, port, s)) return;
    ws_sock = s;

    // generate Sec-WebSocket-Key
    std::vector<uint8_t> keybin(16);
    std::random_device   rd;
    std::mt19937         gen(rd());
    for (size_t i = 0; i < keybin.size(); ++i) keybin[i] = (uint8_t) (gen() & 0xFF);
    std::string key_b64 = base64_encode(keybin);

    char req[2048];
    snprintf(req,
             sizeof(req),
             "GET %s HTTP/1.1\r\n"
             "Host: %s:%d\r\n"
             "Upgrade: websocket\r\n"
             "Connection: Upgrade\r\n"
             "Sec-WebSocket-Key: %s\r\n"
             "Sec-WebSocket-Version: 13\r\n"
             "Origin: http://localhost\r\n"
             "\r\n",
             path.c_str(),
             host.c_str(),
             port,
             key_b64.c_str());

    if (!send_all(ws_sock, req, strlen(req))) {
        close(ws_sock);
        ws_sock = -1;
        return;
    }

    // read headers
    std::string hdrs;
    char        buf[512];
    while (true) {
        ssize_t n = ::recv(ws_sock, buf, sizeof(buf), 0);
        if (n <= 0) {
            close(ws_sock);
            ws_sock = -1;
            return;
        }
        hdrs.append(buf, buf + n);
        if (hdrs.find("\r\n\r\n") != std::string::npos) break;
    }
    if (hdrs.find("101") == std::string::npos) {
        close(ws_sock);
        ws_sock = -1;
        return;
    }

    ws_running   = true;
    ws_connected = true;
    ws_thread    = std::thread(reader_thread_func);

    // Send auth message immediately
    char authbuf[1024];
    snprintf(authbuf, sizeof(authbuf), "{\"type\":\"auth\",\"access_token\":\"%s\"}", WS_TOKEN);
    websocket_send_HAL(authbuf);
}

bool
websocket_isConnected_HAL() {
    return ws_connected.load();
}

void
websocket_loop_HAL() {
    // nothing to do, reader runs in background
}

static uint32_t
rand32() {
    static std::mt19937 rng((unsigned) time(NULL));
    return rng();
}

bool
websocket_send_HAL(const char* topic, const char* payload) {
    if (!ws_connected) return false;
    std::lock_guard<std::mutex> lg(ws_send_mutex);
    size_t                      payload_len = strlen(payload);
    std::vector<uint8_t>        frame;
    frame.push_back(0x81); // FIN + text
    if (payload_len <= 125) {
        frame.push_back(0x80 | (uint8_t) payload_len);
    } else if (payload_len <= 0xFFFF) {
        frame.push_back(0x80 | 126);
        frame.push_back((payload_len >> 8) & 0xFF);
        frame.push_back(payload_len & 0xFF);
    } else {
        frame.push_back(0x80 | 127);
        for (int i = 7; i >= 0; --i) frame.push_back((payload_len >> (8 * i)) & 0xFF);
    }
    uint32_t mask = rand32();
    uint8_t  mask_bytes[4];
    mask_bytes[0] = (mask >> 24) & 0xFF;
    mask_bytes[1] = (mask >> 16) & 0xFF;
    mask_bytes[2] = (mask >> 8) & 0xFF;
    mask_bytes[3] = mask & 0xFF;
    frame.insert(frame.end(), mask_bytes, mask_bytes + 4);
    const uint8_t* p = (const uint8_t*) payload;
    for (size_t i = 0; i < payload_len; i++) frame.push_back(p[i] ^ mask_bytes[i % 4]);
    return send_all(ws_sock, frame.data(), frame.size());
}

void
websocket_shutdown_HAL() {
    if (!ws_running) return;
    ws_running = false;
    if (ws_sock != -1) {
        shutdown(ws_sock, SHUT_RDWR);
        close(ws_sock);
        ws_sock = -1;
    }
    if (ws_thread.joinable()) ws_thread.join();
}
