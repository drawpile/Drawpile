-- Copyright (C) 2022 askmeaboutloom
--
-- This program is free software: you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation, either version 3 of the License, or
-- (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program.  If not, see <http://www.gnu.org/licenses/>.
--
-- --------------------------------------------------------------------
--
-- This code is based on Drawpile, using it under the GNU General Public
-- License, version 3. See 3rdparty/licenses/drawpile/COPYING for details.
local EventTypes <const> = require("event.types")
local Json <const> = require("3rdparty.json.json")

local MSG_TYPE_SERVER_COMMAND <const> = DP.Message.Type.SERVER_COMMAND
local MSG_TYPE_DISCONNECT <const> = DP.Message.Type.DISCONNECT
local MSG_TYPE_PING <const> = DP.Message.Type.PING
local PROTOCOL_VERSION <const> = "4"
local EXT_AUTH_TIMEOUT = 10 -- in seconds

local STATE_EXPECT_HELLO <const> = "expect_hello"
local STATE_AWAIT_CREDENTIALS <const> = "await_credentials"
local STATE_AWAIT_EXT_AUTH_CREDENTIALS <const> = "await_ext_auth_credentials"
local STATE_EXPECT_IDENTIFIED <const> = "expect_identified"
local STATE_EXPECT_ROOM_LIST <const> = "expect_room_list"
local STATE_EXPECT_JOIN <const> = "expect_join"

local COMMAND_HANDLERS <const> = {
    [STATE_EXPECT_HELLO] = "_expect_hello",
    [STATE_AWAIT_CREDENTIALS] = "_expect_nothing",
    [STATE_AWAIT_EXT_AUTH_CREDENTIALS] = "_expect_nothing",
    [STATE_EXPECT_IDENTIFIED] = "_expect_identified",
    [STATE_EXPECT_ROOM_LIST] = "_expect_room_list",
    [STATE_EXPECT_JOIN] = "_expect_join",
}

local function read_flags(in_flags, out_flags)
    if in_flags then
        for _, flag in ipairs(in_flags) do
            out_flags[flag] = true
        end
    end
end

local function compare_rooms(a, b)
    if a.title == b.title then
        return a.id < b.id
    else
        return a.title < b.title
    end
end


local LoginHandler <const> = require("class")("LoginHandler")

function LoginHandler:init(app)
    self._app = app
    self._sessions = {}
    app:subscribe_method(EventTypes.REQUEST_SESSION_JOIN, self)
    app:subscribe_method(EventTypes.REQUEST_SESSION_CANCEL, self)
    app:subscribe_method(EventTypes.LOGIN_PROVIDE_CREDENTIALS, self)
    app:subscribe_method(EventTypes.LOGIN_PROVIDE_ROOM, self)
    app:subscribe_method(EventTypes.LOGIN_PROVIDE_ROOM_PASSWORD, self)
    app:subscribe_method(EventTypes.CLIENT_EVENT, self)
    app:subscribe_method(EventTypes.CLIENT_MESSAGE, self)
end

function LoginHandler:on_request_session_join(event)
    local url = event.url
    local client = DP.Client:new(url)
    local client_id = client.id
    self._sessions[client_id] = {
        client_id = client_id,
        client = client,
        state = STATE_EXPECT_HELLO,
        title = string.format("Session %d", client_id),
        server_flags = {},
        user_flags = {},
        rooms = {},
    }
    self._app:publish(EventTypes.CLIENT_CREATE, {
        client_id = client_id,
        url = url,
    })
end

function LoginHandler:on_request_session_cancel(event)
    local client_id = event.client_id
    local session = self._sessions[client_id]
    if session then
        self._sessions[client_id] = nil
        session.client:terminate()
    end
end

function LoginHandler:_report_error(session, type, args)
    local client_id = session.client_id
    self._sessions[client_id] = nil
    session.client:terminate()

    local event = args or {}
    event.client_id = client_id
    event.type = type
    self._app:publish(EventTypes.LOGIN_ERROR, event)
end

function LoginHandler:_report_error_command(session, command)
    local code = command.code or "unknown"
    self:_report_error(session, "command_error", {
        code = string.gsub(code, "[A-Z]", function (s)
            return "_" .. string.lower(s)
        end),
    })
end

function LoginHandler:_command_has_type(session, command, expected_type)
    local actual_type = command.type
    if actual_type == expected_type then
        return true
    elseif actual_type == "error" then
        self:_report_error_command(session, command)
        return false
    else
        self:_report_error(session, "command_type_mismatch", {
            actual_type = actual_type,
            expected_type = expected_type,
        })
        return false
    end
end

function LoginHandler:_send(session, payload)
    local msg = DP.Message.Command:new(Json.encode(payload))
    session.client:send(msg)
end

function LoginHandler:_expect_nothing(session, command)
    self:_report_error(session, "command_unexpected", {
        command_type = command.type,
    })
    return false
end

function LoginHandler:_expect_hello(session, command)
    if not self:_command_has_type(session, command, "login") then
        return false
    end

    local version = tostring(command.version)
    if version ~= PROTOCOL_VERSION then
        self:_report_error(session, "protocol_version_mismatch", {
            actual_version = version,
            expected_version = PROTOCOL_VERSION,
        })
        return false
    end

    read_flags(command.flags, session.server_flags)

    if session.server_flags.TLS then
        -- TODO: implement TLS, but we'll try connecting anyway I guess?
    end

    session.state = STATE_AWAIT_CREDENTIALS
    self._app:publish(EventTypes.LOGIN_REQUIRE_CREDENTIALS, {
        client_id = session.client_id,
        allow_guests = not session.server_flags.NOGUEST,
        password_required = false,
    })
    return true
end

function LoginHandler:_send_credentials(session, event)
    local username = event.username
    local password = event.password
    local args
    if not password or #password == 0 then
        args = {username}
    else
        args = {username, password}
    end

    session.state = STATE_EXPECT_IDENTIFIED
    self:_send(session, {
        cmd = "ident",
        args = args,
    })
    return true
end

function LoginHandler:_send_ext_auth(session, event)
    local ext_auth = session.ext_auth
    local username = event.username
    ext_auth.username = username
    local body = {
        avatar = false,
        username = username,
        password = event.password,
        nonce = ext_auth.nonce,
        group = ext_auth.group,
    }
    session.client:ext_auth(ext_auth.url, Json.encode(body), EXT_AUTH_TIMEOUT)
    return true
end

function LoginHandler:_require_ext_auth_credentials(session, command)
    local url = command.extauthurl or ""
    if not string.find(url, "^%s*[hH][tT][tT][pP][sS]?://") then
        self:_report_error(session, "ext_auth_url_invalid", {url = url})
        return false
    end

    session.ext_auth = {
        url = url,
        nonce = command.nonce,
    }

    -- The group can't be blank in the auth request, but drawpile-srv returns
    -- an empty string when there's no value. So filter out blank values here.
    local group = command.group
    if group and string.find(group, "%S") then
        session.ext_auth.group = group
    end

    session.state = STATE_AWAIT_EXT_AUTH_CREDENTIALS
    self._app:publish(EventTypes.LOGIN_REQUIRE_CREDENTIALS, {
        client_id = session.client_id,
        allow_guests = not session.server_flags.NOGUEST,
        password_required = true,
        ext_auth_url = url,
    })
    return true;
end

function LoginHandler:_handle_ext_auth(session, message)
    print(message)
    local ok, response = pcall(Json.decode, message)
    if not ok then
        self:_report_error(session, "ext_auth_response", {message = message})
        return false
    end

    local status = response.status
    if status == "auth" then
        session.state = STATE_EXPECT_IDENTIFIED
        self:_send(session, {
            cmd = "ident",
            args = {session.ext_auth.username},
            kwargs = {extauth = response.token},
        })
        return true
    else
        self:_report_error(session, "ext_auth_status", {status = status})
        return false
    end
end

function LoginHandler:_expect_identified(session, command)
    if not self:_command_has_type(session, command, "result") then
        return false
    end

    local state = command.state
    if state == "needPassword" then
        self._app:publish(EventTypes.LOGIN_REQUIRE_CREDENTIALS, {
            client_id = session.client_id,
            allow_guests = not session.server_flags.NOGUEST,
            password_required = true,
        })
        return true
    elseif state == "needExtAuth" then
        return self:_require_ext_auth_credentials(session, command)
    elseif state ~= "identOk" then
        self:_report_error(session, "ident_failed", {state = state})
        return false
    end

    read_flags(command.flags, session.user_flags)
    session.guest = command.guest and true or false
    session.mod = session.user_flags.MOD and true or false

    session.state = STATE_EXPECT_ROOM_LIST
    return true
end

function LoginHandler:_join_room(session, event)
    local kwargs
    local room_password = event.room_password
    if room_password and #room_password ~= 0 then
        kwargs = {password = room_password}
    else
        kwargs = nil
    end

    -- We need to attach a document to handle drawing commands. Intuitively,
    -- that should happen after the login, but drawing commands are processed
    -- immediately while the login sucess response is delayed until the next
    -- event handling loop. That means the document needs to be available as
    -- soon as we receive the join message, making this - right before joining
    -- the session - the latest point where we can do so in time.
    local client = session.client
    if not client.document then
        client:attach_document(DP.Document:new())
    end

    session.state = STATE_EXPECT_JOIN
    self:_send(session, {
        cmd = "join",
        args = {session.selected_room.id},
        kwargs = kwargs,
    })
    return true
end

function LoginHandler:_publish_require_room(session)
    local room_list = {}
    for _, room in pairs(session.rooms) do
        table.insert(room_list, room)
    end
    table.sort(room_list, compare_rooms)
    self._app:publish(EventTypes.LOGIN_REQUIRE_ROOM, {
        client_id = session.client_id,
        title = session.title,
        rooms = room_list,
    })
end

function LoginHandler:_select_room(session, event)
    local room_id = event.room_id
    local room = session.rooms[room_id]
    if room then
        session.selected_room = room
        if room.password and not session.mod then
            self._app:publish(EventTypes.LOGIN_REQUIRE_ROOM_PASSWORD, {
                client_id = session.client_id,
                id = room.id,
                alias = room.alias,
                title = room.title,
                founder = room.founder,
            })
        else
            self:_join_room(session, {})
        end
    else
        warn(string.format("Invalid room '%s' selected", room_id))
        self:_publish_require_room(session)
    end
    return true
end

function LoginHandler:_expect_room_list(session, command)
    if not self:_command_has_type(session, command, "login") then
        return false
    end

    if command.title then
        session.title = command.title
    end

    if command.sessions then
        for _, room in ipairs(command.sessions) do
            local room_id = room.id or room.alias
            if room_id then
                session.rooms[room_id] = {
                    id = room_id,
                    alias = room.alias,
                    title = room.title,
                    founder = room.founder,
                    protocol = room.protocol,
                    users = room.userCount,
                    password = room.hasPassword,
                    closed = room.closed or (room.authOnly and session.guest),
                    nsfm = room.nsfm,
                }
                if not session.first_room_id then
                    session.first_room_id = room_id
                end
            end
            -- TODO auto-join by url
        end
    end

    if command.remove then
        for _, room_id in ipairs(command.remove) do
            session.rooms[room_id] = nil
        end
    end

    if session.server_flags.MULTI then
        self:_publish_require_room(session)
        return true
    elseif not session.first_room_id then
        self:_report_error(session, "missing_single_session")
        return false
    else
        return self:_select_room(session, {room_id = session.first_room_id})
    end
end

function LoginHandler:_expect_join(session, command)
    if command.type == "login" then
        return true -- Disregard further room list updates.
    end

    if not self:_command_has_type(session, command, "result") then
        return false
    end

    local state = command.state
    if state ~= "join" then
        self:_report_error("join_failed", {state = state})
        return false
    end

    local room_flags = {}
    read_flags(command.flags, command.join and command.join.flags)

    self._app:publish(EventTypes.SESSION_CREATE, {
        client_id = session.client_id,
        client = session.client,
        server_flags = session.server_flags,
        user_flags = session.user_flags,
        room_flags = room_flags,
    })
    return false
end

function LoginHandler:_handle_command(session, message)
    local command = Json.decode(message)
    local handler = COMMAND_HANDLERS[session.state]
    return self[handler](self, session, command)
end

function LoginHandler:_run_handler(fn, session, arg)
    local ok, result = pcall(fn, self, session, arg)
    if not ok then
        self:_report_error(session, "internal_error", {
            error = result,
        })
    elseif not result then
        self._sessions[session.client_id] = nil
    end
end

function LoginHandler:on_login_provide_credentials(event)
    local client_id = event.client_id
    local session = self._sessions[client_id]
    if session then
        if session.state == STATE_AWAIT_EXT_AUTH_CREDENTIALS then
            self:_run_handler(self._send_ext_auth, session, event)
        else
            self:_run_handler(self._send_credentials, session, event)
        end
    else
        warn(string.format("Unexpected credentials for session %d", client_id))
    end
end

function LoginHandler:on_login_provide_room(event)
    local client_id = event.client_id
    local session = self._sessions[client_id]
    if session then
        self:_run_handler(self._select_room, session, event)
    else
        warn(string.format("Unexpected room for session %d", client_id))
    end
end

function LoginHandler:on_login_provide_room_password(event)
    local client_id = event.client_id
    local session = self._sessions[client_id]
    if session then
        self:_run_handler(self._join_room, session, event)
    else
        warn(string.format("Unexpected room password for session %d", client_id))
    end
end

function LoginHandler:on_client_event(event)
    local client_id = event.client_id
    local session = self._sessions[client_id]
    if session then
        local type = event.type
        if type == DP.Client.Event.CONNECTION_ESTABLISHED then
            local session = self._sessions[event.client_id]
            session.client:start_ping_timer()
        elseif type == DP.Client.Event.CONNECTION_CLOSED or
               type == DP.Client.Event.FREE then
            self._sessions[event.client_id] = nil
        elseif type == DP.Client.Event.EXT_AUTH_RESULT then
            self:_run_handler(self._handle_ext_auth, session, event.message)
        elseif type == DP.Client.Event.EXT_AUTH_ERROR then
            self:_report_error(session, "ext_auth_error", {
                message = event.message,
            })
        end
    end
end

function LoginHandler:on_client_message(event)
    local session = self._sessions[event.client_id]
    if session then
        local msg = event.message
        local type = msg.type
        if type == MSG_TYPE_SERVER_COMMAND then
            self:_run_handler(self._handle_command, session, msg.message)
        elseif type == MSG_TYPE_DISCONNECT then
            if msg.reason == DP.Message.Disconnect.Reason.KICK then
                self:_report_error(session, "ip_ban")
            else
                self:_report_error(session, "disconnect")
            end
        elseif type ~= MSG_TYPE_PING then
            warn(string.format("Unexpected message type %d (%s) in login",
                               type, DP.Message.Type[type]))
        end
    end
end

return LoginHandler
