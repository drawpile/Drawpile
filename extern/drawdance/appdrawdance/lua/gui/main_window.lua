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

local MAIN_WINDOW_FLAGS = ImGuiWindowFlags.NoResize
                        | ImGuiWindowFlags.NoMove
                        | ImGuiWindowFlags.NoBackground
                        | ImGuiWindowFlags.NoNav
                        | ImGuiWindowFlags.NoDecoration
                        | ImGuiWindowFlags.NoInputs

local DOCK_FLAGS = ImGuiDockNodeFlags.NoDockingInCentralNode
                 | ImGuiDockNodeFlags.PassthruCentralNode
                 | (1 << 14) -- Internal/experimental flag: NoWindowMenuButton

local MainWindow <const> = require("class")("MainWindow", "gui.widget")

function MainWindow:init(app)
    MainWindow.__parent.init(self, app)
    self:create_child("gui.main_menu")
    self._main_tabs = self:create_child("gui.main_tabs")
    self:create_child("gui.about_window")
    self:create_child("gui.join_window")
    self:create_child("gui.licenses_window")
    self:create_child("gui.chat_dock")
    -- self:create_child("gui.layer_list_dock")
    self._connect_dialogs = {}
    self:subscribe_method(EventTypes.CLIENT_CREATE)
    self:subscribe_method(EventTypes.SESSION_DETACH)
    self:subscribe_method(EventTypes.SESSION_CREATE, "on_login_end")
    self:subscribe_method(EventTypes.REQUEST_SESSION_CANCEL, "on_login_end")
end

function MainWindow:on_client_create(event)
    local client_id = event.client_id
    local connect_dialog = self:create_child(ConnectDialog, client_id)
    self._connect_dialogs[client_id] = connect_dialog
end

function MainWindow:_remove_connect_dialog(client_id)
    if self:remove_child(self._connect_dialogs[client_id]) then
        self._connect_dialogs[client_id] = nil
    end
end

function MainWindow:on_session_detach(event)
    if event.reason then
        self:create_child(DisconnectDialog, event.client_id, event.reason,
                            function (dialog) self:remove_child(dialog) end)
    end
end

function MainWindow:on_login_end(event)
    self:_remove_connect_dialog(event.client_id)
end

function MainWindow:show()
    local top = self._main_tabs.height
    ImGui.PushStyleVar(ImGuiStyleVar.WindowBorderSize, 0.0)
    ImGui.PushStyleVar(ImGuiStyleVar.WindowMinSize, ImVec2:new(1.0, 1.0))
    ImGui.PushStyleVar(ImGuiStyleVar.WindowPadding, ImVec2:new(0.0, 0.0))

    local vp = ImGui.GetMainViewport()
    ImGui.SetNextWindowViewport(vp.ID)
    local work_pos = vp.WorkPos
    ImGui.SetNextWindowPos(ImVec2:new(work_pos.x, work_pos.y + top),
                           ImGuiCond.Always)
    local work_size = vp.WorkSize
    ImGui.SetNextWindowSize(ImVec2:new(work_size.x, work_size.y - top),
                            ImGuiCond.Always)

    ImGui.Begin("#main", nil, MAIN_WINDOW_FLAGS)
    ImGui.DockSpace(1, ImVec2:new(0.0, 0.0), DOCK_FLAGS)
    ImGui.End()

    ImGui.PopStyleVar(3)
end

return MainWindow
