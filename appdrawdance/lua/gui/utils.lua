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
local Utils = {}

local DIALOG_WINDOW_FLAGS = ImGuiWindowFlags.NoCollapse
                          | ImGuiWindowFlags.NoDocking

local function set_next_window_size(width, height, cond)
    local ws = ImGui.GetMainViewport().WorkSize
    return ImGui.SetNextWindowSize(
            ImVec2:new(math.min(width, ws.x), math.min(height, ws.y)), cond)
end

function Utils.set_next_window_size_always(width, height)
    return set_next_window_size(width, height, ImGuiCond.Always)
end

function Utils.set_next_window_size_once(width, height)
    return set_next_window_size(width, height, ImGuiCond.Once)
end

local function center_next_window(cond)
    local work_center = ImGui.GetMainViewport():GetWorkCenter()
    ImGui.SetNextWindowPos(work_center, cond, ImVec2:new(0.5, 0.5))
end

function Utils.center_next_window_always()
    center_next_window(ImGuiCond.Always)
end

function Utils.center_next_window_once()
    center_next_window(ImGuiCond.Once)
end

function Utils.begin_dialog(title, open)
    return ImGui.Begin(title, open, DIALOG_WINDOW_FLAGS)
end

return Utils
