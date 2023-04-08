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

local MainMenu <const> = require("class")("MainMenu", "gui.widget")

function MainMenu:_is_leave_enabled()
    local state = self.app:get_current_state()
    return state and state:is_connected()
end

function MainMenu:show()
    local T <const> = Translations:for_category("main_menu")
    if ImGui.BeginMainMenuBar() then
        if ImGui.BeginMenu(T"##mnu_file") then
            if ImGui.MenuItem(T"##mni_file_gui_reload") then
                self:publish(EventTypes.REQUEST_GUI_RELOAD)
            end
            if ImGui.MenuItem(T"##mni_file_quit") then
                self:publish(EventTypes.REQUEST_APP_QUIT)
            end
            ImGui.EndMenu()
        end
        if ImGui.BeginMenu(T"##mnu_session") then
            if ImGui.MenuItem(T"##mni_session_join") then
                self:publish(EventTypes.REQUEST_DIALOG_SHOW, "session_join")
            end

            local state = self.app:get_current_state()
            if state and state:is_connected() then
                if ImGui.MenuItem(T"##mni_session_leave") then
                    self:publish(EventTypes.REQUEST_SESSION_LEAVE, {
                        client_id = state.client_id,
                    })
                end
            else
                ImGui.BeginDisabled()
                ImGui.MenuItem(T"##mni_session_leave")
                ImGui.EndDisabled()
            end
            if state then
                if ImGui.MenuItem(T"##mni_session_close") then
                    self:publish(EventTypes.REQUEST_SESSION_CLOSE, {
                        client_id = state.client_id,
                    })
                end
            else
                ImGui.BeginDisabled()
                ImGui.MenuItem(T"##mni_session_close")
                ImGui.EndDisabled()
            end

            ImGui.EndMenu()
        end
        if ImGui.BeginMenu(T"##mnu_help") then
            if ImGui.MenuItem(T"##mni_help_about") then
                self:publish(EventTypes.REQUEST_DIALOG_SHOW, "about")
            end
            if ImGui.MenuItem(T"##mni_help_licenses") then
                self:publish(EventTypes.REQUEST_DIALOG_SHOW, "licenses")
            end
            ImGui.EndMenu()
        end
        ImGui.EndMainMenuBar()
    end
end

return MainMenu
