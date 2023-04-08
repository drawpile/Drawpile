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
local EventBus <const> = require("class")("EventBus")

function EventBus:init()
    self._events = {}
    self._handlers_by_type = {}
    self._next_index = 1
end

function EventBus:handle_events()
    -- We'll do a manual while loop, since new events may be added while we're
    -- processing them, mutating the events table and the next index value.
    local events, handlers_by_type = self._events, self._handlers_by_type
    local i = 1
    while i < self._next_index do
        -- Grab the next event and clear it along the way.
        local event = events[i]
        events[i] = nil
        i = i + 1
        -- Call all registered subscribers.
        local type = event.type
        local handlers = handlers_by_type[type]
        if handlers then
            self._handlers_in_progress = handlers
            local data = event.data
            for _, handler in ipairs(handlers) do
                local ok, err = xpcall(handler, DP.error_to_trace, data)
                if not ok then
                    warn(string.format("Error handling %s event: %s", type, err))
                end
            end
        end
    end
    -- Clean up for the next round.
    self._next_index = 1
    self._handlers_in_progress = nil
end

function EventBus:publish(type, data)
    local i = self._next_index
    self._next_index = i + 1
    self._events[i] = {type = type, data = data}
end

function EventBus:subscribe(type, handler)
    local handlers_by_type = self._handlers_by_type
    local handlers = handlers_by_type[type]
    if handlers and handlers == self._handlers_in_progress then
        error("Subscribing to running handlers not implemented")
    elseif not handlers then
        handlers = {}
        handlers_by_type[type] = handlers
    end
    table.insert(handlers, handler)
    return handler
end

function EventBus:unsubscribe(type, handler)
    local handlers = self._handlers_by_type[type]
    if handlers then
        if handlers == self._handlers_in_progress then
            error("Unsubscribing from running handlers not implemented")
        end
        for i = 1, #handlers do
            if handlers[i] == handler then
                table.remove(handlers, i)
                return true
            end
        end
    end
    return false
end

return EventBus
