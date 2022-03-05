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
#ifdef __EMSCRIPTEN__
#    error "Use ext_auth_em.c on Emscripten"
#endif
#include "ext_auth.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpcommon/output.h>
#include <curl/curl.h>


static const char *get_curl_error(CURLcode code)
{
    const char *error = curl_easy_strerror(code);
    return error ? error : "Unknown CURLcode";
}

static bool init_curl(void)
{
    DP_ATOMIC_DECLARE_STATIC_SPIN_LOCK(global_init_lock);
    static bool global_init = false;
    static CURLcode code;
    if (!global_init) {
        DP_atomic_lock(&global_init_lock);
        if (!global_init) {
            code = curl_global_init(CURL_GLOBAL_DEFAULT);
        }
        DP_atomic_unlock(&global_init_lock);
    }

    if (code == CURLE_OK) {
        return true;
    }
    else {
        DP_error_set("Initializing cURL failed: %s", get_curl_error(code));
        return false;
    }
}

static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *user)
{
    DP_Output *output = user;
    size_t total = size * nmemb;
    return DP_output_write(output, ptr, total) ? total : 0;
}

char *DP_ext_auth(const char *url, const char *body, long timeout_seconds)
{
    DP_ASSERT(url);
    DP_ASSERT(body);
    if (!init_curl()) {
        return NULL;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        DP_error_set("Initializing cURL handle failed");
        return NULL;
    }

    char errbuf[CURL_ERROR_SIZE];
    struct curl_slist *headers = NULL;
    DP_Output *output = NULL;
    void **buffer_ptr = NULL;
    char *buffer = NULL;

#define SET_CURL_OPT(OPT, ...)                                                \
    do {                                                                      \
        code = curl_easy_setopt(curl, OPT, __VA_ARGS__);                      \
        if (code != CURLE_OK) {                                               \
            DP_error_set("Error setting " #OPT ": %s", get_curl_error(code)); \
            goto cleanup;                                                     \
        }                                                                     \
    } while (0)

    CURLcode code;
    SET_CURL_OPT(CURLOPT_TIMEOUT, timeout_seconds);
    SET_CURL_OPT(CURLOPT_ERRORBUFFER, errbuf);
    SET_CURL_OPT(CURLOPT_WRITEFUNCTION, write_callback);
    SET_CURL_OPT(CURLOPT_URL, url);
    SET_CURL_OPT(CURLOPT_POSTFIELDS, body);

    headers = curl_slist_append(headers, "Content-Type: application/json");
    SET_CURL_OPT(CURLOPT_HTTPHEADER, headers);

    output = DP_mem_output_new(1024, false, &buffer_ptr, NULL);
    if (!output) {
        code = CURLE_WRITE_ERROR;
        goto cleanup;
    }
    SET_CURL_OPT(CURLOPT_WRITEDATA, (void *)output);

#undef SET_CURL_OPT

    DP_debug("POST '%s' with a timeout of %ld seconds", url, timeout_seconds);
    code = curl_easy_perform(curl);
    if (code == CURLE_WRITE_ERROR) {
        DP_error_set("Write error in request: %s - %s", errbuf, DP_error());
        goto cleanup;
    }
    else if (code != CURLE_OK) {
        DP_error_set("Error performing request: %s", errbuf);
        goto cleanup;
    }

    if (!DP_output_write(output, "\0", 1)) {
        code = CURLE_WRITE_ERROR;
        goto cleanup;
    }

cleanup:
    if (code == CURLE_OK) {
        buffer = *buffer_ptr;
    }
    else if (buffer_ptr) {
        DP_free(*buffer_ptr);
    }

    DP_output_free(output);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return buffer;
}

void DP_ext_auth_free(char *buffer)
{
    DP_free(buffer);
}
