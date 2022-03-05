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
#include "web_socket_client.h"
#include "client.h"
#include "client_internal.h"
#include "uri_utils.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpcommon/threading.h>
#include <dpmsg/message.h>
#include <dpmsg/message_queue.h>
#include <emscripten/websocket.h>
#include <uriparser/Uri.h>


DP_ClientUrlValidationResult DP_web_socket_client_url_valid(const char *url)
{
    UriUriA uri;
    if (!DP_uri_parse(&uri, url)) {
        return DP_CLIENT_URL_PARSE_ERROR;
    }

    DP_ClientUrlValidationResult result = DP_CLIENT_URL_VALID;

    if (!DP_uri_text_equal_ignore_case(uri.scheme, "ws")
        && !DP_uri_text_equal_ignore_case(uri.scheme, "wss")) {
        result = DP_CLIENT_URL_SCHEME_UNSUPPORTED;
        goto end;
    }

end:
    uriFreeUriMembersA(&uri);
    return result;
}


static int make_web_socket(DP_Client *client)
{
    EmscriptenWebSocketCreateAttributes attributes;
    emscripten_websocket_init_create_attributes(&attributes);
    attributes.url = DP_client_url(client);
    attributes.protocols = NULL;
    attributes.createOnMainThread = false;
    return emscripten_websocket_new(&attributes);
}

static EM_BOOL on_open(DP_UNUSED int type,
                       DP_UNUSED const EmscriptenWebSocketOpenEvent *event,
                       void *user)
{
    DP_WebSocketCallbackData *cbd = user;
    DP_Client *client = cbd->client;
    if (client) {
        DP_WebSocketClient *wsc = DP_client_inner(client);
        DP_SEMAPHORE_MUST_POST(wsc->sem_queue);
    }
    return true;
}

static EM_BOOL on_error(DP_UNUSED int type,
                        DP_UNUSED const EmscriptenWebSocketErrorEvent *event,
                        void *user)
{
    DP_WebSocketCallbackData *cbd = user;
    DP_Client *client = cbd->client;
    if (client) {
        DP_client_report_event(client, DP_CLIENT_EVENT_NETWORK_ERROR, NULL);
    }
    return true;
}

static EM_BOOL on_close(DP_UNUSED int type,
                        const EmscriptenWebSocketCloseEvent *event, void *user)
{
    DP_WebSocketCallbackData *cbd = user;
    DP_Client *client = cbd->client;
    if (client) {
        DP_client_report_event(client, DP_CLIENT_EVENT_CONNECTION_CLOSED,
                               event->reason);
        DP_web_socket_client_stop(client);
    }
    emscripten_websocket_delete(event->socket);
    DP_free(cbd);
    return true;
}

static EM_BOOL on_message(DP_UNUSED int type,
                          const EmscriptenWebSocketMessageEvent *event,
                          void *user)
{
    DP_WebSocketCallbackData *cbd = user;
    DP_Client *client = cbd->client;
    if (client) {
        // The Drawpile protocol is binary, text messages are always an error.
        if (!event->isText) {
            DP_client_handle_message(client, event->data, event->numBytes);
        }
        else {
            DP_warn("WebSocketClient %d received text message: %s",
                    DP_client_id(client), event->data);
        }
    }
    return true;
}

static bool establish_connection(DP_Client *client, DP_WebSocketClient *wsc)
{
    DP_client_report_event(client, DP_CLIENT_EVENT_CONNECT_SOCKET_START,
                           "WebSocket");

    int socket = make_web_socket(client);
    if (socket <= 0) {
        DP_client_report_event(client, DP_CLIENT_EVENT_CONNECT_SOCKET_ERROR,
                               "websocket_new");
        return false;
    }

    DP_atomic_set(&wsc->socket, socket);

    DP_WebSocketCallbackData *cbd = DP_malloc(sizeof(*wsc->callback_data));
    *cbd = (DP_WebSocketCallbackData){client};
    wsc->callback_data = cbd;

    if (emscripten_websocket_set_onopen_callback(socket, cbd, on_open)
        != EMSCRIPTEN_RESULT_SUCCESS) {
        DP_client_report_event(client, DP_CLIENT_EVENT_CONNECT_SOCKET_ERROR,
                               "websocket_set_onopen_callback");
        return false;
    }

    if (emscripten_websocket_set_onerror_callback(socket, cbd, on_error)
        != EMSCRIPTEN_RESULT_SUCCESS) {
        DP_client_report_event(client, DP_CLIENT_EVENT_CONNECT_SOCKET_ERROR,
                               "websocket_set_onerror_callback");
        return false;
    }

    if (emscripten_websocket_set_onclose_callback(socket, cbd, on_close)
        != EMSCRIPTEN_RESULT_SUCCESS) {
        DP_client_report_event(client, DP_CLIENT_EVENT_CONNECT_SOCKET_ERROR,
                               "websocket_set_onclose_callback");
        return false;
    }

    if (emscripten_websocket_set_onmessage_callback(socket, cbd, on_message)
        != EMSCRIPTEN_RESULT_SUCCESS) {
        DP_client_report_event(client, DP_CLIENT_EVENT_CONNECT_SOCKET_ERROR,
                               "websocket_set_onmessage_callback");
        return false;
    }

    return true;
}

static void run_send(void *data)
{
    DP_Client *client = data;
    DP_WebSocketClient *wsc = DP_client_inner(client);
    if (!establish_connection(client, wsc)) {
        return;
    }

    // Wait for the WebSocket connection to actually be established. We re-use
    // the queue semaphore to signal that so that we don't need a separate lock.
    DP_Semaphore *sem_queue = wsc->sem_queue;
    DP_SEMAPHORE_MUST_WAIT(sem_queue);
    if (!DP_client_running(client)) {
        return;
    }

    DP_client_report_event(client, DP_CLIENT_EVENT_CONNECTION_ESTABLISHED,
                           NULL);

    int socket = DP_atomic_get(&wsc->socket);
    DP_Queue *queue = &wsc->queue;
    DP_Mutex *mutex_queue = wsc->mutex_queue;
    size_t reserved = DP_CLIENT_INITIAL_SEND_BUFFER_SIZE;
    unsigned char *buffer = DP_malloc(reserved);

    while (true) {
        DP_SEMAPHORE_MUST_WAIT(sem_queue);
        if (!DP_client_running(client)) {
            break;
        }

        DP_MUTEX_MUST_LOCK(mutex_queue);
        DP_Message *msg = DP_message_queue_shift(queue);
        DP_MUTEX_MUST_UNLOCK(mutex_queue);

        DP_MessageType type = DP_message_type(msg);
        size_t length = DP_client_message_serialize(msg, &buffer, &reserved);
        DP_message_decref(msg);

        if (emscripten_websocket_send_binary(socket, buffer, length)
            != EMSCRIPTEN_RESULT_SUCCESS) {
            DP_client_report_event(client, DP_CLIENT_EVENT_SEND_ERROR, NULL);
        }

        if (type == DP_MSG_DISCONNECT) {
            DP_client_stop(client);
            break;
        }
    }

    DP_free(buffer);
}

bool DP_web_socket_client_init(DP_Client *client)
{
    DP_ASSERT(client);
    DP_WebSocketClient *wsc = DP_client_inner(client);

    DP_message_queue_init(&wsc->queue, DP_CLIENT_INITIAL_SEND_QUEUE_CAPACITY);

    if (!(wsc->mutex_queue = DP_mutex_new())) {
        return false;
    }

    if (!emscripten_websocket_is_supported()) {
        DP_error_set("WebSockets not supported");
        return false;
    }

    if (!(wsc->sem_queue = DP_semaphore_new(0))) {
        return false;
    }

    if (!(wsc->thread_send = DP_thread_new(run_send, client))) {
        return false;
    }

    return true;
}

void DP_web_socket_client_dispose(DP_Client *client)
{
    DP_ASSERT(client);
    DP_WebSocketClient *wsc = DP_client_inner(client);
    if (wsc->callback_data) {
        wsc->callback_data->client = NULL;
    }
    DP_web_socket_client_stop(client);
    if (wsc->thread_send) {
        DP_SEMAPHORE_MUST_POST(wsc->sem_queue);
        DP_thread_free_join(wsc->thread_send);
    }
    DP_semaphore_free(wsc->sem_queue);
    DP_mutex_free(wsc->mutex_queue);
    DP_message_queue_dispose(&wsc->queue);
}

void DP_web_socket_client_stop(DP_Client *client)
{
    DP_ASSERT(client);
    DP_WebSocketClient *wsc = DP_client_inner(client);
    int socket = DP_atomic_xch(&wsc->socket, 0);
    if (socket > 0) {
        unsigned short ready_state;
        int result = emscripten_websocket_get_ready_state(socket, &ready_state);
        if (result == EMSCRIPTEN_RESULT_SUCCESS) {
            DP_debug("WebSocket ready state %hu", ready_state);
            switch (ready_state) {
            case 2:
                DP_debug("WebSocket is already CLOSING");
                break;
            case 3:
                DP_debug("WebSocket is already CLOSED");
                break;
            default:
                emscripten_websocket_close(socket, 1000, "");
                break;
            }
        }
        else {
            DP_warn("Error getting WebSocket ready state: %d", (int)result);
        }
    }
}

void DP_web_socket_client_send(DP_Client *client, DP_Message *msg)
{
    DP_ASSERT(client);
    DP_ASSERT(msg);
    DP_WebSocketClient *wsc = DP_client_inner(client);
    DP_Mutex *mutex_queue = wsc->mutex_queue;
    DP_MUTEX_MUST_LOCK(mutex_queue);
    DP_message_queue_push_noinc(&wsc->queue, msg);
    DP_MUTEX_MUST_UNLOCK(mutex_queue);
    DP_SEMAPHORE_MUST_POST(wsc->sem_queue);
}
