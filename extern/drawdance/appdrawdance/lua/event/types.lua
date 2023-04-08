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
local EventTypes <const> = {}

EventTypes.types = {
    "REQUEST_APP_QUIT",
    "REQUEST_GUI_RELOAD",
    "REQUEST_DIALOG_SHOW",
    "REQUEST_SESSION_JOIN",
    "REQUEST_SESSION_LEAVE",
    "REQUEST_SESSION_CANCEL",
    "REQUEST_SESSION_SHOW",
    "REQUEST_SESSION_CLOSE",
    "REQUEST_CHAT_MESSAGE_SEND",
    "LOGIN_ERROR",
    "LOGIN_REQUIRE_CREDENTIALS",
    "LOGIN_PROVIDE_CREDENTIALS",
    "LOGIN_REQUIRE_ROOM",
    "LOGIN_PROVIDE_ROOM",
    "LOGIN_REQUIRE_ROOM_PASSWORD",
    "LOGIN_PROVIDE_ROOM_PASSWORD",
    "CLIENT_CREATE",
    "CLIENT_EVENT",
    "CLIENT_MESSAGE",
    "LAYER_PROPS",
    "SESSION_CREATE",
    "SESSION_DETACH",
    "SESSION_SHOW",
    "SESSION_REMOVE",
    "USER_JOIN",
    "USER_LEAVE",
    "CHAT_MESSAGE_RECEIVE",
}

for _, type in ipairs(EventTypes.types) do
    EventTypes[type] = type
end

setmetatable(EventTypes, {
    __index = function (self, type)
        error(string.format("Unknown event type '%s'", type))
    end,
    __newindex = function ()
        error("Newindex on EventTypes is not allowed")
    end,
})

return EventTypes
