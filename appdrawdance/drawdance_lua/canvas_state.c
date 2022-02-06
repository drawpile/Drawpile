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
#include "lua_bindings.h"
#include "lua_util.h"
#include <dpcommon/common.h>
#include <dpengine/canvas_state.h>
#include <dpengine/layer.h>
#include <dpengine/layer_list.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>


static int layer_push(lua_State *L, DP_Layer *l)
{
    DP_Layer **pp = lua_newuserdatauv(L, sizeof(l), 0);
    *pp = DP_layer_incref(l);
    luaL_setmetatable(L, "DP_Layer");
    return 1;
}

DP_LUA_DEFINE_CHECK(DP_Layer, check_layer)
DP_LUA_DEFINE_GC(DP_Layer, layer_gc, DP_layer_decref)


static int layer_index(lua_State *L)
{
    DP_Layer *l = check_layer(L, 1);
    if (lua_type(L, 2) == LUA_TSTRING) {
        const char *key = lua_tostring(L, 2);
        if (strcmp(key, "id") == 0) {
            lua_pushinteger(L, DP_layer_id(l));
            return 1;
        }
        else if (strcmp(key, "opacity") == 0) {
            lua_pushinteger(L, DP_layer_opacity(l));
            return 1;
        }
        else if (strcmp(key, "hidden") == 0) {
            lua_pushboolean(L, DP_layer_hidden(l));
            return 1;
        }
        else if (strcmp(key, "fixed") == 0) {
            lua_pushboolean(L, DP_layer_fixed(l));
            return 1;
        }
        else if (strcmp(key, "title") == 0) {
            size_t length;
            const char *title = DP_layer_title(l, &length);
            if (title) {
                lua_pushlstring(L, title, length);
            }
            else {
                lua_pushnil(L);
            }
            return 1;
        }
    }
    lua_getmetatable(L, 1);
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);
    return 1;
}

static const luaL_Reg layer_methods[] = {
    {"__gc", layer_gc},
    {"__index", layer_index},
    {NULL, NULL},
};


static int layer_list_push(lua_State *L, DP_LayerList *ll)
{
    DP_LayerList **pp = lua_newuserdatauv(L, sizeof(ll), 0);
    *pp = DP_layer_list_incref(ll);
    luaL_setmetatable(L, "DP_LayerList");
    return 1;
}

DP_LUA_DEFINE_CHECK(DP_LayerList, check_layer_list)
DP_LUA_DEFINE_GC(DP_LayerList, layer_list_gc, DP_layer_list_decref)

static int layer_list_len(lua_State *L)
{
    DP_LayerList *ll = check_layer_list(L, 1);
    lua_pushinteger(L, DP_layer_list_layer_count(ll));
    return 1;
}

static int layer_list_index(lua_State *L)
{
    DP_LayerList *ll = check_layer_list(L, 1);
    if (lua_type(L, 2) == LUA_TNUMBER) {
        lua_Integer index = luaL_checkinteger(L, 2);
        if (index >= 1) {
            int count = DP_layer_list_layer_count(ll);
            int i = (int)index - 1;
            if (i < count) {
                DP_Layer *l = DP_layer_list_at_noinc(ll, i);
                return layer_push(L, l);
            }
        }
        lua_pushnil(L);
        return 1;
    }
    lua_getmetatable(L, 1);
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);
    return 1;
}

static const luaL_Reg layer_list_methods[] = {
    {"__gc", layer_list_gc},
    {"__len", layer_list_len},
    {"__index", layer_list_index},
    {NULL, NULL},
};


DP_LUA_DEFINE_CHECK(DP_CanvasState, check_canvas_state)
DP_LUA_DEFINE_GC(DP_CanvasState, canvas_state_gc, DP_canvas_state_decref)

static int canvas_state_index(lua_State *L)
{
    DP_CanvasState *cs = check_canvas_state(L, 1);
    if (lua_type(L, 2) == LUA_TSTRING) {
        const char *key = lua_tostring(L, 2);
        if (strcmp(key, "layers") == 0) {
            DP_LayerList *ll = DP_canvas_state_layers_noinc(cs);
            return layer_list_push(L, ll);
        }
    }
    lua_getmetatable(L, 1);
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);
    return 1;
}

static const luaL_Reg canvas_state_methods[] = {
    {"__gc", canvas_state_gc},
    {"__index", canvas_state_index},
    {NULL, NULL},
};


int DP_lua_canvas_state_init(lua_State *L)
{
    luaL_newmetatable(L, "DP_Layer");
    luaL_setfuncs(L, layer_methods, 0);
    lua_pop(L, 1);
    luaL_newmetatable(L, "DP_LayerList");
    luaL_setfuncs(L, layer_list_methods, 0);
    lua_pop(L, 1);
    luaL_newmetatable(L, "DP_CanvasState");
    luaL_setfuncs(L, canvas_state_methods, 0);
    lua_pop(L, 1);
    return 0;
}
