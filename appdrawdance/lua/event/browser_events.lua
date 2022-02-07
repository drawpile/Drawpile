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
local Json <const> = require("3rdparty.json.json")

local BrowserEvents <const> = require("class")("BrowserEvents")

function BrowserEvents:init(app)
    self._events = {}
    -- Some messages contain userdata, so they don't go to the browser.
    local excluded_event_types = {
        [EventTypes.CLIENT_MESSAGE] = true,
        [EventTypes.SESSION_CREATE] = true,
    }
    for _, type in ipairs(EventTypes.types) do
        if not excluded_event_types[type] then
            app:subscribe(type, function (event)
                table.insert(self._events, {type, event})
            end)
        end
    end
end

function BrowserEvents:_decode_and_publish(app, payload)
    for _, type_and_event in ipairs(Json.decode(payload)) do
        app:publish(type_and_event[1], type_and_event[2])
    end
end

function BrowserEvents:publish_browser_events(app, payload)
    local ok, err = pcall(self._decode_and_publish, self, app, payload)
    if not ok then
        warn(string.format("Error receiving events from browser: %s", err))
    end
end

function BrowserEvents:_encode_and_send(events)
    DP.send_to_browser(Json.encode(events))
end

function BrowserEvents:send_to_browser()
    local events = self._events
    if #events ~= 0 then
        local ok, err = pcall(self._encode_and_send, self, events)
        self._events = {}
        if not ok then
            error(string.format("Error sending events to browser: %s", err))
        end
    end
end

return BrowserEvents
