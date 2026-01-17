#pragma once

#include <ESPAsyncWebServer.h>

// GitHub Pages base URL (with repo path) for outbound links.
const char* web_portal_pages_base_url();

// Origin used for CORS allowlist (scheme + host).
const char* web_portal_cors_origin();

// Attach CORS headers to a response.
void web_portal_add_cors_headers(AsyncWebServerResponse* response);

// Send a preflight response with CORS headers.
void web_portal_send_cors_preflight(AsyncWebServerRequest* request);
