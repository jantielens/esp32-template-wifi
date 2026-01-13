#pragma once

#include "psram_json_allocator.h"

#include <ArduinoJson.h>
#include <ChunkPrint.h>
#include <ESPAsyncWebServer.h>

#include <memory>

template <typename TDoc>
static inline void web_portal_send_json_chunked(
    AsyncWebServerRequest *request,
    const std::shared_ptr<TDoc> &doc,
    int status_code = 200
) {
    if (!request) return;

    if (!doc || doc->capacity() == 0) {
        request->send(503, "application/json", "{\"success\":false,\"message\":\"Out of memory\"}");
        return;
    }

    if (doc->overflowed()) {
        request->send(500, "application/json", "{\"success\":false,\"message\":\"Response too large\"}");
        return;
    }

    const size_t total_len = measureJson(*doc);
    AsyncWebServerResponse *response = request->beginChunkedResponse(
        "application/json",
        [doc, total_len](uint8_t *buffer, size_t max_len, size_t index) -> size_t {
            if (index >= total_len) return 0;
            const size_t remaining = total_len - index;
            const size_t to_write = remaining < max_len ? remaining : max_len;
            ChunkPrint cp(buffer, index, to_write);
            (void)serializeJson(*doc, cp);
            return to_write;
        }
    );

    if (status_code != 200) {
        response->setCode(status_code);
    }

    request->send(response);
}
