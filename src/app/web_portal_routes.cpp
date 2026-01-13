#include "web_portal_routes.h"

#include "board_config.h"

// Forward declarations (implemented in web_portal.cpp)
bool portal_auth_gate(AsyncWebServerRequest *request);

void handleRoot(AsyncWebServerRequest *request);
void handleHome(AsyncWebServerRequest *request);
void handleNetwork(AsyncWebServerRequest *request);
void handleFirmware(AsyncWebServerRequest *request);
void handleCSS(AsyncWebServerRequest *request);
void handleJS(AsyncWebServerRequest *request);

void handleGetMode(AsyncWebServerRequest *request);
void handleGetConfig(AsyncWebServerRequest *request);
void handlePostConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void handleDeleteConfig(AsyncWebServerRequest *request);
void handleGetVersion(AsyncWebServerRequest *request);
void handleGetHealth(AsyncWebServerRequest *request);
void handleReboot(AsyncWebServerRequest *request);

void handleGetFirmwareLatest(AsyncWebServerRequest *request);
void handlePostFirmwareUpdate(AsyncWebServerRequest *request);
void handleGetFirmwareUpdateStatus(AsyncWebServerRequest *request);

#if HAS_DISPLAY
void handleSetDisplayBrightness(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void handleGetDisplaySleep(AsyncWebServerRequest *request);
void handlePostDisplaySleep(AsyncWebServerRequest *request);
void handlePostDisplayWake(AsyncWebServerRequest *request);
void handlePostDisplayActivity(AsyncWebServerRequest *request);
void handleSetDisplayScreen(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
#endif

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
