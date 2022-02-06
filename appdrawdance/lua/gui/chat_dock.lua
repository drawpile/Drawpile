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

local CHAT_WINDOW_FLAGS <const> = ImGuiWindowFlags.NoCollapse
                                | ImGuiWindowFlags.NoScrollbar

local CHAT_TABLE_FLAGS <const> = ImGuiTableFlags.Reorderable
                               | ImGuiTableFlags.Resizable
                               | ImGuiTableFlags.SizingStretchProp
                               | ImGuiTableFlags.NoBordersInBody

local INPUT_FLAGS <const> = ImGuiInputTextFlags.EnterReturnsTrue

local ChatDock <const> = require("class")("ChatDock", "gui.widget")

function ChatDock:init(app)
    ChatDock.__parent.init(self, app)
    self._inputs = {}
    self._height = 500.0
    self._field_height = 10.0
    self._focus_field = false
    self._should_scroll = true
    self._last_scroll_max_y = -1.0
    self:subscribe_method(EventTypes.SESSION_SHOW)
    self:subscribe_method(EventTypes.SESSION_REMOVE)
end

function ChatDock:on_session_show(event)
    self._should_scroll = true
end

function ChatDock:on_session_remove(event)
    local client_id = event.client_id
    self._inputs[client_id] = nil
end

function ChatDock:show()
    local T <const> = Translations:for_category("chat_dock")
    Utils.set_next_window_size_once(600.0, 400.0)
    ImGui.Begin(T"title##chat_dock", nil, CHAT_WINDOW_FLAGS)

    if ImGui.BeginTable("##chat_table", 2, CHAT_TABLE_FLAGS) then
        ImGui.TableSetupColumn(T"hdr_messages", 0, 4.0)
        ImGui.TableSetupColumn(T"hdr_users", 0, 1.0)
        ImGui.TableHeadersRow()
        ImGui.TableNextRow()
        ImGui.TableNextColumn()

        local state = self.app:get_current_state()
        if state then
            local client_id = state.client_id
            if not self._inputs[client_id] then
                self._inputs[client_id] = ImGui.String("")
            end
            local input = self._inputs[client_id]

            local padding = 4.0 + 22.0 -- FIXME: get the padding properly
            local chat_height = self._height - self._field_height - padding
            if ImGui.BeginChild("chat_dock_messages", ImVec2:new(0.0, chat_height)) then
                local scroll_max_y = ImGui.GetScrollMaxY()
                if self._should_scroll or scroll_max_y ~= self._last_scroll_max_y then
                    ImGui.SetScrollY(scroll_max_y)
                    self._should_scroll = false
                end
                self._last_scroll_max_y = scroll_max_y

                for _, chat_message in ipairs(state.chat_messages) do
                    if chat_message.is_join then
                        ImGui.TextWrapped(T{
                            key = "join",
                            cache_key = chat_message.cache_key,
                            name = chat_message.name,
                        })
                    elseif chat_message.is_leave then
                        ImGui.TextWrapped(T{
                            key = "leave",
                            cache_key = chat_message.cache_key,
                            name = chat_message.name,
                        })
                    else
                        ImGui.TextWrapped(T{
                            key = chat_message.is_action and "action" or "message",
                            cache_key = chat_message.cache_key,
                            name = chat_message.name,
                            text = chat_message.text,
                        })
                    end
                end
            end
            ImGui.Spacing()
            ImGui.EndChild()

            local enter_pressed, field_height, input_text
            if state:is_connected() then
                if self._focus_field then
                    ImGui.SetKeyboardFocusHere()
                end
                enter_pressed = ImGui.InputText("##fld_input", input, INPUT_FLAGS)
                field_height = ImGui.GetItemRectSize().y
                input_text = input.value
            else
                ImGui.Text(T"txt_not_connected")
                field_height = ImGui.GetItemRectSize().y
                input_text = ""
            end
            self._focus_field = false

            ImGui.SameLine()
            local button_pressed, button_height
            local has_input_text = string.find(input_text, "%S")
            if has_input_text then
                button_pressed = ImGui.Button("Send")
                button_height = ImGui.GetItemRectSize().y
            else
                ImGui.BeginDisabled()
                ImGui.Button("Send")
                button_height = ImGui.GetItemRectSize().y
                ImGui.EndDisabled()
            end
            self._field_height = math.max(field_height, button_height)

            if has_input_text and (enter_pressed or button_pressed) then
                state:send_chat_message(input.value)
                input.value = ""
                self._focus_field = true
            end

            ImGui.TableNextColumn()
            local users_height = self._height - padding
            if ImGui.BeginChild("chat_dock_users", ImVec2:new(0.0, users_height)) then
                for _, user in ipairs(state:get_user_list()) do
                    ImGui.Text(user.name)
                end
            end
            ImGui.EndChild()
        else
            ImGui.Text(T"txt_no_state")
            ImGui.TableNextColumn()
        end

        ImGui.EndTable()
    end

    local rmin = ImGui.GetWindowContentRegionMin()
    local rmax = ImGui.GetWindowContentRegionMax()
    self._height = rmax.y - rmin.y
    ImGui.End()
end

return ChatDock
