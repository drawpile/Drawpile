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
#include "tcp_socket_client.h"
#include "client.h"
#include "client_internal.h"
#include "uri_utils.h"
#include <dpcommon/atomic.h>
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/threading.h>
#include <dpmsg/message.h>
#include <dpmsg/message_queue.h>
#include <uriparser/Uri.h>

#if defined(_WIN32)
#    error "Networking not implemented on Windows"
#else
#    include <errno.h>
#    include <netdb.h>
#    include <sys/socket.h>
#    include <sys/types.h>
#    include <unistd.h>
#endif


DP_ClientUrlValidationResult DP_tcp_socket_client_url_valid(const char *url)
{
    UriUriA uri;
    if (!DP_uri_parse(&uri, url)) {
        return DP_CLIENT_URL_PARSE_ERROR;
    }

    DP_ClientUrlValidationResult result = DP_CLIENT_URL_VALID;

    if (!DP_uri_text_equal_ignore_case(uri.scheme,
                                       DP_TCP_SOCKET_CLIENT_SCHEME)) {
        result = DP_CLIENT_URL_SCHEME_UNSUPPORTED;
        goto end;
    }

    if (DP_uri_text_length(uri.hostText) == 0) {
        result = DP_CLIENT_URL_HOST_MISSING;
        goto end;
    }

end:
    uriFreeUriMembersA(&uri);
    return result;
}


static struct addrinfo *get_address_info(DP_Client *client)
{
    DP_client_report_event(client, DP_CLIENT_EVENT_RESOLVE_ADDRESS_START, NULL);

    DP_TcpSocketClient *tsc = DP_client_inner(client);
    char *host = DP_uri_text_dup(tsc->uri.hostText);
    if (!host) {
        DP_error_set("URL has no host");
        DP_client_report_error(client, DP_CLIENT_EVENT_RESOLVE_ADDRESS_ERROR);
        return false;
    }

    char *port_or_null = DP_uri_text_dup(tsc->uri.portText);
    const char *port =
        port_or_null ? port_or_null : DP_TCP_SOCKET_CLIENT_DEFAULT_PORT;

    struct addrinfo hints = {0};
    // AI_ADDRCONFIG returns only IPv4/IPv6 addresses if the system actually has
    // adapters for those respective types configured. AI_V4MAPPED maps IPv4
    // addresses to IPv6 equivalents if the system has only an IPv6 adapter.
    // Those flags are the default. We also add AI_NUMERICSERV, which treats the
    // port as numeric and doesn't try to look it up as a service name.
    hints.ai_flags = AI_ADDRCONFIG | AI_V4MAPPED | AI_NUMERICSERV;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo *addrinfos;
    int error = getaddrinfo(host, port, &hints, &addrinfos);
    DP_free(host);
    DP_free(port_or_null);

    if (error == 0) {
        DP_client_report_event(client, DP_CLIENT_EVENT_RESOLVE_ADDRESS_SUCCESS,
                               NULL);
        return addrinfos;
    }
    else {
        DP_error_set("%s", gai_strerror(error));
        DP_client_report_error(client, DP_CLIENT_EVENT_RESOLVE_ADDRESS_ERROR);
        return NULL;
    }
}

static const char *address_family_to_string(int family)
{
    switch (family) {
    case AF_INET:
        return "IPv4";
    case AF_INET6:
        return "IPv6";
    default:
        return NULL;
    }
}

static int open_socket(DP_Client *client, struct addrinfo *addrinfos)
{
    for (struct addrinfo *ai = addrinfos; ai && DP_client_running(client);
         ai = ai->ai_next) {
        DP_client_report_event(client, DP_CLIENT_EVENT_CONNECT_SOCKET_START,
                               address_family_to_string(ai->ai_family));
        int sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (sockfd == -1) {
            DP_error_set("%s", strerror(errno));
            DP_client_report_error(client,
                                   DP_CLIENT_EVENT_CONNECT_SOCKET_ERROR);
        }
        else if (connect(sockfd, ai->ai_addr, ai->ai_addrlen) == -1) {
            DP_error_set("%s", strerror(errno));
            DP_client_report_error(client,
                                   DP_CLIENT_EVENT_CONNECT_SOCKET_ERROR);
            close(sockfd);
        }
        else {
            DP_debug("Connected socket with fd %d", sockfd);
            DP_client_report_event(
                client, DP_CLIENT_EVENT_CONNECT_SOCKET_SUCCESS, NULL);
            return sockfd;
        }
    }
    return -1;
}

static bool try_recv(DP_Client *client, int sockfd, unsigned char *buffer,
                     size_t length)
{
    size_t received = 0;
    while (DP_client_running(client) && received < length) {
        ssize_t result = recv(sockfd, buffer + received, length - received, 0);
        if (result >= 0) {
            received += (size_t)result;
        }
        else {
            if (DP_client_running(client)) {
                DP_client_report_event(client, DP_CLIENT_EVENT_RECV_ERROR,
                                       strerror(errno));
            }
            else {
                DP_debug("Receive error during shutdown: %s", strerror(errno));
            }
            return false;
        }
    }
    return received == length;
}

static void run_recv(void *data)
{
    DP_Client *client = data;
    DP_TcpSocketClient *tsc = DP_client_inner(client);
    int sockfd = DP_atomic_get(&tsc->socket);
    size_t reserved = DP_CLIENT_INITIAL_RECV_BUFFER_SIZE;
    unsigned char *buffer = DP_malloc(reserved);

    while (DP_client_running(client)) {
        if (try_recv(client, sockfd, buffer, DP_MESSAGE_HEADER_LENGTH)) {
            size_t body_length = DP_read_bigendian_uint16(buffer);
            size_t total_length = body_length + DP_MESSAGE_HEADER_LENGTH;
            if (reserved < total_length) {
                buffer = DP_realloc(buffer, total_length);
                reserved = total_length;
            }
            if (try_recv(client, sockfd, buffer + DP_MESSAGE_HEADER_LENGTH,
                         body_length)) {
                DP_client_handle_message(client, buffer, total_length);
            }
        }
    }

    DP_free(buffer);
}

static bool establish_connection(DP_Client *client, DP_TcpSocketClient *tsc)
{
    if (!DP_client_running(client)) {
        return false;
    }

    struct addrinfo *addrinfos = get_address_info(client);
    if (!addrinfos) {
        return false;
    }

    int sockfd = open_socket(client, addrinfos);
    freeaddrinfo(addrinfos);
    if (sockfd == -1) {
        return false;
    }

    DP_atomic_set(&tsc->socket, sockfd);
    if (!DP_client_running(client)) {
        return false;
    }

    if (!(tsc->thread_recv = DP_thread_new(run_recv, client))) {
        DP_client_report_error(client, DP_CLIENT_EVENT_SPAWN_RECV_THREAD_ERROR);
        return false;
    }

    return true;
}

static void run_send(void *data)
{
    DP_Client *client = data;
    DP_TcpSocketClient *tsc = DP_client_inner(client);
    if (!establish_connection(client, tsc)) {
        DP_client_report_event(client, DP_CLIENT_EVENT_CONNECTION_CLOSED, NULL);
        return;
    }

    DP_client_report_event(client, DP_CLIENT_EVENT_CONNECTION_ESTABLISHED,
                           NULL);

    int sockfd = DP_atomic_get(&tsc->socket);
    int sendflag = 0;
#   if defined(SO_NOSIGPIPE)
        int set = 1;
        setsockopt(sockfd, SOL_SOCKET, SO_NOSIGPIPE, &set, sizeof(set));
#   elif defined(MSG_NOSIGNAL)
        // Use flag NOSIGNAL on send call
        sendflag = MSG_NOSIGNAL;
#   endif
    DP_Queue *queue = &tsc->queue;
    DP_Mutex *mutex_queue = tsc->mutex_queue;
    DP_Semaphore *sem_queue = tsc->sem_queue;
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

        size_t sent = 0;
        do {
            ssize_t result =
                send(sockfd, buffer + sent, length - sent, sendflag);
            if (result >= 0) {
                sent += (size_t)result;
            }
            else {
                if (DP_client_running(client)) {
                    DP_client_report_event(client, DP_CLIENT_EVENT_SEND_ERROR,
                                           strerror(errno));
                }
                else {
                    DP_debug("Send error during shutdown: %s", strerror(errno));
                }
                break;
            }
        } while (sent < length);

        if (type == DP_MSG_DISCONNECT) {
            DP_debug("Sent disconnect, stopping client");
            DP_client_stop(client);
            break;
        }
    }

    DP_free(buffer);
}


bool DP_tcp_socket_client_init(DP_Client *client)
{
    DP_ASSERT(client);
    DP_TcpSocketClient *tsc = DP_client_inner(client);

    if (!DP_uri_parse(&tsc->uri, DP_client_url(client))) {
        return false;
    }

    DP_message_queue_init(&tsc->queue, DP_CLIENT_INITIAL_SEND_QUEUE_CAPACITY);

    if (!(tsc->mutex_queue = DP_mutex_new())) {
        return false;
    }

    if (!(tsc->sem_queue = DP_semaphore_new(0))) {
        return false;
    }

    if (!(tsc->thread_send = DP_thread_new(run_send, client))) {
        return false;
    }

    return true;
}


void DP_tcp_socket_client_dispose(DP_Client *client)
{
    DP_ASSERT(client);
    DP_TcpSocketClient *tsc = DP_client_inner(client);
    DP_tcp_socket_client_stop(client);
    DP_thread_free_join(tsc->thread_recv);
    if (tsc->thread_send) {
        DP_SEMAPHORE_MUST_POST(tsc->sem_queue);
        DP_thread_free_join(tsc->thread_send);
    }
    DP_semaphore_free(tsc->sem_queue);
    DP_mutex_free(tsc->mutex_queue);
    DP_message_queue_dispose(&tsc->queue);
    uriFreeUriMembersA(&tsc->uri);
}


void DP_tcp_socket_client_stop(DP_Client *client)
{
    DP_ASSERT(client);
    DP_TcpSocketClient *tsc = DP_client_inner(client);
    int socket = DP_atomic_xch(&tsc->socket, -1);
    if (socket != -1) {
        shutdown(socket, SHUT_RDWR);
        close(socket);
    }
}


void DP_tcp_socket_client_send(DP_Client *client, DP_Message *msg)
{
    DP_ASSERT(client);
    DP_ASSERT(msg);
    DP_TcpSocketClient *tsc = DP_client_inner(client);
    DP_Mutex *mutex_queue = tsc->mutex_queue;
    DP_MUTEX_MUST_LOCK(mutex_queue);
    DP_message_queue_push_noinc(&tsc->queue, msg);
    DP_MUTEX_MUST_UNLOCK(mutex_queue);
    DP_SEMAPHORE_MUST_POST(tsc->sem_queue);
}
