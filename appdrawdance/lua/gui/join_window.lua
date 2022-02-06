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

local DEFAULT_SCHEME <const> = DP.Client.default_scheme()
local SUPPORTED_SCHEMES <const> = (function ()
    local schemes = DP.Client.supported_schemes()
    for i = 1, #schemes do
        schemes[i] = schemes[i] .. "://"
    end
    return table.concat(schemes, ", ")
end)()

local JoinWindow <const> = require("class")("JoinWindow", "gui.dialog")

function JoinWindow:init(app)
    JoinWindow.__parent.init(self, app)
    self._address = ImGui.String("")
    self:subscribe_method(EventTypes.REQUEST_DIALOG_SHOW)
end

function JoinWindow:on_request_dialog_show(name)
    if name == "session_join" then
        self.open.value = true
    end
end

local function trim(str)
    return string.gsub(string.gsub(str, "^%s+", "", 1), "%s+$", "", 1)
end

function JoinWindow:_trigger_join()
    local url = trim(self._address.value)
    if not string.find(url, "^%w*://") then
        url = DEFAULT_SCHEME .. "://" .. url
    end

    local ok, err = DP.Client.url_valid(url)
    if DP.Client.url_valid(url) then
        self:publish(EventTypes.REQUEST_SESSION_JOIN, {url = url})
        return true
    else
        self.error = {
            key = "url_validation_" .. err,
            supported_schemes = SUPPORTED_SCHEMES,
        }
        return false
    end
end

function JoinWindow:show()
    local T <const> = Translations:for_category("join_window")
    Utils.set_next_window_size_once(500.0, 0.0)
    Utils.center_next_window_once()
    Utils.begin_dialog(T"title##join_window", self.open)

    ImGui.Text(T"lbl_address")
    if ImGui.IsWindowAppearing() then
        ImGui.SetKeyboardFocusHere()
    end
    local enter_pressed = ImGui.InputText("##fld_address", self._address,
                                          ImGuiInputTextFlags.EnterReturnsTrue)
    ImGui.Text(self.error and T(self.error) or "")

    if string.find(self._address.value, "%S") then
        local button_pressed = ImGui.Button(T"##btn_join")
        if (enter_pressed or button_pressed) and self:_trigger_join() then
            self.open.value = false
        end
    else
        ImGui.BeginDisabled()
        ImGui.Button(T"##btn_join")
        ImGui.EndDisabled()
    end

    ImGui.SameLine()
    if ImGui.Button(T"##btn_cancel") then
        self.open.value = false
    end

    ImGui.End()
end

return JoinWindow
