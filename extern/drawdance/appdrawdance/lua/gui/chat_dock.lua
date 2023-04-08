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

local SHOUT_FLAG <const> = 1 << 0
local ACTION_FLAG <const> = 1 << 1
local PIN_FLAG <const> = 1 << 2
local JOIN_FLAG <const> = 1 << 9
local LEAVE_FLAG <const> = 1 << 10

local ChatDock <const> = require("class")("ChatDock", "gui.widget")

function ChatDock:init(app)
    ChatDock.__parent.init(self, app)
    self._chat_sessions = {}
    self._height = 500.0
    self._field_height = 10.0
    self._focus_field = false
    self._should_scroll = true
    self._last_scroll_max_y = -1.0
    self:subscribe_method(EventTypes.SESSION_SHOW)
    self:subscribe_method(EventTypes.SESSION_REMOVE)
    self:subscribe_method(EventTypes.USER_JOIN)
    self:subscribe_method(EventTypes.USER_LEAVE)
    self:subscribe_method(EventTypes.CHAT_MESSAGE_RECEIVE)
end

function ChatDock:_get_chat_session(client_id)
    local chat_session = self._chat_sessions[client_id]
    if not chat_session then
        chat_session = {
            client_id = client_id,
            next_chat_message_id = 1,
            users = {},
            messages = {},
            input = ImGui.String(""),
        }
        self._chat_sessions[client_id] = chat_session
    end
    return chat_session
end

function ChatDock:on_session_show(event)
    self:_get_chat_session(event.client_id)
    self._should_scroll = true
end

function ChatDock:on_session_remove(event)
    self._chat_sessions[event.client_id] = nil
end

local function get_user_name(chat_session, context_id)
    local user = chat_session.users[context_id]
    return user and user.name or "#" .. context_id
end

local function add_chat_message(chat_session, context_id, text, flags)
    local chat_message_id = chat_session.next_chat_message_id
    chat_session.next_chat_message_id = chat_message_id + 1
    local cache_key = string.format(
            "chat_message#%d#%d", chat_session.client_id, chat_message_id)
    table.insert(chat_session.messages, {
        chat_message_id = chat_message_id,
        cache_key = cache_key,
        name = get_user_name(chat_session, context_id),
        text = text,
        flags = flags or 0,
    })
end

function ChatDock:on_user_join(event)
    local chat_session = self:_get_chat_session(event.client_id)
    local context_id = event.context_id
    chat_session.users[context_id] = {
        context_id = context_id,
        name = event.name,
    }
    add_chat_message(chat_session, context_id, nil, JOIN_FLAG)
end

function ChatDock:on_user_leave(event)
    local chat_session = self:_get_chat_session(event.client_id)
    local context_id = event.context_id
    add_chat_message(chat_session, context_id, nil, LEAVE_FLAG)
    chat_session.users[context_id] = nil
end

function ChatDock:on_chat_message_receive(event)
    local chat_session = self:_get_chat_session(event.client_id)
    local flags = (event.shout and SHOUT_FLAG or 0)
                | (event.action and ACTION_FLAG or 0)
                | (event.pin and PIN_FLAG or 0)
    add_chat_message(chat_session, event.context_id, event.text, flags)
end

local function get_user_list(users)
    local user_list = {}
    for context_id, user in pairs(users) do
        table.insert(user_list, user)
    end
    table.sort(user_list, function (a, b)
        return string.lower(a.name) < string.lower(b.name)
    end)
    return user_list
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
            local chat_session = self:_get_chat_session(client_id)
            local users = chat_session.users
            local messages = chat_session.messages
            local input = chat_session.input

            local padding = 4.0 + 22.0 -- FIXME: get the padding properly
            local chat_height = self._height - self._field_height - padding
            if ImGui.BeginChild("chat_dock_messages", ImVec2:new(0.0, chat_height)) then
                local scroll_max_y = ImGui.GetScrollMaxY()
                if self._should_scroll or scroll_max_y ~= self._last_scroll_max_y then
                    ImGui.SetScrollY(scroll_max_y)
                    self._should_scroll = false
                end
                self._last_scroll_max_y = scroll_max_y

                for _, chat_message in ipairs(messages) do
                    local flags = chat_message.flags
                    if (flags & JOIN_FLAG) ~= 0 then
                        ImGui.TextWrapped(T{
                            key = "join",
                            cache_key = chat_message.cache_key,
                            name = chat_message.name,
                        })
                    elseif (flags & LEAVE_FLAG) ~= 0 then
                        ImGui.TextWrapped(T{
                            key = "leave",
                            cache_key = chat_message.cache_key,
                            name = chat_message.name,
                        })
                    else
                        ImGui.TextWrapped(T{
                            key = (flags & ACTION_FLAG) ~= 0
                                    and "action" or "message",
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
                self:publish(EventTypes.REQUEST_CHAT_MESSAGE_SEND, {
                    client_id = state.client_id,
                    text = input.value,
                })
                input.value = ""
                self._focus_field = true
            end

            ImGui.TableNextColumn()
            local users_height = self._height - padding
            if ImGui.BeginChild("chat_dock_users", ImVec2:new(0.0, users_height)) then
                for _, user in ipairs(get_user_list(users)) do
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
