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
#ifndef DPCLIENT_WEB_SOCKET_CLIENT_H
#define DPCLIENT_WEB_SOCKET_CLIENT_H

#ifndef __EMSCRIPTEN__
#    error "WebSockets are only available in Emscripten"
#endif

#include "client.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpcommon/queue.h>

typedef struct DP_Client DP_Client;
typedef struct DP_Message DP_Message;
typedef struct DP_Mutex DP_Mutex;
typedef struct DP_Semaphore DP_Semaphore;
typedef struct DP_Thread DP_Thread;


#define DP_WEB_SOCKET_CLIENT_NULL                                \
    (DP_WebSocketClient)                                         \
    {                                                            \
        DP_QUEUE_NULL, NULL, NULL, NULL, DP_ATOMIC_INIT(0), NULL \
    }

typedef struct DP_WebSocketCallbackData {
    DP_Client *client;
} DP_WebSocketCallbackData;

typedef struct DP_WebSocketClient {
    DP_Queue queue;
    DP_Mutex *mutex_queue;
    DP_Semaphore *sem_queue;
    DP_Thread *thread_send;
    DP_Atomic socket;
    DP_WebSocketCallbackData *callback_data;
} DP_WebSocketClient;


DP_ClientUrlValidationResult DP_web_socket_client_url_valid(const char *url);

bool DP_web_socket_client_init(DP_Client *client);

void DP_web_socket_client_dispose(DP_Client *client);

void DP_web_socket_client_stop(DP_Client *client);

void DP_web_socket_client_send(DP_Client *client, DP_Message *msg);


#endif
