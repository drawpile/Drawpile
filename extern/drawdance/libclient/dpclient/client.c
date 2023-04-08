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
#include "client.h"
#include "client_internal.h"
#include "ext_auth.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/queue.h>
#include <dpcommon/threading.h>
#include <dpcommon/worker.h>
#include <dpmsg/message.h>
#include <SDL_timer.h>

#ifdef __EMSCRIPTEN__
#    include "web_socket_client.h"
#    define INNER_SUPPORTED_SCHEMES \
        {                           \
            "wss", "ws", NULL       \
        }
#    define INNER_URL_VALID DP_web_socket_client_url_valid
#    define INNER_NULL      DP_WEB_SOCKET_CLIENT_NULL
#    define INNER_TYPE      DP_WebSocketClient
#    define INNER_INIT      DP_web_socket_client_init
#    define INNER_DISPOSE   DP_web_socket_client_dispose
#    define INNER_STOP      DP_web_socket_client_stop
#    define INNER_SEND      DP_web_socket_client_send
#    define WITH_LENGTH     false
#else
#    include "tcp_socket_client.h"
#    define INNER_SUPPORTED_SCHEMES \
        {                           \
            "drawpile", NULL        \
        }
#    define INNER_URL_VALID DP_tcp_socket_client_url_valid
#    define INNER_NULL      DP_TCP_SOCKET_CLIENT_NULL
#    define INNER_TYPE      DP_TcpSocketClient
#    define INNER_INIT      DP_tcp_socket_client_init
#    define INNER_DISPOSE   DP_tcp_socket_client_dispose
#    define INNER_STOP      DP_tcp_socket_client_stop
#    define INNER_SEND      DP_tcp_socket_client_send
#    define WITH_LENGTH     true
#endif


typedef struct DP_ClientExtAuth {
    DP_Client *client;
    long timeout_seconds;
    char data[];
} DP_ClientExtAuth;

struct DP_Client {
    DP_Atomic running;
    int id;
    char *url;
    const DP_ClientCallbacks *callbacks;
    void *callback_data;
    SDL_TimerID ping_timer;
    INNER_TYPE inner;
};


const char **DP_client_supported_schemes(void)
{
    static const char *supported_schemes[] = INNER_SUPPORTED_SCHEMES;
    return supported_schemes;
}

const char *DP_client_default_scheme(void)
{
    return DP_client_supported_schemes()[0];
}

DP_ClientUrlValidationResult DP_client_url_valid(const char *url)
{
    return INNER_URL_VALID(url);
}


DP_Client *DP_client_new(int id, const char *url,
                         const DP_ClientCallbacks *callbacks,
                         void *callback_data)
{
    DP_ASSERT(url);
    DP_ASSERT(callbacks);

    DP_Client *client = DP_malloc(sizeof(*client));
    *client = (DP_Client){
        DP_ATOMIC_INIT(1), id, DP_strdup(url), callbacks,
        callback_data,     0,  INNER_NULL,
    };

    if (!INNER_INIT(client)) {
        DP_client_free(client);
        return NULL;
    }

    return client;
}

void DP_client_free(DP_Client *client)
{
    if (client) {
        DP_atomic_set(&client->running, 0);
        if (client->ping_timer != 0) {
            SDL_RemoveTimer(client->ping_timer);
        }
        INNER_DISPOSE(client);
        DP_client_report_event(client, DP_CLIENT_EVENT_FREE, NULL);
        DP_free(client->url);
        DP_free(client);
    }
}

int DP_client_id(DP_Client *client)
{
    DP_ASSERT(client);
    return client->id;
}

void *DP_client_callback_data(DP_Client *client)
{
    DP_ASSERT(client);
    return client->callback_data;
}

void DP_client_send_noinc(DP_Client *client, DP_Message *msg)
{
    DP_ASSERT(client);
    DP_ASSERT(msg);
    INNER_SEND(client, msg);
}

void DP_client_send_inc(DP_Client *client, DP_Message *msg)
{
    DP_ASSERT(client);
    DP_ASSERT(msg);
    DP_client_send_noinc(client, DP_message_incref(msg));
}


bool DP_client_running(DP_Client *client)
{
    DP_ASSERT(client);
    return DP_atomic_get(&client->running);
}

void DP_client_stop(DP_Client *client)
{
    DP_ASSERT(client);
    if (DP_atomic_xch(&client->running, 0)) {
        INNER_STOP(client);
        DP_client_report_event(client, DP_CLIENT_EVENT_CONNECTION_CLOSING,
                               NULL);
    }
}

const char *DP_client_url(DP_Client *client)
{
    DP_ASSERT(client);
    return client->url;
}

void *DP_client_inner(DP_Client *client)
{
    return &client->inner;
}


void DP_client_report_event(DP_Client *client, DP_ClientEventType event,
                            const char *message_or_null)
{
    DP_ASSERT(client);
    DP_ASSERT(event >= 0);
    DP_ASSERT(event < DP_CLIENT_EVENT_COUNT);
    DP_debug("Client event %d %s", (int)event,
             message_or_null ? message_or_null : "NULL");
    client->callbacks->event(client->callback_data, client, event,
                             message_or_null);
}

void DP_client_report_error(DP_Client *client, DP_ClientEventType event)
{
    DP_client_report_event(client, event, DP_error());
}


static uint32_t SDLCALL send_ping(uint32_t interval, void *param)
{
    DP_Client *client = param;
    DP_debug("Sending ping for client %d", client->id);
    DP_client_send_noinc(client, DP_msg_ping_new(0, false));
    return interval;
}

bool DP_client_ping_timer_start(DP_Client *client)
{
    DP_ASSERT(client);
    if (client->ping_timer == 0) {
        client->ping_timer =
            SDL_AddTimer(DP_CLIENT_PING_INTERVAL_MS, send_ping, client);
        if (client->ping_timer != 0) {
            DP_debug("Started ping timer %d for client %d", client->ping_timer,
                     client->id);
            return true;
        }
        else {
            DP_error_set("Starting ping timer for client %d failed: %s",
                         client->id, SDL_GetError());
            return false;
        }
    }
    else {
        DP_error_set("Ping timer already set");
        return false;
    }
}


static void ext_auth_on_worker(void *user)
{
    DP_ClientExtAuth *cea = user;
    const char *url = cea->data;
    const char *body = cea->data + strlen(url) + 1;
    char *buffer = DP_ext_auth(url, body, cea->timeout_seconds);
    // FIXME: client might be gone by now, check for that.
    if (buffer) {
        DP_client_report_event(cea->client, DP_CLIENT_EVENT_EXT_AUTH_RESULT,
                               buffer);
        DP_ext_auth_free(buffer);
    }
    else {
        DP_client_report_error(cea->client, DP_CLIENT_EVENT_EXT_AUTH_ERROR);
    }
    DP_free(cea);
}

void DP_client_ext_auth(DP_Client *client, const char *url, const char *body,
                        long timeout_seconds, DP_ClientExtAuthPushFn push,
                        void *user)
{
    DP_ASSERT(client);
    DP_ASSERT(url);
    DP_ASSERT(body);
    size_t url_size = strlen(url) + 1;
    size_t body_size = strlen(body) + 1;
    size_t data_size = url_size + body_size;
    size_t size = DP_FLEX_SIZEOF(struct DP_ClientExtAuth, data, data_size);
    struct DP_ClientExtAuth *cea = DP_malloc(size);
    cea->client = client;
    cea->timeout_seconds = timeout_seconds;
    memcpy(cea->data, url, url_size);
    memcpy(cea->data + url_size, body, body_size);
    push(user, ext_auth_on_worker, cea);
}


void DP_client_handle_message(DP_Client *client, unsigned char *buffer,
                              size_t length)
{
    DP_ASSERT(client);
    DP_ASSERT(buffer);
    DP_Message *msg =
        WITH_LENGTH ? DP_message_deserialize(buffer, length)
                    : DP_message_deserialize_length(buffer, length, length - 2);
    if (msg) {
        client->callbacks->message(client->callback_data, client, msg);
        DP_message_decref(msg);
    }
    else {
        DP_warn("Message error on client %d: %s", client->id, DP_error());
    }
}

static unsigned char *get_buffer(void *user, size_t length)
{
    unsigned char **out_buffer = ((void **)user)[0];
    size_t *out_reserved = ((void **)user)[1];
    if (*out_reserved < length) {
        *out_buffer = DP_realloc(*out_buffer, length);
        *out_reserved = length;
    }
    return *out_buffer;
}

size_t DP_client_message_serialize(DP_Message *msg, unsigned char **out_buffer,
                                   size_t *out_reserved)
{
    return DP_message_serialize(msg, WITH_LENGTH, get_buffer,
                                (void *[]){out_buffer, out_reserved});
}
