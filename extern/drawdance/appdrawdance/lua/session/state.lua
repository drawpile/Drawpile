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
local MSG_TYPE_JOIN <const> = DP.Message.Type.JOIN
local MSG_TYPE_LEAVE <const> = DP.Message.Type.LEAVE
local MSG_TYPE_CHAT <const> = DP.Message.Type.CHAT
local MSG_TYPE_PRIVATE_CHAT <const> = DP.Message.Type.PRIVATE_CHAT

local DISCONNECT_REASONS <const> = {
    [DP.Message.Disconnect.Reason.ERROR] = "disconnect_error",
    [DP.Message.Disconnect.Reason.KICK] = "disconnect_kick",
    [DP.Message.Disconnect.Reason.SHUTDOWN] = "disconnect_shutdown",
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
    elseif type == MSG_TYPE_JOIN then
        self._app:publish(EventTypes.USER_JOIN, {
            client_id = self.client_id,
            context_id = msg.context_id,
            name = msg.name,
        })
    elseif type == MSG_TYPE_LEAVE then
        self._app:publish(EventTypes.USER_LEAVE, {
            client_id = self.client_id,
            context_id = msg.context_id,
        })
    elseif type == MSG_TYPE_CHAT then
        self._app:publish(EventTypes.CHAT_MESSAGE_RECEIVE, {
            client_id = self.client_id,
            context_id = msg.context_id,
            shout = msg.is_shout,
            action = msg.is_action,
            pin = msg.is_pin,
            text = msg.text,
        })
    elseif type == MSG_TYPE_PRIVATE_CHAT then
        print("pm") -- TODO
    end
end

return State
