#include "web_portal_routes.h"
#include "web_portal_auth.h"
#include "web_portal_cors.h"
#include "web_portal_config.h"
#include "web_portal_device_api.h"
#include "web_portal_display.h"
#include "web_portal_firmware.h"
#include "web_portal_ota.h"
#include "web_portal_pages.h"

#include "board_config.h"

void web_portal_register_routes(AsyncWebServer* server) {
    auto handleCorsPreflight = [](AsyncWebServerRequest *request) {
        web_portal_send_cors_preflight(request);
    };

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
    server->on("/api/mode", HTTP_OPTIONS, handleCorsPreflight);
    server->on("/api/mode", HTTP_GET, handleGetMode);

    server->on("/api/config", HTTP_OPTIONS, handleCorsPreflight);
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

    server->on("/api/info", HTTP_OPTIONS, handleCorsPreflight);
    server->on("/api/info", HTTP_GET, handleGetVersion);
    #if HEALTH_HISTORY_ENABLED
    server->on("/api/health/history", HTTP_GET, handleGetHealthHistory);
    #endif
    #if HEALTH_HISTORY_ENABLED
    server->on("/api/health/history", HTTP_OPTIONS, handleCorsPreflight);
    #endif
    server->on("/api/health", HTTP_OPTIONS, handleCorsPreflight);
    server->on("/api/health", HTTP_GET, handleGetHealth);

    server->on("/api/reboot", HTTP_OPTIONS, handleCorsPreflight);
    server->on("/api/reboot", HTTP_POST, handleReboot);

    // GitHub Pages-based firmware updates (URL-driven)
    server->on("/api/firmware/update/status", HTTP_OPTIONS, handleCorsPreflight);
    server->on("/api/firmware/update/status", HTTP_GET, handleGetFirmwareUpdateStatus);
    server->on(
        "/api/firmware/update",
        HTTP_POST,
        [](AsyncWebServerRequest *request) {
            if (!portal_auth_gate(request)) return;
        },
        NULL,
        handlePostFirmwareUpdate
    );
    server->on("/api/firmware/update", HTTP_OPTIONS, handleCorsPreflight);

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
    server->on("/api/display/brightness", HTTP_OPTIONS, handleCorsPreflight);

    // Screen saver API endpoints
    server->on("/api/display/sleep", HTTP_OPTIONS, handleCorsPreflight);
    server->on("/api/display/sleep", HTTP_GET, handleGetDisplaySleep);
    server->on("/api/display/sleep", HTTP_POST, handlePostDisplaySleep);

    server->on("/api/display/wake", HTTP_OPTIONS, handleCorsPreflight);
    server->on("/api/display/wake", HTTP_POST, handlePostDisplayWake);

    server->on("/api/display/activity", HTTP_OPTIONS, handleCorsPreflight);
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
    server->on("/api/display/screen", HTTP_OPTIONS, handleCorsPreflight);
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
    server->on("/api/update", HTTP_OPTIONS, handleCorsPreflight);

}
