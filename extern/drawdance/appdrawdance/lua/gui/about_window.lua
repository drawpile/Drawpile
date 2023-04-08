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
local Translations <const> = require("translations")
local Utils <const> = require("gui.utils")

local AboutWindow <const> = require("class")("AboutWindow", "gui.dialog")

function AboutWindow:init(app)
    AboutWindow.__parent.init(self, app)
    self:subscribe_method(EventTypes.REQUEST_DIALOG_SHOW)
end

function AboutWindow:on_request_dialog_show(name)
    if name == "about" then
        self.open.value = true
    end
end

function AboutWindow:show()
    local T <const> = Translations:for_category("about_window")
    Utils.set_next_window_size_once(600.0, 0.0)
    Utils.center_next_window_once()
    Utils.begin_dialog(T"title##about_window", self.open)
    ImGui.TextWrapped(T{
        key = "txt_about",
        build_type = DP.build_type(),
        version = DP.version(),
        copyright_years = "2022",
    })
    ImGui.Spacing()
    ImGui.Separator()
    ImGui.Spacing()
    ImGui.TextWrapped(T"txt_disclaimer")
    ImGui.End()
end

return AboutWindow
