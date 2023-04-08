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

local LicensesWindow <const> = require("class")("LicensesWindow", "gui.dialog")

function LicensesWindow:init(app)
    LicensesWindow.__parent.init(self, app)
    self._licenses_loaded = false
    self:subscribe_method(EventTypes.REQUEST_DIALOG_SHOW)
end

function LicensesWindow:on_request_dialog_show(name)
    if name == "licenses" then
        self.open.value = true
    end
end

function LicensesWindow:_try_load_licenses(T)
    self._licenses_loaded = true
    local ok, result = pcall(require, "licenses")
    if ok then
        self._licenses = result
    else
        self._licenses_load_error = T{
            cache_key = false,
            key = "txt_licenses_load_error",
            error = result,
        }
    end
end

function LicensesWindow:show()
    local T <const> = Translations:for_category("licenses_window")
    if not self._licenses_loaded then
        self:_try_load_licenses(T)
    end

    Utils.set_next_window_size_once(600.0, 400.0)
    Utils.center_next_window_once()
    Utils.begin_dialog(T"title##licenses_window", self.open)

    local licenses = self._licenses
    if licenses then
        ImGui.BeginTabBar("#tbs_licenses", ImGuiTabBarFlags.FittingPolicyScroll)
        for i, license in ipairs(licenses) do
            local name = license.name
            local label = string.format("%s##tab_license%d", name, i)
            if ImGui.BeginTabItem(label) then
                ImGui.Spacing()
                ImGui.PushStyleColor(ImGuiCol.Text, 0xffff9b4b)
                local url = license.url
                ImGui.TextWrapped(url)
                ImGui.PopStyleColor(1)
                if ImGui.IsItemHovered() then
                    ImGui.SetMouseCursor(ImGuiMouseCursor.Hand)
                end
                if ImGui.IsItemClicked(ImGuiMouseButton.Left) then
                    local ok, result = pcall(DP.open_url, url)

                end

                ImGui.Spacing()
                ImGui.TextWrapped(T{
                    cache_key = string.format("license_description%d", i),
                    key = "txt_license_description",
                    name = name,
                    ident = license.ident,
                })
                ImGui.Spacing()
                ImGui.Separator()
                ImGui.Spacing()
                local child_id = string.format("cld_license%d", i)
                if ImGui.BeginChild(child_id) then
                    -- License texts are split up into multiple text elements
                    -- because if they're too long ImGui ends up choking.
                    ImGui.PushStyleVar(ImGuiStyleVar.ItemSpacing, ImVec2:new(0.0, 0.0))
                    for _, text in ipairs(license.texts) do
                        ImGui.TextWrapped(text)
                    end
                    ImGui.PopStyleVar(1)
                end
                ImGui.EndChild()
                ImGui.EndTabItem()
            end
        end
        ImGui.EndTabBar()
    else
        ImGui.TextWrapped(self._licenses_load_error)
    end

    ImGui.End()
end

return LicensesWindow

