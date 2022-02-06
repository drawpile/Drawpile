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

local DEFAULT_WIDTH <const> = 600.0

local VIEW_STATUS <const> = "_view_status"
local VIEW_CREDENTIALS <const> = "_view_credentials"
local VIEW_ROOMS <const> = "_view_rooms"
local VIEW_ROOM_PASSWORD <const> = "_view_room_password"

local USERNAME_FLAGS <const> = ImGuiInputTextFlags.EnterReturnsTrue

local PASSWORD_FLAGS <const> = ImGuiInputTextFlags.EnterReturnsTrue
                             | ImGuiInputTextFlags.Password

local ROOM_TABLE_FLAGS <const> = ImGuiTableFlags.Reorderable
                               | ImGuiTableFlags.Resizable
                               | ImGuiTableFlags.SizingStretchProp
                               | ImGuiTableFlags.ScrollY

local ROOM_TABLE_SELECT_FLAGS <const> = ImGuiSelectableFlags.SpanAllColumns
                                      | ImGuiSelectableFlags.AllowDoubleClick

local ROOM_PASSWORD_FLAGS = PASSWORD_FLAGS

local STATUS_KEYS <const> = {
    [DP.Client.Event.RESOLVE_ADDRESS_START] = "txt_status_resolve_address_start",
    [DP.Client.Event.RESOLVE_ADDRESS_SUCCESS] = "txt_status_resolve_address_success",
    [DP.Client.Event.RESOLVE_ADDRESS_ERROR] = "txt_status_resolve_address_error",
    [DP.Client.Event.CONNECT_SOCKET_START]  = "txt_status_connect_socket_start",
    [DP.Client.Event.CONNECT_SOCKET_SUCCESS]  = "txt_status_connect_socket_success",
    [DP.Client.Event.CONNECT_SOCKET_ERROR]  = "txt_status_connect_socket_error",
    [DP.Client.Event.SPAWN_RECV_THREAD_ERROR]  = "txt_status_spawn_recv_thread_error",
    [DP.Client.Event.NETWORK_ERROR]  = "txt_status_network_error",
    [DP.Client.Event.CONNECTION_ESTABLISHED]  = "txt_status_connection_established",
}

local ConnectDialog <const> = require("class")("ConnectDialog", "gui.widget")

function ConnectDialog:init(app, client_id)
    local T <const> = Translations:for_category("connect_dialog")
    ConnectDialog.__parent.init(self, app)
    self._client_id = client_id
    self._status = T"txt_status_init"
    self._view = VIEW_STATUS
    self._view_changed = true
    self._center_next_frame = false
    self._last_width = DEFAULT_WIDTH
    self._username = ImGui.String("")
    self._password = ImGui.String("")
    self._password_required = false
    self._rooms = {}
    self._room_password = ImGui.String("")
    self._closed = false
    self:subscribe_method(EventTypes.CLIENT_EVENT)
    self:subscribe_method(EventTypes.LOGIN_ERROR)
    self:subscribe_method(EventTypes.LOGIN_REQUIRE_CREDENTIALS)
    self:subscribe_method(EventTypes.LOGIN_REQUIRE_ROOM)
    self:subscribe_method(EventTypes.LOGIN_REQUIRE_ROOM_PASSWORD)
end

function ConnectDialog:on_client_event(event)
    if event.client_id == self._client_id then
        local T <const> = Translations:for_category("connect_dialog")
        local type, message = event.type, event.message
        if type == DP.Client.Event.CONNECTION_CLOSED or
           type == DP.Client.Event.FREE then
            self._closed = true
        elseif type ~= DP.Client.Event.EXT_AUTH_RESULT and
               type ~= DP.Client.Event.EXT_AUTH_ERROR then
            local key = STATUS_KEYS[type]
            if not key then
                if message then
                    key = "txt_status_unknown_message"
                else
                    key = "txt_status_unknown"
                end
            end
            self._status = T{
                cache_key = false,
                key = key,
                type = type,
                name = DP.Client.Event[type],
                message = message,
            }
        end
        if self._view ~= VIEW_STATUS then
            self._view_changed = true
            self._view = VIEW_STATUS
        end
    end
end

function ConnectDialog:on_login_error(event)
    if event.client_id == self._client_id then
        self._closed = true
        local T <const> = Translations:for_category("connect_dialog")
        local args = {}
        for key, value in pairs(event) do
            args[key] = value
        end
        args.cache_key = false
        args.key = "txt_status_login_error_" .. event.type
        self._status = T(args)
    end
end

function ConnectDialog:on_login_require_credentials(event)
    if event.client_id == self._client_id then
        local T <const> = Translations:for_category("connect_dialog")
        self._view = VIEW_CREDENTIALS
        self._view_changed = true
        if event.ext_auth_url then
            self._status = T{
                cache_key = false,
                key = "txt_status_require_credentials_ext_auth",
                username = self._username.value,
                url = event.ext_auth_url,
            }
            self._password_required = true
        elseif event.allow_guests then
            if event.password_required then
                self._status = T{
                    cache_key = false,
                    key = "txt_status_require_credentials_password_allow_guests",
                    username = self._username.value,
                }
                self._password_required = true
            else
                self._status = T"txt_status_require_credentials_allow_guests"
                self._password_required = false
            end
        else
            self._status = T"txt_status_require_credentials_no_guests"
            self._password_required = true
        end
    end
end

function ConnectDialog:on_login_require_room(event)
    if event.client_id == self._client_id then
        local T <const> = Translations:for_category("connect_dialog")
        self._status = T"txt_status_require_room"
        self._view = VIEW_ROOMS
        self._view_changed = true
        self._rooms = event.rooms
        -- The currently selected room might have gone away.
        local found_room_id = false
        if self._selected_room_id then
            for _, room in ipairs(self._rooms) do
                if self._selected_room_id == room.id then
                    found_room_id = true -- Still there, keep it selected.
                    break
                end
            end
        end
        if not found_room_id then
            self._selected_room_id = nil
        end
    end
end

function ConnectDialog:on_login_require_room_password(event)
    if event.client_id == self._client_id then
        local T <const> = Translations:for_category("connect_dialog")
        self._status = T"txt_status_require_room_password"
        self._view = VIEW_ROOM_PASSWORD
        self._view_changed = true
    end
end

function ConnectDialog:_view_status(T)
    ImGui.Spacing()
    return VIEW_STATUS
end

function ConnectDialog:_view_credentials(T)
    local password_required = self._password_required

    local focus
    if self._view_changed then
        if password_required and string.find(self._username.value, "%S") then
            focus = "password"
        else
            focus = "username"
        end
    else
        focus = nil
    end

    if focus == "username" then ImGui.SetKeyboardFocusHere() end
    local enter_pressed =
        ImGui.InputText(T"##fld_username", self._username, USERNAME_FLAGS)

    if password_required then
        if focus == "password" then ImGui.SetKeyboardFocusHere() end
        if ImGui.InputText(T"##fld_password", self._password, PASSWORD_FLAGS) then
            enter_pressed = true
        end
    end

    local next_view = VIEW_CREDENTIALS
    local username_entered = string.find(self._username.value, "%S")
    local password_entered = string.find(self._password.value, "%S")
    if username_entered and (password_entered or not password_required) then
        local button_pressed = ImGui.Button(T"##btn_ok")
        if button_pressed or enter_pressed then
            next_view = VIEW_STATUS
            self._status = T"txt_status_login"
            self:publish(EventTypes.LOGIN_PROVIDE_CREDENTIALS, {
                client_id = self._client_id,
                username = self._username.value,
                password = password_required and self._password.value or nil,
            })
        end
    else
        ImGui.BeginDisabled()
        ImGui.Button(T"##btn_ok")
        ImGui.EndDisabled()
    end

    ImGui.SameLine()
    return next_view
end

function ConnectDialog:_view_rooms(T)
    local double_clicked = false
    if ImGui.BeginTable("##connect_dialog_rooms" .. self._client_id, 4,
                        ROOM_TABLE_FLAGS, ImVec2:new(0.0, 400.0)) then
        ImGui.TableSetupColumn(T"hdr_title", 0, 5.0)
        ImGui.TableSetupColumn(T"hdr_founder", 0, 3.0)
        ImGui.TableSetupColumn(T"hdr_users", 0, 1.0)
        ImGui.TableSetupColumn(T"hdr_flags", 0, 3.0)
        ImGui.TableHeadersRow()
        for _, room in ipairs(self._rooms) do
            ImGui.TableNextRow()
            ImGui.TableNextColumn()
            local label
            local room_id = room.id
            local alias = room.alias
            if alias and alias ~= "" then
                label = string.format("%s [%s]##%s", room.title, alias, room_id)
            else
                label = string.format("%s##%s", room.title, room_id)
            end
            if ImGui.Selectable(label, self._selected_room_id == room_id,
                                ROOM_TABLE_SELECT_FLAGS) then
                self._selected_room_id = room_id
                if ImGui.IsMouseDoubleClicked(ImGuiMouseButton.Left) then
                    double_clicked = true
                end
            end
            ImGui.TableNextColumn()
            ImGui.Text(room.founder or "")
            ImGui.TableNextColumn()
            ImGui.Text(room.users or "0")
            ImGui.TableNextColumn()
            local flags = {}
            if room.closed then table.insert(flags, "closed") end
            if room.nsfm then table.insert(flags, "nsfm") end
            if room.password then table.insert(flags, "password") end
            if #flags ~= 0 then
                ImGui.Text(T{
                    cache_key = "col_flags" .. table.concat(flags, "_"),
                    key = "col_flags",
                    flags = flags,
                })
            end
        end
        ImGui.EndTable()
    end

    local next_view = VIEW_ROOMS
    if self._selected_room_id then
        local button_pressed = ImGui.Button(T"##btn_join")
        if button_pressed or double_clicked then
            next_view = VIEW_STATUS
            self._status = T"txt_status_join"
            self:publish(EventTypes.LOGIN_PROVIDE_ROOM, {
                client_id = self._client_id,
                room_id = self._selected_room_id,
            })
        end
    else
        ImGui.BeginDisabled()
        ImGui.Button(T"##btn_join")
        ImGui.EndDisabled()
    end

    ImGui.SameLine()
    return next_view
end

function ConnectDialog:_view_room_password(T)
    if self._view_changed then ImGui.SetKeyboardFocusHere() end
    local enter_pressed = ImGui.InputText(
            T"##fld_room_password", self._room_password, ROOM_PASSWORD_FLAGS)

    local next_view = VIEW_ROOM_PASSWORD
    local room_password_entered = string.find(self._room_password.value, "%S")
    if room_password_entered then
        local button_pressed = ImGui.Button(T"##btn_ok")
        if button_pressed or enter_pressed then
            next_view = VIEW_STATUS
            self._status = T"txt_status_join"
            self:publish(EventTypes.LOGIN_PROVIDE_ROOM_PASSWORD, {
                client_id = self._client_id,
                room_password = self._room_password.value,
            })
        end
    else
        ImGui.BeginDisabled()
        ImGui.Button(T"##btn_ok")
        ImGui.EndDisabled()
    end

    ImGui.SameLine()
    return next_view
end

function ConnectDialog:show()
    local T <const> = Translations:for_category("connect_dialog")
    local client_id = self._client_id

    if self._center_next_frame then
        self._center_next_frame = false
        Utils.center_next_window_always()
    end

    local status, last_status = self._status, self._last_status
    self._last_status = status
    if self._view_changed then
        Utils.set_next_window_size_always(DEFAULT_WIDTH, 0.0)
        self._center_next_frame = true
    elseif last_status and status ~= last_status then
        Utils.set_next_window_size_always(self._last_width, 0.0)
    end

    Utils.begin_dialog(T"title##connect_dialog" .. client_id)
    ImGui.TextWrapped(status)

    local next_view = self[self._view](self, T)
    self._view_changed = next_view ~= self._view
    self._view = next_view

    if ImGui.Button(self._closed and T"##btn_close" or T"##btn_cancel") then
        self:publish(EventTypes.REQUEST_SESSION_CANCEL, {
            client_id = self._client_id,
        })
    end

    self._last_width = ImGui.GetWindowWidth()
    ImGui.End()
end

return ConnectDialog
