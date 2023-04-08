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
#include "ui.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <SDL.h>
#include <limits.h>

static_assert(SDL_BUTTON_LEFT == 1, "SDL_BUTTON_LEFT == 1");
static_assert(SDL_BUTTON_MIDDLE == 2, "SDL_BUTTON_MIDDLE == 2");
static_assert(SDL_BUTTON_RIGHT == 3, "SDL_BUTTON_RIGHT == 3");
static_assert(SDL_BUTTON_X1 == 4, "SDL_BUTTON_X1 == 4");
static_assert(SDL_BUTTON_X2 == 5, "SDL_BUTTON_X2 == 5");

#define FALLBACK_DELTA_TIME (1.0 / 60.0)


static void init_cursors(SDL_Cursor **cursors)
{
    for (SDL_SystemCursor id = 0; id < SDL_NUM_SYSTEM_CURSORS; ++id) {
        SDL_Cursor *cursor = SDL_CreateSystemCursor(id);
        if (cursor) {
            cursors[id] = cursor;
        }
        else {
            DP_warn("Error loading system cursor %d: %s", (int)id,
                    SDL_GetError());
        }
    }
}

void DP_user_inputs_init(DP_UserInputs *inputs)
{
    *inputs = (DP_UserInputs){SDL_GetPerformanceFrequency(),
                              0,
                              0.0,
                              0,
                              0,
                              0,
                              0,
                              0,
                              0,
                              LLONG_MIN,
                              false,
                              0.0f,
                              0.0f,
                              0.0f,
                              SDL_NUM_SYSTEM_CURSORS,
                              {0},
                              {0},
                              {0}};
    init_cursors(inputs->cursors);
}

static void dispose_cursors(SDL_Cursor **cursors)
{
    for (SDL_SystemCursor id = 0; id < SDL_NUM_SYSTEM_CURSORS; ++id) {
        SDL_Cursor *cursor = cursors[id];
        if (cursor) {
            SDL_FreeCursor(cursor);
        }
    }
}

void DP_user_inputs_dispose(DP_UserInputs *inputs)
{
    dispose_cursors(inputs->cursors);
}


static void calculate_delta_time(DP_UserInputs *inputs)
{
    unsigned long long now = SDL_GetPerformanceCounter();
    inputs->delta_time =
        inputs->delta_time != 0
            ? (double)((now - inputs->last_frame_time) / inputs->frequency)
            : FALLBACK_DELTA_TIME;
    inputs->last_frame_time = now;
}

static void clear_deltas(DP_UserInputs *inputs)
{
    inputs->mouse_delta_x = 0;
    inputs->mouse_delta_y = 0;
    inputs->mouse_wheel_x = 0;
    inputs->mouse_wheel_y = 0;
    inputs->have_current_finger_id = false;
    inputs->finger_delta_x = 0.0f;
    inputs->finger_delta_y = 0.0f;
    inputs->finger_pinch = 0.0f;
}

static void clear_presses_and_releases(DP_UserInputs *inputs)
{
    for (int i = 0; i < SDL_NUM_SCANCODES; ++i) {
        inputs->scan_codes[i] &=
            DP_int_to_uint8(~(DP_UI_PRESSED | DP_UI_RELEASED));
    }
    for (int i = 0; i < DP_UI_MOUSE_BUTTON_COUNT; ++i) {
        inputs->mouse_buttons[i] &=
            DP_int_to_uint8(~(DP_UI_PRESSED | DP_UI_RELEASED));
    }
}

void DP_user_inputs_next_frame(DP_UserInputs *inputs)
{
    DP_ASSERT(inputs);
    calculate_delta_time(inputs);
    clear_deltas(inputs);
    clear_presses_and_releases(inputs);
}


static void on_key_down(DP_UserInputs *inputs, SDL_KeyboardEvent *ke)
{
    DP_ASSERT(ke->keysym.scancode >= 0);
    DP_ASSERT(ke->keysym.scancode < SDL_NUM_SCANCODES);
    inputs->scan_codes[ke->keysym.scancode] =
        DP_int_to_uint8(DP_UI_PRESSED | DP_UI_HELD);
}

static void on_key_up(DP_UserInputs *inputs, SDL_KeyboardEvent *ke)
{
    DP_ASSERT(ke->keysym.scancode >= 0);
    DP_ASSERT(ke->keysym.scancode < SDL_NUM_SCANCODES);
    uint8_t *code = &inputs->scan_codes[ke->keysym.scancode];
    *code = DP_int_to_uint8(DP_UI_RELEASED | (*code & ~DP_UI_HELD));
}

static void on_mouse_down(DP_UserInputs *inputs, SDL_MouseButtonEvent *mbe)
{
    DP_ASSERT(mbe->button >= DP_UI_MOUSE_BUTTON_MIN);
    DP_ASSERT(mbe->button <= DP_UI_MOUSE_BUTTON_MAX);
    inputs->mouse_buttons[mbe->button - 1] =
        DP_int_to_uint8(DP_UI_PRESSED | DP_UI_HELD);
}

static void on_mouse_up(DP_UserInputs *inputs, SDL_MouseButtonEvent *mbe)
{
    DP_ASSERT(mbe->button >= DP_UI_MOUSE_BUTTON_MIN);
    DP_ASSERT(mbe->button <= DP_UI_MOUSE_BUTTON_MAX);
    uint8_t *button = &inputs->mouse_buttons[mbe->button - 1];
    *button = DP_int_to_uint8(DP_UI_RELEASED | (*button & ~DP_UI_HELD));
}

static void on_mouse_motion(DP_UserInputs *inputs, SDL_MouseMotionEvent *mme)
{
    inputs->mouse_delta_x += mme->xrel;
    inputs->mouse_delta_y += mme->yrel;
}

static void on_mouse_wheel(DP_UserInputs *inputs, SDL_MouseWheelEvent *mwe)
{
    int multiplier = mwe->direction == SDL_MOUSEWHEEL_FLIPPED ? -1 : 1;
    inputs->mouse_wheel_x += mwe->x * multiplier;
    inputs->mouse_wheel_y += mwe->y * multiplier;
}

static void on_finger_motion(DP_UserInputs *inputs, SDL_TouchFingerEvent *tfe)
{
    // Don't accumulate the motion from multiple fingers, just use the first.
    SDL_FingerID finger_id = tfe->fingerId;
    if (!inputs->have_current_finger_id) {
        inputs->have_current_finger_id = true;
        inputs->current_finger_id = finger_id;
        inputs->finger_delta_x = tfe->dx;
        inputs->finger_delta_y = tfe->dy;
    }
    else if (inputs->current_finger_id == finger_id) {
        inputs->finger_delta_x += tfe->dx;
        inputs->finger_delta_y += tfe->dy;
    }
}

static void on_multigesture(DP_UserInputs *inputs, SDL_MultiGestureEvent *mge)
{
    inputs->finger_pinch += mge->dDist;
}

void DP_user_inputs_handle(DP_UserInputs *inputs, SDL_Event *event)
{
    DP_ASSERT(inputs);
    DP_ASSERT(event);
    switch (event->type) {
    case SDL_KEYDOWN:
        on_key_down(inputs, &event->key);
        break;
    case SDL_KEYUP:
        on_key_up(inputs, &event->key);
        break;
    case SDL_MOUSEBUTTONDOWN:
        on_mouse_down(inputs, &event->button);
        break;
    case SDL_MOUSEBUTTONUP:
        on_mouse_up(inputs, &event->button);
        break;
    case SDL_MOUSEMOTION:
        on_mouse_motion(inputs, &event->motion);
        break;
    case SDL_MOUSEWHEEL:
        on_mouse_wheel(inputs, &event->wheel);
        break;
    case SDL_FINGERMOTION:
        on_finger_motion(inputs, &event->tfinger);
        break;
    case SDL_MULTIGESTURE:
        on_multigesture(inputs, &event->mgesture);
        break;
    default:
        break;
    }
}

void DP_user_input_view_dimensions_set(DP_UserInputs *inputs, int view_width,
                                       int view_height)
{
    DP_ASSERT(inputs);
    inputs->view_width = view_width;
    inputs->view_height = view_height;
}

void DP_user_inputs_cursor_set(DP_UserInputs *inputs, SDL_SystemCursor id)
{
    DP_ASSERT(inputs);
    DP_ASSERT(id >= 0);
    DP_ASSERT(id <= SDL_NUM_SYSTEM_CURSORS); // Max id means don't change it.
    inputs->next_cursor_id = id;
}

void DP_user_inputs_render(DP_UserInputs *inputs)
{
    DP_ASSERT(inputs);
    SDL_SystemCursor next_cursor_id = inputs->next_cursor_id;
    if (next_cursor_id != SDL_NUM_SYSTEM_CURSORS) {
        SDL_Cursor *cursor = inputs->cursors[next_cursor_id];
        if (cursor) {
            SDL_SetCursor(cursor);
        }
    }
}
