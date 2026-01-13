#include "web_portal_routes.h"
#include "web_portal_auth.h"
#include "web_portal_config.h"
#include "web_portal_display.h"
#include "web_portal_firmware.h"
#include "web_portal_pages.h"

#include "board_config.h"

void handleGetMode(AsyncWebServerRequest *request);
void handleGetVersion(AsyncWebServerRequest *request);
void handleGetHealth(AsyncWebServerRequest *request);
void handleReboot(AsyncWebServerRequest *request);
void handleOTAUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);

void web_portal_register_routes(AsyncWebServer* server) {
    // Page routes
    server->on("/", HTTP_GET, handleRoot);
    server->on("/home.html", HTTP_GET, handleHome);
    server->on("/network.html", HTTP_GET, handleNetwork);
    server->on("/firmware.html", HTTP_GET, handleFirmware);

    // Asset routes
    server->on("/portal.css", HTTP_GET, handleCSS);
    server->on("/portal.js", HTTP_GET, handleJS);

    // API endpoints
    // NOTE: Keep more specific routes registered before more general/prefix routes.
    // Some AsyncWebServer matchers can behave like prefix matches depending on configuration.
    server->on("/api/mode", HTTP_GET, handleGetMode);
    server->on("/api/config", HTTP_GET, handleGetConfig);

    server->on(
        "/api/config",
        HTTP_POST,
        [](AsyncWebServerRequest *request) {
            if (!portal_auth_gate(request)) return;
        },
        NULL,
        handlePostConfig
    );

    server->on("/api/config", HTTP_DELETE, handleDeleteConfig);
    server->on("/api/info", HTTP_GET, handleGetVersion);
    server->on("/api/health", HTTP_GET, handleGetHealth);
    server->on("/api/reboot", HTTP_POST, handleReboot);

    // GitHub Releases-based firmware updates (stable only)
    server->on("/api/firmware/latest", HTTP_GET, handleGetFirmwareLatest);
    server->on("/api/firmware/update/status", HTTP_GET, handleGetFirmwareUpdateStatus);
    server->on("/api/firmware/update", HTTP_POST, handlePostFirmwareUpdate);

#if HAS_DISPLAY
    // Display API endpoints
    server->on(
        "/api/display/brightness",
        HTTP_PUT,
        [](AsyncWebServerRequest *request) {
            if (!portal_auth_gate(request)) return;
        },
        NULL,
        handleSetDisplayBrightness
    );

    // Screen saver API endpoints
    server->on("/api/display/sleep", HTTP_GET, handleGetDisplaySleep);
    server->on("/api/display/sleep", HTTP_POST, handlePostDisplaySleep);
    server->on("/api/display/wake", HTTP_POST, handlePostDisplayWake);
    server->on("/api/display/activity", HTTP_POST, handlePostDisplayActivity);

    // Runtime-only screen switch
    server->on(
        "/api/display/screen",
        HTTP_PUT,
        [](AsyncWebServerRequest *request) {
            if (!portal_auth_gate(request)) return;
        },
        NULL,
        handleSetDisplayScreen
    );
#endif

    // OTA upload endpoint
    server->on(
        "/api/update",
        HTTP_POST,
        [](AsyncWebServerRequest *request) {
            if (!portal_auth_gate(request)) return;
        },
        handleOTAUpload
    );
}
