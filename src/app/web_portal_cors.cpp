#include "web_portal_cors.h"

#include "repo_slug_config.h"

#include <Arduino.h>

static String g_pages_base_url;
static String g_cors_origin;

// CORS is restricted to the GitHub Pages origin.
static constexpr bool kAllowAllOrigins = false;

static void ensure_urls() {
    if (g_pages_base_url.length() > 0 || g_cors_origin.length() > 0) {
        return;
    }

    if (strlen(REPO_OWNER) == 0 || strlen(REPO_NAME) == 0) {
        return;
    }

    g_pages_base_url = String("https://") + REPO_OWNER + ".github.io/" + REPO_NAME;
    g_cors_origin = String("https://") + REPO_OWNER + ".github.io";
}

const char* web_portal_pages_base_url() {
    ensure_urls();
    return g_pages_base_url.c_str();
}

const char* web_portal_cors_origin() {
    ensure_urls();
    if (kAllowAllOrigins) return "*";
    return g_cors_origin.c_str();
}

void web_portal_add_cors_headers(AsyncWebServerResponse* response) {
    if (!response) return;

    const char* origin = web_portal_cors_origin();
    if (!origin || strlen(origin) == 0) return;

    response->addHeader("Access-Control-Allow-Origin", origin);
    response->addHeader("Access-Control-Allow-Headers", "Authorization, Content-Type");
    response->addHeader("Access-Control-Allow-Methods", "GET,POST,PUT,DELETE,OPTIONS");
    response->addHeader("Vary", "Origin");
}

void web_portal_send_cors_preflight(AsyncWebServerRequest* request) {
    if (!request) return;
    AsyncWebServerResponse* response = request->beginResponse(204);
    web_portal_add_cors_headers(response);
    request->send(response);
}
