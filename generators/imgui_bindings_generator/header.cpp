//- Copyright (c) 2022 askmeaboutloom
//-
//- Permission is hereby granted, free of charge, to any person obtaining a copy
//- of this software and associated documentation files (the "Software"), to deal
//- in the Software without restriction, including without limitation the rights
//- to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//- copies of the Software, and to permit persons to whom the Software is
//- furnished to do so, subject to the following conditions:
//-
//- The above copyright notice and this permission notice shall be included in all
//- copies or substantial portions of the Software.
//-
//- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//- OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//- SOFTWARE.
/*
 * This file is generated generated automatically.
 *
 * The generator is Copyright (c) 2022 askmeaboutloom.
 *
 * That generator is based on cimgui <https://github.com/cimgui/cimgui>,
 * which is Copyright (c) 2015 Stephan Dilly.
 *
 * That is based on Dear Imgui <https://github.com/ocornut/imgui>, which is
 * Copyright (c) 2014-2022 Omar Cornut.
 *
 * They and this code are all licensed under the terms of the MIT license:
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <imgui.h>

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}


extern "C" void DP_lua_imgui_begin(const char *begin_name, int end_id, void (*end_fn)(void));

#define IMGUILUA_BEGIN(L, BEGIN, END, END_ID) do { \
        DP_lua_imgui_begin(#BEGIN, END_ID, END); \
    } while (0)

extern "C" void DP_lua_imgui_end(lua_State *L, const char *end_name, int end_id);

#define IMGUILUA_END(L, END, END_ID, ...) do { \
        DP_lua_imgui_end(L, #END, END_ID); \
    } while (0)


static ImVec2 ImGuiLua_ToImVec2(lua_State *L, int index)
{
    return *static_cast<ImVec2 *>(luaL_checkudata(L, index, "ImVec2"));
}

static void ImGuiLua_PushImVec2(lua_State *L, const ImVec2 &im_vec2)
{
    ImVec2 *u = static_cast<ImVec2 *>(lua_newuserdatauv(L, sizeof(*u), 0));
    *u = im_vec2;
    luaL_setmetatable(L, "ImVec2");
}

static ImVec4 ImGuiLua_ToImVec4(lua_State *L, int index)
{
    return *static_cast<ImVec4 *>(luaL_checkudata(L, index, "ImVec4"));
}

static void ImGuiLua_PushImVec4(lua_State *L, const ImVec4 &im_vec4)
{
    ImVec4 *u = static_cast<ImVec4 *>(lua_newuserdatauv(L, sizeof(*u), 0));
    *u = im_vec4;
    luaL_setmetatable(L, "ImVec4");
}

extern "C" int ImGuiLua_NewImVec2(lua_State *L)
{
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    ImGuiLua_PushImVec2(L, ImVec2(x, y));
    return 1;
}


#define IMGUILUA_ENUM(X)     /* nothing, just a marker for the generator */
#define IMGUILUA_FUNCTION(X) /* nothing, just a marker for the generator */
#define IMGUILUA_METHOD(X)   /* nothing, just a marker for the generator */
#define IMGUILUA_STRUCT(X)   /* nothing, just a marker for the generator */

#define IMGUILUA_CHECK_ARGC(WANT) \
    int ARGC = lua_gettop(L); \
    if (ARGC != WANT) { \
        return luaL_error(L, "Wrong number of arguments: got %d, wanted %d", ARGC, WANT); \
    }

#define IMGUILUA_CHECK_ARGC_MAX(MAX) \
    int ARGC = lua_gettop(L); \
    if (ARGC > MAX) { \
        return luaL_error(L, "Wrong number of arguments: got %d, wanted between 0 and %d", ARGC, MAX); \
    }

#define IMGUILUA_CHECK_ARGC_BETWEEN(MIN, MAX) \
    int ARGC = lua_gettop(L); \
    if (ARGC < MIN || ARGC > MAX) { \
        return luaL_error(L, "Wrong number of arguments: got %d, wanted between %d and %d", ARGC, MIN, MAX); \
    }


static void ImGuiLua_SetEnumValue(lua_State *L, const char *name, int value)
{
    lua_pushinteger(L, static_cast<lua_Integer>(value));
    lua_setfield(L, -2, name);
}

static void ImGuiLua_GetOrCreateTable(lua_State *L, const char *name)
{
    lua_pushstring(L, name);
    int type = lua_rawget(L, -2);
    switch (type) {
    case LUA_TTABLE:
        break;
    case LUA_TNIL:
    case LUA_TNONE:
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushstring(L, name);
        lua_pushvalue(L, -2);
        lua_rawset(L, -4);
        break;
    default:
        lua_pop(L, 1);
        luaL_error(L, "Value '%s' should be a table, but is %s",
                   name, lua_typename(L, type));
    }
}


IMGUILUA_FUNCTION(ImGui::Bool)
static int ImGuiLua_Bool(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(1);
    bool value = ARGC == 0 ? false : lua_toboolean(L, 1);
    bool *ref = static_cast<bool *>(lua_newuserdatauv(L, sizeof(*ref), 0));
    luaL_setmetatable(L, "ImGuiLuaBoolRef");
    *ref = value;
    return 1;
}

IMGUILUA_FUNCTION(ImGui::Int)
static int ImGuiLua_Int(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(1);
    int value = ARGC == 0 ? 0 : static_cast<int>(lua_tointeger(L, 1));
    int *ref = static_cast<int *>(lua_newuserdatauv(L, sizeof(*ref), 0));
    luaL_setmetatable(L, "ImGuiLuaIntRef");
    *ref = value;
    return 1;
}

IMGUILUA_FUNCTION(ImGui::Float)
static int ImGuiLua_Float(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(1);
    float value = ARGC == 0 ? 0.0f : static_cast<float>(lua_tonumber(L, 1));
    float *ref = static_cast<float *>(lua_newuserdatauv(L, sizeof(*ref), 0));
    luaL_setmetatable(L, "ImGuiLuaFloatRef");
    *ref = value;
    return 1;
}

IMGUILUA_FUNCTION(ImGui::String)
static int ImGuiLua_String(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(1);
    lua_newuserdatauv(L, 0, 1);
    luaL_setmetatable(L, "ImGuiLuaStringRef");

    if (ARGC == 0) {
        lua_pushliteral(L, "");
    }
    else {
        luaL_checkstring(L, 1);
        lua_pushvalue(L, 1);
    }
    lua_setiuservalue(L, -2, 1);

    return 1;
}
