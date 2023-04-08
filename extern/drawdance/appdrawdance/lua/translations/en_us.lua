-- Copyright (c) 2022 askmeaboutloom
--
-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the "Software"), to deal
-- in the Software without restriction, including without limitation the rights
-- to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
-- copies of the Software, and to permit persons to whom the Software is
-- furnished to do so, subject to the following conditions:
--
-- The above copyright notice and this permission notice shall be included in
-- all copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
-- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
-- OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
-- SOFTWARE.
local Template <const> = require("template")

return {
    main_menu = {
        mnu_file = "File",
        mni_file_gui_reload = "Reload GUI",
        mni_file_quit = "Quit",
        mnu_session = "Session",
        mni_session_join = "Join...",
        mni_session_leave = "Leave",
        mni_session_close = "Close",
        mnu_help = "Help",
        mni_help_about = "About",
        mni_help_licenses = "Licenses",
    },
    join_window = {
        title = "Join Session",
        lbl_address = "Host address or URL:",
        btn_join = "Join",
        btn_cancel = "Cancel",
        url_validation_parse_error = Template:new"Error: URL could not be parsed.",
        url_validation_scheme_unsupported = Template:new"Error: unsupported scheme (can only use {supported_schemes}.)",
        url_validation_host_missing = Template:new"Error: URL is missing a host.",
    },
    about_window = {
        title = "About",
        txt_about = Template:new_strip_margin[[
            |Drawdance {version} ({build_type}) - A Drawpile Client
            |
            |Copyright {copyright_years} askmeaboutloom.
            |
            |Drawdance is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
            |
            |This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
            |
            |You should have received a copy of the GNU General Public License along with this program. If not, see http://www.gnu.org/licenses/.
        ]],
        txt_disclaimer = Template:new_strip_margin[[
            |Drawdance is a separate project from Drawpile. Please don't report Drawdance issues to the Drawpile maintainers.
            |
            |Licenses for Drawdance, Drawpile and various other third-party components can be found under Help > Licenses.
        ]],
    },
    licenses_window = {
        title = "Licenses",
        tab_drawdance = "Drawdance",
        txt_drawdance_license = "Drawdance license goes here",
        txt_licenses_load_error = Template:new"Error loading licenses: {error}",
        txt_license_description = function (args)
            local descs = {
                ["BSD-3-Clause"] = "the BSD 3-Clause \"New\" or \"Revised\" License",
                ["curl"] = "the curl License",
                ["GPL-2.0-or-later"] = "the GNU General Public License (GPL), version 2 or later; Drawdance is using it under version 3",
                ["GPL-3.0"] = "the GNU General Public License (GPL), version 3",
                ["GPL-3.0-or-later"] = "the GNU General Public License (GPL), version 3 or later",
                ["MIT"] = "the MIT License",
                ["Unlicense"] = "the Unlicense, therefore it is in the Public Domain",
                ["Zlib"] = "the zlib License",
            }
            local ident = args.ident
            local desc = descs[ident] or ident
            return string.format("%s is licensed under %s. The license text follows below.", args.name, desc)
        end,
    },
    connect_dialog = {
        title = "Connecting",
        btn_cancel = "Cancel",
        btn_close = "Close",
        btn_ok = "OK",
        btn_join = "Join",
        hdr_id = "Identifier",
        hdr_title = "Title",
        hdr_founder = "Started by",
        hdr_users = "Users",
        hdr_flags = "Flags",
        col_flags = function (args)
            return table.concat(args.flags, ", ")
        end,
        fld_username = "Username",
        fld_password = "Password",
        fld_room_password = "Room Password",
        txt_status_init = "Connecting...",
        txt_status_unknown = Template:new"{name}",
        txt_status_unknown_message = Template:new"{name} {message}",
        txt_status_resolve_address_start = "Resolving address...",
        txt_status_resolve_address_success = "Address resolved successfully.",
        txt_status_resolve_address_error = Template:new"Error resolving address: {message}",
        txt_status_connect_socket_start = Template:new"Connecting via {message}...",
        txt_status_connect_socket_success = "Connected.",
        txt_status_connect_socket_error = Template:new"Error connecting: {message}",
        txt_status_spawn_recv_thread_error = Template:new"Error spawning receive thread: {message}",
        txt_status_network_error = Template:new"Network error.",
        txt_status_connection_established = "Connection established.",
        txt_status_require_credentials_allow_guests = "Please enter a username. Guest logins are allowed.",
        txt_status_require_credentials_password_allow_guests = Template:new"The username \"{username}\" is registered on this server. Either enter its password (not the session password!) or pick a different username that's not registered.",
        txt_status_require_credentials_no_guests = Template:new"Please enter a username and password. Guest logins are not allowed, so if you didn't register on this server, you can't join.",
        txt_status_require_credentials_ext_auth = Template:new"The username \"{username}\" is registered on \"{url}\". Please enter the username and password registered there (not the session password!)",
        txt_status_login = "Logging in...",
        txt_status_require_room = "Please pick a room to join.",
        txt_status_require_room_password = "Please enter the room's password (not your account password!) to join.",
        txt_status_join = "Joining...",
        txt_status_login_error_command_error = function (args)
            local reasons = {
                none = "no reason given",
                not_found = "session not found",
                bad_password = "incorrect password",
                bad_username = "invalid username",
                banned_name = "username has been locked",
                name_inuse = "username is already taken",
                closed = "session is closed",
                banned = "you have been banned from this session",
                id_in_use = "session alias is reserved",
            }
            local code = args.code or "none"
            local reason = reasons[code] or code
            return string.format("Login error: %s.", reason)
        end,
        txt_status_login_error_command_type_mismatch = Template:new"Handshake error: expected \"{expected_type}\" command, but got \"{actual_type}\".",
        txt_status_login_error_command_unexpected = Template:new"Handshake error: got a \"{command_type}\" command when not expecting to receive anything.",
        txt_status_login_error_protocol_version_mismatch = Template:new"Incompatible protocols: you are on version {expected_version}, but the server is on version {actual_version}.",
        txt_status_login_error_ext_auth_url_invalid = Template:new"Server error: invalid ext-auth URL \"{url}\"",
        txt_status_login_error_ext_auth_error = Template:new"Ext-Auth error: {message}",
        txt_status_login_error_ext_auth_response_error = Template:new"Ext-Auth invalid response: {message}",
        txt_status_login_error_ext_auth_status = function (args)
            local reasons = {
                none = "no reason given",
                badpass = "incorrect ext-auth password",
                optgroup = "group membership needed",
            }
            local status = args.status or "none"
            local reason = reasons[status] or status
            return string.format("Ext-auth error: %s.", reason)
        end,
        txt_status_login_error_ident_failed = Template:new"Handshake error: unexpected ident state \"{state}\".",
        txt_status_login_error_internal_error = Template:new"Internal error: {error}",
        txt_status_login_error_join_failed = Template:new"Handshake error: unexpected join state \"{state}\".",
        txt_status_login_error_no_single_session = "Error: session not yet started.",
        txt_status_login_error_ip_ban = "Error: your IP address is banned from this server.",
        txt_status_login_error_disconnect = "Error: you were disconnected by the server.",
    },
    credentials_dialog = {
        title = "Credentials",
        btn_ok = "OK",
        btn_cancel = "Cancel",
        fld_username = "Username",
        fld_password = "Password",
    },
    disconnect_dialog = {
        title = "Disconnected",
        btn_ok = "OK",
        txt_reason_disconnect_error = "Disconnected by server: an error occurred.",
        txt_reason_disconnect_kick = "Disconnected by server: you were kicked.",
        txt_reason_disconnect_shutdown = "Disconnected: server shutting down.",
        txt_reason_disconnect_other = "Received disconnect: unknown reason.",
        txt_reason_connection_closed = "Connection closed unexpectedly.",
        txt_reason_client_free = "Disconnected because client was freed unexpectedly.",
        txt_reason_network_error = "Disconnected due to network error.",
        txt_reason_send_error = "Disconnected due to send error.",
        txt_reason_recv_error = "Disconnected due to receive error.",
    },
    layer_list_dock = {
        title = "Layers",
    },
    chat_dock = {
        title = "Chat",
        join = Template:new">> {name} joined.",
        leave = Template:new"<< {name} left.",
        action = Template:new"** {name} {text}",
        message = Template:new"{name}: {text}",
        hdr_messages = "Messages",
        hdr_users = "Users",
        txt_no_state = "No sessions.",
        txt_not_connected = "The session is not connected.",
    },
}
