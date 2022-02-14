/*
 * Copyright (c) 2022 askmeaboutloom
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef DRAWDANCE_UI_H
#define DRAWDANCE_UI_H
#include <dpcommon/common.h>
#include <SDL.h>

#define DP_UI_MOUSE_BUTTON_MIN SDL_BUTTON_LEFT
#define DP_UI_MOUSE_BUTTON_MAX SDL_BUTTON_X2
#define DP_UI_MOUSE_BUTTON_COUNT \
    (DP_UI_MOUSE_BUTTON_MAX - DP_UI_MOUSE_BUTTON_MIN + 1)

#define DP_UI_HELD     (1 << 1)
#define DP_UI_PRESSED  (1 << 2)
#define DP_UI_RELEASED (1 << 3)


typedef struct DP_UserInputs {
    unsigned long long frequency;
    unsigned long long last_frame_time;
    double delta_time;
    int view_width, view_height;
    int mouse_delta_x, mouse_delta_y;
    int mouse_wheel_x, mouse_wheel_y;
    SDL_FingerID current_finger_id;
    bool have_current_finger_id;
    float finger_delta_x, finger_delta_y;
    float finger_pinch;
    SDL_SystemCursor next_cursor_id;
    SDL_Cursor *cursors[SDL_NUM_SYSTEM_CURSORS];
    uint8_t scan_codes[SDL_NUM_SCANCODES];
    uint8_t mouse_buttons[DP_UI_MOUSE_BUTTON_COUNT];
} DP_UserInputs;

void DP_user_inputs_init(DP_UserInputs *inputs);

void DP_user_inputs_dispose(DP_UserInputs *inputs);

void DP_user_inputs_next_frame(DP_UserInputs *inputs);

void DP_user_inputs_handle(DP_UserInputs *inputs, SDL_Event *event);

void DP_user_input_view_dimensions_set(DP_UserInputs *inputs, int view_width,
                                       int view_height);

void DP_user_inputs_cursor_set(DP_UserInputs *inputs, SDL_SystemCursor id);

void DP_user_inputs_render(DP_UserInputs *inputs);


#endif
