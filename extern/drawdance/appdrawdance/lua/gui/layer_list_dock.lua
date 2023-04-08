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
local Translations <const> = require("translations")
local EventTypes <const> = require("event.types")
local Utils <const> = require("gui.utils")

local LayerListDock <const> = require("class")("LayerListDock", "gui.widget")

function LayerListDock:init(app)
    LayerListDock.__parent.init(self, app)
    self._layer_props = {}
    self:subscribe_method(EventTypes.LAYER_PROPS)
end

function LayerListDock:on_layer_props(event)
    self._layer_props = event
end

function LayerListDock:_show_layers(layer_props)
    for i = #layer_props, 1, -1 do
        local l = layer_props[i]
        local children = l.children
        ImGui.Text(string.format("%s %s %d: %s %d%%",
                children and (l.isolated and "Group" or "Pass") or "Layer",
                l.hidden and "X" or "O", l.id, l.title or "(nil)",
                math.floor(l.opacity / 32768.0 * 100.0 + 0.5)))
        if children then
            ImGui.Indent()
            self:_show_layers(children)
            ImGui.Unindent()
        end
    end
end

function LayerListDock:show()
    local T <const> = Translations:for_category("layer_list_dock")
    Utils.set_next_window_size_once(500.0, 100.0)
    Utils.begin_dialog(T"title##layer_list_dock")

    local layer_props = self._layer_props
    if #layer_props > 0 then
        self:_show_layers(layer_props)
    else
        ImGui.Text("No Layers")
    end

    ImGui.End()
end

return LayerListDock
