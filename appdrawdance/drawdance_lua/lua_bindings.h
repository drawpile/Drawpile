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
#ifndef DRAWDANCE_LUA_BINDINGS_H
#define DRAWDANCE_LUA_BINDINGS_H
#include <dpclient/client.h>
#include <dpcommon/common.h>
#include <lua.h>

typedef struct DP_App DP_App;
typedef struct DP_Message DP_Message;
typedef struct DP_LayerPropsList DP_LayerPropsList;

typedef struct DP_LuaWarnBuffer {
    size_t capacity;
    size_t used;
    char *buffer;
} DP_LuaWarnBuffer;

typedef struct DP_LuaAppState DP_LuaAppState;


bool DP_lua_bindings_init(lua_State *L, DP_App *app, DP_LuaWarnBuffer *wb);


DP_LuaAppState *DP_lua_app_state(lua_State *L);

DP_App *DP_lua_app(lua_State *L);

void DP_lua_app_state_client_event_push(DP_LuaAppState *state, int client_id,
                                        DP_ClientEventType type,
                                        const char *message_or_null);

void DP_lua_app_state_client_message_push(DP_LuaAppState *state, int client_id,
                                          DP_Message *msg);


int DP_lua_app_new(lua_State *L);

void DP_lua_app_free(lua_State *L, int ref);

void DP_lua_app_handle_events(lua_State *L, int ref, DP_LayerPropsList *lpl);

#ifdef DRAWDANCE_IMGUI
void DP_lua_app_prepare_gui(lua_State *L, int ref);
#endif


void DP_lua_warn_buffer_dispose(DP_LuaWarnBuffer *wb);

#endif
