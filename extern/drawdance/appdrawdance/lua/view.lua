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
local View <const> = require("class")("View")

local MOVE_MULTIPLIER <const> = 50.0
local ROTATION_MULTIPLIER <const> = 5.0
local MIN_ZOOM_PERCENT <const> = 10
local MAX_ZOOM_PERCENT <const> = 1000
local REF_DELTA_TIME <const> = 1.0 / 60.0
local REF_INTERP <const> = 0.5

local function clamp(min, value, max)
    return math.max(min, math.min(value, max))
end

function View:init()
    self._dragging_space = false
    self._dragging_left_mouse = false
    self._dragging_middle_mouse = false
    self._mouse_delta_x = 0.0
    self._mouse_delta_y = 0.0
    self._finger_delta_x = 0.0
    self._finger_delta_y = 0.0
    self._last_view_state = nil
end

function View:_handle_mouse_inputs(mouse_captured, op)
    self._mouse_delta_x, self._mouse_delta_y = DP.UI.get_mouse_delta_xy()
    self._finger_delta_x, self._finger_delta_y = DP.UI.get_finger_delta_xy()

    if not mouse_captured then
        local wheel = DP.UI.get_mouse_wheel_y()
        if wheel < 0.0 then
            op.zoom = op.zoom - 1
        elseif wheel > 0.0 then
            op.zoom = op.zoom + 1
        end

        op.draggable = true

        local finger = self._finger_delta_x ~= 0 or self._finger_delta_y ~= 0
        local function check_drag(key, button)
            self[key] = (not finger and DP.UI.mouse_pressed(button)) or (
                    self[key] and DP.UI.mouse_held(button))
            if self[key] then
                op.drag = true
            end
        end
        check_drag("_dragging_left_mouse", DP.UI.MouseButton.LEFT)
        check_drag("_dragging_middle_mouse", DP.UI.MouseButton.MIDDLE)
    end
end

function View:_handle_keyboard_inputs(keyboard_captured, op)
    if not keyboard_captured then
        local key_mods = DP.UI.get_key_mods()
        if key_mods == DP.UI.KeyMod.NONE then
            if DP.UI.scan_pressed(DP.UI.Scancode.LEFT) then
                op.move_x = op.move_x + 1
            end
            if DP.UI.scan_pressed(DP.UI.Scancode.RIGHT) then
                op.move_x = op.move_x - 1
            end
            if DP.UI.scan_pressed(DP.UI.Scancode.UP) then
                op.move_y = op.move_y + 1
            end
            if DP.UI.scan_pressed(DP.UI.Scancode.DOWN) then
                op.move_y = op.move_y - 1
            end
        elseif key_mods == DP.UI.KeyMod.CTRL then
            if DP.UI.scan_pressed(DP.UI.Scancode.LEFT) then
                op.rotate = op.rotate - 1
            end
            if DP.UI.scan_pressed(DP.UI.Scancode.RIGHT) then
                op.rotate = op.rotate + 1
            end
            if DP.UI.scan_pressed(DP.UI.Scancode.UP)
                    or DP.UI.scan_pressed(DP.UI.Scancode.EQUALS) then
                op.zoom = op.zoom + 1
            end
            if DP.UI.scan_pressed(DP.UI.Scancode.DOWN)
                    or DP.UI.scan_pressed(DP.UI.Scancode.MINUS) then
                op.zoom = op.zoom - 1
            end
            if DP.UI.scan_pressed(DP.UI.Scancode.R) then
                op.reset_move = true
                op.reset_zoom = true
                op.reset_rotate = true
            end
        end

        self._dragging_space =
                (op.draggable and DP.UI.scan_pressed(DP.UI.Scancode.SPACE)) or
                (self._dragging_space and DP.UI.scan_held(DP.UI.Scancode.SPACE))
        if self._dragging_space then
            op.drag = true
        end
    end
end

function View:_handle_drag(use_imgui_cursor, op)
    if op.drag then
        op.drag_x = op.drag_x + self._mouse_delta_x
        op.drag_y = op.drag_y + self._mouse_delta_y
        if use_imgui_cursor then
            ImGui.SetMouseCursor(ImGuiMouseCursor.ResizeAll)
        else
            DP.UI.set_cursor(DP.UI.SystemCursor.SIZEALL)
        end
    elseif op.draggable then
        if use_imgui_cursor then
            ImGui.SetMouseCursor(ImGuiMouseCursor.Hand)
        else
            DP.UI.set_cursor(DP.UI.SystemCursor.HAND)
        end
    end
end

function View:_handle_finger_inputs(mouse_captured, op)
    if not mouse_captured then
        local fx, fy = self._finger_delta_x, self._finger_delta_y
        local fp = DP.UI.get_finger_pinch()
        if fx ~= 0 or fy ~= 0 or fp ~= 0 then
            local w, h = DP.UI.get_view_width_height()
            -- These multiplications are just guesses that seem to work.
            -- There's probably a proper way to translate these?
            op.drag_x = op.drag_x + fx * w * 0.25
            op.drag_y = op.drag_y + fy * h * 0.25
            op.pinch = fp * (w + h) * 0.25
        end
    end
end

function View:_apply_inputs(view_state, move_x, move_y, drag_x, drag_y, zoom,
                            pinch, rotate, reset_move, reset_zoom, reset_rotate)
    local x, y = view_state.x, view_state.y
    local zp = view_state.zoom_percent
    local r = view_state.rotation_in_degrees

    if reset_zoom then
        zp = 100
    else
        local zoom_scale
        if (zoom > 0 and zp >= 100) or
           (zoom < 0 and zp > 100) then
            zoom_scale = 50
        else
            zoom_scale = 10
        end
        zp = zp + zoom * zoom_scale + pinch
        zp = clamp(MIN_ZOOM_PERCENT, zp, MAX_ZOOM_PERCENT)
    end

    if reset_move then
        x = 0.0
        y = 0.0
    else
        local scale = zp / 100.0
        x = x + ((move_x * MOVE_MULTIPLIER + drag_x) / scale)
        y = y + ((move_y * MOVE_MULTIPLIER + drag_y) / scale)
    end

    if reset_rotate then
        r = 0.0
    else
        r = r + rotate * ROTATION_MULTIPLIER
    end

    view_state.x, view_state.y = x, y
    view_state.zoom_percent = zp
    view_state.rotation_in_degrees = r
end

function View:_apply_transform(view_state)
    local x, y, scale, rotation_in_radians = DP.App.canvas_renderer_transform()
    local interp
    if view_state == self._last_view_state then
        -- This should make it framerate-independent? The idea is that it
        -- interpolates by 0.5 every frame at 60 fps, adjusting according
        -- to the actual delta time and clamping to a value between 0 and 1.
        local delta_time = DP.UI.get_delta_time()
        interp = clamp(0.0, REF_INTERP * (REF_DELTA_TIME / delta_time), 1.0)
    else
        interp = 1.0
    end

    local target_x, target_y = view_state.x, view_state.y
    local delta_pos = math.sqrt((target_x - x)^2.0 + (target_y - y)^2.0)
    if delta_pos < 0.01 or interp == 1.0 then
        x = target_x
        y = target_y
    else
        x = x + (target_x - x) * interp
        y = y + (target_y - y) * interp
    end

    local target_scale = view_state.zoom_percent / 100.0
    local delta_scale = target_scale - scale
    if math.abs(delta_scale) < 0.01 or interp == 1.0 then
        scale = target_scale
    else
        scale = scale + delta_scale * interp
    end

    local target_rotation = math.rad(view_state.rotation_in_degrees)
    local delta_rotation = target_rotation - rotation_in_radians
    if math.abs(delta_rotation) < 0.01 or interp == 1.0 then
        rotation_in_radians = target_rotation
    else
        rotation_in_radians = rotation_in_radians + delta_rotation * interp
    end

    DP.App.canvas_renderer_transform_set(x, y, scale, rotation_in_radians)
end

function View:handle_inputs(keyboard_captured, mouse_captured, use_imgui_cursor,
            view_state)
    local op = {
        move_x = 0,
        move_y = 0,
        drag_x = 0.0,
        drag_y = 0.0,
        rotate = 0,
        zoom = 0,
        pinch = 0.0,
        reset_move = false,
        reset_zoom = false,
        reset_rotate = false,
        draggable = false,
        drag = false,
    }
    self:_handle_mouse_inputs(mouse_captured, op)
    self:_handle_keyboard_inputs(keyboard_captured, op)
    self:_handle_drag(use_imgui_cursor, op)
    self:_handle_finger_inputs(mouse_captured, op)
    self:_apply_inputs(view_state, op.move_x, op.move_y, op.drag_x, op.drag_y,
                       op.zoom, op.pinch, op.rotate, op.reset_move,
                       op.reset_zoom, op.reset_rotate)
    self:_apply_transform(view_state)
    self._last_view_state = view_state
end

return View
