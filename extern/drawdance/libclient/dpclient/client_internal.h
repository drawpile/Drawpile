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
#include <stdbool.h>

typedef struct DP_Message DP_Message;


#define DP_CLIENT_INITIAL_SEND_BUFFER_SIZE    64
#define DP_CLIENT_INITIAL_RECV_BUFFER_SIZE    64
#define DP_CLIENT_INITIAL_SEND_QUEUE_CAPACITY 64


bool DP_client_running(DP_Client *client);

void DP_client_stop(DP_Client *client);

const char *DP_client_url(DP_Client *client);

void *DP_client_inner(DP_Client *client);


void DP_client_report_event(DP_Client *client, DP_ClientEventType event,
                            const char *message_or_null);

void DP_client_report_error(DP_Client *client, DP_ClientEventType event);


void DP_client_handle_message(DP_Client *client, unsigned char *buffer,
                              size_t length);


size_t DP_client_message_serialize(DP_Message *msg, unsigned char **out_buffer,
                                   size_t *out_reserved);
