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
#ifndef DRAWDANCE_GUI_H
#define DRAWDANCE_GUI_H
#include <SDL.h>

#ifdef __cplusplus
extern "C" {
#else
#    include <stdbool.h>
#endif


bool DP_imgui_init(void);
void DP_imgui_quit(void);

bool DP_imgui_init_sdl(SDL_Window *window, SDL_GLContext gl_context);
void DP_imgui_quit_sdl(void);

bool DP_imgui_init_gl(void);
void DP_imgui_quit_gl(void);

void DP_imgui_handle_event(SDL_Event *event);

void DP_imgui_new_frame_gl(void);

void DP_imgui_new_frame(void);

void DP_imgui_render(SDL_Window *window, SDL_GLContext gl_context);


#ifdef __cplusplus
}
#endif
#endif
