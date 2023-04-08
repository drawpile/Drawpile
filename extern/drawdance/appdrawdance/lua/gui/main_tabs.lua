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
local ConnectDialog <const> = require("gui.connect_dialog")
local DisconnectDialog <const> = require("gui.disconnect_dialog")
local EventTypes <const> = require("event.types")

local WINDOW_FLAGS <const> = ImGuiWindowFlags.NoDecoration
                           | ImGuiWindowFlags.NoMove
                           | ImGuiWindowFlags.NoDocking

local TAB_BAR_FLAGS <const> = ImGuiTabBarFlags.Reorderable
                            | ImGuiTabBarFlags.FittingPolicyResizeDown

local TAB_ITEM_FLAGS <const> = ImGuiTabItemFlags.UnsavedDocument

local MainTabs <const> = require("class")("MainTabs", "gui.widget")

function MainTabs:init(app)
    MainTabs.__parent.init(self, app)
    self._tab_open = ImGui.Bool()
    self._client_ids_removed = {}
    self.height = 0.0
    self:subscribe_method(EventTypes.SESSION_SHOW)
    self:subscribe_method(EventTypes.SESSION_REMOVE)
end

function MainTabs:on_session_show(event)
    self._client_id_shown = event.client_id
end

function MainTabs:on_session_remove(event)
    table.insert(self._client_ids_removed, event.client_id)
end

function MainTabs:show()
    local vp = ImGui.GetMainViewport()
    ImGui.PushStyleVar(ImGuiStyleVar.WindowBorderSize, 0.0)
    ImGui.PushStyleVar(ImGuiStyleVar.WindowMinSize, ImVec2:new(1.0, 1.0))
    ImGui.PushStyleVar(ImGuiStyleVar.WindowPadding, ImVec2:new(0.0, 0.0))

    ImGui.SetNextWindowViewport(vp.ID)
    ImGui.SetNextWindowPos(vp.WorkPos, ImGuiCond.Always)
    ImGui.SetNextWindowSize(ImVec2:new(vp.WorkSize.x, 0.0), ImGuiCond.Always)
    ImGui.Begin("##main_tabs", nil, WINDOW_FLAGS)

    local client_id_shown = self._client_id_shown
    local tab_open = self._tab_open
    tab_open.value = true

    if ImGui.BeginTabBar("main_tabs_tab_bar", TAB_BAR_FLAGS) then
        local client_ids_removed = self._client_ids_removed
        while #client_ids_removed > 0 do
            local client_id = table.remove(client_ids_removed, 1)
            ImGui.SetTabItemClosed("main_tabs_tab" .. client_id)
        end

        local states = self.app:get_states()
        local current_state = self.app:get_current_state()
        for _, state in ipairs(states) do
            local client_id = state.client_id
            local label = string.format("Session %d##main_tabs_tab%d", client_id, client_id)

            local flags = ImGuiTabItemFlags.None
            if state:is_connected() then
                flags = flags | ImGuiTabItemFlags.UnsavedDocument

            end
            if client_id_shown == client_id then
                flags = flags | ImGuiTabItemFlags.SetSelected
                self._client_id_shown = nil
            end

            if ImGui.BeginTabItem(label, tab_open, flags) then
                if not client_id_shown and state ~= current_state then
                    self:publish(EventTypes.REQUEST_SESSION_SHOW, {
                        client_id = client_id,
                    })
                end
                ImGui.EndTabItem()
            end

            if not tab_open.value then
                self:publish(EventTypes.REQUEST_SESSION_CLOSE, {
                    client_id = client_id,
                })
                tab_open.value = true
            end
        end
        ImGui.EndTabBar()
    end

    self.height = ImGui.GetWindowHeight()
    ImGui.End()
    ImGui.PopStyleVar(3)
end

return MainTabs
