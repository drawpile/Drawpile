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
local State <const> = require("session.state")

local StateHandler <const> = require("class")("StateHandler")

function StateHandler:init(app)
    self._app = app
    self._states = {}
    app:subscribe_method(EventTypes.SESSION_CREATE, self)
    app:subscribe_method(EventTypes.REQUEST_SESSION_LEAVE, self)
    app:subscribe_method(EventTypes.REQUEST_SESSION_SHOW, self)
    app:subscribe_method(EventTypes.REQUEST_SESSION_CLOSE, self)
    app:subscribe_method(EventTypes.CLIENT_EVENT, self)
    app:subscribe_method(EventTypes.CLIENT_MESSAGE, self)
    app:subscribe_method(EventTypes.REQUEST_CHAT_MESSAGE_SEND, self)
end

function StateHandler:_show_session(client_id)
    local state = self._states[client_id]
    if state and self._current_state_id ~= client_id then
        self._current_state_id = client_id
        DP.App.document_set(state.document)
        self._app:publish(EventTypes.SESSION_SHOW, {
            client_id = client_id,
        })
    end
end

function StateHandler:_get_state(client_id)
    local state = self._states[client_id]
    if state then
        return state
    elseif state == false then
        return nil
    else
        state = State:new(self._app, client_id)
        self._states[client_id] = state
        return state
    end
end

function StateHandler:on_session_create(event)
    local state = self:_get_state(event.client_id)
    state:attach_client(event.client)
    self:_show_session(event.client_id)
end

function StateHandler:on_request_session_leave(event)
    local state = self._states[event.client_id]
    if state then
        state:disconnect()
    end
end

function StateHandler:on_request_session_show(event)
    self:_show_session(event.client_id)
end

function StateHandler:on_request_session_close(event)
    local client_id = event.client_id
    local state = self._states[client_id]
    if state then
        state:disconnect()
        self._states[client_id] = false
        if self._current_state_id == client_id then
            self._current_state_id = nil
            DP.App.document_set(nil)
        end
        self._app:publish(EventTypes.SESSION_REMOVE, {
            client_id = client_id,
        })
    end
end

function StateHandler:on_client_event(event)
    local state = self:_get_state(event.client_id)
    if state then
        state:handle_event(event)
    end
end

function StateHandler:on_client_message(event)
    local state = self:_get_state(event.client_id)
    if state then
        state:handle_message(event.message)
    end
end

function StateHandler:on_request_chat_message_send(event)
    local state = self._states[event.client_id]
    if state then
        state:send_chat_message(event.text)
    end
end

function StateHandler:get_states()
    local states = {}
    for _, state in pairs(self._states) do
        if state and state.document then
            table.insert(states, state)
        end
    end
    return states
end

function StateHandler:get_current_state()
    local client_id = self._current_state_id
    return client_id and self._states[client_id]
end

return StateHandler
