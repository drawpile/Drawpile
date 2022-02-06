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

local DisconnectDialog <const> = require("class")("DisconnectDialog", "gui.dialog")

function DisconnectDialog:init(app, client_id, reason, on_close)
    local T <const> = Translations:for_category("disconnect_dialog")
    DisconnectDialog.__parent.init(self, app, true)
    self._client_id = client_id
    self._text = T("txt_reason_" .. reason)
    self._on_close = on_close
end

function DisconnectDialog:show()
    local T <const> = Translations:for_category("disconnect_dialog")

    Utils.set_next_window_size_once(600.0, 400.0)
    Utils.center_next_window_once()
    Utils.begin_dialog(T"title##disconnect_dialog" .. self._client_id, self.open)

    ImGui.TextWrapped(self._text)
    ImGui.Spacing()

    if ImGui.Button(T"##btn_ok") then
        self.open.value = false
    end

    ImGui.End()
end

function DisconnectDialog:prepare()
    DisconnectDialog.__parent.prepare(self)
    if not self.open.value then
        self._on_close(self)
    end
end

return DisconnectDialog
