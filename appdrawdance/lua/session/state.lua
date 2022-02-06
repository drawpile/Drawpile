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
local EventTypes <const> = require("event.types")

local MSG_TYPE_DISCONNECT <const> = DP.Message.Type.DISCONNECT
local MSG_TYPE_USER_JOIN <const> = DP.Message.Type.USER_JOIN
local MSG_TYPE_USER_LEAVE <const> = DP.Message.Type.USER_LEAVE
local MSG_TYPE_CHAT <const> = DP.Message.Type.CHAT
local MSG_TYPE_PRIVATE_CHAT <const> = DP.Message.Type.PRIVATE_CHAT

local SHOUT_FLAG <const> = 1 << 0
local ACTION_FLAG <const> = 1 << 1
local PIN_FLAG <const> = 1 << 2
local JOIN_FLAG <const> = 1 << 9
local LEAVE_FLAG <const> = 1 << 10

local DISCONNECT_REASONS <const> = {
    [DP.Message.Disconnect.Reason.ERROR] = "disconnect_error",
    [DP.Message.Disconnect.Reason.KICK] = "disconnect_kick",
    [DP.Message.Disconnect.Reason.SHUTDOWN] = "disconnect_shutdown",
}

local ChatMessage <const> = {
    __index = function (self, key)
        if key == "is_shout" then
            return (self.flags & SHOUT_FLAG) ~= 0
        elseif key == "is_action" then
            return (self.flags & ACTION_FLAG) ~= 0
        elseif key == "is_pin" then
            return (self.flags & PIN_FLAG) ~= 0
        elseif key == "is_join" then
            return (self.flags & JOIN_FLAG) ~= 0
        elseif key == "is_leave" then
            return (self.flags & LEAVE_FLAG) ~= 0
        else
            return nil
        end
    end,
}


local State <const> = require("class")("State")

function State:init(app, client_id)
    self._app = app
    self.client_id = client_id
    self._next_chat_message_id = 1
    self.users = {}
    self.chat_messages = {}
    self.view_state = {
        x = 0.0,
        y = 0.0,
        zoom_percent = 100,
        rotation_in_degrees = 0.0,
    }
end

function State:attach_client(client)
    self._client = client
    self.document = client.document
end

function State:is_connected()
    return self._client and true or false
end

function State:disconnect()
    if self:is_connected() then
        local msg = DP.Message.Disconnect:new()
        self._client:send(msg)
    end
end

function State:send_chat_message(text)
    local client = self._client
    if client then
        client:send(DP.Message.Chat:new(text))
    else
        error(string.format(
                "Error sending chat message: client %d not connected",
                self.client_id))
    end
end

function State:get_user_list()
    local user_list = {}
    for context_id, user in pairs(self.users) do
        table.insert(user_list, user)
    end
    table.sort(user_list, function (a, b)
        return string.lower(a.name) < string.lower(b.name)
    end)
    return user_list
end

function State:_get_user_name(context_id)
    local user = self.users[context_id]
    return user and user.name or "#" .. context_id
end

function State:_add_chat_message(context_id, text, flags)
    if not flags then
        flags = 0
    end
    local id = self._next_chat_message_id
    self._next_chat_message_id = id + 1
    table.insert(self.chat_messages, setmetatable({
        id = id,
        cache_key = "chat_message#" .. id,
        context_id = context_id,
        name = self:_get_user_name(context_id),
        text = text,
        flags = flags or 0,
    }, ChatMessage))
end

function State:_detach_state(reason, disconnect)
    local client = self._client
    if client then
        self._client = nil
        if disconnect then
            client:terminate()
        end
        self._app:publish(EventTypes.SESSION_DETACH, {
            client_id = self.client_id,
            reason = reason,
        })
    end
end

function State:handle_event(event)
    local type = event.type
    if type == DP.Client.Event.CONNECTION_CLOSING then
        self:_detach_state(nil, true)
    elseif type == DP.Client.Event.CONNECTION_CLOSED then
        self:_detach_state("connection_closed", false)
    elseif type == DP.Client.Event.FREE then
        self:_detach_state("client_free", false)
    elseif type == DP.Client.Event.NETWORK_ERROR then
        self:_detach_state("network_error", true)
    elseif type == DP.Client.Event.SEND_ERROR then
        self:_detach_state("send_error", true)
    elseif type == DP.Client.Event.RECV_ERROR then
        self:_detach_state("recv_error", true)
    end
end

function State:handle_message(msg)
    local type = msg.type
    if type == MSG_TYPE_DISCONNECT then
        local reason = DISCONNECT_REASONS[msg.reason] or "disconnect_other"
        self:_detach_state(reason, true)
    elseif type == MSG_TYPE_USER_JOIN then
        local context_id = msg.context_id
        local name = msg.name
        self.users[context_id] = {
            context_id = context_id,
            name = name,
        }
        self:_add_chat_message(context_id, nil, JOIN_FLAG)
    elseif type == MSG_TYPE_USER_LEAVE then
        local context_id = msg.context_id
        self:_add_chat_message(context_id, nil, LEAVE_FLAG)
        self.users[context_id] = nil
    elseif type == MSG_TYPE_CHAT then
        local flags = (msg.is_shout and SHOUT_FLAG or 0)
                    | (msg.is_action and ACTION_FLAG or 0)
                    | (msg.is_pin and PIN_FLAG or 0)
        self:_add_chat_message(msg.context_id, msg.text, flags)
    elseif type == MSG_TYPE_PRIVATE_CHAT then
        print("pm") -- TODO
    end
end

return State
