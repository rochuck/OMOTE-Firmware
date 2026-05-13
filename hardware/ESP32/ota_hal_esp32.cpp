#include "ota_hal_esp32.h"

#if (ENABLE_OTA == 1)
#include <ESPmDNS.h>
#include <Update.h>
#include <WebServer.h>

static WebServer otaServer(3232);

static tOtaStartCallback    s_start_cb    = NULL;
static tOtaProgressCallback s_progress_cb = NULL;

void set_ota_start_cb_HAL(tOtaStartCallback cb)    { s_start_cb    = cb; }
void set_ota_progress_cb_HAL(tOtaProgressCallback cb) { s_progress_cb = cb; }

static size_t ota_total    = 0;
static size_t ota_written  = 0;
static int    ota_last_pct = -1;

static void handleUpdatePost() {
    otaServer.sendHeader("Connection", "close");
    if (Update.hasError()) {
        otaServer.send(200, "text/plain", "FAIL");
        if (s_progress_cb) { s_progress_cb(-1); }
    } else {
        otaServer.send(200, "text/plain", "OK");
        Serial.println("OTA: rebooting");
        delay(100);
        ESP.restart();
    }
}

static void handleUpdateUpload() {
    HTTPUpload& upload = otaServer.upload();

    if (upload.status == UPLOAD_FILE_START) {
        ota_written  = 0;
        ota_last_pct = -1;
        ota_total    = otaServer.header("Content-Length").toInt();
        Serial.printf("OTA: receiving, expected %u bytes\n", ota_total);
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
        if (s_start_cb) { s_start_cb(); }

    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
        ota_written += upload.currentSize;
        // Only fire the progress callback when the integer percent advances.
        // The callback redraws LVGL, which stalls TCP — once per percent is
        // smooth enough and keeps the upload from crawling.
        if (s_progress_cb && ota_total > 0) {
            int pct = (int)((ota_written * 100) / ota_total);
            if (pct > 99) { pct = 99; } // reserve 100% for after Update.end()
            if (pct != ota_last_pct) {
                ota_last_pct = pct;
                s_progress_cb(pct);
            }
        }

    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            Serial.printf("OTA: success, %u bytes written\n", upload.totalSize);
            if (s_progress_cb) { s_progress_cb(100); }
        } else {
            Update.printError(Serial);
        }
    }
}

void init_ota_HAL(void) {
    const char* headerKeys[] = {"Content-Length"};
    otaServer.collectHeaders(headerKeys, 1);
    MDNS.begin("OMOTE");
    MDNS.addService("http", "tcp", 3232);
    otaServer.on("/update", HTTP_POST, handleUpdatePost, handleUpdateUpload);
    otaServer.begin();
    Serial.println("OTA: HTTP server ready on port 3232 (OMOTE.local)");
}

void ota_loop_HAL(void) {
    otaServer.handleClient();
}

#endif
