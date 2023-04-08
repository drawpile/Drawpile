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
local Widget <const> = require("class")("Widget")

function Widget:init(app)
    self.app = app
    self._widget_alive = true
    self._widget_children = {}
    self._widget_event_handlers = {}
end

function Widget:dispose()
    if self._widget_alive then
        self._widget_alive = false

        local app = self.app
        self.app = nil

        local children = self._widget_children
        self._widget_children = nil
        if children then
            for _, child in ipairs(children) do
                if child then
                    local ok, err = pcall(function ()
                        child:dispose()
                    end)
                    if not ok then
                        warn(string.format("Dispose: %s", err))
                    end
                end
            end
        end

        if app then
            local event_handlers = self._widget_event_handlers
            self._widget_event_handlers = nil
            if event_handlers then
                for _, event_handler in ipairs(event_handlers) do
                    app:unsubscribe(event_handler.type, event_handler.handler)
                end
            end
        end
    else
        print("Widget.dispose: already disposed")
    end
end

function Widget:__gc()
    if self._widget_alive then
        warn("Widget.__gc: cleaning up missing dispose")
        self:dispose()
    end
end

function Widget:prepare()
    local show = self.show
    if show then
        show(self)
    end
    for _, child in ipairs(self._widget_children) do
        child:prepare()
    end
end

function Widget:add_child(child)
    assert(child, "Widget.add_child: no child given")
    table.insert(self._widget_children, child)
    return child
end

function Widget:create_child(child_class, ...)
    if type(child_class) == "string" then
        child_class = require(child_class)
    end
    local child = child_class:new(self.app, ...)
    return self:add_child(child)
end

function Widget:remove_child(child, keep_alive)
    if child then
        local children = self._widget_children
        for i = 1, #children do
            if children[i] == child then
                table.remove(children, i)
                if not keep_alive then
                    child:dispose()
                end
                return true
            end
        end
    end
    return false
end

function Widget:publish(type, data)
    return self.app:publish(type, data)
end

local function save_handler(self, actual_handler)
    table.insert(self._widget_event_handlers, {
        type = type,
        handler = actual_handler,
    })
    return actual_handler
end

function Widget:subscribe(type, handler)
    return save_handler(self, self.app:subscribe(type, handler))
end

function Widget:subscribe_method(type, method)
    return save_handler(self, self.app:subscribe_method(type, self, method))
end

function Widget:unsubscribe(type, handler)
    local app_found = self.app:unsubscribe(type, handler)
    local event_handlers = self._widget_event_handlers
    for i = 1, #event_handlers do
        local event_handler = event_handlers[i]
        if event_handler.type == type and event_handler.handler == handler then
            table.remove(event_handlers, i)
            return app_found, true
        end
    end
    return app_found, false
end


return Widget
