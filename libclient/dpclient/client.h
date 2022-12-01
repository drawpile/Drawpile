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
#ifndef DPCLIENT_CLIENT_H
#define DPCLIENT_CLIENT_H
#include <dpcommon/common.h>

typedef struct DP_Message DP_Message;


#define DP_CLIENT_PING_INTERVAL_MS (15000u)

typedef enum DP_ClientUrlValidationResult {
    DP_CLIENT_URL_VALID,
    DP_CLIENT_URL_PARSE_ERROR,
    DP_CLIENT_URL_SCHEME_UNSUPPORTED,
    DP_CLIENT_URL_HOST_MISSING,
} DP_ClientUrlValidationResult;

typedef enum DP_ClientEventType {
    DP_CLIENT_EVENT_RESOLVE_ADDRESS_START,
    DP_CLIENT_EVENT_RESOLVE_ADDRESS_SUCCESS,
    DP_CLIENT_EVENT_RESOLVE_ADDRESS_ERROR,
    DP_CLIENT_EVENT_CONNECT_SOCKET_START,
    DP_CLIENT_EVENT_CONNECT_SOCKET_SUCCESS,
    DP_CLIENT_EVENT_CONNECT_SOCKET_ERROR,
    DP_CLIENT_EVENT_SPAWN_RECV_THREAD_ERROR,
    DP_CLIENT_EVENT_NETWORK_ERROR,
    DP_CLIENT_EVENT_SEND_ERROR,
    DP_CLIENT_EVENT_RECV_ERROR,
    DP_CLIENT_EVENT_CONNECTION_ESTABLISHED,
    DP_CLIENT_EVENT_CONNECTION_CLOSING,
    DP_CLIENT_EVENT_CONNECTION_CLOSED,
    DP_CLIENT_EVENT_EXT_AUTH_RESULT,
    DP_CLIENT_EVENT_EXT_AUTH_ERROR,
    DP_CLIENT_EVENT_FREE,
    DP_CLIENT_EVENT_COUNT,
} DP_ClientEventType;

typedef struct DP_Client DP_Client;

typedef struct DP_ClientCallbacks {
    void (*event)(void *data, DP_Client *client, DP_ClientEventType type,
                  const char *message_or_null);
    void (*message)(void *data, DP_Client *client, DP_Message *msg);
} DP_ClientCallbacks;

typedef void (*DP_ClientExtAuthExecFn)(void *ext_auth_params);
typedef void (*DP_ClientExtAuthPushFn)(void *user,
                                       DP_ClientExtAuthExecFn exec_fn,
                                       void *ext_auth_params);


const char **DP_client_supported_schemes(void);

const char *DP_client_default_scheme(void);

DP_ClientUrlValidationResult DP_client_url_valid(const char *url);


DP_Client *DP_client_new(int id, const char *url,
                         const DP_ClientCallbacks *callbacks,
                         void *callback_data);

void DP_client_free(DP_Client *client);

int DP_client_id(DP_Client *client);

void *DP_client_callback_data(DP_Client *client);

void DP_client_send_noinc(DP_Client *client, DP_Message *msg);

void DP_client_send_inc(DP_Client *client, DP_Message *msg);

bool DP_client_ping_timer_start(DP_Client *client);

void DP_client_ext_auth(DP_Client *client, const char *url, const char *body,
                        long timeout_seconds, DP_ClientExtAuthPushFn push,
                        void *user);


#endif
