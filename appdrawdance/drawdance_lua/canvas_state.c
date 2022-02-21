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
#include <dpengine/layer_props.h>
#include <dpengine/layer_props_list.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>


static int layer_props_push(lua_State *L, DP_LayerProps *lp)
{
    DP_LayerProps **pp = lua_newuserdatauv(L, sizeof(lp), 0);
    *pp = DP_layer_props_incref(lp);
    luaL_setmetatable(L, "DP_LayerProps");
    return 1;
}

DP_LUA_DEFINE_CHECK(DP_LayerProps, check_layer_props)
DP_LUA_DEFINE_GC(DP_LayerProps, layer_props_gc, DP_layer_props_decref)


static int layer_props_index(lua_State *L)
{
    DP_LayerProps *l = check_layer_props(L, 1);
    if (lua_type(L, 2) == LUA_TSTRING) {
        const char *key = lua_tostring(L, 2);
        if (strcmp(key, "id") == 0) {
            lua_pushinteger(L, DP_layer_props_id(l));
            return 1;
        }
        else if (strcmp(key, "opacity") == 0) {
            lua_pushinteger(L, DP_layer_props_opacity(l));
            return 1;
        }
        else if (strcmp(key, "hidden") == 0) {
            lua_pushboolean(L, DP_layer_props_hidden(l));
            return 1;
        }
        else if (strcmp(key, "fixed") == 0) {
            lua_pushboolean(L, DP_layer_props_fixed(l));
            return 1;
        }
        else if (strcmp(key, "title") == 0) {
            size_t length;
            const char *title = DP_layer_props_title(l, &length);
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

static const luaL_Reg layer_props_methods[] = {
    {"__gc", layer_props_gc},
    {"__index", layer_props_index},
    {NULL, NULL},
};


static int layer_props_list_push(lua_State *L, DP_LayerPropsList *lpl)
{
    DP_LayerPropsList **pp = lua_newuserdatauv(L, sizeof(lpl), 0);
    *pp = DP_layer_props_list_incref(lpl);
    luaL_setmetatable(L, "DP_LayerPropsList");
    return 1;
}

DP_LUA_DEFINE_CHECK(DP_LayerPropsList, check_layer_props_list)
DP_LUA_DEFINE_GC(DP_LayerPropsList, layer_props_list_gc,
                 DP_layer_props_list_decref)

static int layer_list_len(lua_State *L)
{
    DP_LayerPropsList *lpl = check_layer_props_list(L, 1);
    lua_pushinteger(L, DP_layer_props_list_count(lpl));
    return 1;
}

static int layer_list_index(lua_State *L)
{
    DP_LayerPropsList *lpl = check_layer_props_list(L, 1);
    if (lua_type(L, 2) == LUA_TNUMBER) {
        lua_Integer index = luaL_checkinteger(L, 2);
        if (index >= 1) {
            int count = DP_layer_props_list_count(lpl);
            int i = (int)index - 1;
            if (i < count) {
                DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, i);
                return layer_props_push(L, lp);
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

static const luaL_Reg layer_props_list_methods[] = {
    {"__gc", layer_props_list_gc},
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
        if (strcmp(key, "layer_props") == 0) {
            DP_LayerPropsList *lpl = DP_canvas_state_layer_props_noinc(cs);
            return layer_props_list_push(L, lpl);
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
    luaL_newmetatable(L, "DP_LayerProps");
    luaL_setfuncs(L, layer_props_methods, 0);
    lua_pop(L, 1);
    luaL_newmetatable(L, "DP_LayerPropsList");
    luaL_setfuncs(L, layer_props_list_methods, 0);
    lua_pop(L, 1);
    luaL_newmetatable(L, "DP_CanvasState");
    luaL_setfuncs(L, canvas_state_methods, 0);
    lua_pop(L, 1);
    return 0;
}
