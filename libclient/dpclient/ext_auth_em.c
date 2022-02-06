/*
 * Copyright (c) 2022 askmeaboutloom
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef __EMSCRIPTEN__
#    error "Use ext_auth.c outside of Emscripten"
#endif
#include "ext_auth.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <emscripten/fetch.h>


char *DP_ext_auth(const char *url, const char *body, long timeout_seconds)
{
    DP_ASSERT(url);
    DP_ASSERT(body);

    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "POST");
    attr.attributes =
        EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | EMSCRIPTEN_FETCH_SYNCHRONOUS;
    unsigned long timeout_ms = DP_long_to_ulong(timeout_seconds) * 1000UL;
    attr.timeoutMSecs = timeout_ms;
    const char *headers[] = {"Content-Type", "application/json", NULL};
    attr.requestHeaders = headers;
    attr.requestData = body;
    attr.requestDataSize = strlen(body);

    DP_debug("Fetching %s", url);
    emscripten_fetch_t *fetch = emscripten_fetch(&attr, url);
    if (!fetch) {
        DP_error_set("Error starting fetch to %s", url);
        return NULL;
    }

    char *buffer;
    if (fetch->data) {
        size_t length = (size_t)fetch->numBytes;
        DP_debug("Fetched %zu bytes", length);
        buffer = DP_malloc(length + 1);
        memcpy(buffer, fetch->data, length);
        buffer[length] = '\0';
    }
    else {
        buffer = NULL;
        DP_error_set("Fetch to %s failed with status %d", url, fetch->status);
    }

    int result = emscripten_fetch_close(fetch);
    if (result != EMSCRIPTEN_RESULT_SUCCESS) {
        DP_warn("Error closing fetch to %s: %d", url, result);
    }
    return buffer;
}

void DP_ext_auth_free(char *buffer)
{
    DP_free(buffer);
}
