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
import { LoginErrorEvent } from "../Drawdance";
import { DrawdanceClientEvent } from "../DrawdanceEvents";

export type ConnectDialogMode =
  | "INITIAL"
  | "DISCONNECTED"
  | "CONNECTING"
  | "AUTH"
  | "EXT_AUTH"
  | "LOGGING_IN"
  | "ROOM"
  | "JOINING"
  | "ROOM_PASSWORD";

export const CLIENT_EVENT_DESCRIPTIONS: {
  [key: number]: ((message?: string) => string) | undefined;
} = Object.freeze({
  [DrawdanceClientEvent.RESOLVE_ADDRESS_START]: (_message?: string): string =>
    "Resolving address...",
  [DrawdanceClientEvent.RESOLVE_ADDRESS_SUCCESS]: (_message?: string): string =>
    "Address resolved successfully.",
  [DrawdanceClientEvent.RESOLVE_ADDRESS_ERROR]: (message?: string): string =>
    `Error resolving address: ${message}`,
  [DrawdanceClientEvent.CONNECT_SOCKET_START]: (message?: string): string =>
    `Connecting via ${message}...`,
  [DrawdanceClientEvent.CONNECT_SOCKET_SUCCESS]: (_message?: string): string =>
    "Connected.",
  [DrawdanceClientEvent.CONNECT_SOCKET_ERROR]: (message?: string): string =>
    `Error connecting: ${message}`,
  [DrawdanceClientEvent.SPAWN_RECV_THREAD_ERROR]: (message?: string): string =>
    `Error spawning receive thread: ${message}`,
  [DrawdanceClientEvent.NETWORK_ERROR]: (_message?: string): string =>
    "Network error.",
  [DrawdanceClientEvent.CONNECTION_ESTABLISHED]: (_message?: string): string =>
    "Connection established.",
});

export const DISCONNECT_REASONS: { [key: string]: string | undefined } =
  Object.freeze({
    disconnect_error: "Disconnected by server: an error occurred.",
    disconnect_kick: "Disconnected by server: you were kicked.",
    disconnect_shutdown: "Disconnected: server shutting down.",
    disconnect_other: "Received disconnect: unknown reason.",
    connection_closed: "Connection closed unexpectedly.",
    client_free: "Disconnected because client was freed unexpectedly.",
    network_error: "Disconnected due to network error.",
    send_error: "Disconnected due to send error.",
    recv_error: "Disconnected due to receive error.",
  });

export const LOGIN_ERRORS: {
  [key: string]: ((event: LoginErrorEvent) => string) | undefined;
} = Object.freeze({
  command_error: ({ code }: LoginErrorEvent): string => {
    const reasons: { [key: string]: string } = {
      none: "no reason given",
      not_found: "session not found",
      bad_password: "incorrect password",
      bad_username: "invalid username",
      banned_name: "username has been locked",
      name_inuse: "username is already taken",
      closed: "session is closed",
      banned: "you have been banned from this session",
      id_in_use: "session alias is reserved",
    };
    return `Login error: ${reasons[code || "none"] || code}.`;
  },
  command_type_mismatch: (event: LoginErrorEvent): string =>
    `Handshake error: expected "${event.expected_type}" command, but got "${event.actual_type}".`,
  command_unexpected: (event: LoginErrorEvent): string =>
    `Handshake error: got a "${event.command_type}" command when not expecting to receive anything.`,
  protocol_version_mismatch: (event: LoginErrorEvent): string =>
    `Incompatible protocols: you are on version ${event.expected_version}, but the server is on version ${event.actual_version}.`,
  ext_auth_url_invalid: (event: LoginErrorEvent): string =>
    `Server error: invalid ext-auth URL "${event.url}"`,
  ext_auth_error: (event: LoginErrorEvent): string =>
    `Ext-Auth error: ${event.message}`,
  ext_auth_response_error: (event: LoginErrorEvent): string =>
    `Ext-Auth invalid response: ${event.message}`,
  ext_auth_status: ({ status }: LoginErrorEvent): string => {
    const reasons: { [key: string]: string } = {
      none: "no reason given",
      badpass: "incorrect ext-auth password",
      optgroup: "group membership needed",
    };
    return `Ext-auth error: ${reasons[status || "none"] || status}`;
  },
  ident_failed: (event: LoginErrorEvent): string =>
    `Handshake error: unexpected ident state "${event.state}".`,
  internal_error: (event: LoginErrorEvent): string =>
    `Internal error: ${event.error}`,
  join_failed: (event: LoginErrorEvent): string =>
    `Handshake error: unexpected join state "${event.state}".`,
  no_single_session: (_event: LoginErrorEvent): string =>
    "Error: session not yet started.",
  ip_ban: (_event: LoginErrorEvent): string =>
    "Error: your IP address is banned from this server.",
  disconnect: (_event: LoginErrorEvent): string =>
    "Error: you were disconnected by the server.",
});
