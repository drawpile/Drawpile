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
#include <imgui.h>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}


extern "C" void DP_lua_imgui_begin(const char *begin_name, int end_id,
                                   void (*end_fn)(void));

#define IMGUILUA_BEGIN(L, BEGIN, END, END_ID)    \
    do {                                         \
        DP_lua_imgui_begin(#BEGIN, END_ID, END); \
    } while (0)

extern "C" void DP_lua_imgui_end(lua_State *L, const char *end_name,
                                 int end_id);

#define IMGUILUA_END(L, END, END_ID, ...)  \
    do {                                   \
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

#define IMGUILUA_CHECK_ARGC(WANT)                                            \
    int ARGC = lua_gettop(L);                                                \
    if (ARGC != WANT) {                                                      \
        return luaL_error(L, "Wrong number of arguments: got %d, wanted %d", \
                          ARGC, WANT);                                       \
    }

#define IMGUILUA_CHECK_ARGC_MAX(MAX)                                         \
    int ARGC = lua_gettop(L);                                                \
    if (ARGC > MAX) {                                                        \
        return luaL_error(                                                   \
            L, "Wrong number of arguments: got %d, wanted between 0 and %d", \
            ARGC, MAX);                                                      \
    }

#define IMGUILUA_CHECK_ARGC_BETWEEN(MIN, MAX)                                 \
    int ARGC = lua_gettop(L);                                                 \
    if (ARGC < MIN || ARGC > MAX) {                                           \
        return luaL_error(                                                    \
            L, "Wrong number of arguments: got %d, wanted between %d and %d", \
            ARGC, MIN, MAX);                                                  \
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
        luaL_error(L, "Value '%s' should be a table, but is %s", name,
                   lua_typename(L, type));
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
        lua_pushvalue(L, 1);
    }
    lua_setiuservalue(L, -2, 1);

    return 1;
}


IMGUILUA_ENUM(ImDrawFlags)
static void ImGuiLua_RegisterEnumImDrawFlags(lua_State *L)
{
    ImGuiLua_SetEnumValue(L, "None", 0);
    ImGuiLua_SetEnumValue(L, "Closed", 1 << 0);
    ImGuiLua_SetEnumValue(L, "RoundCornersTopLeft", 1 << 4);
    ImGuiLua_SetEnumValue(L, "RoundCornersTopRight", 1 << 5);
    ImGuiLua_SetEnumValue(L, "RoundCornersBottomLeft", 1 << 6);
    ImGuiLua_SetEnumValue(L, "RoundCornersBottomRight", 1 << 7);
    ImGuiLua_SetEnumValue(L, "RoundCornersNone", 1 << 8);
    ImGuiLua_SetEnumValue(L, "RoundCornersTop",
                          ImDrawFlags_RoundCornersTopLeft
                              | ImDrawFlags_RoundCornersTopRight);
    ImGuiLua_SetEnumValue(L, "RoundCornersBottom",
                          ImDrawFlags_RoundCornersBottomLeft
                              | ImDrawFlags_RoundCornersBottomRight);
    ImGuiLua_SetEnumValue(L, "RoundCornersLeft",
                          ImDrawFlags_RoundCornersBottomLeft
                              | ImDrawFlags_RoundCornersTopLeft);
    ImGuiLua_SetEnumValue(L, "RoundCornersRight",
                          ImDrawFlags_RoundCornersBottomRight
                              | ImDrawFlags_RoundCornersTopRight);
    ImGuiLua_SetEnumValue(L, "RoundCornersAll",
                          ImDrawFlags_RoundCornersTopLeft
                              | ImDrawFlags_RoundCornersTopRight
                              | ImDrawFlags_RoundCornersBottomLeft
                              | ImDrawFlags_RoundCornersBottomRight);
    ImGuiLua_SetEnumValue(L, "RoundCornersDefault_",
                          ImDrawFlags_RoundCornersAll);
    ImGuiLua_SetEnumValue(L, "RoundCornersMask_",
                          ImDrawFlags_RoundCornersAll
                              | ImDrawFlags_RoundCornersNone);
}

IMGUILUA_ENUM(ImDrawListFlags)
static void ImGuiLua_RegisterEnumImDrawListFlags(lua_State *L)
{
    ImGuiLua_SetEnumValue(L, "None", 0);
    ImGuiLua_SetEnumValue(L, "AntiAliasedLines", 1 << 0);
    ImGuiLua_SetEnumValue(L, "AntiAliasedLinesUseTex", 1 << 1);
    ImGuiLua_SetEnumValue(L, "AntiAliasedFill", 1 << 2);
    ImGuiLua_SetEnumValue(L, "AllowVtxOffset", 1 << 3);
}

IMGUILUA_ENUM(ImFontAtlasFlags)
static void ImGuiLua_RegisterEnumImFontAtlasFlags(lua_State *L)
{
    ImGuiLua_SetEnumValue(L, "None", 0);
    ImGuiLua_SetEnumValue(L, "NoPowerOfTwoHeight", 1 << 0);
    ImGuiLua_SetEnumValue(L, "NoMouseCursors", 1 << 1);
    ImGuiLua_SetEnumValue(L, "NoBakedLines", 1 << 2);
}

IMGUILUA_ENUM(ImGuiBackendFlags)
static void ImGuiLua_RegisterEnumImGuiBackendFlags(lua_State *L)
{
    ImGuiLua_SetEnumValue(L, "None", 0);
    ImGuiLua_SetEnumValue(L, "HasGamepad", 1 << 0);
    ImGuiLua_SetEnumValue(L, "HasMouseCursors", 1 << 1);
    ImGuiLua_SetEnumValue(L, "HasSetMousePos", 1 << 2);
    ImGuiLua_SetEnumValue(L, "RendererHasVtxOffset", 1 << 3);
    ImGuiLua_SetEnumValue(L, "PlatformHasViewports", 1 << 10);
    ImGuiLua_SetEnumValue(L, "HasMouseHoveredViewport", 1 << 11);
    ImGuiLua_SetEnumValue(L, "RendererHasViewports", 1 << 12);
}

IMGUILUA_ENUM(ImGuiButtonFlags)
static void ImGuiLua_RegisterEnumImGuiButtonFlags(lua_State *L)
{
    ImGuiLua_SetEnumValue(L, "None", 0);
    ImGuiLua_SetEnumValue(L, "MouseButtonLeft", 1 << 0);
    ImGuiLua_SetEnumValue(L, "MouseButtonRight", 1 << 1);
    ImGuiLua_SetEnumValue(L, "MouseButtonMiddle", 1 << 2);
    ImGuiLua_SetEnumValue(L, "MouseButtonMask_",
                          ImGuiButtonFlags_MouseButtonLeft
                              | ImGuiButtonFlags_MouseButtonRight
                              | ImGuiButtonFlags_MouseButtonMiddle);
    ImGuiLua_SetEnumValue(L, "MouseButtonDefault_",
                          ImGuiButtonFlags_MouseButtonLeft);
}

IMGUILUA_ENUM(ImGuiCol)
static void ImGuiLua_RegisterEnumImGuiCol(lua_State *L)
{
    ImGuiLua_SetEnumValue(L, "Text", 0);
    ImGuiLua_SetEnumValue(L, "TextDisabled", 1);
    ImGuiLua_SetEnumValue(L, "WindowBg", 2);
    ImGuiLua_SetEnumValue(L, "ChildBg", 3);
    ImGuiLua_SetEnumValue(L, "PopupBg", 4);
    ImGuiLua_SetEnumValue(L, "Border", 5);
    ImGuiLua_SetEnumValue(L, "BorderShadow", 6);
    ImGuiLua_SetEnumValue(L, "FrameBg", 7);
    ImGuiLua_SetEnumValue(L, "FrameBgHovered", 8);
    ImGuiLua_SetEnumValue(L, "FrameBgActive", 9);
    ImGuiLua_SetEnumValue(L, "TitleBg", 10);
    ImGuiLua_SetEnumValue(L, "TitleBgActive", 11);
    ImGuiLua_SetEnumValue(L, "TitleBgCollapsed", 12);
    ImGuiLua_SetEnumValue(L, "MenuBarBg", 13);
    ImGuiLua_SetEnumValue(L, "ScrollbarBg", 14);
    ImGuiLua_SetEnumValue(L, "ScrollbarGrab", 15);
    ImGuiLua_SetEnumValue(L, "ScrollbarGrabHovered", 16);
    ImGuiLua_SetEnumValue(L, "ScrollbarGrabActive", 17);
    ImGuiLua_SetEnumValue(L, "CheckMark", 18);
    ImGuiLua_SetEnumValue(L, "SliderGrab", 19);
    ImGuiLua_SetEnumValue(L, "SliderGrabActive", 20);
    ImGuiLua_SetEnumValue(L, "Button", 21);
    ImGuiLua_SetEnumValue(L, "ButtonHovered", 22);
    ImGuiLua_SetEnumValue(L, "ButtonActive", 23);
    ImGuiLua_SetEnumValue(L, "Header", 24);
    ImGuiLua_SetEnumValue(L, "HeaderHovered", 25);
    ImGuiLua_SetEnumValue(L, "HeaderActive", 26);
    ImGuiLua_SetEnumValue(L, "Separator", 27);
    ImGuiLua_SetEnumValue(L, "SeparatorHovered", 28);
    ImGuiLua_SetEnumValue(L, "SeparatorActive", 29);
    ImGuiLua_SetEnumValue(L, "ResizeGrip", 30);
    ImGuiLua_SetEnumValue(L, "ResizeGripHovered", 31);
    ImGuiLua_SetEnumValue(L, "ResizeGripActive", 32);
    ImGuiLua_SetEnumValue(L, "Tab", 33);
    ImGuiLua_SetEnumValue(L, "TabHovered", 34);
    ImGuiLua_SetEnumValue(L, "TabActive", 35);
    ImGuiLua_SetEnumValue(L, "TabUnfocused", 36);
    ImGuiLua_SetEnumValue(L, "TabUnfocusedActive", 37);
    ImGuiLua_SetEnumValue(L, "DockingPreview", 38);
    ImGuiLua_SetEnumValue(L, "DockingEmptyBg", 39);
    ImGuiLua_SetEnumValue(L, "PlotLines", 40);
    ImGuiLua_SetEnumValue(L, "PlotLinesHovered", 41);
    ImGuiLua_SetEnumValue(L, "PlotHistogram", 42);
    ImGuiLua_SetEnumValue(L, "PlotHistogramHovered", 43);
    ImGuiLua_SetEnumValue(L, "TableHeaderBg", 44);
    ImGuiLua_SetEnumValue(L, "TableBorderStrong", 45);
    ImGuiLua_SetEnumValue(L, "TableBorderLight", 46);
    ImGuiLua_SetEnumValue(L, "TableRowBg", 47);
    ImGuiLua_SetEnumValue(L, "TableRowBgAlt", 48);
    ImGuiLua_SetEnumValue(L, "TextSelectedBg", 49);
    ImGuiLua_SetEnumValue(L, "DragDropTarget", 50);
    ImGuiLua_SetEnumValue(L, "NavHighlight", 51);
    ImGuiLua_SetEnumValue(L, "NavWindowingHighlight", 52);
    ImGuiLua_SetEnumValue(L, "NavWindowingDimBg", 53);
    ImGuiLua_SetEnumValue(L, "ModalWindowDimBg", 54);
    ImGuiLua_SetEnumValue(L, "COUNT", 55);
}

IMGUILUA_ENUM(ImGuiColorEditFlags)
static void ImGuiLua_RegisterEnumImGuiColorEditFlags(lua_State *L)
{
    ImGuiLua_SetEnumValue(L, "None", 0);
    ImGuiLua_SetEnumValue(L, "NoAlpha", 1 << 1);
    ImGuiLua_SetEnumValue(L, "NoPicker", 1 << 2);
    ImGuiLua_SetEnumValue(L, "NoOptions", 1 << 3);
    ImGuiLua_SetEnumValue(L, "NoSmallPreview", 1 << 4);
    ImGuiLua_SetEnumValue(L, "NoInputs", 1 << 5);
    ImGuiLua_SetEnumValue(L, "NoTooltip", 1 << 6);
    ImGuiLua_SetEnumValue(L, "NoLabel", 1 << 7);
    ImGuiLua_SetEnumValue(L, "NoSidePreview", 1 << 8);
    ImGuiLua_SetEnumValue(L, "NoDragDrop", 1 << 9);
    ImGuiLua_SetEnumValue(L, "NoBorder", 1 << 10);
    ImGuiLua_SetEnumValue(L, "AlphaBar", 1 << 16);
    ImGuiLua_SetEnumValue(L, "AlphaPreview", 1 << 17);
    ImGuiLua_SetEnumValue(L, "AlphaPreviewHalf", 1 << 18);
    ImGuiLua_SetEnumValue(L, "HDR", 1 << 19);
    ImGuiLua_SetEnumValue(L, "DisplayRGB", 1 << 20);
    ImGuiLua_SetEnumValue(L, "DisplayHSV", 1 << 21);
    ImGuiLua_SetEnumValue(L, "DisplayHex", 1 << 22);
    ImGuiLua_SetEnumValue(L, "Uint8", 1 << 23);
    ImGuiLua_SetEnumValue(L, "Float", 1 << 24);
    ImGuiLua_SetEnumValue(L, "PickerHueBar", 1 << 25);
    ImGuiLua_SetEnumValue(L, "PickerHueWheel", 1 << 26);
    ImGuiLua_SetEnumValue(L, "InputRGB", 1 << 27);
    ImGuiLua_SetEnumValue(L, "InputHSV", 1 << 28);
    ImGuiLua_SetEnumValue(
        L, "DefaultOptions_",
        ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_DisplayRGB
            | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_PickerHueBar);
    ImGuiLua_SetEnumValue(L, "DisplayMask_",
                          ImGuiColorEditFlags_DisplayRGB
                              | ImGuiColorEditFlags_DisplayHSV
                              | ImGuiColorEditFlags_DisplayHex);
    ImGuiLua_SetEnumValue(L, "DataTypeMask_",
                          ImGuiColorEditFlags_Uint8
                              | ImGuiColorEditFlags_Float);
    ImGuiLua_SetEnumValue(L, "PickerMask_",
                          ImGuiColorEditFlags_PickerHueWheel
                              | ImGuiColorEditFlags_PickerHueBar);
    ImGuiLua_SetEnumValue(L, "InputMask_",
                          ImGuiColorEditFlags_InputRGB
                              | ImGuiColorEditFlags_InputHSV);
}

IMGUILUA_ENUM(ImGuiComboFlags)
static void ImGuiLua_RegisterEnumImGuiComboFlags(lua_State *L)
{
    ImGuiLua_SetEnumValue(L, "None", 0);
    ImGuiLua_SetEnumValue(L, "PopupAlignLeft", 1 << 0);
    ImGuiLua_SetEnumValue(L, "HeightSmall", 1 << 1);
    ImGuiLua_SetEnumValue(L, "HeightRegular", 1 << 2);
    ImGuiLua_SetEnumValue(L, "HeightLarge", 1 << 3);
    ImGuiLua_SetEnumValue(L, "HeightLargest", 1 << 4);
    ImGuiLua_SetEnumValue(L, "NoArrowButton", 1 << 5);
    ImGuiLua_SetEnumValue(L, "NoPreview", 1 << 6);
    ImGuiLua_SetEnumValue(
        L, "HeightMask_",
        ImGuiComboFlags_HeightSmall | ImGuiComboFlags_HeightRegular
            | ImGuiComboFlags_HeightLarge | ImGuiComboFlags_HeightLargest);
}

IMGUILUA_ENUM(ImGuiCond)
static void ImGuiLua_RegisterEnumImGuiCond(lua_State *L)
{
    ImGuiLua_SetEnumValue(L, "None", 0);
    ImGuiLua_SetEnumValue(L, "Always", 1 << 0);
    ImGuiLua_SetEnumValue(L, "Once", 1 << 1);
    ImGuiLua_SetEnumValue(L, "FirstUseEver", 1 << 2);
    ImGuiLua_SetEnumValue(L, "Appearing", 1 << 3);
}

IMGUILUA_ENUM(ImGuiConfigFlags)
static void ImGuiLua_RegisterEnumImGuiConfigFlags(lua_State *L)
{
    ImGuiLua_SetEnumValue(L, "None", 0);
    ImGuiLua_SetEnumValue(L, "NavEnableKeyboard", 1 << 0);
    ImGuiLua_SetEnumValue(L, "NavEnableGamepad", 1 << 1);
    ImGuiLua_SetEnumValue(L, "NavEnableSetMousePos", 1 << 2);
    ImGuiLua_SetEnumValue(L, "NavNoCaptureKeyboard", 1 << 3);
    ImGuiLua_SetEnumValue(L, "NoMouse", 1 << 4);
    ImGuiLua_SetEnumValue(L, "NoMouseCursorChange", 1 << 5);
    ImGuiLua_SetEnumValue(L, "DockingEnable", 1 << 6);
    ImGuiLua_SetEnumValue(L, "ViewportsEnable", 1 << 10);
    ImGuiLua_SetEnumValue(L, "DpiEnableScaleViewports", 1 << 14);
    ImGuiLua_SetEnumValue(L, "DpiEnableScaleFonts", 1 << 15);
    ImGuiLua_SetEnumValue(L, "IsSRGB", 1 << 20);
    ImGuiLua_SetEnumValue(L, "IsTouchScreen", 1 << 21);
}

IMGUILUA_ENUM(ImGuiDataType)
static void ImGuiLua_RegisterEnumImGuiDataType(lua_State *L)
{
    ImGuiLua_SetEnumValue(L, "S8", 0);
    ImGuiLua_SetEnumValue(L, "U8", 1);
    ImGuiLua_SetEnumValue(L, "S16", 2);
    ImGuiLua_SetEnumValue(L, "U16", 3);
    ImGuiLua_SetEnumValue(L, "S32", 4);
    ImGuiLua_SetEnumValue(L, "U32", 5);
    ImGuiLua_SetEnumValue(L, "S64", 6);
    ImGuiLua_SetEnumValue(L, "U64", 7);
    ImGuiLua_SetEnumValue(L, "Float", 8);
    ImGuiLua_SetEnumValue(L, "Double", 9);
    ImGuiLua_SetEnumValue(L, "COUNT", 10);
}

IMGUILUA_ENUM(ImGuiDir)
static void ImGuiLua_RegisterEnumImGuiDir(lua_State *L)
{
    ImGuiLua_SetEnumValue(L, "None", -1);
    ImGuiLua_SetEnumValue(L, "Left", 0);
    ImGuiLua_SetEnumValue(L, "Right", 1);
    ImGuiLua_SetEnumValue(L, "Up", 2);
    ImGuiLua_SetEnumValue(L, "Down", 3);
    ImGuiLua_SetEnumValue(L, "COUNT", 4);
}

IMGUILUA_ENUM(ImGuiDockNodeFlags)
static void ImGuiLua_RegisterEnumImGuiDockNodeFlags(lua_State *L)
{
    ImGuiLua_SetEnumValue(L, "None", 0);
    ImGuiLua_SetEnumValue(L, "KeepAliveOnly", 1 << 0);
    ImGuiLua_SetEnumValue(L, "NoDockingInCentralNode", 1 << 2);
    ImGuiLua_SetEnumValue(L, "PassthruCentralNode", 1 << 3);
    ImGuiLua_SetEnumValue(L, "NoSplit", 1 << 4);
    ImGuiLua_SetEnumValue(L, "NoResize", 1 << 5);
    ImGuiLua_SetEnumValue(L, "AutoHideTabBar", 1 << 6);
}

IMGUILUA_ENUM(ImGuiDragDropFlags)
static void ImGuiLua_RegisterEnumImGuiDragDropFlags(lua_State *L)
{
    ImGuiLua_SetEnumValue(L, "None", 0);
    ImGuiLua_SetEnumValue(L, "SourceNoPreviewTooltip", 1 << 0);
    ImGuiLua_SetEnumValue(L, "SourceNoDisableHover", 1 << 1);
    ImGuiLua_SetEnumValue(L, "SourceNoHoldToOpenOthers", 1 << 2);
    ImGuiLua_SetEnumValue(L, "SourceAllowNullID", 1 << 3);
    ImGuiLua_SetEnumValue(L, "SourceExtern", 1 << 4);
    ImGuiLua_SetEnumValue(L, "SourceAutoExpirePayload", 1 << 5);
    ImGuiLua_SetEnumValue(L, "AcceptBeforeDelivery", 1 << 10);
    ImGuiLua_SetEnumValue(L, "AcceptNoDrawDefaultRect", 1 << 11);
    ImGuiLua_SetEnumValue(L, "AcceptNoPreviewTooltip", 1 << 12);
    ImGuiLua_SetEnumValue(L, "AcceptPeekOnly",
                          ImGuiDragDropFlags_AcceptBeforeDelivery
                              | ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
}

IMGUILUA_ENUM(ImGuiFocusedFlags)
static void ImGuiLua_RegisterEnumImGuiFocusedFlags(lua_State *L)
{
    ImGuiLua_SetEnumValue(L, "None", 0);
    ImGuiLua_SetEnumValue(L, "ChildWindows", 1 << 0);
    ImGuiLua_SetEnumValue(L, "RootWindow", 1 << 1);
    ImGuiLua_SetEnumValue(L, "AnyWindow", 1 << 2);
    ImGuiLua_SetEnumValue(L, "NoPopupHierarchy", 1 << 3);
    ImGuiLua_SetEnumValue(L, "DockHierarchy", 1 << 4);
    ImGuiLua_SetEnumValue(L, "RootAndChildWindows",
                          ImGuiFocusedFlags_RootWindow
                              | ImGuiFocusedFlags_ChildWindows);
}

IMGUILUA_ENUM(ImGuiHoveredFlags)
static void ImGuiLua_RegisterEnumImGuiHoveredFlags(lua_State *L)
{
    ImGuiLua_SetEnumValue(L, "None", 0);
    ImGuiLua_SetEnumValue(L, "ChildWindows", 1 << 0);
    ImGuiLua_SetEnumValue(L, "RootWindow", 1 << 1);
    ImGuiLua_SetEnumValue(L, "AnyWindow", 1 << 2);
    ImGuiLua_SetEnumValue(L, "NoPopupHierarchy", 1 << 3);
    ImGuiLua_SetEnumValue(L, "DockHierarchy", 1 << 4);
    ImGuiLua_SetEnumValue(L, "AllowWhenBlockedByPopup", 1 << 5);
    ImGuiLua_SetEnumValue(L, "AllowWhenBlockedByActiveItem", 1 << 7);
    ImGuiLua_SetEnumValue(L, "AllowWhenOverlapped", 1 << 8);
    ImGuiLua_SetEnumValue(L, "AllowWhenDisabled", 1 << 9);
    ImGuiLua_SetEnumValue(L, "RectOnly",
                          ImGuiHoveredFlags_AllowWhenBlockedByPopup
                              | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem
                              | ImGuiHoveredFlags_AllowWhenOverlapped);
    ImGuiLua_SetEnumValue(L, "RootAndChildWindows",
                          ImGuiHoveredFlags_RootWindow
                              | ImGuiHoveredFlags_ChildWindows);
}

IMGUILUA_ENUM(ImGuiInputTextFlags)
static void ImGuiLua_RegisterEnumImGuiInputTextFlags(lua_State *L)
{
    ImGuiLua_SetEnumValue(L, "None", 0);
    ImGuiLua_SetEnumValue(L, "CharsDecimal", 1 << 0);
    ImGuiLua_SetEnumValue(L, "CharsHexadecimal", 1 << 1);
    ImGuiLua_SetEnumValue(L, "CharsUppercase", 1 << 2);
    ImGuiLua_SetEnumValue(L, "CharsNoBlank", 1 << 3);
    ImGuiLua_SetEnumValue(L, "AutoSelectAll", 1 << 4);
    ImGuiLua_SetEnumValue(L, "EnterReturnsTrue", 1 << 5);
    ImGuiLua_SetEnumValue(L, "CallbackCompletion", 1 << 6);
    ImGuiLua_SetEnumValue(L, "CallbackHistory", 1 << 7);
    ImGuiLua_SetEnumValue(L, "CallbackAlways", 1 << 8);
    ImGuiLua_SetEnumValue(L, "CallbackCharFilter", 1 << 9);
    ImGuiLua_SetEnumValue(L, "AllowTabInput", 1 << 10);
    ImGuiLua_SetEnumValue(L, "CtrlEnterForNewLine", 1 << 11);
    ImGuiLua_SetEnumValue(L, "NoHorizontalScroll", 1 << 12);
    ImGuiLua_SetEnumValue(L, "AlwaysOverwrite", 1 << 13);
    ImGuiLua_SetEnumValue(L, "ReadOnly", 1 << 14);
    ImGuiLua_SetEnumValue(L, "Password", 1 << 15);
    ImGuiLua_SetEnumValue(L, "NoUndoRedo", 1 << 16);
    ImGuiLua_SetEnumValue(L, "CharsScientific", 1 << 17);
    ImGuiLua_SetEnumValue(L, "CallbackResize", 1 << 18);
    ImGuiLua_SetEnumValue(L, "CallbackEdit", 1 << 19);
}

IMGUILUA_ENUM(ImGuiKeyModFlags)
static void ImGuiLua_RegisterEnumImGuiKeyModFlags(lua_State *L)
{
    ImGuiLua_SetEnumValue(L, "None", 0);
    ImGuiLua_SetEnumValue(L, "Ctrl", 1 << 0);
    ImGuiLua_SetEnumValue(L, "Shift", 1 << 1);
    ImGuiLua_SetEnumValue(L, "Alt", 1 << 2);
    ImGuiLua_SetEnumValue(L, "Super", 1 << 3);
}

IMGUILUA_ENUM(ImGuiKey)
static void ImGuiLua_RegisterEnumImGuiKey(lua_State *L)
{
    ImGuiLua_SetEnumValue(L, "None", 0);
    ImGuiLua_SetEnumValue(L, "Tab", 512);
    ImGuiLua_SetEnumValue(L, "LeftArrow", 513);
    ImGuiLua_SetEnumValue(L, "RightArrow", 514);
    ImGuiLua_SetEnumValue(L, "UpArrow", 515);
    ImGuiLua_SetEnumValue(L, "DownArrow", 516);
    ImGuiLua_SetEnumValue(L, "PageUp", 517);
    ImGuiLua_SetEnumValue(L, "PageDown", 518);
    ImGuiLua_SetEnumValue(L, "Home", 519);
    ImGuiLua_SetEnumValue(L, "End", 520);
    ImGuiLua_SetEnumValue(L, "Insert", 521);
    ImGuiLua_SetEnumValue(L, "Delete", 522);
    ImGuiLua_SetEnumValue(L, "Backspace", 523);
    ImGuiLua_SetEnumValue(L, "Space", 524);
    ImGuiLua_SetEnumValue(L, "Enter", 525);
    ImGuiLua_SetEnumValue(L, "Escape", 526);
    ImGuiLua_SetEnumValue(L, "LeftCtrl", 527);
    ImGuiLua_SetEnumValue(L, "LeftShift", 528);
    ImGuiLua_SetEnumValue(L, "LeftAlt", 529);
    ImGuiLua_SetEnumValue(L, "LeftSuper", 530);
    ImGuiLua_SetEnumValue(L, "RightCtrl", 531);
    ImGuiLua_SetEnumValue(L, "RightShift", 532);
    ImGuiLua_SetEnumValue(L, "RightAlt", 533);
    ImGuiLua_SetEnumValue(L, "RightSuper", 534);
    ImGuiLua_SetEnumValue(L, "Menu", 535);
    ImGuiLua_SetEnumValue(L, "0", 536);
    ImGuiLua_SetEnumValue(L, "1", 537);
    ImGuiLua_SetEnumValue(L, "2", 538);
    ImGuiLua_SetEnumValue(L, "3", 539);
    ImGuiLua_SetEnumValue(L, "4", 540);
    ImGuiLua_SetEnumValue(L, "5", 541);
    ImGuiLua_SetEnumValue(L, "6", 542);
    ImGuiLua_SetEnumValue(L, "7", 543);
    ImGuiLua_SetEnumValue(L, "8", 544);
    ImGuiLua_SetEnumValue(L, "9", 545);
    ImGuiLua_SetEnumValue(L, "A", 546);
    ImGuiLua_SetEnumValue(L, "B", 547);
    ImGuiLua_SetEnumValue(L, "C", 548);
    ImGuiLua_SetEnumValue(L, "D", 549);
    ImGuiLua_SetEnumValue(L, "E", 550);
    ImGuiLua_SetEnumValue(L, "F", 551);
    ImGuiLua_SetEnumValue(L, "G", 552);
    ImGuiLua_SetEnumValue(L, "H", 553);
    ImGuiLua_SetEnumValue(L, "I", 554);
    ImGuiLua_SetEnumValue(L, "J", 555);
    ImGuiLua_SetEnumValue(L, "K", 556);
    ImGuiLua_SetEnumValue(L, "L", 557);
    ImGuiLua_SetEnumValue(L, "M", 558);
    ImGuiLua_SetEnumValue(L, "N", 559);
    ImGuiLua_SetEnumValue(L, "O", 560);
    ImGuiLua_SetEnumValue(L, "P", 561);
    ImGuiLua_SetEnumValue(L, "Q", 562);
    ImGuiLua_SetEnumValue(L, "R", 563);
    ImGuiLua_SetEnumValue(L, "S", 564);
    ImGuiLua_SetEnumValue(L, "T", 565);
    ImGuiLua_SetEnumValue(L, "U", 566);
    ImGuiLua_SetEnumValue(L, "V", 567);
    ImGuiLua_SetEnumValue(L, "W", 568);
    ImGuiLua_SetEnumValue(L, "X", 569);
    ImGuiLua_SetEnumValue(L, "Y", 570);
    ImGuiLua_SetEnumValue(L, "Z", 571);
    ImGuiLua_SetEnumValue(L, "F1", 572);
    ImGuiLua_SetEnumValue(L, "F2", 573);
    ImGuiLua_SetEnumValue(L, "F3", 574);
    ImGuiLua_SetEnumValue(L, "F4", 575);
    ImGuiLua_SetEnumValue(L, "F5", 576);
    ImGuiLua_SetEnumValue(L, "F6", 577);
    ImGuiLua_SetEnumValue(L, "F7", 578);
    ImGuiLua_SetEnumValue(L, "F8", 579);
    ImGuiLua_SetEnumValue(L, "F9", 580);
    ImGuiLua_SetEnumValue(L, "F10", 581);
    ImGuiLua_SetEnumValue(L, "F11", 582);
    ImGuiLua_SetEnumValue(L, "F12", 583);
    ImGuiLua_SetEnumValue(L, "Apostrophe", 584);
    ImGuiLua_SetEnumValue(L, "Comma", 585);
    ImGuiLua_SetEnumValue(L, "Minus", 586);
    ImGuiLua_SetEnumValue(L, "Period", 587);
    ImGuiLua_SetEnumValue(L, "Slash", 588);
    ImGuiLua_SetEnumValue(L, "Semicolon", 589);
    ImGuiLua_SetEnumValue(L, "Equal", 590);
    ImGuiLua_SetEnumValue(L, "LeftBracket", 591);
    ImGuiLua_SetEnumValue(L, "Backslash", 592);
    ImGuiLua_SetEnumValue(L, "RightBracket", 593);
    ImGuiLua_SetEnumValue(L, "GraveAccent", 594);
    ImGuiLua_SetEnumValue(L, "CapsLock", 595);
    ImGuiLua_SetEnumValue(L, "ScrollLock", 596);
    ImGuiLua_SetEnumValue(L, "NumLock", 597);
    ImGuiLua_SetEnumValue(L, "PrintScreen", 598);
    ImGuiLua_SetEnumValue(L, "Pause", 599);
    ImGuiLua_SetEnumValue(L, "Keypad0", 600);
    ImGuiLua_SetEnumValue(L, "Keypad1", 601);
    ImGuiLua_SetEnumValue(L, "Keypad2", 602);
    ImGuiLua_SetEnumValue(L, "Keypad3", 603);
    ImGuiLua_SetEnumValue(L, "Keypad4", 604);
    ImGuiLua_SetEnumValue(L, "Keypad5", 605);
    ImGuiLua_SetEnumValue(L, "Keypad6", 606);
    ImGuiLua_SetEnumValue(L, "Keypad7", 607);
    ImGuiLua_SetEnumValue(L, "Keypad8", 608);
    ImGuiLua_SetEnumValue(L, "Keypad9", 609);
    ImGuiLua_SetEnumValue(L, "KeypadDecimal", 610);
    ImGuiLua_SetEnumValue(L, "KeypadDivide", 611);
    ImGuiLua_SetEnumValue(L, "KeypadMultiply", 612);
    ImGuiLua_SetEnumValue(L, "KeypadSubtract", 613);
    ImGuiLua_SetEnumValue(L, "KeypadAdd", 614);
    ImGuiLua_SetEnumValue(L, "KeypadEnter", 615);
    ImGuiLua_SetEnumValue(L, "KeypadEqual", 616);
    ImGuiLua_SetEnumValue(L, "GamepadStart", 617);
    ImGuiLua_SetEnumValue(L, "GamepadBack", 618);
    ImGuiLua_SetEnumValue(L, "GamepadFaceUp", 619);
    ImGuiLua_SetEnumValue(L, "GamepadFaceDown", 620);
    ImGuiLua_SetEnumValue(L, "GamepadFaceLeft", 621);
    ImGuiLua_SetEnumValue(L, "GamepadFaceRight", 622);
    ImGuiLua_SetEnumValue(L, "GamepadDpadUp", 623);
    ImGuiLua_SetEnumValue(L, "GamepadDpadDown", 624);
    ImGuiLua_SetEnumValue(L, "GamepadDpadLeft", 625);
    ImGuiLua_SetEnumValue(L, "GamepadDpadRight", 626);
    ImGuiLua_SetEnumValue(L, "GamepadL1", 627);
    ImGuiLua_SetEnumValue(L, "GamepadR1", 628);
    ImGuiLua_SetEnumValue(L, "GamepadL2", 629);
    ImGuiLua_SetEnumValue(L, "GamepadR2", 630);
    ImGuiLua_SetEnumValue(L, "GamepadL3", 631);
    ImGuiLua_SetEnumValue(L, "GamepadR3", 632);
    ImGuiLua_SetEnumValue(L, "GamepadLStickUp", 633);
    ImGuiLua_SetEnumValue(L, "GamepadLStickDown", 634);
    ImGuiLua_SetEnumValue(L, "GamepadLStickLeft", 635);
    ImGuiLua_SetEnumValue(L, "GamepadLStickRight", 636);
    ImGuiLua_SetEnumValue(L, "GamepadRStickUp", 637);
    ImGuiLua_SetEnumValue(L, "GamepadRStickDown", 638);
    ImGuiLua_SetEnumValue(L, "GamepadRStickLeft", 639);
    ImGuiLua_SetEnumValue(L, "GamepadRStickRight", 640);
    ImGuiLua_SetEnumValue(L, "ModCtrl", 641);
    ImGuiLua_SetEnumValue(L, "ModShift", 642);
    ImGuiLua_SetEnumValue(L, "ModAlt", 643);
    ImGuiLua_SetEnumValue(L, "ModSuper", 644);
    ImGuiLua_SetEnumValue(L, "COUNT", 645);
}

IMGUILUA_ENUM(ImGuiMouseButton)
static void ImGuiLua_RegisterEnumImGuiMouseButton(lua_State *L)
{
    ImGuiLua_SetEnumValue(L, "Left", 0);
    ImGuiLua_SetEnumValue(L, "Right", 1);
    ImGuiLua_SetEnumValue(L, "Middle", 2);
    ImGuiLua_SetEnumValue(L, "COUNT", 5);
}

IMGUILUA_ENUM(ImGuiMouseCursor)
static void ImGuiLua_RegisterEnumImGuiMouseCursor(lua_State *L)
{
    ImGuiLua_SetEnumValue(L, "None", -1);
    ImGuiLua_SetEnumValue(L, "Arrow", 0);
    ImGuiLua_SetEnumValue(L, "TextInput", 1);
    ImGuiLua_SetEnumValue(L, "ResizeAll", 2);
    ImGuiLua_SetEnumValue(L, "ResizeNS", 3);
    ImGuiLua_SetEnumValue(L, "ResizeEW", 4);
    ImGuiLua_SetEnumValue(L, "ResizeNESW", 5);
    ImGuiLua_SetEnumValue(L, "ResizeNWSE", 6);
    ImGuiLua_SetEnumValue(L, "Hand", 7);
    ImGuiLua_SetEnumValue(L, "NotAllowed", 8);
    ImGuiLua_SetEnumValue(L, "COUNT", 9);
}

IMGUILUA_ENUM(ImGuiNavInput)
static void ImGuiLua_RegisterEnumImGuiNavInput(lua_State *L)
{
    ImGuiLua_SetEnumValue(L, "Activate", 0);
    ImGuiLua_SetEnumValue(L, "Cancel", 1);
    ImGuiLua_SetEnumValue(L, "Input", 2);
    ImGuiLua_SetEnumValue(L, "Menu", 3);
    ImGuiLua_SetEnumValue(L, "DpadLeft", 4);
    ImGuiLua_SetEnumValue(L, "DpadRight", 5);
    ImGuiLua_SetEnumValue(L, "DpadUp", 6);
    ImGuiLua_SetEnumValue(L, "DpadDown", 7);
    ImGuiLua_SetEnumValue(L, "LStickLeft", 8);
    ImGuiLua_SetEnumValue(L, "LStickRight", 9);
    ImGuiLua_SetEnumValue(L, "LStickUp", 10);
    ImGuiLua_SetEnumValue(L, "LStickDown", 11);
    ImGuiLua_SetEnumValue(L, "FocusPrev", 12);
    ImGuiLua_SetEnumValue(L, "FocusNext", 13);
    ImGuiLua_SetEnumValue(L, "TweakSlow", 14);
    ImGuiLua_SetEnumValue(L, "TweakFast", 15);
    ImGuiLua_SetEnumValue(L, "KeyLeft_", 16);
    ImGuiLua_SetEnumValue(L, "KeyRight_", 17);
    ImGuiLua_SetEnumValue(L, "KeyUp_", 18);
    ImGuiLua_SetEnumValue(L, "KeyDown_", 19);
    ImGuiLua_SetEnumValue(L, "COUNT", 20);
}

IMGUILUA_ENUM(ImGuiPopupFlags)
static void ImGuiLua_RegisterEnumImGuiPopupFlags(lua_State *L)
{
    ImGuiLua_SetEnumValue(L, "None", 0);
    ImGuiLua_SetEnumValue(L, "MouseButtonLeft", 0);
    ImGuiLua_SetEnumValue(L, "MouseButtonRight", 1);
    ImGuiLua_SetEnumValue(L, "MouseButtonMiddle", 2);
    ImGuiLua_SetEnumValue(L, "MouseButtonMask_", 0x1F);
    ImGuiLua_SetEnumValue(L, "MouseButtonDefault_", 1);
    ImGuiLua_SetEnumValue(L, "NoOpenOverExistingPopup", 1 << 5);
    ImGuiLua_SetEnumValue(L, "NoOpenOverItems", 1 << 6);
    ImGuiLua_SetEnumValue(L, "AnyPopupId", 1 << 7);
    ImGuiLua_SetEnumValue(L, "AnyPopupLevel", 1 << 8);
    ImGuiLua_SetEnumValue(L, "AnyPopup",
                          ImGuiPopupFlags_AnyPopupId
                              | ImGuiPopupFlags_AnyPopupLevel);
}

IMGUILUA_ENUM(ImGuiSelectableFlags)
static void ImGuiLua_RegisterEnumImGuiSelectableFlags(lua_State *L)
{
    ImGuiLua_SetEnumValue(L, "None", 0);
    ImGuiLua_SetEnumValue(L, "DontClosePopups", 1 << 0);
    ImGuiLua_SetEnumValue(L, "SpanAllColumns", 1 << 1);
    ImGuiLua_SetEnumValue(L, "AllowDoubleClick", 1 << 2);
    ImGuiLua_SetEnumValue(L, "Disabled", 1 << 3);
    ImGuiLua_SetEnumValue(L, "AllowItemOverlap", 1 << 4);
}

IMGUILUA_ENUM(ImGuiSliderFlags)
static void ImGuiLua_RegisterEnumImGuiSliderFlags(lua_State *L)
{
    ImGuiLua_SetEnumValue(L, "None", 0);
    ImGuiLua_SetEnumValue(L, "AlwaysClamp", 1 << 4);
    ImGuiLua_SetEnumValue(L, "Logarithmic", 1 << 5);
    ImGuiLua_SetEnumValue(L, "NoRoundToFormat", 1 << 6);
    ImGuiLua_SetEnumValue(L, "NoInput", 1 << 7);
    ImGuiLua_SetEnumValue(L, "InvalidMask_", 0x7000000F);
}

IMGUILUA_ENUM(ImGuiSortDirection)
static void ImGuiLua_RegisterEnumImGuiSortDirection(lua_State *L)
{
    ImGuiLua_SetEnumValue(L, "None", 0);
    ImGuiLua_SetEnumValue(L, "Ascending", 1);
    ImGuiLua_SetEnumValue(L, "Descending", 2);
}

IMGUILUA_ENUM(ImGuiStyleVar)
static void ImGuiLua_RegisterEnumImGuiStyleVar(lua_State *L)
{
    ImGuiLua_SetEnumValue(L, "Alpha", 0);
    ImGuiLua_SetEnumValue(L, "DisabledAlpha", 1);
    ImGuiLua_SetEnumValue(L, "WindowPadding", 2);
    ImGuiLua_SetEnumValue(L, "WindowRounding", 3);
    ImGuiLua_SetEnumValue(L, "WindowBorderSize", 4);
    ImGuiLua_SetEnumValue(L, "WindowMinSize", 5);
    ImGuiLua_SetEnumValue(L, "WindowTitleAlign", 6);
    ImGuiLua_SetEnumValue(L, "ChildRounding", 7);
    ImGuiLua_SetEnumValue(L, "ChildBorderSize", 8);
    ImGuiLua_SetEnumValue(L, "PopupRounding", 9);
    ImGuiLua_SetEnumValue(L, "PopupBorderSize", 10);
    ImGuiLua_SetEnumValue(L, "FramePadding", 11);
    ImGuiLua_SetEnumValue(L, "FrameRounding", 12);
    ImGuiLua_SetEnumValue(L, "FrameBorderSize", 13);
    ImGuiLua_SetEnumValue(L, "ItemSpacing", 14);
    ImGuiLua_SetEnumValue(L, "ItemInnerSpacing", 15);
    ImGuiLua_SetEnumValue(L, "IndentSpacing", 16);
    ImGuiLua_SetEnumValue(L, "CellPadding", 17);
    ImGuiLua_SetEnumValue(L, "ScrollbarSize", 18);
    ImGuiLua_SetEnumValue(L, "ScrollbarRounding", 19);
    ImGuiLua_SetEnumValue(L, "GrabMinSize", 20);
    ImGuiLua_SetEnumValue(L, "GrabRounding", 21);
    ImGuiLua_SetEnumValue(L, "TabRounding", 22);
    ImGuiLua_SetEnumValue(L, "ButtonTextAlign", 23);
    ImGuiLua_SetEnumValue(L, "SelectableTextAlign", 24);
    ImGuiLua_SetEnumValue(L, "COUNT", 25);
}

IMGUILUA_ENUM(ImGuiTabBarFlags)
static void ImGuiLua_RegisterEnumImGuiTabBarFlags(lua_State *L)
{
    ImGuiLua_SetEnumValue(L, "None", 0);
    ImGuiLua_SetEnumValue(L, "Reorderable", 1 << 0);
    ImGuiLua_SetEnumValue(L, "AutoSelectNewTabs", 1 << 1);
    ImGuiLua_SetEnumValue(L, "TabListPopupButton", 1 << 2);
    ImGuiLua_SetEnumValue(L, "NoCloseWithMiddleMouseButton", 1 << 3);
    ImGuiLua_SetEnumValue(L, "NoTabListScrollingButtons", 1 << 4);
    ImGuiLua_SetEnumValue(L, "NoTooltip", 1 << 5);
    ImGuiLua_SetEnumValue(L, "FittingPolicyResizeDown", 1 << 6);
    ImGuiLua_SetEnumValue(L, "FittingPolicyScroll", 1 << 7);
    ImGuiLua_SetEnumValue(L, "FittingPolicyMask_",
                          ImGuiTabBarFlags_FittingPolicyResizeDown
                              | ImGuiTabBarFlags_FittingPolicyScroll);
    ImGuiLua_SetEnumValue(L, "FittingPolicyDefault_",
                          ImGuiTabBarFlags_FittingPolicyResizeDown);
}

IMGUILUA_ENUM(ImGuiTabItemFlags)
static void ImGuiLua_RegisterEnumImGuiTabItemFlags(lua_State *L)
{
    ImGuiLua_SetEnumValue(L, "None", 0);
    ImGuiLua_SetEnumValue(L, "UnsavedDocument", 1 << 0);
    ImGuiLua_SetEnumValue(L, "SetSelected", 1 << 1);
    ImGuiLua_SetEnumValue(L, "NoCloseWithMiddleMouseButton", 1 << 2);
    ImGuiLua_SetEnumValue(L, "NoPushId", 1 << 3);
    ImGuiLua_SetEnumValue(L, "NoTooltip", 1 << 4);
    ImGuiLua_SetEnumValue(L, "NoReorder", 1 << 5);
    ImGuiLua_SetEnumValue(L, "Leading", 1 << 6);
    ImGuiLua_SetEnumValue(L, "Trailing", 1 << 7);
}

IMGUILUA_ENUM(ImGuiTableBgTarget)
static void ImGuiLua_RegisterEnumImGuiTableBgTarget(lua_State *L)
{
    ImGuiLua_SetEnumValue(L, "None", 0);
    ImGuiLua_SetEnumValue(L, "RowBg0", 1);
    ImGuiLua_SetEnumValue(L, "RowBg1", 2);
    ImGuiLua_SetEnumValue(L, "CellBg", 3);
}

IMGUILUA_ENUM(ImGuiTableColumnFlags)
static void ImGuiLua_RegisterEnumImGuiTableColumnFlags(lua_State *L)
{
    ImGuiLua_SetEnumValue(L, "None", 0);
    ImGuiLua_SetEnumValue(L, "Disabled", 1 << 0);
    ImGuiLua_SetEnumValue(L, "DefaultHide", 1 << 1);
    ImGuiLua_SetEnumValue(L, "DefaultSort", 1 << 2);
    ImGuiLua_SetEnumValue(L, "WidthStretch", 1 << 3);
    ImGuiLua_SetEnumValue(L, "WidthFixed", 1 << 4);
    ImGuiLua_SetEnumValue(L, "NoResize", 1 << 5);
    ImGuiLua_SetEnumValue(L, "NoReorder", 1 << 6);
    ImGuiLua_SetEnumValue(L, "NoHide", 1 << 7);
    ImGuiLua_SetEnumValue(L, "NoClip", 1 << 8);
    ImGuiLua_SetEnumValue(L, "NoSort", 1 << 9);
    ImGuiLua_SetEnumValue(L, "NoSortAscending", 1 << 10);
    ImGuiLua_SetEnumValue(L, "NoSortDescending", 1 << 11);
    ImGuiLua_SetEnumValue(L, "NoHeaderLabel", 1 << 12);
    ImGuiLua_SetEnumValue(L, "NoHeaderWidth", 1 << 13);
    ImGuiLua_SetEnumValue(L, "PreferSortAscending", 1 << 14);
    ImGuiLua_SetEnumValue(L, "PreferSortDescending", 1 << 15);
    ImGuiLua_SetEnumValue(L, "IndentEnable", 1 << 16);
    ImGuiLua_SetEnumValue(L, "IndentDisable", 1 << 17);
    ImGuiLua_SetEnumValue(L, "IsEnabled", 1 << 24);
    ImGuiLua_SetEnumValue(L, "IsVisible", 1 << 25);
    ImGuiLua_SetEnumValue(L, "IsSorted", 1 << 26);
    ImGuiLua_SetEnumValue(L, "IsHovered", 1 << 27);
    ImGuiLua_SetEnumValue(L, "WidthMask_",
                          ImGuiTableColumnFlags_WidthStretch
                              | ImGuiTableColumnFlags_WidthFixed);
    ImGuiLua_SetEnumValue(L, "IndentMask_",
                          ImGuiTableColumnFlags_IndentEnable
                              | ImGuiTableColumnFlags_IndentDisable);
    ImGuiLua_SetEnumValue(
        L, "StatusMask_",
        ImGuiTableColumnFlags_IsEnabled | ImGuiTableColumnFlags_IsVisible
            | ImGuiTableColumnFlags_IsSorted | ImGuiTableColumnFlags_IsHovered);
    ImGuiLua_SetEnumValue(L, "NoDirectResize_", 1 << 30);
}

IMGUILUA_ENUM(ImGuiTableFlags)
static void ImGuiLua_RegisterEnumImGuiTableFlags(lua_State *L)
{
    ImGuiLua_SetEnumValue(L, "None", 0);
    ImGuiLua_SetEnumValue(L, "Resizable", 1 << 0);
    ImGuiLua_SetEnumValue(L, "Reorderable", 1 << 1);
    ImGuiLua_SetEnumValue(L, "Hideable", 1 << 2);
    ImGuiLua_SetEnumValue(L, "Sortable", 1 << 3);
    ImGuiLua_SetEnumValue(L, "NoSavedSettings", 1 << 4);
    ImGuiLua_SetEnumValue(L, "ContextMenuInBody", 1 << 5);
    ImGuiLua_SetEnumValue(L, "RowBg", 1 << 6);
    ImGuiLua_SetEnumValue(L, "BordersInnerH", 1 << 7);
    ImGuiLua_SetEnumValue(L, "BordersOuterH", 1 << 8);
    ImGuiLua_SetEnumValue(L, "BordersInnerV", 1 << 9);
    ImGuiLua_SetEnumValue(L, "BordersOuterV", 1 << 10);
    ImGuiLua_SetEnumValue(L, "BordersH",
                          ImGuiTableFlags_BordersInnerH
                              | ImGuiTableFlags_BordersOuterH);
    ImGuiLua_SetEnumValue(L, "BordersV",
                          ImGuiTableFlags_BordersInnerV
                              | ImGuiTableFlags_BordersOuterV);
    ImGuiLua_SetEnumValue(L, "BordersInner",
                          ImGuiTableFlags_BordersInnerV
                              | ImGuiTableFlags_BordersInnerH);
    ImGuiLua_SetEnumValue(L, "BordersOuter",
                          ImGuiTableFlags_BordersOuterV
                              | ImGuiTableFlags_BordersOuterH);
    ImGuiLua_SetEnumValue(L, "Borders",
                          ImGuiTableFlags_BordersInner
                              | ImGuiTableFlags_BordersOuter);
    ImGuiLua_SetEnumValue(L, "NoBordersInBody", 1 << 11);
    ImGuiLua_SetEnumValue(L, "NoBordersInBodyUntilResize", 1 << 12);
    ImGuiLua_SetEnumValue(L, "SizingFixedFit", 1 << 13);
    ImGuiLua_SetEnumValue(L, "SizingFixedSame", 2 << 13);
    ImGuiLua_SetEnumValue(L, "SizingStretchProp", 3 << 13);
    ImGuiLua_SetEnumValue(L, "SizingStretchSame", 4 << 13);
    ImGuiLua_SetEnumValue(L, "NoHostExtendX", 1 << 16);
    ImGuiLua_SetEnumValue(L, "NoHostExtendY", 1 << 17);
    ImGuiLua_SetEnumValue(L, "NoKeepColumnsVisible", 1 << 18);
    ImGuiLua_SetEnumValue(L, "PreciseWidths", 1 << 19);
    ImGuiLua_SetEnumValue(L, "NoClip", 1 << 20);
    ImGuiLua_SetEnumValue(L, "PadOuterX", 1 << 21);
    ImGuiLua_SetEnumValue(L, "NoPadOuterX", 1 << 22);
    ImGuiLua_SetEnumValue(L, "NoPadInnerX", 1 << 23);
    ImGuiLua_SetEnumValue(L, "ScrollX", 1 << 24);
    ImGuiLua_SetEnumValue(L, "ScrollY", 1 << 25);
    ImGuiLua_SetEnumValue(L, "SortMulti", 1 << 26);
    ImGuiLua_SetEnumValue(L, "SortTristate", 1 << 27);
    ImGuiLua_SetEnumValue(L, "SizingMask_",
                          ImGuiTableFlags_SizingFixedFit
                              | ImGuiTableFlags_SizingFixedSame
                              | ImGuiTableFlags_SizingStretchProp
                              | ImGuiTableFlags_SizingStretchSame);
}

IMGUILUA_ENUM(ImGuiTableRowFlags)
static void ImGuiLua_RegisterEnumImGuiTableRowFlags(lua_State *L)
{
    ImGuiLua_SetEnumValue(L, "None", 0);
    ImGuiLua_SetEnumValue(L, "Headers", 1 << 0);
}

IMGUILUA_ENUM(ImGuiTreeNodeFlags)
static void ImGuiLua_RegisterEnumImGuiTreeNodeFlags(lua_State *L)
{
    ImGuiLua_SetEnumValue(L, "None", 0);
    ImGuiLua_SetEnumValue(L, "Selected", 1 << 0);
    ImGuiLua_SetEnumValue(L, "Framed", 1 << 1);
    ImGuiLua_SetEnumValue(L, "AllowItemOverlap", 1 << 2);
    ImGuiLua_SetEnumValue(L, "NoTreePushOnOpen", 1 << 3);
    ImGuiLua_SetEnumValue(L, "NoAutoOpenOnLog", 1 << 4);
    ImGuiLua_SetEnumValue(L, "DefaultOpen", 1 << 5);
    ImGuiLua_SetEnumValue(L, "OpenOnDoubleClick", 1 << 6);
    ImGuiLua_SetEnumValue(L, "OpenOnArrow", 1 << 7);
    ImGuiLua_SetEnumValue(L, "Leaf", 1 << 8);
    ImGuiLua_SetEnumValue(L, "Bullet", 1 << 9);
    ImGuiLua_SetEnumValue(L, "FramePadding", 1 << 10);
    ImGuiLua_SetEnumValue(L, "SpanAvailWidth", 1 << 11);
    ImGuiLua_SetEnumValue(L, "SpanFullWidth", 1 << 12);
    ImGuiLua_SetEnumValue(L, "NavLeftJumpsBackHere", 1 << 13);
    ImGuiLua_SetEnumValue(L, "CollapsingHeader",
                          ImGuiTreeNodeFlags_Framed
                              | ImGuiTreeNodeFlags_NoTreePushOnOpen
                              | ImGuiTreeNodeFlags_NoAutoOpenOnLog);
}

IMGUILUA_ENUM(ImGuiViewportFlags)
static void ImGuiLua_RegisterEnumImGuiViewportFlags(lua_State *L)
{
    ImGuiLua_SetEnumValue(L, "None", 0);
    ImGuiLua_SetEnumValue(L, "IsPlatformWindow", 1 << 0);
    ImGuiLua_SetEnumValue(L, "IsPlatformMonitor", 1 << 1);
    ImGuiLua_SetEnumValue(L, "OwnedByApp", 1 << 2);
    ImGuiLua_SetEnumValue(L, "NoDecoration", 1 << 3);
    ImGuiLua_SetEnumValue(L, "NoTaskBarIcon", 1 << 4);
    ImGuiLua_SetEnumValue(L, "NoFocusOnAppearing", 1 << 5);
    ImGuiLua_SetEnumValue(L, "NoFocusOnClick", 1 << 6);
    ImGuiLua_SetEnumValue(L, "NoInputs", 1 << 7);
    ImGuiLua_SetEnumValue(L, "NoRendererClear", 1 << 8);
    ImGuiLua_SetEnumValue(L, "TopMost", 1 << 9);
    ImGuiLua_SetEnumValue(L, "Minimized", 1 << 10);
    ImGuiLua_SetEnumValue(L, "NoAutoMerge", 1 << 11);
    ImGuiLua_SetEnumValue(L, "CanHostOtherWindows", 1 << 12);
}

IMGUILUA_ENUM(ImGuiWindowFlags)
static void ImGuiLua_RegisterEnumImGuiWindowFlags(lua_State *L)
{
    ImGuiLua_SetEnumValue(L, "None", 0);
    ImGuiLua_SetEnumValue(L, "NoTitleBar", 1 << 0);
    ImGuiLua_SetEnumValue(L, "NoResize", 1 << 1);
    ImGuiLua_SetEnumValue(L, "NoMove", 1 << 2);
    ImGuiLua_SetEnumValue(L, "NoScrollbar", 1 << 3);
    ImGuiLua_SetEnumValue(L, "NoScrollWithMouse", 1 << 4);
    ImGuiLua_SetEnumValue(L, "NoCollapse", 1 << 5);
    ImGuiLua_SetEnumValue(L, "AlwaysAutoResize", 1 << 6);
    ImGuiLua_SetEnumValue(L, "NoBackground", 1 << 7);
    ImGuiLua_SetEnumValue(L, "NoSavedSettings", 1 << 8);
    ImGuiLua_SetEnumValue(L, "NoMouseInputs", 1 << 9);
    ImGuiLua_SetEnumValue(L, "MenuBar", 1 << 10);
    ImGuiLua_SetEnumValue(L, "HorizontalScrollbar", 1 << 11);
    ImGuiLua_SetEnumValue(L, "NoFocusOnAppearing", 1 << 12);
    ImGuiLua_SetEnumValue(L, "NoBringToFrontOnFocus", 1 << 13);
    ImGuiLua_SetEnumValue(L, "AlwaysVerticalScrollbar", 1 << 14);
    ImGuiLua_SetEnumValue(L, "AlwaysHorizontalScrollbar", 1 << 15);
    ImGuiLua_SetEnumValue(L, "AlwaysUseWindowPadding", 1 << 16);
    ImGuiLua_SetEnumValue(L, "NoNavInputs", 1 << 18);
    ImGuiLua_SetEnumValue(L, "NoNavFocus", 1 << 19);
    ImGuiLua_SetEnumValue(L, "UnsavedDocument", 1 << 20);
    ImGuiLua_SetEnumValue(L, "NoDocking", 1 << 21);
    ImGuiLua_SetEnumValue(
        L, "NoNav", ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus);
    ImGuiLua_SetEnumValue(
        L, "NoDecoration",
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse);
    ImGuiLua_SetEnumValue(L, "NoInputs",
                          ImGuiWindowFlags_NoMouseInputs
                              | ImGuiWindowFlags_NoNavInputs
                              | ImGuiWindowFlags_NoNavFocus);
    ImGuiLua_SetEnumValue(L, "NavFlattened", 1 << 23);
    ImGuiLua_SetEnumValue(L, "ChildWindow", 1 << 24);
    ImGuiLua_SetEnumValue(L, "Tooltip", 1 << 25);
    ImGuiLua_SetEnumValue(L, "Popup", 1 << 26);
    ImGuiLua_SetEnumValue(L, "Modal", 1 << 27);
    ImGuiLua_SetEnumValue(L, "ChildMenu", 1 << 28);
    ImGuiLua_SetEnumValue(L, "DockNodeHost", 1 << 29);
}


// struct ImColor: Unknown argument type 'ImColor*' (ImColor*)

// struct ImDrawChannel: Unknown argument type 'ImDrawChannel*' (ImDrawChannel*)

// struct ImDrawCmd: Unknown argument type 'ImDrawCmd*' (ImDrawCmd*)

// struct ImDrawCmdHeader: Unknown argument type 'ImDrawCmdHeader*'
// (ImDrawCmdHeader*)

// struct ImDrawData: Unknown argument type 'ImDrawData*' (ImDrawData*)

// struct ImDrawList: Unknown argument type 'ImDrawList*' (ImDrawList*)

// struct ImDrawListSplitter: Unknown argument type 'ImDrawListSplitter*'
// (ImDrawListSplitter*)

// struct ImDrawVert: Unknown argument type 'ImDrawVert*' (ImDrawVert*)

// struct ImFont: Unknown argument type 'ImFont*' (ImFont*)

// struct ImFontAtlas: Unknown argument type 'ImFontAtlas*' (ImFontAtlas*)

// struct ImFontAtlasCustomRect: Unknown argument type 'ImFontAtlasCustomRect*'
// (ImFontAtlasCustomRect*)

// struct ImFontConfig: Unknown argument type 'ImFontConfig*' (ImFontConfig*)

// struct ImFontGlyph: Unknown argument type 'ImFontGlyph*' (ImFontGlyph*)

// struct ImFontGlyphRangesBuilder: Unknown argument type
// 'ImFontGlyphRangesBuilder*' (ImFontGlyphRangesBuilder*)

static int ImGuiLua_IndexImGuiIO(lua_State *L)
{
    ImGuiIO *self;
    self = *static_cast<ImGuiIO **>(luaL_checkudata(L, 1, "ImGuiIO"));
    size_t len;
    const char *key = luaL_checklstring(L, 2, &len);

    if (len == 11 && memcmp(key, "ConfigFlags", 11) == 0) {
        int RETVAL = self->ConfigFlags;
        lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
        return 1;
    }

    if (len == 12 && memcmp(key, "BackendFlags", 12) == 0) {
        int RETVAL = self->BackendFlags;
        lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
        return 1;
    }

    if (len == 11 && memcmp(key, "DisplaySize", 11) == 0) {
        ImVec2 RETVAL = self->DisplaySize;
        ImGuiLua_PushImVec2(L, RETVAL);
        return 1;
    }

    if (len == 9 && memcmp(key, "DeltaTime", 9) == 0) {
        float RETVAL = self->DeltaTime;
        lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
        return 1;
    }

    if (len == 13 && memcmp(key, "IniSavingRate", 13) == 0) {
        float RETVAL = self->IniSavingRate;
        lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
        return 1;
    }

    if (len == 11 && memcmp(key, "IniFilename", 11) == 0) {
        const char *RETVAL = self->IniFilename;
        lua_pushstring(L, RETVAL);
        return 1;
    }

    if (len == 11 && memcmp(key, "LogFilename", 11) == 0) {
        const char *RETVAL = self->LogFilename;
        lua_pushstring(L, RETVAL);
        return 1;
    }

    if (len == 20 && memcmp(key, "MouseDoubleClickTime", 20) == 0) {
        float RETVAL = self->MouseDoubleClickTime;
        lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
        return 1;
    }

    if (len == 23 && memcmp(key, "MouseDoubleClickMaxDist", 23) == 0) {
        float RETVAL = self->MouseDoubleClickMaxDist;
        lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
        return 1;
    }

    if (len == 18 && memcmp(key, "MouseDragThreshold", 18) == 0) {
        float RETVAL = self->MouseDragThreshold;
        lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
        return 1;
    }

    if (len == 14 && memcmp(key, "KeyRepeatDelay", 14) == 0) {
        float RETVAL = self->KeyRepeatDelay;
        lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
        return 1;
    }

    if (len == 13 && memcmp(key, "KeyRepeatRate", 13) == 0) {
        float RETVAL = self->KeyRepeatRate;
        lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
        return 1;
    }

    // void* UserData: Unknown return type 'void*'

    // ImFontAtlas* Fonts: Unknown return type 'ImFontAtlas*'

    if (len == 15 && memcmp(key, "FontGlobalScale", 15) == 0) {
        float RETVAL = self->FontGlobalScale;
        lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
        return 1;
    }

    if (len == 20 && memcmp(key, "FontAllowUserScaling", 20) == 0) {
        bool RETVAL = self->FontAllowUserScaling;
        lua_pushboolean(L, RETVAL);
        return 1;
    }

    // ImFont* FontDefault: Unknown return type 'ImFont*'

    if (len == 23 && memcmp(key, "DisplayFramebufferScale", 23) == 0) {
        ImVec2 RETVAL = self->DisplayFramebufferScale;
        ImGuiLua_PushImVec2(L, RETVAL);
        return 1;
    }

    if (len == 20 && memcmp(key, "ConfigDockingNoSplit", 20) == 0) {
        bool RETVAL = self->ConfigDockingNoSplit;
        lua_pushboolean(L, RETVAL);
        return 1;
    }

    if (len == 22 && memcmp(key, "ConfigDockingWithShift", 22) == 0) {
        bool RETVAL = self->ConfigDockingWithShift;
        lua_pushboolean(L, RETVAL);
        return 1;
    }

    if (len == 25 && memcmp(key, "ConfigDockingAlwaysTabBar", 25) == 0) {
        bool RETVAL = self->ConfigDockingAlwaysTabBar;
        lua_pushboolean(L, RETVAL);
        return 1;
    }

    if (len == 31 && memcmp(key, "ConfigDockingTransparentPayload", 31) == 0) {
        bool RETVAL = self->ConfigDockingTransparentPayload;
        lua_pushboolean(L, RETVAL);
        return 1;
    }

    if (len == 26 && memcmp(key, "ConfigViewportsNoAutoMerge", 26) == 0) {
        bool RETVAL = self->ConfigViewportsNoAutoMerge;
        lua_pushboolean(L, RETVAL);
        return 1;
    }

    if (len == 28 && memcmp(key, "ConfigViewportsNoTaskBarIcon", 28) == 0) {
        bool RETVAL = self->ConfigViewportsNoTaskBarIcon;
        lua_pushboolean(L, RETVAL);
        return 1;
    }

    if (len == 27 && memcmp(key, "ConfigViewportsNoDecoration", 27) == 0) {
        bool RETVAL = self->ConfigViewportsNoDecoration;
        lua_pushboolean(L, RETVAL);
        return 1;
    }

    if (len == 30 && memcmp(key, "ConfigViewportsNoDefaultParent", 30) == 0) {
        bool RETVAL = self->ConfigViewportsNoDefaultParent;
        lua_pushboolean(L, RETVAL);
        return 1;
    }

    if (len == 15 && memcmp(key, "MouseDrawCursor", 15) == 0) {
        bool RETVAL = self->MouseDrawCursor;
        lua_pushboolean(L, RETVAL);
        return 1;
    }

    if (len == 21 && memcmp(key, "ConfigMacOSXBehaviors", 21) == 0) {
        bool RETVAL = self->ConfigMacOSXBehaviors;
        lua_pushboolean(L, RETVAL);
        return 1;
    }

    if (len == 28 && memcmp(key, "ConfigInputTrickleEventQueue", 28) == 0) {
        bool RETVAL = self->ConfigInputTrickleEventQueue;
        lua_pushboolean(L, RETVAL);
        return 1;
    }

    if (len == 26 && memcmp(key, "ConfigInputTextCursorBlink", 26) == 0) {
        bool RETVAL = self->ConfigInputTextCursorBlink;
        lua_pushboolean(L, RETVAL);
        return 1;
    }

    if (len == 26 && memcmp(key, "ConfigDragClickToInputText", 26) == 0) {
        bool RETVAL = self->ConfigDragClickToInputText;
        lua_pushboolean(L, RETVAL);
        return 1;
    }

    if (len == 28 && memcmp(key, "ConfigWindowsResizeFromEdges", 28) == 0) {
        bool RETVAL = self->ConfigWindowsResizeFromEdges;
        lua_pushboolean(L, RETVAL);
        return 1;
    }

    if (len == 33
        && memcmp(key, "ConfigWindowsMoveFromTitleBarOnly", 33) == 0) {
        bool RETVAL = self->ConfigWindowsMoveFromTitleBarOnly;
        lua_pushboolean(L, RETVAL);
        return 1;
    }

    if (len == 24 && memcmp(key, "ConfigMemoryCompactTimer", 24) == 0) {
        float RETVAL = self->ConfigMemoryCompactTimer;
        lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
        return 1;
    }

    if (len == 19 && memcmp(key, "BackendPlatformName", 19) == 0) {
        const char *RETVAL = self->BackendPlatformName;
        lua_pushstring(L, RETVAL);
        return 1;
    }

    if (len == 19 && memcmp(key, "BackendRendererName", 19) == 0) {
        const char *RETVAL = self->BackendRendererName;
        lua_pushstring(L, RETVAL);
        return 1;
    }

    // void* BackendPlatformUserData: Unknown return type 'void*'

    // void* BackendRendererUserData: Unknown return type 'void*'

    // void* BackendLanguageUserData: Unknown return type 'void*'

    // const char*(*)(void* user_data) GetClipboardTextFn: Unknown return type
    // 'const char*(*)(void* user_data)'

    // void(*)(void* user_data,const char* text) SetClipboardTextFn: Unknown
    // return type 'void(*)(void* user_data,const char* text)'

    // void* ClipboardUserData: Unknown return type 'void*'

    // void(*)(ImGuiViewport* viewport,ImGuiPlatformImeData* data)
    // SetPlatformImeDataFn: Unknown return type 'void(*)(ImGuiViewport*
    // viewport,ImGuiPlatformImeData* data)'

    // void* _UnusedPadding: Unknown return type 'void*'

    if (len == 16 && memcmp(key, "WantCaptureMouse", 16) == 0) {
        bool RETVAL = self->WantCaptureMouse;
        lua_pushboolean(L, RETVAL);
        return 1;
    }

    if (len == 19 && memcmp(key, "WantCaptureKeyboard", 19) == 0) {
        bool RETVAL = self->WantCaptureKeyboard;
        lua_pushboolean(L, RETVAL);
        return 1;
    }

    if (len == 13 && memcmp(key, "WantTextInput", 13) == 0) {
        bool RETVAL = self->WantTextInput;
        lua_pushboolean(L, RETVAL);
        return 1;
    }

    if (len == 15 && memcmp(key, "WantSetMousePos", 15) == 0) {
        bool RETVAL = self->WantSetMousePos;
        lua_pushboolean(L, RETVAL);
        return 1;
    }

    if (len == 19 && memcmp(key, "WantSaveIniSettings", 19) == 0) {
        bool RETVAL = self->WantSaveIniSettings;
        lua_pushboolean(L, RETVAL);
        return 1;
    }

    if (len == 9 && memcmp(key, "NavActive", 9) == 0) {
        bool RETVAL = self->NavActive;
        lua_pushboolean(L, RETVAL);
        return 1;
    }

    if (len == 10 && memcmp(key, "NavVisible", 10) == 0) {
        bool RETVAL = self->NavVisible;
        lua_pushboolean(L, RETVAL);
        return 1;
    }

    if (len == 9 && memcmp(key, "Framerate", 9) == 0) {
        float RETVAL = self->Framerate;
        lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
        return 1;
    }

    if (len == 21 && memcmp(key, "MetricsRenderVertices", 21) == 0) {
        int RETVAL = self->MetricsRenderVertices;
        lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
        return 1;
    }

    if (len == 20 && memcmp(key, "MetricsRenderIndices", 20) == 0) {
        int RETVAL = self->MetricsRenderIndices;
        lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
        return 1;
    }

    if (len == 20 && memcmp(key, "MetricsRenderWindows", 20) == 0) {
        int RETVAL = self->MetricsRenderWindows;
        lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
        return 1;
    }

    if (len == 20 && memcmp(key, "MetricsActiveWindows", 20) == 0) {
        int RETVAL = self->MetricsActiveWindows;
        lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
        return 1;
    }

    if (len == 24 && memcmp(key, "MetricsActiveAllocations", 24) == 0) {
        int RETVAL = self->MetricsActiveAllocations;
        lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
        return 1;
    }

    if (len == 10 && memcmp(key, "MouseDelta", 10) == 0) {
        ImVec2 RETVAL = self->MouseDelta;
        ImGuiLua_PushImVec2(L, RETVAL);
        return 1;
    }

    // int KeyMap[ImGuiKey_COUNT]: arrays not supported

    // bool KeysDown[512]: arrays not supported

    if (len == 8 && memcmp(key, "MousePos", 8) == 0) {
        ImVec2 RETVAL = self->MousePos;
        ImGuiLua_PushImVec2(L, RETVAL);
        return 1;
    }

    // bool MouseDown[5]: arrays not supported

    if (len == 10 && memcmp(key, "MouseWheel", 10) == 0) {
        float RETVAL = self->MouseWheel;
        lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
        return 1;
    }

    if (len == 11 && memcmp(key, "MouseWheelH", 11) == 0) {
        float RETVAL = self->MouseWheelH;
        lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
        return 1;
    }

    if (len == 20 && memcmp(key, "MouseHoveredViewport", 20) == 0) {
        unsigned int RETVAL = self->MouseHoveredViewport;
        lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
        return 1;
    }

    if (len == 7 && memcmp(key, "KeyCtrl", 7) == 0) {
        bool RETVAL = self->KeyCtrl;
        lua_pushboolean(L, RETVAL);
        return 1;
    }

    if (len == 8 && memcmp(key, "KeyShift", 8) == 0) {
        bool RETVAL = self->KeyShift;
        lua_pushboolean(L, RETVAL);
        return 1;
    }

    if (len == 6 && memcmp(key, "KeyAlt", 6) == 0) {
        bool RETVAL = self->KeyAlt;
        lua_pushboolean(L, RETVAL);
        return 1;
    }

    if (len == 8 && memcmp(key, "KeySuper", 8) == 0) {
        bool RETVAL = self->KeySuper;
        lua_pushboolean(L, RETVAL);
        return 1;
    }

    // float NavInputs[ImGuiNavInput_COUNT]: arrays not supported

    if (len == 7 && memcmp(key, "KeyMods", 7) == 0) {
        int RETVAL = self->KeyMods;
        lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
        return 1;
    }

    if (len == 11 && memcmp(key, "KeyModsPrev", 11) == 0) {
        int RETVAL = self->KeyModsPrev;
        lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
        return 1;
    }

    // ImGuiKeyData KeysData[645]: Unknown return type 'ImGuiKeyData'

    if (len == 32 && memcmp(key, "WantCaptureMouseUnlessPopupClose", 32) == 0) {
        bool RETVAL = self->WantCaptureMouseUnlessPopupClose;
        lua_pushboolean(L, RETVAL);
        return 1;
    }

    if (len == 12 && memcmp(key, "MousePosPrev", 12) == 0) {
        ImVec2 RETVAL = self->MousePosPrev;
        ImGuiLua_PushImVec2(L, RETVAL);
        return 1;
    }

    // ImVec2 MouseClickedPos[5]: arrays not supported

    // double MouseClickedTime[5]: arrays not supported

    // bool MouseClicked[5]: arrays not supported

    // bool MouseDoubleClicked[5]: arrays not supported

    // ImU16 MouseClickedCount[5]: Unknown return type 'ImU16'

    // ImU16 MouseClickedLastCount[5]: Unknown return type 'ImU16'

    // bool MouseReleased[5]: arrays not supported

    // bool MouseDownOwned[5]: arrays not supported

    // bool MouseDownOwnedUnlessPopupClose[5]: arrays not supported

    // float MouseDownDuration[5]: arrays not supported

    // float MouseDownDurationPrev[5]: arrays not supported

    // ImVec2 MouseDragMaxDistanceAbs[5]: arrays not supported

    // float MouseDragMaxDistanceSqr[5]: arrays not supported

    // float NavInputsDownDuration[ImGuiNavInput_COUNT]: arrays not supported

    // float NavInputsDownDurationPrev[ImGuiNavInput_COUNT]: arrays not
    // supported

    if (len == 11 && memcmp(key, "PenPressure", 11) == 0) {
        float RETVAL = self->PenPressure;
        lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
        return 1;
    }

    if (len == 12 && memcmp(key, "AppFocusLost", 12) == 0) {
        bool RETVAL = self->AppFocusLost;
        lua_pushboolean(L, RETVAL);
        return 1;
    }

    // ImS8 BackendUsingLegacyKeyArrays: Unknown return type 'ImS8'

    if (len == 31 && memcmp(key, "BackendUsingLegacyNavInputArray", 31) == 0) {
        bool RETVAL = self->BackendUsingLegacyNavInputArray;
        lua_pushboolean(L, RETVAL);
        return 1;
    }

    // ImWchar16 InputQueueSurrogate: Unknown return type 'ImWchar16'

    // ImVector_ImWchar InputQueueCharacters: Unknown return type
    // 'ImVector_ImWchar'

    luaL_getmetafield(L, 1, key);
    return 1;
}

static int ImGuiLua_NewindexImGuiIO(lua_State *L)
{
    ImGuiIO *self;
    self = *static_cast<ImGuiIO **>(luaL_checkudata(L, 1, "ImGuiIO"));
    size_t len;
    const char *key = luaL_checklstring(L, 2, &len);

    if (len == 11 && memcmp(key, "ConfigFlags", 11) == 0) {
        int ARGVAL;
        ARGVAL = static_cast<int>(luaL_checkinteger(L, 3));
        self->ConfigFlags = ARGVAL;
        return 0;
    }

    if (len == 12 && memcmp(key, "BackendFlags", 12) == 0) {
        int ARGVAL;
        ARGVAL = static_cast<int>(luaL_checkinteger(L, 3));
        self->BackendFlags = ARGVAL;
        return 0;
    }

    if (len == 11 && memcmp(key, "DisplaySize", 11) == 0) {
        ImVec2 ARGVAL;
        ARGVAL = ImGuiLua_ToImVec2(L, 3);
        self->DisplaySize = ARGVAL;
        return 0;
    }

    if (len == 9 && memcmp(key, "DeltaTime", 9) == 0) {
        float ARGVAL;
        ARGVAL = static_cast<float>(luaL_checknumber(L, 3));
        self->DeltaTime = ARGVAL;
        return 0;
    }

    if (len == 13 && memcmp(key, "IniSavingRate", 13) == 0) {
        float ARGVAL;
        ARGVAL = static_cast<float>(luaL_checknumber(L, 3));
        self->IniSavingRate = ARGVAL;
        return 0;
    }

    if (len == 11 && memcmp(key, "IniFilename", 11) == 0) {
        const char *ARGVAL;
        ARGVAL = lua_tostring(L, 3);
        self->IniFilename = ARGVAL;
        return 0;
    }

    if (len == 11 && memcmp(key, "LogFilename", 11) == 0) {
        const char *ARGVAL;
        ARGVAL = lua_tostring(L, 3);
        self->LogFilename = ARGVAL;
        return 0;
    }

    if (len == 20 && memcmp(key, "MouseDoubleClickTime", 20) == 0) {
        float ARGVAL;
        ARGVAL = static_cast<float>(luaL_checknumber(L, 3));
        self->MouseDoubleClickTime = ARGVAL;
        return 0;
    }

    if (len == 23 && memcmp(key, "MouseDoubleClickMaxDist", 23) == 0) {
        float ARGVAL;
        ARGVAL = static_cast<float>(luaL_checknumber(L, 3));
        self->MouseDoubleClickMaxDist = ARGVAL;
        return 0;
    }

    if (len == 18 && memcmp(key, "MouseDragThreshold", 18) == 0) {
        float ARGVAL;
        ARGVAL = static_cast<float>(luaL_checknumber(L, 3));
        self->MouseDragThreshold = ARGVAL;
        return 0;
    }

    if (len == 14 && memcmp(key, "KeyRepeatDelay", 14) == 0) {
        float ARGVAL;
        ARGVAL = static_cast<float>(luaL_checknumber(L, 3));
        self->KeyRepeatDelay = ARGVAL;
        return 0;
    }

    if (len == 13 && memcmp(key, "KeyRepeatRate", 13) == 0) {
        float ARGVAL;
        ARGVAL = static_cast<float>(luaL_checknumber(L, 3));
        self->KeyRepeatRate = ARGVAL;
        return 0;
    }

    // void* UserData: Unknown argument type 'void*'

    // ImFontAtlas* Fonts: Unknown argument type 'ImFontAtlas*'

    if (len == 15 && memcmp(key, "FontGlobalScale", 15) == 0) {
        float ARGVAL;
        ARGVAL = static_cast<float>(luaL_checknumber(L, 3));
        self->FontGlobalScale = ARGVAL;
        return 0;
    }

    if (len == 20 && memcmp(key, "FontAllowUserScaling", 20) == 0) {
        bool ARGVAL;
        ARGVAL = lua_toboolean(L, 3);
        self->FontAllowUserScaling = ARGVAL;
        return 0;
    }

    // ImFont* FontDefault: Unknown argument type 'ImFont*'

    if (len == 23 && memcmp(key, "DisplayFramebufferScale", 23) == 0) {
        ImVec2 ARGVAL;
        ARGVAL = ImGuiLua_ToImVec2(L, 3);
        self->DisplayFramebufferScale = ARGVAL;
        return 0;
    }

    if (len == 20 && memcmp(key, "ConfigDockingNoSplit", 20) == 0) {
        bool ARGVAL;
        ARGVAL = lua_toboolean(L, 3);
        self->ConfigDockingNoSplit = ARGVAL;
        return 0;
    }

    if (len == 22 && memcmp(key, "ConfigDockingWithShift", 22) == 0) {
        bool ARGVAL;
        ARGVAL = lua_toboolean(L, 3);
        self->ConfigDockingWithShift = ARGVAL;
        return 0;
    }

    if (len == 25 && memcmp(key, "ConfigDockingAlwaysTabBar", 25) == 0) {
        bool ARGVAL;
        ARGVAL = lua_toboolean(L, 3);
        self->ConfigDockingAlwaysTabBar = ARGVAL;
        return 0;
    }

    if (len == 31 && memcmp(key, "ConfigDockingTransparentPayload", 31) == 0) {
        bool ARGVAL;
        ARGVAL = lua_toboolean(L, 3);
        self->ConfigDockingTransparentPayload = ARGVAL;
        return 0;
    }

    if (len == 26 && memcmp(key, "ConfigViewportsNoAutoMerge", 26) == 0) {
        bool ARGVAL;
        ARGVAL = lua_toboolean(L, 3);
        self->ConfigViewportsNoAutoMerge = ARGVAL;
        return 0;
    }

    if (len == 28 && memcmp(key, "ConfigViewportsNoTaskBarIcon", 28) == 0) {
        bool ARGVAL;
        ARGVAL = lua_toboolean(L, 3);
        self->ConfigViewportsNoTaskBarIcon = ARGVAL;
        return 0;
    }

    if (len == 27 && memcmp(key, "ConfigViewportsNoDecoration", 27) == 0) {
        bool ARGVAL;
        ARGVAL = lua_toboolean(L, 3);
        self->ConfigViewportsNoDecoration = ARGVAL;
        return 0;
    }

    if (len == 30 && memcmp(key, "ConfigViewportsNoDefaultParent", 30) == 0) {
        bool ARGVAL;
        ARGVAL = lua_toboolean(L, 3);
        self->ConfigViewportsNoDefaultParent = ARGVAL;
        return 0;
    }

    if (len == 15 && memcmp(key, "MouseDrawCursor", 15) == 0) {
        bool ARGVAL;
        ARGVAL = lua_toboolean(L, 3);
        self->MouseDrawCursor = ARGVAL;
        return 0;
    }

    if (len == 21 && memcmp(key, "ConfigMacOSXBehaviors", 21) == 0) {
        bool ARGVAL;
        ARGVAL = lua_toboolean(L, 3);
        self->ConfigMacOSXBehaviors = ARGVAL;
        return 0;
    }

    if (len == 28 && memcmp(key, "ConfigInputTrickleEventQueue", 28) == 0) {
        bool ARGVAL;
        ARGVAL = lua_toboolean(L, 3);
        self->ConfigInputTrickleEventQueue = ARGVAL;
        return 0;
    }

    if (len == 26 && memcmp(key, "ConfigInputTextCursorBlink", 26) == 0) {
        bool ARGVAL;
        ARGVAL = lua_toboolean(L, 3);
        self->ConfigInputTextCursorBlink = ARGVAL;
        return 0;
    }

    if (len == 26 && memcmp(key, "ConfigDragClickToInputText", 26) == 0) {
        bool ARGVAL;
        ARGVAL = lua_toboolean(L, 3);
        self->ConfigDragClickToInputText = ARGVAL;
        return 0;
    }

    if (len == 28 && memcmp(key, "ConfigWindowsResizeFromEdges", 28) == 0) {
        bool ARGVAL;
        ARGVAL = lua_toboolean(L, 3);
        self->ConfigWindowsResizeFromEdges = ARGVAL;
        return 0;
    }

    if (len == 33
        && memcmp(key, "ConfigWindowsMoveFromTitleBarOnly", 33) == 0) {
        bool ARGVAL;
        ARGVAL = lua_toboolean(L, 3);
        self->ConfigWindowsMoveFromTitleBarOnly = ARGVAL;
        return 0;
    }

    if (len == 24 && memcmp(key, "ConfigMemoryCompactTimer", 24) == 0) {
        float ARGVAL;
        ARGVAL = static_cast<float>(luaL_checknumber(L, 3));
        self->ConfigMemoryCompactTimer = ARGVAL;
        return 0;
    }

    if (len == 19 && memcmp(key, "BackendPlatformName", 19) == 0) {
        const char *ARGVAL;
        ARGVAL = lua_tostring(L, 3);
        self->BackendPlatformName = ARGVAL;
        return 0;
    }

    if (len == 19 && memcmp(key, "BackendRendererName", 19) == 0) {
        const char *ARGVAL;
        ARGVAL = lua_tostring(L, 3);
        self->BackendRendererName = ARGVAL;
        return 0;
    }

    // void* BackendPlatformUserData: Unknown argument type 'void*'

    // void* BackendRendererUserData: Unknown argument type 'void*'

    // void* BackendLanguageUserData: Unknown argument type 'void*'

    // const char*(*)(void* user_data) GetClipboardTextFn: Unknown argument type
    // 'const char*(*)(void* user_data)'

    // void(*)(void* user_data,const char* text) SetClipboardTextFn: Unknown
    // argument type 'void(*)(void* user_data,const char* text)'

    // void* ClipboardUserData: Unknown argument type 'void*'

    // void(*)(ImGuiViewport* viewport,ImGuiPlatformImeData* data)
    // SetPlatformImeDataFn: Unknown argument type 'void(*)(ImGuiViewport*
    // viewport,ImGuiPlatformImeData* data)'

    // void* _UnusedPadding: Unknown argument type 'void*'

    if (len == 16 && memcmp(key, "WantCaptureMouse", 16) == 0) {
        bool ARGVAL;
        ARGVAL = lua_toboolean(L, 3);
        self->WantCaptureMouse = ARGVAL;
        return 0;
    }

    if (len == 19 && memcmp(key, "WantCaptureKeyboard", 19) == 0) {
        bool ARGVAL;
        ARGVAL = lua_toboolean(L, 3);
        self->WantCaptureKeyboard = ARGVAL;
        return 0;
    }

    if (len == 13 && memcmp(key, "WantTextInput", 13) == 0) {
        bool ARGVAL;
        ARGVAL = lua_toboolean(L, 3);
        self->WantTextInput = ARGVAL;
        return 0;
    }

    if (len == 15 && memcmp(key, "WantSetMousePos", 15) == 0) {
        bool ARGVAL;
        ARGVAL = lua_toboolean(L, 3);
        self->WantSetMousePos = ARGVAL;
        return 0;
    }

    if (len == 19 && memcmp(key, "WantSaveIniSettings", 19) == 0) {
        bool ARGVAL;
        ARGVAL = lua_toboolean(L, 3);
        self->WantSaveIniSettings = ARGVAL;
        return 0;
    }

    if (len == 9 && memcmp(key, "NavActive", 9) == 0) {
        bool ARGVAL;
        ARGVAL = lua_toboolean(L, 3);
        self->NavActive = ARGVAL;
        return 0;
    }

    if (len == 10 && memcmp(key, "NavVisible", 10) == 0) {
        bool ARGVAL;
        ARGVAL = lua_toboolean(L, 3);
        self->NavVisible = ARGVAL;
        return 0;
    }

    if (len == 9 && memcmp(key, "Framerate", 9) == 0) {
        float ARGVAL;
        ARGVAL = static_cast<float>(luaL_checknumber(L, 3));
        self->Framerate = ARGVAL;
        return 0;
    }

    if (len == 21 && memcmp(key, "MetricsRenderVertices", 21) == 0) {
        int ARGVAL;
        ARGVAL = static_cast<int>(luaL_checkinteger(L, 3));
        self->MetricsRenderVertices = ARGVAL;
        return 0;
    }

    if (len == 20 && memcmp(key, "MetricsRenderIndices", 20) == 0) {
        int ARGVAL;
        ARGVAL = static_cast<int>(luaL_checkinteger(L, 3));
        self->MetricsRenderIndices = ARGVAL;
        return 0;
    }

    if (len == 20 && memcmp(key, "MetricsRenderWindows", 20) == 0) {
        int ARGVAL;
        ARGVAL = static_cast<int>(luaL_checkinteger(L, 3));
        self->MetricsRenderWindows = ARGVAL;
        return 0;
    }

    if (len == 20 && memcmp(key, "MetricsActiveWindows", 20) == 0) {
        int ARGVAL;
        ARGVAL = static_cast<int>(luaL_checkinteger(L, 3));
        self->MetricsActiveWindows = ARGVAL;
        return 0;
    }

    if (len == 24 && memcmp(key, "MetricsActiveAllocations", 24) == 0) {
        int ARGVAL;
        ARGVAL = static_cast<int>(luaL_checkinteger(L, 3));
        self->MetricsActiveAllocations = ARGVAL;
        return 0;
    }

    if (len == 10 && memcmp(key, "MouseDelta", 10) == 0) {
        ImVec2 ARGVAL;
        ARGVAL = ImGuiLua_ToImVec2(L, 3);
        self->MouseDelta = ARGVAL;
        return 0;
    }

    // int KeyMap[ImGuiKey_COUNT]: arrays not supported

    // bool KeysDown[512]: arrays not supported

    if (len == 8 && memcmp(key, "MousePos", 8) == 0) {
        ImVec2 ARGVAL;
        ARGVAL = ImGuiLua_ToImVec2(L, 3);
        self->MousePos = ARGVAL;
        return 0;
    }

    // bool MouseDown[5]: arrays not supported

    if (len == 10 && memcmp(key, "MouseWheel", 10) == 0) {
        float ARGVAL;
        ARGVAL = static_cast<float>(luaL_checknumber(L, 3));
        self->MouseWheel = ARGVAL;
        return 0;
    }

    if (len == 11 && memcmp(key, "MouseWheelH", 11) == 0) {
        float ARGVAL;
        ARGVAL = static_cast<float>(luaL_checknumber(L, 3));
        self->MouseWheelH = ARGVAL;
        return 0;
    }

    if (len == 20 && memcmp(key, "MouseHoveredViewport", 20) == 0) {
        unsigned int ARGVAL;
        ARGVAL = static_cast<unsigned int>(luaL_checkinteger(L, 3));
        self->MouseHoveredViewport = ARGVAL;
        return 0;
    }

    if (len == 7 && memcmp(key, "KeyCtrl", 7) == 0) {
        bool ARGVAL;
        ARGVAL = lua_toboolean(L, 3);
        self->KeyCtrl = ARGVAL;
        return 0;
    }

    if (len == 8 && memcmp(key, "KeyShift", 8) == 0) {
        bool ARGVAL;
        ARGVAL = lua_toboolean(L, 3);
        self->KeyShift = ARGVAL;
        return 0;
    }

    if (len == 6 && memcmp(key, "KeyAlt", 6) == 0) {
        bool ARGVAL;
        ARGVAL = lua_toboolean(L, 3);
        self->KeyAlt = ARGVAL;
        return 0;
    }

    if (len == 8 && memcmp(key, "KeySuper", 8) == 0) {
        bool ARGVAL;
        ARGVAL = lua_toboolean(L, 3);
        self->KeySuper = ARGVAL;
        return 0;
    }

    // float NavInputs[ImGuiNavInput_COUNT]: arrays not supported

    if (len == 7 && memcmp(key, "KeyMods", 7) == 0) {
        int ARGVAL;
        ARGVAL = static_cast<int>(luaL_checkinteger(L, 3));
        self->KeyMods = ARGVAL;
        return 0;
    }

    if (len == 11 && memcmp(key, "KeyModsPrev", 11) == 0) {
        int ARGVAL;
        ARGVAL = static_cast<int>(luaL_checkinteger(L, 3));
        self->KeyModsPrev = ARGVAL;
        return 0;
    }

    // ImGuiKeyData KeysData[645]: Unknown argument type 'ImGuiKeyData'

    if (len == 32 && memcmp(key, "WantCaptureMouseUnlessPopupClose", 32) == 0) {
        bool ARGVAL;
        ARGVAL = lua_toboolean(L, 3);
        self->WantCaptureMouseUnlessPopupClose = ARGVAL;
        return 0;
    }

    if (len == 12 && memcmp(key, "MousePosPrev", 12) == 0) {
        ImVec2 ARGVAL;
        ARGVAL = ImGuiLua_ToImVec2(L, 3);
        self->MousePosPrev = ARGVAL;
        return 0;
    }

    // ImVec2 MouseClickedPos[5]: arrays not supported

    // double MouseClickedTime[5]: arrays not supported

    // bool MouseClicked[5]: arrays not supported

    // bool MouseDoubleClicked[5]: arrays not supported

    // ImU16 MouseClickedCount[5]: Unknown argument type 'ImU16'

    // ImU16 MouseClickedLastCount[5]: Unknown argument type 'ImU16'

    // bool MouseReleased[5]: arrays not supported

    // bool MouseDownOwned[5]: arrays not supported

    // bool MouseDownOwnedUnlessPopupClose[5]: arrays not supported

    // float MouseDownDuration[5]: arrays not supported

    // float MouseDownDurationPrev[5]: arrays not supported

    // ImVec2 MouseDragMaxDistanceAbs[5]: arrays not supported

    // float MouseDragMaxDistanceSqr[5]: arrays not supported

    // float NavInputsDownDuration[ImGuiNavInput_COUNT]: arrays not supported

    // float NavInputsDownDurationPrev[ImGuiNavInput_COUNT]: arrays not
    // supported

    if (len == 11 && memcmp(key, "PenPressure", 11) == 0) {
        float ARGVAL;
        ARGVAL = static_cast<float>(luaL_checknumber(L, 3));
        self->PenPressure = ARGVAL;
        return 0;
    }

    if (len == 12 && memcmp(key, "AppFocusLost", 12) == 0) {
        bool ARGVAL;
        ARGVAL = lua_toboolean(L, 3);
        self->AppFocusLost = ARGVAL;
        return 0;
    }

    // ImS8 BackendUsingLegacyKeyArrays: Unknown argument type 'ImS8'

    if (len == 31 && memcmp(key, "BackendUsingLegacyNavInputArray", 31) == 0) {
        bool ARGVAL;
        ARGVAL = lua_toboolean(L, 3);
        self->BackendUsingLegacyNavInputArray = ARGVAL;
        return 0;
    }

    // ImWchar16 InputQueueSurrogate: Unknown argument type 'ImWchar16'

    // ImVector_ImWchar InputQueueCharacters: Unknown argument type
    // 'ImVector_ImWchar'

    return luaL_error(L, "ImGuiIO has no field '%s'", key);
}

// method ImGuiIO::AddFocusEvent

IMGUILUA_METHOD(ImGuiIO::AddFocusEvent)
static int ImGuiIO_AddFocusEvent(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(2);
    ImGuiIO *self;
    self = *static_cast<ImGuiIO **>(luaL_checkudata(L, 1, "ImGuiIO"));
    bool focused;
    focused = lua_toboolean(L, 2);
    self->AddFocusEvent(focused);
    return 0;
}


// method ImGuiIO::AddInputCharacter

IMGUILUA_METHOD(ImGuiIO::AddInputCharacter)
static int ImGuiIO_AddInputCharacter(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(2);
    ImGuiIO *self;
    self = *static_cast<ImGuiIO **>(luaL_checkudata(L, 1, "ImGuiIO"));
    unsigned int c;
    c = static_cast<unsigned int>(luaL_checkinteger(L, 2));
    self->AddInputCharacter(c);
    return 0;
}


// method ImGuiIO::AddInputCharacterUTF16

// void ImGuiIO_AddInputCharacterUTF16(ImGuiIO*, ImWchar16): Unknown argument
// type 'ImWchar16' (ImWchar16)


// method ImGuiIO::AddInputCharactersUTF8

IMGUILUA_METHOD(ImGuiIO::AddInputCharactersUTF8)
static int ImGuiIO_AddInputCharactersUTF8(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(2);
    ImGuiIO *self;
    self = *static_cast<ImGuiIO **>(luaL_checkudata(L, 1, "ImGuiIO"));
    const char *str;
    str = lua_tostring(L, 2);
    self->AddInputCharactersUTF8(str);
    return 0;
}


// method ImGuiIO::AddKeyAnalogEvent

IMGUILUA_METHOD(ImGuiIO::AddKeyAnalogEvent)
static int ImGuiIO_AddKeyAnalogEvent(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(4);
    ImGuiIO *self;
    self = *static_cast<ImGuiIO **>(luaL_checkudata(L, 1, "ImGuiIO"));
    int key;
    key = static_cast<int>(luaL_checkinteger(L, 2));
    bool down;
    down = lua_toboolean(L, 3);
    float v;
    v = static_cast<float>(luaL_checknumber(L, 4));
    self->AddKeyAnalogEvent(key, down, v);
    return 0;
}


// method ImGuiIO::AddKeyEvent

IMGUILUA_METHOD(ImGuiIO::AddKeyEvent)
static int ImGuiIO_AddKeyEvent(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(3);
    ImGuiIO *self;
    self = *static_cast<ImGuiIO **>(luaL_checkudata(L, 1, "ImGuiIO"));
    int key;
    key = static_cast<int>(luaL_checkinteger(L, 2));
    bool down;
    down = lua_toboolean(L, 3);
    self->AddKeyEvent(key, down);
    return 0;
}


// method ImGuiIO::AddMouseButtonEvent

IMGUILUA_METHOD(ImGuiIO::AddMouseButtonEvent)
static int ImGuiIO_AddMouseButtonEvent(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(3);
    ImGuiIO *self;
    self = *static_cast<ImGuiIO **>(luaL_checkudata(L, 1, "ImGuiIO"));
    int button;
    button = static_cast<int>(luaL_checkinteger(L, 2));
    bool down;
    down = lua_toboolean(L, 3);
    self->AddMouseButtonEvent(button, down);
    return 0;
}


// method ImGuiIO::AddMousePosEvent

IMGUILUA_METHOD(ImGuiIO::AddMousePosEvent)
static int ImGuiIO_AddMousePosEvent(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(3);
    ImGuiIO *self;
    self = *static_cast<ImGuiIO **>(luaL_checkudata(L, 1, "ImGuiIO"));
    float x;
    x = static_cast<float>(luaL_checknumber(L, 2));
    float y;
    y = static_cast<float>(luaL_checknumber(L, 3));
    self->AddMousePosEvent(x, y);
    return 0;
}


// method ImGuiIO::AddMouseViewportEvent

IMGUILUA_METHOD(ImGuiIO::AddMouseViewportEvent)
static int ImGuiIO_AddMouseViewportEvent(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(2);
    ImGuiIO *self;
    self = *static_cast<ImGuiIO **>(luaL_checkudata(L, 1, "ImGuiIO"));
    unsigned int id;
    id = static_cast<unsigned int>(luaL_checkinteger(L, 2));
    self->AddMouseViewportEvent(id);
    return 0;
}


// method ImGuiIO::AddMouseWheelEvent

IMGUILUA_METHOD(ImGuiIO::AddMouseWheelEvent)
static int ImGuiIO_AddMouseWheelEvent(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(3);
    ImGuiIO *self;
    self = *static_cast<ImGuiIO **>(luaL_checkudata(L, 1, "ImGuiIO"));
    float wh_x;
    wh_x = static_cast<float>(luaL_checknumber(L, 2));
    float wh_y;
    wh_y = static_cast<float>(luaL_checknumber(L, 3));
    self->AddMouseWheelEvent(wh_x, wh_y);
    return 0;
}


// method ImGuiIO::ClearInputCharacters

IMGUILUA_METHOD(ImGuiIO::ClearInputCharacters)
static int ImGuiIO_ClearInputCharacters(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    ImGuiIO *self;
    self = *static_cast<ImGuiIO **>(luaL_checkudata(L, 1, "ImGuiIO"));
    self->ClearInputCharacters();
    return 0;
}


// method ImGuiIO::ClearInputKeys

IMGUILUA_METHOD(ImGuiIO::ClearInputKeys)
static int ImGuiIO_ClearInputKeys(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    ImGuiIO *self;
    self = *static_cast<ImGuiIO **>(luaL_checkudata(L, 1, "ImGuiIO"));
    self->ClearInputKeys();
    return 0;
}


// method ImGuiIO::SetKeyEventNativeData

IMGUILUA_METHOD(ImGuiIO::SetKeyEventNativeData)
static int ImGuiIO_SetKeyEventNativeData(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(4, 5);
    ImGuiIO *self;
    self = *static_cast<ImGuiIO **>(luaL_checkudata(L, 1, "ImGuiIO"));
    int key;
    key = static_cast<int>(luaL_checkinteger(L, 2));
    int native_keycode;
    native_keycode = static_cast<int>(luaL_checkinteger(L, 3));
    int native_scancode;
    native_scancode = static_cast<int>(luaL_checkinteger(L, 4));
    int native_legacy_index;
    if (ARGC < 5) {
        native_legacy_index = -1;
    }
    else {
        native_legacy_index = static_cast<int>(luaL_checkinteger(L, 5));
    }
    self->SetKeyEventNativeData(key, native_keycode, native_scancode,
                                native_legacy_index);
    return 0;
}


IMGUILUA_STRUCT(ImGuiIO)
static void ImGuiLua_RegisterStructImGuiIO(lua_State *L)
{
    luaL_newmetatable(L, "ImGuiIO");
    lua_pushcfunction(L, ImGuiLua_IndexImGuiIO);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, ImGuiLua_NewindexImGuiIO);
    lua_setfield(L, -2, "__newindex");
    lua_pushcfunction(L, ImGuiIO_AddFocusEvent);
    lua_setfield(L, -2, "AddFocusEvent");
    lua_pushcfunction(L, ImGuiIO_AddInputCharacter);
    lua_setfield(L, -2, "AddInputCharacter");
    lua_pushcfunction(L, ImGuiIO_AddInputCharactersUTF8);
    lua_setfield(L, -2, "AddInputCharactersUTF8");
    lua_pushcfunction(L, ImGuiIO_AddKeyAnalogEvent);
    lua_setfield(L, -2, "AddKeyAnalogEvent");
    lua_pushcfunction(L, ImGuiIO_AddKeyEvent);
    lua_setfield(L, -2, "AddKeyEvent");
    lua_pushcfunction(L, ImGuiIO_AddMouseButtonEvent);
    lua_setfield(L, -2, "AddMouseButtonEvent");
    lua_pushcfunction(L, ImGuiIO_AddMousePosEvent);
    lua_setfield(L, -2, "AddMousePosEvent");
    lua_pushcfunction(L, ImGuiIO_AddMouseViewportEvent);
    lua_setfield(L, -2, "AddMouseViewportEvent");
    lua_pushcfunction(L, ImGuiIO_AddMouseWheelEvent);
    lua_setfield(L, -2, "AddMouseWheelEvent");
    lua_pushcfunction(L, ImGuiIO_ClearInputCharacters);
    lua_setfield(L, -2, "ClearInputCharacters");
    lua_pushcfunction(L, ImGuiIO_ClearInputKeys);
    lua_setfield(L, -2, "ClearInputKeys");
    lua_pushcfunction(L, ImGuiIO_SetKeyEventNativeData);
    lua_setfield(L, -2, "SetKeyEventNativeData");
    lua_pop(L, 1);
}

// struct ImGuiInputTextCallbackData: Unknown argument type
// 'ImGuiInputTextCallbackData*' (ImGuiInputTextCallbackData*)

// struct ImGuiKeyData: Unknown argument type 'ImGuiKeyData*' (ImGuiKeyData*)

// struct ImGuiListClipper: Unknown argument type 'ImGuiListClipper*'
// (ImGuiListClipper*)

// struct ImGuiOnceUponAFrame: Unknown argument type 'ImGuiOnceUponAFrame*'
// (ImGuiOnceUponAFrame*)

// struct ImGuiPayload: Unknown argument type 'ImGuiPayload*' (ImGuiPayload*)

// struct ImGuiPlatformIO: Unknown argument type 'ImGuiPlatformIO*'
// (ImGuiPlatformIO*)

// struct ImGuiPlatformImeData: Unknown argument type 'ImGuiPlatformImeData*'
// (ImGuiPlatformImeData*)

// struct ImGuiPlatformMonitor: Unknown argument type 'ImGuiPlatformMonitor*'
// (ImGuiPlatformMonitor*)

// struct ImGuiSizeCallbackData: Unknown argument type 'ImGuiSizeCallbackData*'
// (ImGuiSizeCallbackData*)

// struct ImGuiStorage: Unknown argument type 'ImGuiStorage*' (ImGuiStorage*)

// struct ImGuiStoragePair: Unknown argument type 'ImGuiStoragePair*'
// (ImGuiStoragePair*)

// struct ImGuiStyle: Unknown argument type 'ImGuiStyle*' (ImGuiStyle*)

// struct ImGuiTableColumnSortSpecs: Unknown argument type
// 'ImGuiTableColumnSortSpecs*' (ImGuiTableColumnSortSpecs*)

// struct ImGuiTableSortSpecs: Unknown argument type 'ImGuiTableSortSpecs*'
// (ImGuiTableSortSpecs*)

// struct ImGuiTextBuffer: Unknown argument type 'ImGuiTextBuffer*'
// (ImGuiTextBuffer*)

// struct ImGuiTextFilter: Unknown argument type 'ImGuiTextFilter*'
// (ImGuiTextFilter*)

// struct ImGuiTextRange: Unknown argument type 'ImGuiTextRange*'
// (ImGuiTextRange*)

static int ImGuiLua_IndexImGuiViewport(lua_State *L)
{
    ImGuiViewport *self;
    self =
        *static_cast<ImGuiViewport **>(luaL_checkudata(L, 1, "ImGuiViewport"));
    size_t len;
    const char *key = luaL_checklstring(L, 2, &len);

    if (len == 2 && memcmp(key, "ID", 2) == 0) {
        unsigned int RETVAL = self->ID;
        lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
        return 1;
    }

    if (len == 5 && memcmp(key, "Flags", 5) == 0) {
        int RETVAL = self->Flags;
        lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
        return 1;
    }

    if (len == 3 && memcmp(key, "Pos", 3) == 0) {
        ImVec2 RETVAL = self->Pos;
        ImGuiLua_PushImVec2(L, RETVAL);
        return 1;
    }

    if (len == 4 && memcmp(key, "Size", 4) == 0) {
        ImVec2 RETVAL = self->Size;
        ImGuiLua_PushImVec2(L, RETVAL);
        return 1;
    }

    if (len == 7 && memcmp(key, "WorkPos", 7) == 0) {
        ImVec2 RETVAL = self->WorkPos;
        ImGuiLua_PushImVec2(L, RETVAL);
        return 1;
    }

    if (len == 8 && memcmp(key, "WorkSize", 8) == 0) {
        ImVec2 RETVAL = self->WorkSize;
        ImGuiLua_PushImVec2(L, RETVAL);
        return 1;
    }

    if (len == 8 && memcmp(key, "DpiScale", 8) == 0) {
        float RETVAL = self->DpiScale;
        lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
        return 1;
    }

    if (len == 16 && memcmp(key, "ParentViewportId", 16) == 0) {
        unsigned int RETVAL = self->ParentViewportId;
        lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
        return 1;
    }

    // ImDrawData* DrawData: Unknown return type 'ImDrawData*'

    // void* RendererUserData: Unknown return type 'void*'

    // void* PlatformUserData: Unknown return type 'void*'

    // void* PlatformHandle: Unknown return type 'void*'

    // void* PlatformHandleRaw: Unknown return type 'void*'

    if (len == 19 && memcmp(key, "PlatformRequestMove", 19) == 0) {
        bool RETVAL = self->PlatformRequestMove;
        lua_pushboolean(L, RETVAL);
        return 1;
    }

    if (len == 21 && memcmp(key, "PlatformRequestResize", 21) == 0) {
        bool RETVAL = self->PlatformRequestResize;
        lua_pushboolean(L, RETVAL);
        return 1;
    }

    if (len == 20 && memcmp(key, "PlatformRequestClose", 20) == 0) {
        bool RETVAL = self->PlatformRequestClose;
        lua_pushboolean(L, RETVAL);
        return 1;
    }

    luaL_getmetafield(L, 1, key);
    return 1;
}

static int ImGuiLua_NewindexImGuiViewport(lua_State *L)
{
    ImGuiViewport *self;
    self =
        *static_cast<ImGuiViewport **>(luaL_checkudata(L, 1, "ImGuiViewport"));
    size_t len;
    const char *key = luaL_checklstring(L, 2, &len);

    if (len == 2 && memcmp(key, "ID", 2) == 0) {
        unsigned int ARGVAL;
        ARGVAL = static_cast<unsigned int>(luaL_checkinteger(L, 3));
        self->ID = ARGVAL;
        return 0;
    }

    if (len == 5 && memcmp(key, "Flags", 5) == 0) {
        int ARGVAL;
        ARGVAL = static_cast<int>(luaL_checkinteger(L, 3));
        self->Flags = ARGVAL;
        return 0;
    }

    if (len == 3 && memcmp(key, "Pos", 3) == 0) {
        ImVec2 ARGVAL;
        ARGVAL = ImGuiLua_ToImVec2(L, 3);
        self->Pos = ARGVAL;
        return 0;
    }

    if (len == 4 && memcmp(key, "Size", 4) == 0) {
        ImVec2 ARGVAL;
        ARGVAL = ImGuiLua_ToImVec2(L, 3);
        self->Size = ARGVAL;
        return 0;
    }

    if (len == 7 && memcmp(key, "WorkPos", 7) == 0) {
        ImVec2 ARGVAL;
        ARGVAL = ImGuiLua_ToImVec2(L, 3);
        self->WorkPos = ARGVAL;
        return 0;
    }

    if (len == 8 && memcmp(key, "WorkSize", 8) == 0) {
        ImVec2 ARGVAL;
        ARGVAL = ImGuiLua_ToImVec2(L, 3);
        self->WorkSize = ARGVAL;
        return 0;
    }

    if (len == 8 && memcmp(key, "DpiScale", 8) == 0) {
        float ARGVAL;
        ARGVAL = static_cast<float>(luaL_checknumber(L, 3));
        self->DpiScale = ARGVAL;
        return 0;
    }

    if (len == 16 && memcmp(key, "ParentViewportId", 16) == 0) {
        unsigned int ARGVAL;
        ARGVAL = static_cast<unsigned int>(luaL_checkinteger(L, 3));
        self->ParentViewportId = ARGVAL;
        return 0;
    }

    // ImDrawData* DrawData: Unknown argument type 'ImDrawData*'

    // void* RendererUserData: Unknown argument type 'void*'

    // void* PlatformUserData: Unknown argument type 'void*'

    // void* PlatformHandle: Unknown argument type 'void*'

    // void* PlatformHandleRaw: Unknown argument type 'void*'

    if (len == 19 && memcmp(key, "PlatformRequestMove", 19) == 0) {
        bool ARGVAL;
        ARGVAL = lua_toboolean(L, 3);
        self->PlatformRequestMove = ARGVAL;
        return 0;
    }

    if (len == 21 && memcmp(key, "PlatformRequestResize", 21) == 0) {
        bool ARGVAL;
        ARGVAL = lua_toboolean(L, 3);
        self->PlatformRequestResize = ARGVAL;
        return 0;
    }

    if (len == 20 && memcmp(key, "PlatformRequestClose", 20) == 0) {
        bool ARGVAL;
        ARGVAL = lua_toboolean(L, 3);
        self->PlatformRequestClose = ARGVAL;
        return 0;
    }

    return luaL_error(L, "ImGuiViewport has no field '%s'", key);
}

// method ImGuiViewport::GetCenter

IMGUILUA_METHOD(ImGuiViewport::GetCenter)
static int ImGuiViewport_GetCenter(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    ImGuiViewport *self;
    self =
        *static_cast<ImGuiViewport **>(luaL_checkudata(L, 1, "ImGuiViewport"));
    ImVec2 RETVAL = self->GetCenter();
    ImGuiLua_PushImVec2(L, RETVAL);
    return 1;
}


// method ImGuiViewport::GetWorkCenter

IMGUILUA_METHOD(ImGuiViewport::GetWorkCenter)
static int ImGuiViewport_GetWorkCenter(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    ImGuiViewport *self;
    self =
        *static_cast<ImGuiViewport **>(luaL_checkudata(L, 1, "ImGuiViewport"));
    ImVec2 RETVAL = self->GetWorkCenter();
    ImGuiLua_PushImVec2(L, RETVAL);
    return 1;
}


IMGUILUA_STRUCT(ImGuiViewport)
static void ImGuiLua_RegisterStructImGuiViewport(lua_State *L)
{
    luaL_newmetatable(L, "ImGuiViewport");
    lua_pushcfunction(L, ImGuiLua_IndexImGuiViewport);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, ImGuiLua_NewindexImGuiViewport);
    lua_setfield(L, -2, "__newindex");
    lua_pushcfunction(L, ImGuiViewport_GetCenter);
    lua_setfield(L, -2, "GetCenter");
    lua_pushcfunction(L, ImGuiViewport_GetWorkCenter);
    lua_setfield(L, -2, "GetWorkCenter");
    lua_pop(L, 1);
}

static int ImGuiLua_IndexImGuiWindowClass(lua_State *L)
{
    ImGuiWindowClass *self;
    self = static_cast<ImGuiWindowClass *>(
        luaL_checkudata(L, 1, "ImGuiWindowClass"));
    size_t len;
    const char *key = luaL_checklstring(L, 2, &len);

    if (len == 7 && memcmp(key, "ClassId", 7) == 0) {
        unsigned int RETVAL = self->ClassId;
        lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
        return 1;
    }

    if (len == 16 && memcmp(key, "ParentViewportId", 16) == 0) {
        unsigned int RETVAL = self->ParentViewportId;
        lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
        return 1;
    }

    if (len == 24 && memcmp(key, "ViewportFlagsOverrideSet", 24) == 0) {
        int RETVAL = self->ViewportFlagsOverrideSet;
        lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
        return 1;
    }

    if (len == 26 && memcmp(key, "ViewportFlagsOverrideClear", 26) == 0) {
        int RETVAL = self->ViewportFlagsOverrideClear;
        lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
        return 1;
    }

    if (len == 23 && memcmp(key, "TabItemFlagsOverrideSet", 23) == 0) {
        int RETVAL = self->TabItemFlagsOverrideSet;
        lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
        return 1;
    }

    if (len == 24 && memcmp(key, "DockNodeFlagsOverrideSet", 24) == 0) {
        int RETVAL = self->DockNodeFlagsOverrideSet;
        lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
        return 1;
    }

    if (len == 19 && memcmp(key, "DockingAlwaysTabBar", 19) == 0) {
        bool RETVAL = self->DockingAlwaysTabBar;
        lua_pushboolean(L, RETVAL);
        return 1;
    }

    if (len == 21 && memcmp(key, "DockingAllowUnclassed", 21) == 0) {
        bool RETVAL = self->DockingAllowUnclassed;
        lua_pushboolean(L, RETVAL);
        return 1;
    }

    luaL_getmetafield(L, 1, key);
    return 1;
}

static int ImGuiLua_NewindexImGuiWindowClass(lua_State *L)
{
    ImGuiWindowClass *self;
    self = static_cast<ImGuiWindowClass *>(
        luaL_checkudata(L, 1, "ImGuiWindowClass"));
    size_t len;
    const char *key = luaL_checklstring(L, 2, &len);

    if (len == 7 && memcmp(key, "ClassId", 7) == 0) {
        unsigned int ARGVAL;
        ARGVAL = static_cast<unsigned int>(luaL_checkinteger(L, 3));
        self->ClassId = ARGVAL;
        return 0;
    }

    if (len == 16 && memcmp(key, "ParentViewportId", 16) == 0) {
        unsigned int ARGVAL;
        ARGVAL = static_cast<unsigned int>(luaL_checkinteger(L, 3));
        self->ParentViewportId = ARGVAL;
        return 0;
    }

    if (len == 24 && memcmp(key, "ViewportFlagsOverrideSet", 24) == 0) {
        int ARGVAL;
        ARGVAL = static_cast<int>(luaL_checkinteger(L, 3));
        self->ViewportFlagsOverrideSet = ARGVAL;
        return 0;
    }

    if (len == 26 && memcmp(key, "ViewportFlagsOverrideClear", 26) == 0) {
        int ARGVAL;
        ARGVAL = static_cast<int>(luaL_checkinteger(L, 3));
        self->ViewportFlagsOverrideClear = ARGVAL;
        return 0;
    }

    if (len == 23 && memcmp(key, "TabItemFlagsOverrideSet", 23) == 0) {
        int ARGVAL;
        ARGVAL = static_cast<int>(luaL_checkinteger(L, 3));
        self->TabItemFlagsOverrideSet = ARGVAL;
        return 0;
    }

    if (len == 24 && memcmp(key, "DockNodeFlagsOverrideSet", 24) == 0) {
        int ARGVAL;
        ARGVAL = static_cast<int>(luaL_checkinteger(L, 3));
        self->DockNodeFlagsOverrideSet = ARGVAL;
        return 0;
    }

    if (len == 19 && memcmp(key, "DockingAlwaysTabBar", 19) == 0) {
        bool ARGVAL;
        ARGVAL = lua_toboolean(L, 3);
        self->DockingAlwaysTabBar = ARGVAL;
        return 0;
    }

    if (len == 21 && memcmp(key, "DockingAllowUnclassed", 21) == 0) {
        bool ARGVAL;
        ARGVAL = lua_toboolean(L, 3);
        self->DockingAllowUnclassed = ARGVAL;
        return 0;
    }

    return luaL_error(L, "ImGuiWindowClass has no field '%s'", key);
}

IMGUILUA_STRUCT(ImGuiWindowClass)
static void ImGuiLua_RegisterStructImGuiWindowClass(lua_State *L)
{
    luaL_newmetatable(L, "ImGuiWindowClass");
    lua_pushcfunction(L, ImGuiLua_IndexImGuiWindowClass);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, ImGuiLua_NewindexImGuiWindowClass);
    lua_setfield(L, -2, "__newindex");
    lua_pop(L, 1);
}

static int ImGuiLua_IndexImVec2(lua_State *L)
{
    ImVec2 *self;
    self = static_cast<ImVec2 *>(luaL_checkudata(L, 1, "ImVec2"));
    size_t len;
    const char *key = luaL_checklstring(L, 2, &len);

    if (len == 1 && memcmp(key, "x", 1) == 0) {
        float RETVAL = self->x;
        lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
        return 1;
    }

    if (len == 1 && memcmp(key, "y", 1) == 0) {
        float RETVAL = self->y;
        lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
        return 1;
    }

    luaL_getmetafield(L, 1, key);
    return 1;
}

static int ImGuiLua_NewindexImVec2(lua_State *L)
{
    ImVec2 *self;
    self = static_cast<ImVec2 *>(luaL_checkudata(L, 1, "ImVec2"));
    size_t len;
    const char *key = luaL_checklstring(L, 2, &len);

    if (len == 1 && memcmp(key, "x", 1) == 0) {
        float ARGVAL;
        ARGVAL = static_cast<float>(luaL_checknumber(L, 3));
        self->x = ARGVAL;
        return 0;
    }

    if (len == 1 && memcmp(key, "y", 1) == 0) {
        float ARGVAL;
        ARGVAL = static_cast<float>(luaL_checknumber(L, 3));
        self->y = ARGVAL;
        return 0;
    }

    return luaL_error(L, "ImVec2 has no field '%s'", key);
}

IMGUILUA_STRUCT(ImVec2)
static void ImGuiLua_RegisterStructImVec2(lua_State *L)
{
    luaL_newmetatable(L, "ImVec2");
    lua_pushcfunction(L, ImGuiLua_IndexImVec2);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, ImGuiLua_NewindexImVec2);
    lua_setfield(L, -2, "__newindex");
    lua_pop(L, 1);
}

static int ImGuiLua_IndexImVec4(lua_State *L)
{
    ImVec4 *self;
    self = static_cast<ImVec4 *>(luaL_checkudata(L, 1, "ImVec4"));
    size_t len;
    const char *key = luaL_checklstring(L, 2, &len);

    if (len == 1 && memcmp(key, "x", 1) == 0) {
        float RETVAL = self->x;
        lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
        return 1;
    }

    if (len == 1 && memcmp(key, "y", 1) == 0) {
        float RETVAL = self->y;
        lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
        return 1;
    }

    if (len == 1 && memcmp(key, "z", 1) == 0) {
        float RETVAL = self->z;
        lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
        return 1;
    }

    if (len == 1 && memcmp(key, "w", 1) == 0) {
        float RETVAL = self->w;
        lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
        return 1;
    }

    luaL_getmetafield(L, 1, key);
    return 1;
}

static int ImGuiLua_NewindexImVec4(lua_State *L)
{
    ImVec4 *self;
    self = static_cast<ImVec4 *>(luaL_checkudata(L, 1, "ImVec4"));
    size_t len;
    const char *key = luaL_checklstring(L, 2, &len);

    if (len == 1 && memcmp(key, "x", 1) == 0) {
        float ARGVAL;
        ARGVAL = static_cast<float>(luaL_checknumber(L, 3));
        self->x = ARGVAL;
        return 0;
    }

    if (len == 1 && memcmp(key, "y", 1) == 0) {
        float ARGVAL;
        ARGVAL = static_cast<float>(luaL_checknumber(L, 3));
        self->y = ARGVAL;
        return 0;
    }

    if (len == 1 && memcmp(key, "z", 1) == 0) {
        float ARGVAL;
        ARGVAL = static_cast<float>(luaL_checknumber(L, 3));
        self->z = ARGVAL;
        return 0;
    }

    if (len == 1 && memcmp(key, "w", 1) == 0) {
        float ARGVAL;
        ARGVAL = static_cast<float>(luaL_checknumber(L, 3));
        self->w = ARGVAL;
        return 0;
    }

    return luaL_error(L, "ImVec4 has no field '%s'", key);
}

IMGUILUA_STRUCT(ImVec4)
static void ImGuiLua_RegisterStructImVec4(lua_State *L)
{
    luaL_newmetatable(L, "ImVec4");
    lua_pushcfunction(L, ImGuiLua_IndexImVec4);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, ImGuiLua_NewindexImVec4);
    lua_setfield(L, -2, "__newindex");
    lua_pop(L, 1);
}

static int ImGuiLua_IndexImGuiLuaBoolRef(lua_State *L)
{
    bool *self;
    self = static_cast<bool *>(luaL_checkudata(L, 1, "ImGuiLuaBoolRef"));
    size_t len;
    const char *key = luaL_checklstring(L, 2, &len);

    if (len == 5 && memcmp(key, "value", 5) == 0) {
        bool RETVAL = *self;
        lua_pushboolean(L, RETVAL);
        return 1;
    }

    luaL_getmetafield(L, 1, key);
    return 1;
}

static int ImGuiLua_NewindexImGuiLuaBoolRef(lua_State *L)
{
    bool *self;
    self = static_cast<bool *>(luaL_checkudata(L, 1, "ImGuiLuaBoolRef"));
    size_t len;
    const char *key = luaL_checklstring(L, 2, &len);

    if (len == 5 && memcmp(key, "value", 5) == 0) {
        bool ARGVAL;
        ARGVAL = lua_toboolean(L, 3);
        *self = ARGVAL;
        return 0;
    }

    return luaL_error(L, "ImGuiLuaBoolRef has no field '%s'", key);
}

IMGUILUA_STRUCT(ImGuiLuaBoolRef)
static void ImGuiLua_RegisterStructImGuiLuaBoolRef(lua_State *L)
{
    luaL_newmetatable(L, "ImGuiLuaBoolRef");
    lua_pushcfunction(L, ImGuiLua_IndexImGuiLuaBoolRef);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, ImGuiLua_NewindexImGuiLuaBoolRef);
    lua_setfield(L, -2, "__newindex");
    lua_pop(L, 1);
}

static int ImGuiLua_IndexImGuiLuaFloatRef(lua_State *L)
{
    float *self;
    self = static_cast<float *>(luaL_checkudata(L, 1, "ImGuiLuaFloatRef"));
    size_t len;
    const char *key = luaL_checklstring(L, 2, &len);

    if (len == 5 && memcmp(key, "value", 5) == 0) {
        float RETVAL = *self;
        lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
        return 1;
    }

    luaL_getmetafield(L, 1, key);
    return 1;
}

static int ImGuiLua_NewindexImGuiLuaFloatRef(lua_State *L)
{
    float *self;
    self = static_cast<float *>(luaL_checkudata(L, 1, "ImGuiLuaFloatRef"));
    size_t len;
    const char *key = luaL_checklstring(L, 2, &len);

    if (len == 5 && memcmp(key, "value", 5) == 0) {
        float ARGVAL;
        ARGVAL = static_cast<float>(luaL_checknumber(L, 3));
        *self = ARGVAL;
        return 0;
    }

    return luaL_error(L, "ImGuiLuaFloatRef has no field '%s'", key);
}

IMGUILUA_STRUCT(ImGuiLuaFloatRef)
static void ImGuiLua_RegisterStructImGuiLuaFloatRef(lua_State *L)
{
    luaL_newmetatable(L, "ImGuiLuaFloatRef");
    lua_pushcfunction(L, ImGuiLua_IndexImGuiLuaFloatRef);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, ImGuiLua_NewindexImGuiLuaFloatRef);
    lua_setfield(L, -2, "__newindex");
    lua_pop(L, 1);
}

static int ImGuiLua_IndexImGuiLuaIntRef(lua_State *L)
{
    int *self;
    self = static_cast<int *>(luaL_checkudata(L, 1, "ImGuiLuaIntRef"));
    size_t len;
    const char *key = luaL_checklstring(L, 2, &len);

    if (len == 5 && memcmp(key, "value", 5) == 0) {
        int RETVAL = *self;
        lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
        return 1;
    }

    luaL_getmetafield(L, 1, key);
    return 1;
}

static int ImGuiLua_NewindexImGuiLuaIntRef(lua_State *L)
{
    int *self;
    self = static_cast<int *>(luaL_checkudata(L, 1, "ImGuiLuaIntRef"));
    size_t len;
    const char *key = luaL_checklstring(L, 2, &len);

    if (len == 5 && memcmp(key, "value", 5) == 0) {
        int ARGVAL;
        ARGVAL = static_cast<int>(luaL_checkinteger(L, 3));
        *self = ARGVAL;
        return 0;
    }

    return luaL_error(L, "ImGuiLuaIntRef has no field '%s'", key);
}

IMGUILUA_STRUCT(ImGuiLuaIntRef)
static void ImGuiLua_RegisterStructImGuiLuaIntRef(lua_State *L)
{
    luaL_newmetatable(L, "ImGuiLuaIntRef");
    lua_pushcfunction(L, ImGuiLua_IndexImGuiLuaIntRef);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, ImGuiLua_NewindexImGuiLuaIntRef);
    lua_setfield(L, -2, "__newindex");
    lua_pop(L, 1);
}

static int ImGuiLua_IndexImGuiLuaStringRef(lua_State *L)
{
    luaL_checkudata(L, 1, "ImGuiLuaStringRef");
    size_t len;
    const char *key = luaL_checklstring(L, 2, &len);

    if (len == 5 && memcmp(key, "value", 5) == 0) {
        lua_getiuservalue(L, 1, 1);
        return 1;
    }

    luaL_getmetafield(L, 1, key);
    return 1;
}

static int ImGuiLua_NewindexImGuiLuaStringRef(lua_State *L)
{
    luaL_checkudata(L, 1, "ImGuiLuaStringRef");
    size_t len;
    const char *key = luaL_checklstring(L, 2, &len);

    if (len == 5 && memcmp(key, "value", 5) == 0) {
        luaL_checkstring(L, 3);
        lua_pushvalue(L, 3);
        lua_setiuservalue(L, 1, 1);
        return 0;
    }

    return luaL_error(L, "ImGuiLuaStringRef has no field '%s'", key);
}

IMGUILUA_STRUCT(ImGuiLuaStringRef)
static void ImGuiLua_RegisterStructImGuiLuaStringRef(lua_State *L)
{
    luaL_newmetatable(L, "ImGuiLuaStringRef");
    lua_pushcfunction(L, ImGuiLua_IndexImGuiLuaStringRef);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, ImGuiLua_NewindexImGuiLuaStringRef);
    lua_setfield(L, -2, "__newindex");
    lua_pop(L, 1);
}


// method ImColor::~ImColor has no namespace


// static method ImColor::HSV has no namespace


// method ImColor::ImColor has no namespace


// method ImColor::SetHSV has no namespace


// method ImDrawCmd::~ImDrawCmd has no namespace


// method ImDrawCmd::GetTexID has no namespace


// method ImDrawCmd::ImDrawCmd has no namespace


// method ImDrawData::~ImDrawData has no namespace


// method ImDrawData::Clear has no namespace


// method ImDrawData::DeIndexAllBuffers has no namespace


// method ImDrawData::ImDrawData has no namespace


// method ImDrawData::ScaleClipRects has no namespace


// method ImDrawList::~ImDrawList has no namespace


// method ImDrawList::AddBezierCubic has no namespace


// method ImDrawList::AddBezierQuadratic has no namespace


// method ImDrawList::AddCallback has no namespace


// method ImDrawList::AddCircle has no namespace


// method ImDrawList::AddCircleFilled has no namespace


// method ImDrawList::AddConvexPolyFilled has no namespace


// method ImDrawList::AddDrawCmd has no namespace


// method ImDrawList::AddImage has no namespace


// method ImDrawList::AddImageQuad has no namespace


// method ImDrawList::AddImageRounded has no namespace


// method ImDrawList::AddLine has no namespace


// method ImDrawList::AddNgon has no namespace


// method ImDrawList::AddNgonFilled has no namespace


// method ImDrawList::AddPolyline has no namespace


// method ImDrawList::AddQuad has no namespace


// method ImDrawList::AddQuadFilled has no namespace


// method ImDrawList::AddRect has no namespace


// method ImDrawList::AddRectFilled has no namespace


// method ImDrawList::AddRectFilledMultiColor has no namespace


// method ImDrawList::AddText has no namespace


// method ImDrawList::AddTriangle has no namespace


// method ImDrawList::AddTriangleFilled has no namespace


// method ImDrawList::ChannelsMerge has no namespace


// method ImDrawList::ChannelsSetCurrent has no namespace


// method ImDrawList::ChannelsSplit has no namespace


// method ImDrawList::CloneOutput has no namespace


// method ImDrawList::GetClipRectMax has no namespace


// method ImDrawList::GetClipRectMin has no namespace


// method ImDrawList::ImDrawList has no namespace


// method ImDrawList::PathArcTo has no namespace


// method ImDrawList::PathArcToFast has no namespace


// method ImDrawList::PathBezierCubicCurveTo has no namespace


// method ImDrawList::PathBezierQuadraticCurveTo has no namespace


// method ImDrawList::PathClear has no namespace


// method ImDrawList::PathFillConvex has no namespace


// method ImDrawList::PathLineTo has no namespace


// method ImDrawList::PathLineToMergeDuplicate has no namespace


// method ImDrawList::PathRect has no namespace


// method ImDrawList::PathStroke has no namespace


// method ImDrawList::PopClipRect has no namespace


// method ImDrawList::PopTextureID has no namespace


// method ImDrawList::PrimQuadUV has no namespace


// method ImDrawList::PrimRect has no namespace


// method ImDrawList::PrimRectUV has no namespace


// method ImDrawList::PrimReserve has no namespace


// method ImDrawList::PrimUnreserve has no namespace


// method ImDrawList::PrimVtx has no namespace


// method ImDrawList::PrimWriteIdx has no namespace


// method ImDrawList::PrimWriteVtx has no namespace


// method ImDrawList::PushClipRect has no namespace


// method ImDrawList::PushClipRectFullScreen has no namespace


// method ImDrawList::PushTextureID has no namespace


// method ImDrawList::_CalcCircleAutoSegmentCount has no namespace


// method ImDrawList::_ClearFreeMemory has no namespace


// method ImDrawList::_OnChangedClipRect has no namespace


// method ImDrawList::_OnChangedTextureID has no namespace


// method ImDrawList::_OnChangedVtxOffset has no namespace


// method ImDrawList::_PathArcToFastEx has no namespace


// method ImDrawList::_PathArcToN has no namespace


// method ImDrawList::_PopUnusedDrawCmd has no namespace


// method ImDrawList::_ResetForNewFrame has no namespace


// method ImDrawList::_TryMergeDrawCmds has no namespace


// method ImDrawListSplitter::~ImDrawListSplitter has no namespace


// method ImDrawListSplitter::Clear has no namespace


// method ImDrawListSplitter::ClearFreeMemory has no namespace


// method ImDrawListSplitter::ImDrawListSplitter has no namespace


// method ImDrawListSplitter::Merge has no namespace


// method ImDrawListSplitter::SetCurrentChannel has no namespace


// method ImDrawListSplitter::Split has no namespace


// method ImFont::~ImFont has no namespace


// method ImFont::AddGlyph has no namespace


// method ImFont::AddRemapChar has no namespace


// method ImFont::BuildLookupTable has no namespace


// method ImFont::CalcTextSizeA has no namespace


// method ImFont::CalcWordWrapPositionA has no namespace


// method ImFont::ClearOutputData has no namespace


// method ImFont::FindGlyph has no namespace


// method ImFont::FindGlyphNoFallback has no namespace


// method ImFont::GetCharAdvance has no namespace


// method ImFont::GetDebugName has no namespace


// method ImFont::GrowIndex has no namespace


// method ImFont::ImFont has no namespace


// method ImFont::IsGlyphRangeUnused has no namespace


// method ImFont::IsLoaded has no namespace


// method ImFont::RenderChar has no namespace


// method ImFont::RenderText has no namespace


// method ImFont::SetGlyphVisible has no namespace


// method ImFontAtlas::~ImFontAtlas has no namespace


// method ImFontAtlas::AddCustomRectFontGlyph has no namespace


// method ImFontAtlas::AddCustomRectRegular has no namespace


// method ImFontAtlas::AddFont has no namespace


// method ImFontAtlas::AddFontDefault has no namespace


// method ImFontAtlas::AddFontFromFileTTF has no namespace


// method ImFontAtlas::AddFontFromMemoryCompressedBase85TTF has no namespace


// method ImFontAtlas::AddFontFromMemoryCompressedTTF has no namespace


// method ImFontAtlas::AddFontFromMemoryTTF has no namespace


// method ImFontAtlas::Build has no namespace


// method ImFontAtlas::CalcCustomRectUV has no namespace


// method ImFontAtlas::Clear has no namespace


// method ImFontAtlas::ClearFonts has no namespace


// method ImFontAtlas::ClearInputData has no namespace


// method ImFontAtlas::ClearTexData has no namespace


// method ImFontAtlas::GetCustomRectByIndex has no namespace


// method ImFontAtlas::GetGlyphRangesChineseFull has no namespace


// method ImFontAtlas::GetGlyphRangesChineseSimplifiedCommon has no namespace


// method ImFontAtlas::GetGlyphRangesCyrillic has no namespace


// method ImFontAtlas::GetGlyphRangesDefault has no namespace


// method ImFontAtlas::GetGlyphRangesJapanese has no namespace


// method ImFontAtlas::GetGlyphRangesKorean has no namespace


// method ImFontAtlas::GetGlyphRangesThai has no namespace


// method ImFontAtlas::GetGlyphRangesVietnamese has no namespace


// method ImFontAtlas::GetMouseCursorTexData has no namespace


// method ImFontAtlas::GetTexDataAsAlpha8 has no namespace


// method ImFontAtlas::GetTexDataAsRGBA32 has no namespace


// method ImFontAtlas::ImFontAtlas has no namespace


// method ImFontAtlas::IsBuilt has no namespace


// method ImFontAtlas::SetTexID has no namespace


// method ImFontAtlasCustomRect::~ImFontAtlasCustomRect has no namespace


// method ImFontAtlasCustomRect::ImFontAtlasCustomRect has no namespace


// method ImFontAtlasCustomRect::IsPacked has no namespace


// method ImFontConfig::~ImFontConfig has no namespace


// method ImFontConfig::ImFontConfig has no namespace


// method ImFontGlyphRangesBuilder::~ImFontGlyphRangesBuilder has no namespace


// method ImFontGlyphRangesBuilder::AddChar has no namespace


// method ImFontGlyphRangesBuilder::AddRanges has no namespace


// method ImFontGlyphRangesBuilder::AddText has no namespace


// method ImFontGlyphRangesBuilder::BuildRanges has no namespace


// method ImFontGlyphRangesBuilder::Clear has no namespace


// method ImFontGlyphRangesBuilder::GetBit has no namespace


// method ImFontGlyphRangesBuilder::ImFontGlyphRangesBuilder has no namespace


// method ImFontGlyphRangesBuilder::SetBit has no namespace


// function ImGui::AcceptDragDropPayload

// const ImGuiPayload* ImGui_AcceptDragDropPayload(const char*,
// ImGuiDragDropFlags): Unknown return type 'const ImGuiPayload*' (const
// ImGuiPayload*)


// function ImGui::AlignTextToFramePadding

IMGUILUA_FUNCTION(ImGui::AlignTextToFramePadding)
static int ImGui_AlignTextToFramePadding(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImGui::AlignTextToFramePadding();
    return 0;
}


// function ImGui::ArrowButton

IMGUILUA_FUNCTION(ImGui::ArrowButton)
static int ImGui_ArrowButton(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(2);
    const char *str_id;
    str_id = lua_tostring(L, 1);
    int dir;
    dir = static_cast<int>(luaL_checkinteger(L, 2));
    bool RETVAL = ImGui::ArrowButton(str_id, dir);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::Begin

IMGUILUA_FUNCTION(ImGui::Begin)
static int ImGui_Begin(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(1, 3);
    const char *name;
    name = lua_tostring(L, 1);
    bool *p_open;
    if (ARGC < 2) {
        p_open = NULL;
    }
    else {
        if (lua_type(L, 2) == LUA_TNIL) {
            p_open = NULL;
        }
        else {
            p_open =
                static_cast<bool *>(luaL_checkudata(L, 2, "ImGuiLuaBoolRef"));
        }
    }
    int flags;
    if (ARGC < 3) {
        flags = 0;
    }
    else {
        flags = static_cast<int>(luaL_checkinteger(L, 3));
    }
    bool RETVAL = ImGui::Begin(name, p_open, flags);
    IMGUILUA_BEGIN(L, ImGui::Begin, ImGui::End, 1);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::BeginChild

static int ImGui_BeginChild_uv2bi(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(1, 4);
    unsigned int id;
    id = static_cast<unsigned int>(luaL_checkinteger(L, 1));
    ImVec2 size;
    if (ARGC < 2) {
        size = ImVec2(0, 0);
    }
    else {
        size = ImGuiLua_ToImVec2(L, 2);
    }
    bool border;
    if (ARGC < 3) {
        border = false;
    }
    else {
        border = lua_toboolean(L, 3);
    }
    int flags;
    if (ARGC < 4) {
        flags = 0;
    }
    else {
        flags = static_cast<int>(luaL_checkinteger(L, 4));
    }
    bool RETVAL = ImGui::BeginChild(id, size, border, flags);
    IMGUILUA_BEGIN(L, ImGui::BeginChild, ImGui::EndChild, 2);
    lua_pushboolean(L, RETVAL);
    return 1;
}

static int ImGui_BeginChild_sv2bi(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(1, 4);
    const char *str_id;
    str_id = lua_tostring(L, 1);
    ImVec2 size;
    if (ARGC < 2) {
        size = ImVec2(0, 0);
    }
    else {
        size = ImGuiLua_ToImVec2(L, 2);
    }
    bool border;
    if (ARGC < 3) {
        border = false;
    }
    else {
        border = lua_toboolean(L, 3);
    }
    int flags;
    if (ARGC < 4) {
        flags = 0;
    }
    else {
        flags = static_cast<int>(luaL_checkinteger(L, 4));
    }
    bool RETVAL = ImGui::BeginChild(str_id, size, border, flags);
    IMGUILUA_BEGIN(L, ImGui::BeginChild, ImGui::EndChild, 2);
    lua_pushboolean(L, RETVAL);
    return 1;
}

IMGUILUA_FUNCTION(ImGui::BeginChild)
static int ImGui_BeginChild(lua_State *L)
{
    if (lua_type(L, 1) == LUA_TNUMBER) {
        return ImGui_BeginChild_uv2bi(L);
    }
    else {
        return ImGui_BeginChild_sv2bi(L);
    }
}


// function ImGui::BeginChildFrame

IMGUILUA_FUNCTION(ImGui::BeginChildFrame)
static int ImGui_BeginChildFrame(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(2, 3);
    unsigned int id;
    id = static_cast<unsigned int>(luaL_checkinteger(L, 1));
    ImVec2 size;
    size = ImGuiLua_ToImVec2(L, 2);
    int flags;
    if (ARGC < 3) {
        flags = 0;
    }
    else {
        flags = static_cast<int>(luaL_checkinteger(L, 3));
    }
    bool RETVAL = ImGui::BeginChildFrame(id, size, flags);
    if (RETVAL) {
        IMGUILUA_BEGIN(L, ImGui::BeginChildFrame, ImGui::EndChildFrame, 3);
    }
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::BeginCombo

IMGUILUA_FUNCTION(ImGui::BeginCombo)
static int ImGui_BeginCombo(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(2, 3);
    const char *label;
    label = lua_tostring(L, 1);
    const char *preview_value;
    preview_value = lua_tostring(L, 2);
    int flags;
    if (ARGC < 3) {
        flags = 0;
    }
    else {
        flags = static_cast<int>(luaL_checkinteger(L, 3));
    }
    bool RETVAL = ImGui::BeginCombo(label, preview_value, flags);
    if (RETVAL) {
        IMGUILUA_BEGIN(L, ImGui::BeginCombo, ImGui::EndCombo, 4);
    }
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::BeginDisabled

IMGUILUA_FUNCTION(ImGui::BeginDisabled)
static int ImGui_BeginDisabled(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(1);
    bool disabled;
    if (ARGC < 1) {
        disabled = true;
    }
    else {
        disabled = lua_toboolean(L, 1);
    }
    ImGui::BeginDisabled(disabled);
    return 0;
}


// function ImGui::BeginDragDropSource

IMGUILUA_FUNCTION(ImGui::BeginDragDropSource)
static int ImGui_BeginDragDropSource(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(1);
    int flags;
    if (ARGC < 1) {
        flags = 0;
    }
    else {
        flags = static_cast<int>(luaL_checkinteger(L, 1));
    }
    bool RETVAL = ImGui::BeginDragDropSource(flags);
    if (RETVAL) {
        IMGUILUA_BEGIN(L, ImGui::BeginDragDropSource, ImGui::EndDragDropSource,
                       5);
    }
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::BeginDragDropTarget

IMGUILUA_FUNCTION(ImGui::BeginDragDropTarget)
static int ImGui_BeginDragDropTarget(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    bool RETVAL = ImGui::BeginDragDropTarget();
    if (RETVAL) {
        IMGUILUA_BEGIN(L, ImGui::BeginDragDropTarget, ImGui::EndDragDropTarget,
                       6);
    }
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::BeginGroup

IMGUILUA_FUNCTION(ImGui::BeginGroup)
static int ImGui_BeginGroup(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImGui::BeginGroup();
    IMGUILUA_BEGIN(L, ImGui::BeginGroup, ImGui::EndGroup, 7);
    return 0;
}


// function ImGui::BeginListBox

IMGUILUA_FUNCTION(ImGui::BeginListBox)
static int ImGui_BeginListBox(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(1, 2);
    const char *label;
    label = lua_tostring(L, 1);
    ImVec2 size;
    if (ARGC < 2) {
        size = ImVec2(0, 0);
    }
    else {
        size = ImGuiLua_ToImVec2(L, 2);
    }
    bool RETVAL = ImGui::BeginListBox(label, size);
    if (RETVAL) {
        IMGUILUA_BEGIN(L, ImGui::BeginListBox, ImGui::EndListBox, 8);
    }
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::BeginMainMenuBar

IMGUILUA_FUNCTION(ImGui::BeginMainMenuBar)
static int ImGui_BeginMainMenuBar(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    bool RETVAL = ImGui::BeginMainMenuBar();
    if (RETVAL) {
        IMGUILUA_BEGIN(L, ImGui::BeginMainMenuBar, ImGui::EndMainMenuBar, 9);
    }
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::BeginMenu

IMGUILUA_FUNCTION(ImGui::BeginMenu)
static int ImGui_BeginMenu(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(1, 2);
    const char *label;
    label = lua_tostring(L, 1);
    bool enabled;
    if (ARGC < 2) {
        enabled = true;
    }
    else {
        enabled = lua_toboolean(L, 2);
    }
    bool RETVAL = ImGui::BeginMenu(label, enabled);
    if (RETVAL) {
        IMGUILUA_BEGIN(L, ImGui::BeginMenu, ImGui::EndMenu, 10);
    }
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::BeginMenuBar

IMGUILUA_FUNCTION(ImGui::BeginMenuBar)
static int ImGui_BeginMenuBar(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    bool RETVAL = ImGui::BeginMenuBar();
    if (RETVAL) {
        IMGUILUA_BEGIN(L, ImGui::BeginMenuBar, ImGui::EndMenuBar, 11);
    }
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::BeginPopup

IMGUILUA_FUNCTION(ImGui::BeginPopup)
static int ImGui_BeginPopup(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(1, 2);
    const char *str_id;
    str_id = lua_tostring(L, 1);
    int flags;
    if (ARGC < 2) {
        flags = 0;
    }
    else {
        flags = static_cast<int>(luaL_checkinteger(L, 2));
    }
    bool RETVAL = ImGui::BeginPopup(str_id, flags);
    if (RETVAL) {
        IMGUILUA_BEGIN(L, ImGui::BeginPopup, ImGui::EndPopup, 12);
    }
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::BeginPopupContextItem

IMGUILUA_FUNCTION(ImGui::BeginPopupContextItem)
static int ImGui_BeginPopupContextItem(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(2);
    const char *str_id;
    if (ARGC < 1) {
        str_id = NULL;
    }
    else {
        str_id = lua_tostring(L, 1);
    }
    int popup_flags;
    if (ARGC < 2) {
        popup_flags = 1;
    }
    else {
        popup_flags = static_cast<int>(luaL_checkinteger(L, 2));
    }
    bool RETVAL = ImGui::BeginPopupContextItem(str_id, popup_flags);
    if (RETVAL) {
        IMGUILUA_BEGIN(L, ImGui::BeginPopupContextItem, ImGui::EndPopup, 12);
    }
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::BeginPopupContextVoid

IMGUILUA_FUNCTION(ImGui::BeginPopupContextVoid)
static int ImGui_BeginPopupContextVoid(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(2);
    const char *str_id;
    if (ARGC < 1) {
        str_id = NULL;
    }
    else {
        str_id = lua_tostring(L, 1);
    }
    int popup_flags;
    if (ARGC < 2) {
        popup_flags = 1;
    }
    else {
        popup_flags = static_cast<int>(luaL_checkinteger(L, 2));
    }
    bool RETVAL = ImGui::BeginPopupContextVoid(str_id, popup_flags);
    if (RETVAL) {
        IMGUILUA_BEGIN(L, ImGui::BeginPopupContextVoid, ImGui::EndPopup, 12);
    }
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::BeginPopupContextWindow

IMGUILUA_FUNCTION(ImGui::BeginPopupContextWindow)
static int ImGui_BeginPopupContextWindow(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(2);
    const char *str_id;
    if (ARGC < 1) {
        str_id = NULL;
    }
    else {
        str_id = lua_tostring(L, 1);
    }
    int popup_flags;
    if (ARGC < 2) {
        popup_flags = 1;
    }
    else {
        popup_flags = static_cast<int>(luaL_checkinteger(L, 2));
    }
    bool RETVAL = ImGui::BeginPopupContextWindow(str_id, popup_flags);
    if (RETVAL) {
        IMGUILUA_BEGIN(L, ImGui::BeginPopupContextWindow, ImGui::EndPopup, 12);
    }
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::BeginPopupModal

IMGUILUA_FUNCTION(ImGui::BeginPopupModal)
static int ImGui_BeginPopupModal(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(1, 3);
    const char *name;
    name = lua_tostring(L, 1);
    bool *p_open;
    if (ARGC < 2) {
        p_open = NULL;
    }
    else {
        if (lua_type(L, 2) == LUA_TNIL) {
            p_open = NULL;
        }
        else {
            p_open =
                static_cast<bool *>(luaL_checkudata(L, 2, "ImGuiLuaBoolRef"));
        }
    }
    int flags;
    if (ARGC < 3) {
        flags = 0;
    }
    else {
        flags = static_cast<int>(luaL_checkinteger(L, 3));
    }
    bool RETVAL = ImGui::BeginPopupModal(name, p_open, flags);
    if (RETVAL) {
        IMGUILUA_BEGIN(L, ImGui::BeginPopupModal, ImGui::EndPopup, 12);
    }
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::BeginTabBar

IMGUILUA_FUNCTION(ImGui::BeginTabBar)
static int ImGui_BeginTabBar(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(1, 2);
    const char *str_id;
    str_id = lua_tostring(L, 1);
    int flags;
    if (ARGC < 2) {
        flags = 0;
    }
    else {
        flags = static_cast<int>(luaL_checkinteger(L, 2));
    }
    bool RETVAL = ImGui::BeginTabBar(str_id, flags);
    if (RETVAL) {
        IMGUILUA_BEGIN(L, ImGui::BeginTabBar, ImGui::EndTabBar, 13);
    }
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::BeginTabItem

IMGUILUA_FUNCTION(ImGui::BeginTabItem)
static int ImGui_BeginTabItem(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(1, 3);
    const char *label;
    label = lua_tostring(L, 1);
    bool *p_open;
    if (ARGC < 2) {
        p_open = NULL;
    }
    else {
        if (lua_type(L, 2) == LUA_TNIL) {
            p_open = NULL;
        }
        else {
            p_open =
                static_cast<bool *>(luaL_checkudata(L, 2, "ImGuiLuaBoolRef"));
        }
    }
    int flags;
    if (ARGC < 3) {
        flags = 0;
    }
    else {
        flags = static_cast<int>(luaL_checkinteger(L, 3));
    }
    bool RETVAL = ImGui::BeginTabItem(label, p_open, flags);
    if (RETVAL) {
        IMGUILUA_BEGIN(L, ImGui::BeginTabItem, ImGui::EndTabItem, 14);
    }
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::BeginTable

IMGUILUA_FUNCTION(ImGui::BeginTable)
static int ImGui_BeginTable(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(2, 5);
    const char *str_id;
    str_id = lua_tostring(L, 1);
    int column;
    column = static_cast<int>(luaL_checkinteger(L, 2));
    int flags;
    if (ARGC < 3) {
        flags = 0;
    }
    else {
        flags = static_cast<int>(luaL_checkinteger(L, 3));
    }
    ImVec2 outer_size;
    if (ARGC < 4) {
        outer_size = ImVec2(0.0f, 0.0f);
    }
    else {
        outer_size = ImGuiLua_ToImVec2(L, 4);
    }
    float inner_width;
    if (ARGC < 5) {
        inner_width = 0.0f;
    }
    else {
        inner_width = static_cast<float>(luaL_checknumber(L, 5));
    }
    bool RETVAL =
        ImGui::BeginTable(str_id, column, flags, outer_size, inner_width);
    if (RETVAL) {
        IMGUILUA_BEGIN(L, ImGui::BeginTable, ImGui::EndTable, 15);
    }
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::BeginTooltip

IMGUILUA_FUNCTION(ImGui::BeginTooltip)
static int ImGui_BeginTooltip(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImGui::BeginTooltip();
    IMGUILUA_BEGIN(L, ImGui::BeginTooltip, ImGui::EndTooltip, 16);
    return 0;
}


// function ImGui::Bullet

IMGUILUA_FUNCTION(ImGui::Bullet)
static int ImGui_Bullet(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImGui::Bullet();
    return 0;
}


// function ImGui::BulletText is excluded


// function ImGui::BulletTextV is excluded


// function ImGui::Button

IMGUILUA_FUNCTION(ImGui::Button)
static int ImGui_Button(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(1, 2);
    const char *label;
    label = lua_tostring(L, 1);
    ImVec2 size;
    if (ARGC < 2) {
        size = ImVec2(0, 0);
    }
    else {
        size = ImGuiLua_ToImVec2(L, 2);
    }
    bool RETVAL = ImGui::Button(label, size);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::CalcItemWidth

IMGUILUA_FUNCTION(ImGui::CalcItemWidth)
static int ImGui_CalcItemWidth(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    float RETVAL = ImGui::CalcItemWidth();
    lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
    return 1;
}


// function ImGui::CalcTextSize

IMGUILUA_FUNCTION(ImGui::CalcTextSize)
static int ImGui_CalcTextSize(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(1, 4);
    const char *text;
    text = lua_tostring(L, 1);
    const char *text_end;
    if (ARGC < 2) {
        text_end = NULL;
    }
    else {
        text_end = lua_tostring(L, 2);
    }
    bool hide_text_after_double_hash;
    if (ARGC < 3) {
        hide_text_after_double_hash = false;
    }
    else {
        hide_text_after_double_hash = lua_toboolean(L, 3);
    }
    float wrap_width;
    if (ARGC < 4) {
        wrap_width = -1.0f;
    }
    else {
        wrap_width = static_cast<float>(luaL_checknumber(L, 4));
    }
    ImVec2 RETVAL = ImGui::CalcTextSize(
        text, text_end, hide_text_after_double_hash, wrap_width);
    ImGuiLua_PushImVec2(L, RETVAL);
    return 1;
}


// function ImGui::CaptureKeyboardFromApp

IMGUILUA_FUNCTION(ImGui::CaptureKeyboardFromApp)
static int ImGui_CaptureKeyboardFromApp(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(1);
    bool want_capture_keyboard_value;
    if (ARGC < 1) {
        want_capture_keyboard_value = true;
    }
    else {
        want_capture_keyboard_value = lua_toboolean(L, 1);
    }
    ImGui::CaptureKeyboardFromApp(want_capture_keyboard_value);
    return 0;
}


// function ImGui::CaptureMouseFromApp

IMGUILUA_FUNCTION(ImGui::CaptureMouseFromApp)
static int ImGui_CaptureMouseFromApp(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(1);
    bool want_capture_mouse_value;
    if (ARGC < 1) {
        want_capture_mouse_value = true;
    }
    else {
        want_capture_mouse_value = lua_toboolean(L, 1);
    }
    ImGui::CaptureMouseFromApp(want_capture_mouse_value);
    return 0;
}


// function ImGui::Checkbox

IMGUILUA_FUNCTION(ImGui::Checkbox)
static int ImGui_Checkbox(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(2);
    const char *label;
    label = lua_tostring(L, 1);
    bool *v;
    if (lua_type(L, 2) == LUA_TNIL) {
        v = NULL;
    }
    else {
        v = static_cast<bool *>(luaL_checkudata(L, 2, "ImGuiLuaBoolRef"));
    }
    bool RETVAL = ImGui::Checkbox(label, v);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::CheckboxFlags

// bool ImGui_CheckboxFlags(const char*, unsigned int*, unsigned int): Unknown
// argument type 'unsigned int*' (unsigned int*)

IMGUILUA_FUNCTION(ImGui::CheckboxFlags)
static int ImGui_CheckboxFlags(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(3);
    const char *label;
    label = lua_tostring(L, 1);
    int *flags;
    flags = static_cast<int *>(luaL_checkudata(L, 2, "ImGuiLuaIntRef"));
    int flags_value;
    flags_value = static_cast<int>(luaL_checkinteger(L, 3));
    bool RETVAL = ImGui::CheckboxFlags(label, flags, flags_value);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::CloseCurrentPopup

IMGUILUA_FUNCTION(ImGui::CloseCurrentPopup)
static int ImGui_CloseCurrentPopup(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImGui::CloseCurrentPopup();
    return 0;
}


// function ImGui::CollapsingHeader

static int ImGui_CollapsingHeader_si(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(1, 2);
    const char *label;
    label = lua_tostring(L, 1);
    int flags;
    if (ARGC < 2) {
        flags = 0;
    }
    else {
        flags = static_cast<int>(luaL_checkinteger(L, 2));
    }
    bool RETVAL = ImGui::CollapsingHeader(label, flags);
    lua_pushboolean(L, RETVAL);
    return 1;
}

static int ImGui_CollapsingHeader_sbPi(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(2, 3);
    const char *label;
    label = lua_tostring(L, 1);
    bool *p_visible;
    if (lua_type(L, 2) == LUA_TNIL) {
        p_visible = NULL;
    }
    else {
        p_visible =
            static_cast<bool *>(luaL_checkudata(L, 2, "ImGuiLuaBoolRef"));
    }
    int flags;
    if (ARGC < 3) {
        flags = 0;
    }
    else {
        flags = static_cast<int>(luaL_checkinteger(L, 3));
    }
    bool RETVAL = ImGui::CollapsingHeader(label, p_visible, flags);
    lua_pushboolean(L, RETVAL);
    return 1;
}

IMGUILUA_FUNCTION(ImGui::CollapsingHeader)
static int ImGui_CollapsingHeader(lua_State *L)
{
    int ARGC = lua_gettop(L);
    if (ARGC == 1 || (ARGC == 2 && lua_type(L, 2) == LUA_TNUMBER)) {
        return ImGui_CollapsingHeader_si(L);
    }
    else if (ARGC == 2 || ARGC == 3) {
        return ImGui_CollapsingHeader_sbPi(L);
    }
    else {
        return luaL_error(
            L, "Wrong number of arguments: got %d, wanted between %d and %d",
            ARGC, 1, 3);
    }
}


// function ImGui::ColorButton

IMGUILUA_FUNCTION(ImGui::ColorButton)
static int ImGui_ColorButton(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(2, 4);
    const char *desc_id;
    desc_id = lua_tostring(L, 1);
    ImVec4 col;
    col = ImGuiLua_ToImVec4(L, 2);
    int flags;
    if (ARGC < 3) {
        flags = 0;
    }
    else {
        flags = static_cast<int>(luaL_checkinteger(L, 3));
    }
    ImVec2 size;
    if (ARGC < 4) {
        size = ImVec2(0, 0);
    }
    else {
        size = ImGuiLua_ToImVec2(L, 4);
    }
    bool RETVAL = ImGui::ColorButton(desc_id, col, flags, size);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::ColorConvertFloat4ToU32

IMGUILUA_FUNCTION(ImGui::ColorConvertFloat4ToU32)
static int ImGui_ColorConvertFloat4ToU32(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    ImVec4 in;
    in = ImGuiLua_ToImVec4(L, 1);
    ImU32 RETVAL = ImGui::ColorConvertFloat4ToU32(in);
    lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
    return 1;
}


// function ImGui::ColorConvertHSVtoRGB

IMGUILUA_FUNCTION(ImGui::ColorConvertHSVtoRGB)
static int ImGui_ColorConvertHSVtoRGB(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(6);
    float h;
    h = static_cast<float>(luaL_checknumber(L, 1));
    float s;
    s = static_cast<float>(luaL_checknumber(L, 2));
    float v;
    v = static_cast<float>(luaL_checknumber(L, 3));
    float *out_r;
    out_r = static_cast<float *>(luaL_checkudata(L, 4, "ImGuiLuaFloatRef"));
    float *out_g;
    out_g = static_cast<float *>(luaL_checkudata(L, 5, "ImGuiLuaFloatRef"));
    float *out_b;
    out_b = static_cast<float *>(luaL_checkudata(L, 6, "ImGuiLuaFloatRef"));
    ImGui::ColorConvertHSVtoRGB(h, s, v, *out_r, *out_g, *out_b);
    return 0;
}


// function ImGui::ColorConvertRGBtoHSV

IMGUILUA_FUNCTION(ImGui::ColorConvertRGBtoHSV)
static int ImGui_ColorConvertRGBtoHSV(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(6);
    float r;
    r = static_cast<float>(luaL_checknumber(L, 1));
    float g;
    g = static_cast<float>(luaL_checknumber(L, 2));
    float b;
    b = static_cast<float>(luaL_checknumber(L, 3));
    float *out_h;
    out_h = static_cast<float *>(luaL_checkudata(L, 4, "ImGuiLuaFloatRef"));
    float *out_s;
    out_s = static_cast<float *>(luaL_checkudata(L, 5, "ImGuiLuaFloatRef"));
    float *out_v;
    out_v = static_cast<float *>(luaL_checkudata(L, 6, "ImGuiLuaFloatRef"));
    ImGui::ColorConvertRGBtoHSV(r, g, b, *out_h, *out_s, *out_v);
    return 0;
}


// function ImGui::ColorConvertU32ToFloat4

IMGUILUA_FUNCTION(ImGui::ColorConvertU32ToFloat4)
static int ImGui_ColorConvertU32ToFloat4(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    ImU32 in;
    in = static_cast<ImU32>(luaL_checkinteger(L, 1));
    ImVec4 RETVAL = ImGui::ColorConvertU32ToFloat4(in);
    ImGuiLua_PushImVec4(L, RETVAL);
    return 1;
}


// function ImGui::ColorEdit3

// bool ImGui_ColorEdit3(const char*, float[3], ImGuiColorEditFlags): Unknown
// argument type 'float[3]' (float[3])


// function ImGui::ColorEdit4

// bool ImGui_ColorEdit4(const char*, float[4], ImGuiColorEditFlags): Unknown
// argument type 'float[4]' (float[4])


// function ImGui::ColorPicker3

// bool ImGui_ColorPicker3(const char*, float[3], ImGuiColorEditFlags): Unknown
// argument type 'float[3]' (float[3])


// function ImGui::ColorPicker4

// bool ImGui_ColorPicker4(const char*, float[4], ImGuiColorEditFlags, const
// float*): Unknown argument type 'float[4]' (float[4])


// function ImGui::Columns

IMGUILUA_FUNCTION(ImGui::Columns)
static int ImGui_Columns(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(3);
    int count;
    if (ARGC < 1) {
        count = 1;
    }
    else {
        count = static_cast<int>(luaL_checkinteger(L, 1));
    }
    const char *id;
    if (ARGC < 2) {
        id = NULL;
    }
    else {
        id = lua_tostring(L, 2);
    }
    bool border;
    if (ARGC < 3) {
        border = true;
    }
    else {
        border = lua_toboolean(L, 3);
    }
    ImGui::Columns(count, id, border);
    return 0;
}


// function ImGui::Combo

// bool ImGui_Combo(const char*, int*, bool(*)(void* data,int idx,const char**
// out_text), void*, int, int): Unknown argument type 'bool(*)(void* data,int
// idx,const char** out_text)' (bool(*)(void* data,int idx,const char**
// out_text))

// bool ImGui_Combo(const char*, int*, const char* const[], int, int): Unknown
// argument type 'const char* const[]' (const char* const[])

IMGUILUA_FUNCTION(ImGui::Combo)
static int ImGui_Combo(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(3, 4);
    const char *label;
    label = lua_tostring(L, 1);
    int *current_item;
    current_item = static_cast<int *>(luaL_checkudata(L, 2, "ImGuiLuaIntRef"));
    const char *items_separated_by_zeros;
    items_separated_by_zeros = lua_tostring(L, 3);
    int popup_max_height_in_items;
    if (ARGC < 4) {
        popup_max_height_in_items = -1;
    }
    else {
        popup_max_height_in_items = static_cast<int>(luaL_checkinteger(L, 4));
    }
    bool RETVAL = ImGui::Combo(label, current_item, items_separated_by_zeros,
                               popup_max_height_in_items);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::CreateContext

// ImGuiContext* ImGui_CreateContext(ImFontAtlas*): Unknown return type
// 'ImGuiContext*' (ImGuiContext*)


// function ImGui::DebugCheckVersionAndDataLayout

// bool ImGui_DebugCheckVersionAndDataLayout(const char*, size_t, size_t,
// size_t, size_t, size_t, size_t): Unknown argument type 'size_t' (size_t)


// function ImGui::DestroyContext

// void ImGui_DestroyContext(ImGuiContext*): Unknown argument type
// 'ImGuiContext*' (ImGuiContext*)


// function ImGui::DestroyPlatformWindows

IMGUILUA_FUNCTION(ImGui::DestroyPlatformWindows)
static int ImGui_DestroyPlatformWindows(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImGui::DestroyPlatformWindows();
    return 0;
}


// function ImGui::DockSpace

IMGUILUA_FUNCTION(ImGui::DockSpace)
static int ImGui_DockSpace(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(1, 4);
    unsigned int id;
    id = static_cast<unsigned int>(luaL_checkinteger(L, 1));
    ImVec2 size;
    if (ARGC < 2) {
        size = ImVec2(0, 0);
    }
    else {
        size = ImGuiLua_ToImVec2(L, 2);
    }
    int flags;
    if (ARGC < 3) {
        flags = 0;
    }
    else {
        flags = static_cast<int>(luaL_checkinteger(L, 3));
    }
    const ImGuiWindowClass *window_class;
    if (ARGC < 4) {
        window_class = NULL;
    }
    else {
        window_class = static_cast<ImGuiWindowClass *>(
            luaL_checkudata(L, 4, "ImGuiWindowClass"));
    }
    unsigned int RETVAL = ImGui::DockSpace(id, size, flags, window_class);
    lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
    return 1;
}


// function ImGui::DockSpaceOverViewport

IMGUILUA_FUNCTION(ImGui::DockSpaceOverViewport)
static int ImGui_DockSpaceOverViewport(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(3);
    const ImGuiViewport *viewport;
    if (ARGC < 1) {
        viewport = NULL;
    }
    else {
        viewport = *static_cast<ImGuiViewport **>(
            luaL_checkudata(L, 1, "ImGuiViewport"));
    }
    int flags;
    if (ARGC < 2) {
        flags = 0;
    }
    else {
        flags = static_cast<int>(luaL_checkinteger(L, 2));
    }
    const ImGuiWindowClass *window_class;
    if (ARGC < 3) {
        window_class = NULL;
    }
    else {
        window_class = static_cast<ImGuiWindowClass *>(
            luaL_checkudata(L, 3, "ImGuiWindowClass"));
    }
    unsigned int RETVAL =
        ImGui::DockSpaceOverViewport(viewport, flags, window_class);
    lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
    return 1;
}


// function ImGui::DragFloat

IMGUILUA_FUNCTION(ImGui::DragFloat)
static int ImGui_DragFloat(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(2, 7);
    const char *label;
    label = lua_tostring(L, 1);
    float *v;
    v = static_cast<float *>(luaL_checkudata(L, 2, "ImGuiLuaFloatRef"));
    float v_speed;
    if (ARGC < 3) {
        v_speed = 1.0f;
    }
    else {
        v_speed = static_cast<float>(luaL_checknumber(L, 3));
    }
    float v_min;
    if (ARGC < 4) {
        v_min = 0.0f;
    }
    else {
        v_min = static_cast<float>(luaL_checknumber(L, 4));
    }
    float v_max;
    if (ARGC < 5) {
        v_max = 0.0f;
    }
    else {
        v_max = static_cast<float>(luaL_checknumber(L, 5));
    }
    const char *format;
    if (ARGC < 6) {
        format = "%.3f";
    }
    else {
        format = lua_tostring(L, 6);
    }
    int flags;
    if (ARGC < 7) {
        flags = 0;
    }
    else {
        flags = static_cast<int>(luaL_checkinteger(L, 7));
    }
    bool RETVAL =
        ImGui::DragFloat(label, v, v_speed, v_min, v_max, format, flags);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::DragFloat2

// bool ImGui_DragFloat2(const char*, float[2], float, float, float, const
// char*, ImGuiSliderFlags): Unknown argument type 'float[2]' (float[2])


// function ImGui::DragFloat3

// bool ImGui_DragFloat3(const char*, float[3], float, float, float, const
// char*, ImGuiSliderFlags): Unknown argument type 'float[3]' (float[3])


// function ImGui::DragFloat4

// bool ImGui_DragFloat4(const char*, float[4], float, float, float, const
// char*, ImGuiSliderFlags): Unknown argument type 'float[4]' (float[4])


// function ImGui::DragFloatRange2

IMGUILUA_FUNCTION(ImGui::DragFloatRange2)
static int ImGui_DragFloatRange2(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(3, 9);
    const char *label;
    label = lua_tostring(L, 1);
    float *v_current_min;
    v_current_min =
        static_cast<float *>(luaL_checkudata(L, 2, "ImGuiLuaFloatRef"));
    float *v_current_max;
    v_current_max =
        static_cast<float *>(luaL_checkudata(L, 3, "ImGuiLuaFloatRef"));
    float v_speed;
    if (ARGC < 4) {
        v_speed = 1.0f;
    }
    else {
        v_speed = static_cast<float>(luaL_checknumber(L, 4));
    }
    float v_min;
    if (ARGC < 5) {
        v_min = 0.0f;
    }
    else {
        v_min = static_cast<float>(luaL_checknumber(L, 5));
    }
    float v_max;
    if (ARGC < 6) {
        v_max = 0.0f;
    }
    else {
        v_max = static_cast<float>(luaL_checknumber(L, 6));
    }
    const char *format;
    if (ARGC < 7) {
        format = "%.3f";
    }
    else {
        format = lua_tostring(L, 7);
    }
    const char *format_max;
    if (ARGC < 8) {
        format_max = NULL;
    }
    else {
        format_max = lua_tostring(L, 8);
    }
    int flags;
    if (ARGC < 9) {
        flags = 0;
    }
    else {
        flags = static_cast<int>(luaL_checkinteger(L, 9));
    }
    bool RETVAL =
        ImGui::DragFloatRange2(label, v_current_min, v_current_max, v_speed,
                               v_min, v_max, format, format_max, flags);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::DragInt

IMGUILUA_FUNCTION(ImGui::DragInt)
static int ImGui_DragInt(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(2, 7);
    const char *label;
    label = lua_tostring(L, 1);
    int *v;
    v = static_cast<int *>(luaL_checkudata(L, 2, "ImGuiLuaIntRef"));
    float v_speed;
    if (ARGC < 3) {
        v_speed = 1.0f;
    }
    else {
        v_speed = static_cast<float>(luaL_checknumber(L, 3));
    }
    int v_min;
    if (ARGC < 4) {
        v_min = 0;
    }
    else {
        v_min = static_cast<int>(luaL_checkinteger(L, 4));
    }
    int v_max;
    if (ARGC < 5) {
        v_max = 0;
    }
    else {
        v_max = static_cast<int>(luaL_checkinteger(L, 5));
    }
    const char *format;
    if (ARGC < 6) {
        format = "%d";
    }
    else {
        format = lua_tostring(L, 6);
    }
    int flags;
    if (ARGC < 7) {
        flags = 0;
    }
    else {
        flags = static_cast<int>(luaL_checkinteger(L, 7));
    }
    bool RETVAL =
        ImGui::DragInt(label, v, v_speed, v_min, v_max, format, flags);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::DragInt2

// bool ImGui_DragInt2(const char*, int[2], float, int, int, const char*,
// ImGuiSliderFlags): Unknown argument type 'int[2]' (int[2])


// function ImGui::DragInt3

// bool ImGui_DragInt3(const char*, int[3], float, int, int, const char*,
// ImGuiSliderFlags): Unknown argument type 'int[3]' (int[3])


// function ImGui::DragInt4

// bool ImGui_DragInt4(const char*, int[4], float, int, int, const char*,
// ImGuiSliderFlags): Unknown argument type 'int[4]' (int[4])


// function ImGui::DragIntRange2

IMGUILUA_FUNCTION(ImGui::DragIntRange2)
static int ImGui_DragIntRange2(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(3, 9);
    const char *label;
    label = lua_tostring(L, 1);
    int *v_current_min;
    v_current_min = static_cast<int *>(luaL_checkudata(L, 2, "ImGuiLuaIntRef"));
    int *v_current_max;
    v_current_max = static_cast<int *>(luaL_checkudata(L, 3, "ImGuiLuaIntRef"));
    float v_speed;
    if (ARGC < 4) {
        v_speed = 1.0f;
    }
    else {
        v_speed = static_cast<float>(luaL_checknumber(L, 4));
    }
    int v_min;
    if (ARGC < 5) {
        v_min = 0;
    }
    else {
        v_min = static_cast<int>(luaL_checkinteger(L, 5));
    }
    int v_max;
    if (ARGC < 6) {
        v_max = 0;
    }
    else {
        v_max = static_cast<int>(luaL_checkinteger(L, 6));
    }
    const char *format;
    if (ARGC < 7) {
        format = "%d";
    }
    else {
        format = lua_tostring(L, 7);
    }
    const char *format_max;
    if (ARGC < 8) {
        format_max = NULL;
    }
    else {
        format_max = lua_tostring(L, 8);
    }
    int flags;
    if (ARGC < 9) {
        flags = 0;
    }
    else {
        flags = static_cast<int>(luaL_checkinteger(L, 9));
    }
    bool RETVAL =
        ImGui::DragIntRange2(label, v_current_min, v_current_max, v_speed,
                             v_min, v_max, format, format_max, flags);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::DragScalar

// bool ImGui_DragScalar(const char*, ImGuiDataType, void*, float, const void*,
// const void*, const char*, ImGuiSliderFlags): Unknown argument type 'void*'
// (void*)


// function ImGui::DragScalarN

// bool ImGui_DragScalarN(const char*, ImGuiDataType, void*, int, float, const
// void*, const void*, const char*, ImGuiSliderFlags): Unknown argument type
// 'void*' (void*)


// function ImGui::Dummy

IMGUILUA_FUNCTION(ImGui::Dummy)
static int ImGui_Dummy(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    ImVec2 size;
    size = ImGuiLua_ToImVec2(L, 1);
    ImGui::Dummy(size);
    return 0;
}


// function ImGui::End

IMGUILUA_FUNCTION(ImGui::End)
static int ImGui_End(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    IMGUILUA_END(L, ImGui::End, 1, ImGui::Begin);
    ImGui::End();
    return 0;
}


// function ImGui::EndChild

IMGUILUA_FUNCTION(ImGui::EndChild)
static int ImGui_EndChild(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    IMGUILUA_END(L, ImGui::EndChild, 2, ImGui::BeginChild);
    ImGui::EndChild();
    return 0;
}


// function ImGui::EndChildFrame

IMGUILUA_FUNCTION(ImGui::EndChildFrame)
static int ImGui_EndChildFrame(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    IMGUILUA_END(L, ImGui::EndChildFrame, 3, ImGui::BeginChildFrame);
    ImGui::EndChildFrame();
    return 0;
}


// function ImGui::EndCombo

IMGUILUA_FUNCTION(ImGui::EndCombo)
static int ImGui_EndCombo(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    IMGUILUA_END(L, ImGui::EndCombo, 4, ImGui::BeginCombo);
    ImGui::EndCombo();
    return 0;
}


// function ImGui::EndDisabled

IMGUILUA_FUNCTION(ImGui::EndDisabled)
static int ImGui_EndDisabled(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImGui::EndDisabled();
    return 0;
}


// function ImGui::EndDragDropSource

IMGUILUA_FUNCTION(ImGui::EndDragDropSource)
static int ImGui_EndDragDropSource(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    IMGUILUA_END(L, ImGui::EndDragDropSource, 5, ImGui::BeginDragDropSource);
    ImGui::EndDragDropSource();
    return 0;
}


// function ImGui::EndDragDropTarget

IMGUILUA_FUNCTION(ImGui::EndDragDropTarget)
static int ImGui_EndDragDropTarget(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    IMGUILUA_END(L, ImGui::EndDragDropTarget, 6, ImGui::BeginDragDropTarget);
    ImGui::EndDragDropTarget();
    return 0;
}


// function ImGui::EndFrame is excluded


// function ImGui::EndGroup

IMGUILUA_FUNCTION(ImGui::EndGroup)
static int ImGui_EndGroup(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    IMGUILUA_END(L, ImGui::EndGroup, 7, ImGui::BeginGroup);
    ImGui::EndGroup();
    return 0;
}


// function ImGui::EndListBox

IMGUILUA_FUNCTION(ImGui::EndListBox)
static int ImGui_EndListBox(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    IMGUILUA_END(L, ImGui::EndListBox, 8, ImGui::BeginListBox);
    ImGui::EndListBox();
    return 0;
}


// function ImGui::EndMainMenuBar

IMGUILUA_FUNCTION(ImGui::EndMainMenuBar)
static int ImGui_EndMainMenuBar(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    IMGUILUA_END(L, ImGui::EndMainMenuBar, 9, ImGui::BeginMainMenuBar);
    ImGui::EndMainMenuBar();
    return 0;
}


// function ImGui::EndMenu

IMGUILUA_FUNCTION(ImGui::EndMenu)
static int ImGui_EndMenu(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    IMGUILUA_END(L, ImGui::EndMenu, 10, ImGui::BeginMenu);
    ImGui::EndMenu();
    return 0;
}


// function ImGui::EndMenuBar

IMGUILUA_FUNCTION(ImGui::EndMenuBar)
static int ImGui_EndMenuBar(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    IMGUILUA_END(L, ImGui::EndMenuBar, 11, ImGui::BeginMenuBar);
    ImGui::EndMenuBar();
    return 0;
}


// function ImGui::EndPopup

IMGUILUA_FUNCTION(ImGui::EndPopup)
static int ImGui_EndPopup(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    IMGUILUA_END(L, ImGui::EndPopup, 12, ImGui::BeginPopup,
                 ImGui::BeginPopupContextItem, ImGui::BeginPopupContextVoid,
                 ImGui::BeginPopupContextWindow, ImGui::BeginPopupModal);
    ImGui::EndPopup();
    return 0;
}


// function ImGui::EndTabBar

IMGUILUA_FUNCTION(ImGui::EndTabBar)
static int ImGui_EndTabBar(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    IMGUILUA_END(L, ImGui::EndTabBar, 13, ImGui::BeginTabBar);
    ImGui::EndTabBar();
    return 0;
}


// function ImGui::EndTabItem

IMGUILUA_FUNCTION(ImGui::EndTabItem)
static int ImGui_EndTabItem(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    IMGUILUA_END(L, ImGui::EndTabItem, 14, ImGui::BeginTabItem);
    ImGui::EndTabItem();
    return 0;
}


// function ImGui::EndTable

IMGUILUA_FUNCTION(ImGui::EndTable)
static int ImGui_EndTable(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    IMGUILUA_END(L, ImGui::EndTable, 15, ImGui::BeginTable);
    ImGui::EndTable();
    return 0;
}


// function ImGui::EndTooltip

IMGUILUA_FUNCTION(ImGui::EndTooltip)
static int ImGui_EndTooltip(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    IMGUILUA_END(L, ImGui::EndTooltip, 16, ImGui::BeginTooltip);
    ImGui::EndTooltip();
    return 0;
}


// function ImGui::FindViewportByID

IMGUILUA_FUNCTION(ImGui::FindViewportByID)
static int ImGui_FindViewportByID(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    unsigned int id;
    id = static_cast<unsigned int>(luaL_checkinteger(L, 1));
    ImGuiViewport *RETVAL = ImGui::FindViewportByID(id);
    ImGuiViewport **pp =
        static_cast<ImGuiViewport **>(lua_newuserdatauv(L, sizeof(*pp), 0));
    *pp = RETVAL;
    luaL_setmetatable(L, "ImGuiViewport");
    return 1;
}


// function ImGui::FindViewportByPlatformHandle

// ImGuiViewport* ImGui_FindViewportByPlatformHandle(void*): Unknown argument
// type 'void*' (void*)


// function ImGui::GetAllocatorFunctions

// void ImGui_GetAllocatorFunctions(ImGuiMemAllocFunc*, ImGuiMemFreeFunc*,
// void**): Unknown argument type 'ImGuiMemAllocFunc*' (ImGuiMemAllocFunc*)


// function ImGui::GetBackgroundDrawList

// ImDrawList* ImGui_GetBackgroundDrawList(): Unknown return type 'ImDrawList*'
// (ImDrawList*)

// ImDrawList* ImGui_GetBackgroundDrawList(ImGuiViewport*): Unknown return type
// 'ImDrawList*' (ImDrawList*)


// function ImGui::GetClipboardText

IMGUILUA_FUNCTION(ImGui::GetClipboardText)
static int ImGui_GetClipboardText(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    const char *RETVAL = ImGui::GetClipboardText();
    lua_pushstring(L, RETVAL);
    return 1;
}


// function ImGui::GetColorU32

static int ImGui_GetColorU32_if(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(1, 2);
    int idx;
    idx = static_cast<int>(luaL_checkinteger(L, 1));
    float alpha_mul;
    if (ARGC < 2) {
        alpha_mul = 1.0f;
    }
    else {
        alpha_mul = static_cast<float>(luaL_checknumber(L, 2));
    }
    ImU32 RETVAL = ImGui::GetColorU32(idx, alpha_mul);
    lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
    return 1;
}

static int ImGui_GetColorU32_u32(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    ImU32 col;
    col = static_cast<ImU32>(luaL_checkinteger(L, 1));
    ImU32 RETVAL = ImGui::GetColorU32(col);
    lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
    return 1;
}

static int ImGui_GetColorU32_v4(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    ImVec4 col;
    col = ImGuiLua_ToImVec4(L, 1);
    ImU32 RETVAL = ImGui::GetColorU32(col);
    lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
    return 1;
}

IMGUILUA_FUNCTION(ImGui::GetColorU32)
static int ImGui_GetColorU32(lua_State *L)
{
    int ARGC = lua_gettop(L);
    if (ARGC == 1) {
        if (lua_isnumber(L, 1)) {
            return ImGui_GetColorU32_u32(L);
        }
        else {
            return ImGui_GetColorU32_v4(L);
        }
    }
    else if (ARGC == 2) {
        return ImGui_GetColorU32_if(L);
    }
    else {
        return luaL_error(
            L, "Wrong number of arguments: got %d, wanted between %d and %d",
            ARGC, 1, 2);
    }
}


// function ImGui::GetColumnIndex

IMGUILUA_FUNCTION(ImGui::GetColumnIndex)
static int ImGui_GetColumnIndex(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    int RETVAL = ImGui::GetColumnIndex();
    lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
    return 1;
}


// function ImGui::GetColumnOffset

IMGUILUA_FUNCTION(ImGui::GetColumnOffset)
static int ImGui_GetColumnOffset(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(1);
    int column_index;
    if (ARGC < 1) {
        column_index = -1;
    }
    else {
        column_index = static_cast<int>(luaL_checkinteger(L, 1));
    }
    float RETVAL = ImGui::GetColumnOffset(column_index);
    lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
    return 1;
}


// function ImGui::GetColumnWidth

IMGUILUA_FUNCTION(ImGui::GetColumnWidth)
static int ImGui_GetColumnWidth(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(1);
    int column_index;
    if (ARGC < 1) {
        column_index = -1;
    }
    else {
        column_index = static_cast<int>(luaL_checkinteger(L, 1));
    }
    float RETVAL = ImGui::GetColumnWidth(column_index);
    lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
    return 1;
}


// function ImGui::GetColumnsCount

IMGUILUA_FUNCTION(ImGui::GetColumnsCount)
static int ImGui_GetColumnsCount(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    int RETVAL = ImGui::GetColumnsCount();
    lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
    return 1;
}


// function ImGui::GetContentRegionAvail

IMGUILUA_FUNCTION(ImGui::GetContentRegionAvail)
static int ImGui_GetContentRegionAvail(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImVec2 RETVAL = ImGui::GetContentRegionAvail();
    ImGuiLua_PushImVec2(L, RETVAL);
    return 1;
}


// function ImGui::GetContentRegionMax

IMGUILUA_FUNCTION(ImGui::GetContentRegionMax)
static int ImGui_GetContentRegionMax(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImVec2 RETVAL = ImGui::GetContentRegionMax();
    ImGuiLua_PushImVec2(L, RETVAL);
    return 1;
}


// function ImGui::GetCurrentContext

// ImGuiContext* ImGui_GetCurrentContext(): Unknown return type 'ImGuiContext*'
// (ImGuiContext*)


// function ImGui::GetCursorPos

IMGUILUA_FUNCTION(ImGui::GetCursorPos)
static int ImGui_GetCursorPos(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImVec2 RETVAL = ImGui::GetCursorPos();
    ImGuiLua_PushImVec2(L, RETVAL);
    return 1;
}


// function ImGui::GetCursorPosX

IMGUILUA_FUNCTION(ImGui::GetCursorPosX)
static int ImGui_GetCursorPosX(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    float RETVAL = ImGui::GetCursorPosX();
    lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
    return 1;
}


// function ImGui::GetCursorPosY

IMGUILUA_FUNCTION(ImGui::GetCursorPosY)
static int ImGui_GetCursorPosY(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    float RETVAL = ImGui::GetCursorPosY();
    lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
    return 1;
}


// function ImGui::GetCursorScreenPos

IMGUILUA_FUNCTION(ImGui::GetCursorScreenPos)
static int ImGui_GetCursorScreenPos(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImVec2 RETVAL = ImGui::GetCursorScreenPos();
    ImGuiLua_PushImVec2(L, RETVAL);
    return 1;
}


// function ImGui::GetCursorStartPos

IMGUILUA_FUNCTION(ImGui::GetCursorStartPos)
static int ImGui_GetCursorStartPos(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImVec2 RETVAL = ImGui::GetCursorStartPos();
    ImGuiLua_PushImVec2(L, RETVAL);
    return 1;
}


// function ImGui::GetDragDropPayload

// const ImGuiPayload* ImGui_GetDragDropPayload(): Unknown return type 'const
// ImGuiPayload*' (const ImGuiPayload*)


// function ImGui::GetDrawData

// ImDrawData* ImGui_GetDrawData(): Unknown return type 'ImDrawData*'
// (ImDrawData*)


// function ImGui::GetDrawListSharedData

// ImDrawListSharedData* ImGui_GetDrawListSharedData(): Unknown return type
// 'ImDrawListSharedData*' (ImDrawListSharedData*)


// function ImGui::GetFont

// ImFont* ImGui_GetFont(): Unknown return type 'ImFont*' (ImFont*)


// function ImGui::GetFontSize

IMGUILUA_FUNCTION(ImGui::GetFontSize)
static int ImGui_GetFontSize(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    float RETVAL = ImGui::GetFontSize();
    lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
    return 1;
}


// function ImGui::GetFontTexUvWhitePixel

IMGUILUA_FUNCTION(ImGui::GetFontTexUvWhitePixel)
static int ImGui_GetFontTexUvWhitePixel(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImVec2 RETVAL = ImGui::GetFontTexUvWhitePixel();
    ImGuiLua_PushImVec2(L, RETVAL);
    return 1;
}


// function ImGui::GetForegroundDrawList

// ImDrawList* ImGui_GetForegroundDrawList(): Unknown return type 'ImDrawList*'
// (ImDrawList*)

// ImDrawList* ImGui_GetForegroundDrawList(ImGuiViewport*): Unknown return type
// 'ImDrawList*' (ImDrawList*)


// function ImGui::GetFrameCount

IMGUILUA_FUNCTION(ImGui::GetFrameCount)
static int ImGui_GetFrameCount(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    int RETVAL = ImGui::GetFrameCount();
    lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
    return 1;
}


// function ImGui::GetFrameHeight

IMGUILUA_FUNCTION(ImGui::GetFrameHeight)
static int ImGui_GetFrameHeight(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    float RETVAL = ImGui::GetFrameHeight();
    lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
    return 1;
}


// function ImGui::GetFrameHeightWithSpacing

IMGUILUA_FUNCTION(ImGui::GetFrameHeightWithSpacing)
static int ImGui_GetFrameHeightWithSpacing(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    float RETVAL = ImGui::GetFrameHeightWithSpacing();
    lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
    return 1;
}


// function ImGui::GetID is excluded


// function ImGui::GetIO is excluded


// function ImGui::GetItemRectMax

IMGUILUA_FUNCTION(ImGui::GetItemRectMax)
static int ImGui_GetItemRectMax(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImVec2 RETVAL = ImGui::GetItemRectMax();
    ImGuiLua_PushImVec2(L, RETVAL);
    return 1;
}


// function ImGui::GetItemRectMin

IMGUILUA_FUNCTION(ImGui::GetItemRectMin)
static int ImGui_GetItemRectMin(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImVec2 RETVAL = ImGui::GetItemRectMin();
    ImGuiLua_PushImVec2(L, RETVAL);
    return 1;
}


// function ImGui::GetItemRectSize

IMGUILUA_FUNCTION(ImGui::GetItemRectSize)
static int ImGui_GetItemRectSize(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImVec2 RETVAL = ImGui::GetItemRectSize();
    ImGuiLua_PushImVec2(L, RETVAL);
    return 1;
}


// function ImGui::GetKeyIndex

IMGUILUA_FUNCTION(ImGui::GetKeyIndex)
static int ImGui_GetKeyIndex(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    int key;
    key = static_cast<int>(luaL_checkinteger(L, 1));
    int RETVAL = ImGui::GetKeyIndex(key);
    lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
    return 1;
}


// function ImGui::GetKeyName

IMGUILUA_FUNCTION(ImGui::GetKeyName)
static int ImGui_GetKeyName(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    int key;
    key = static_cast<int>(luaL_checkinteger(L, 1));
    const char *RETVAL = ImGui::GetKeyName(key);
    lua_pushstring(L, RETVAL);
    return 1;
}


// function ImGui::GetKeyPressedAmount

IMGUILUA_FUNCTION(ImGui::GetKeyPressedAmount)
static int ImGui_GetKeyPressedAmount(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(3);
    int key;
    key = static_cast<int>(luaL_checkinteger(L, 1));
    float repeat_delay;
    repeat_delay = static_cast<float>(luaL_checknumber(L, 2));
    float rate;
    rate = static_cast<float>(luaL_checknumber(L, 3));
    int RETVAL = ImGui::GetKeyPressedAmount(key, repeat_delay, rate);
    lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
    return 1;
}


// function ImGui::GetMainViewport

IMGUILUA_FUNCTION(ImGui::GetMainViewport)
static int ImGui_GetMainViewport(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImGuiViewport *RETVAL = ImGui::GetMainViewport();
    ImGuiViewport **pp =
        static_cast<ImGuiViewport **>(lua_newuserdatauv(L, sizeof(*pp), 0));
    *pp = RETVAL;
    luaL_setmetatable(L, "ImGuiViewport");
    return 1;
}


// function ImGui::GetMouseClickedCount

IMGUILUA_FUNCTION(ImGui::GetMouseClickedCount)
static int ImGui_GetMouseClickedCount(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    int button;
    button = static_cast<int>(luaL_checkinteger(L, 1));
    int RETVAL = ImGui::GetMouseClickedCount(button);
    lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
    return 1;
}


// function ImGui::GetMouseCursor

IMGUILUA_FUNCTION(ImGui::GetMouseCursor)
static int ImGui_GetMouseCursor(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    int RETVAL = ImGui::GetMouseCursor();
    lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
    return 1;
}


// function ImGui::GetMouseDragDelta

IMGUILUA_FUNCTION(ImGui::GetMouseDragDelta)
static int ImGui_GetMouseDragDelta(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(2);
    int button;
    if (ARGC < 1) {
        button = 0;
    }
    else {
        button = static_cast<int>(luaL_checkinteger(L, 1));
    }
    float lock_threshold;
    if (ARGC < 2) {
        lock_threshold = -1.0f;
    }
    else {
        lock_threshold = static_cast<float>(luaL_checknumber(L, 2));
    }
    ImVec2 RETVAL = ImGui::GetMouseDragDelta(button, lock_threshold);
    ImGuiLua_PushImVec2(L, RETVAL);
    return 1;
}


// function ImGui::GetMousePos

IMGUILUA_FUNCTION(ImGui::GetMousePos)
static int ImGui_GetMousePos(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImVec2 RETVAL = ImGui::GetMousePos();
    ImGuiLua_PushImVec2(L, RETVAL);
    return 1;
}


// function ImGui::GetMousePosOnOpeningCurrentPopup

IMGUILUA_FUNCTION(ImGui::GetMousePosOnOpeningCurrentPopup)
static int ImGui_GetMousePosOnOpeningCurrentPopup(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImVec2 RETVAL = ImGui::GetMousePosOnOpeningCurrentPopup();
    ImGuiLua_PushImVec2(L, RETVAL);
    return 1;
}


// function ImGui::GetPlatformIO

// ImGuiPlatformIO* ImGui_GetPlatformIO(): Unknown return type
// 'ImGuiPlatformIO*' (ImGuiPlatformIO*)


// function ImGui::GetScrollMaxX

IMGUILUA_FUNCTION(ImGui::GetScrollMaxX)
static int ImGui_GetScrollMaxX(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    float RETVAL = ImGui::GetScrollMaxX();
    lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
    return 1;
}


// function ImGui::GetScrollMaxY

IMGUILUA_FUNCTION(ImGui::GetScrollMaxY)
static int ImGui_GetScrollMaxY(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    float RETVAL = ImGui::GetScrollMaxY();
    lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
    return 1;
}


// function ImGui::GetScrollX

IMGUILUA_FUNCTION(ImGui::GetScrollX)
static int ImGui_GetScrollX(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    float RETVAL = ImGui::GetScrollX();
    lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
    return 1;
}


// function ImGui::GetScrollY

IMGUILUA_FUNCTION(ImGui::GetScrollY)
static int ImGui_GetScrollY(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    float RETVAL = ImGui::GetScrollY();
    lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
    return 1;
}


// function ImGui::GetStateStorage

// ImGuiStorage* ImGui_GetStateStorage(): Unknown return type 'ImGuiStorage*'
// (ImGuiStorage*)


// function ImGui::GetStyle

// ImGuiStyle* ImGui_GetStyle(): Unknown return type 'ImGuiStyle*' (ImGuiStyle*)


// function ImGui::GetStyleColorName

IMGUILUA_FUNCTION(ImGui::GetStyleColorName)
static int ImGui_GetStyleColorName(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    int idx;
    idx = static_cast<int>(luaL_checkinteger(L, 1));
    const char *RETVAL = ImGui::GetStyleColorName(idx);
    lua_pushstring(L, RETVAL);
    return 1;
}


// function ImGui::GetStyleColorVec4

// const ImVec4* ImGui_GetStyleColorVec4(ImGuiCol): Unknown return type 'const
// ImVec4*' (const ImVec4*)


// function ImGui::GetTextLineHeight

IMGUILUA_FUNCTION(ImGui::GetTextLineHeight)
static int ImGui_GetTextLineHeight(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    float RETVAL = ImGui::GetTextLineHeight();
    lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
    return 1;
}


// function ImGui::GetTextLineHeightWithSpacing

IMGUILUA_FUNCTION(ImGui::GetTextLineHeightWithSpacing)
static int ImGui_GetTextLineHeightWithSpacing(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    float RETVAL = ImGui::GetTextLineHeightWithSpacing();
    lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
    return 1;
}


// function ImGui::GetTime

IMGUILUA_FUNCTION(ImGui::GetTime)
static int ImGui_GetTime(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    double RETVAL = ImGui::GetTime();
    lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
    return 1;
}


// function ImGui::GetTreeNodeToLabelSpacing

IMGUILUA_FUNCTION(ImGui::GetTreeNodeToLabelSpacing)
static int ImGui_GetTreeNodeToLabelSpacing(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    float RETVAL = ImGui::GetTreeNodeToLabelSpacing();
    lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
    return 1;
}


// function ImGui::GetVersion

IMGUILUA_FUNCTION(ImGui::GetVersion)
static int ImGui_GetVersion(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    const char *RETVAL = ImGui::GetVersion();
    lua_pushstring(L, RETVAL);
    return 1;
}


// function ImGui::GetWindowContentRegionMax

IMGUILUA_FUNCTION(ImGui::GetWindowContentRegionMax)
static int ImGui_GetWindowContentRegionMax(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImVec2 RETVAL = ImGui::GetWindowContentRegionMax();
    ImGuiLua_PushImVec2(L, RETVAL);
    return 1;
}


// function ImGui::GetWindowContentRegionMin

IMGUILUA_FUNCTION(ImGui::GetWindowContentRegionMin)
static int ImGui_GetWindowContentRegionMin(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImVec2 RETVAL = ImGui::GetWindowContentRegionMin();
    ImGuiLua_PushImVec2(L, RETVAL);
    return 1;
}


// function ImGui::GetWindowDockID

IMGUILUA_FUNCTION(ImGui::GetWindowDockID)
static int ImGui_GetWindowDockID(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    unsigned int RETVAL = ImGui::GetWindowDockID();
    lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
    return 1;
}


// function ImGui::GetWindowDpiScale

IMGUILUA_FUNCTION(ImGui::GetWindowDpiScale)
static int ImGui_GetWindowDpiScale(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    float RETVAL = ImGui::GetWindowDpiScale();
    lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
    return 1;
}


// function ImGui::GetWindowDrawList

// ImDrawList* ImGui_GetWindowDrawList(): Unknown return type 'ImDrawList*'
// (ImDrawList*)


// function ImGui::GetWindowHeight

IMGUILUA_FUNCTION(ImGui::GetWindowHeight)
static int ImGui_GetWindowHeight(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    float RETVAL = ImGui::GetWindowHeight();
    lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
    return 1;
}


// function ImGui::GetWindowPos

IMGUILUA_FUNCTION(ImGui::GetWindowPos)
static int ImGui_GetWindowPos(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImVec2 RETVAL = ImGui::GetWindowPos();
    ImGuiLua_PushImVec2(L, RETVAL);
    return 1;
}


// function ImGui::GetWindowSize

IMGUILUA_FUNCTION(ImGui::GetWindowSize)
static int ImGui_GetWindowSize(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImVec2 RETVAL = ImGui::GetWindowSize();
    ImGuiLua_PushImVec2(L, RETVAL);
    return 1;
}


// function ImGui::GetWindowViewport

IMGUILUA_FUNCTION(ImGui::GetWindowViewport)
static int ImGui_GetWindowViewport(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImGuiViewport *RETVAL = ImGui::GetWindowViewport();
    ImGuiViewport **pp =
        static_cast<ImGuiViewport **>(lua_newuserdatauv(L, sizeof(*pp), 0));
    *pp = RETVAL;
    luaL_setmetatable(L, "ImGuiViewport");
    return 1;
}


// function ImGui::GetWindowWidth

IMGUILUA_FUNCTION(ImGui::GetWindowWidth)
static int ImGui_GetWindowWidth(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    float RETVAL = ImGui::GetWindowWidth();
    lua_pushnumber(L, static_cast<lua_Number>(RETVAL));
    return 1;
}


// function ImGui::Image

// void ImGui_Image(ImTextureID, const ImVec2, const ImVec2, const ImVec2, const
// ImVec4, const ImVec4): Unknown argument type 'ImTextureID' (ImTextureID)


// function ImGui::ImageButton

// bool ImGui_ImageButton(ImTextureID, const ImVec2, const ImVec2, const ImVec2,
// int, const ImVec4, const ImVec4): Unknown argument type 'ImTextureID'
// (ImTextureID)


// function ImGui::Indent

IMGUILUA_FUNCTION(ImGui::Indent)
static int ImGui_Indent(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(1);
    float indent_w;
    if (ARGC < 1) {
        indent_w = 0.0f;
    }
    else {
        indent_w = static_cast<float>(luaL_checknumber(L, 1));
    }
    ImGui::Indent(indent_w);
    return 0;
}


// function ImGui::InputDouble

// bool ImGui_InputDouble(const char*, double*, double, double, const char*,
// ImGuiInputTextFlags): Unknown argument type 'double*' (double*)


// function ImGui::InputFloat

IMGUILUA_FUNCTION(ImGui::InputFloat)
static int ImGui_InputFloat(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(2, 6);
    const char *label;
    label = lua_tostring(L, 1);
    float *v;
    v = static_cast<float *>(luaL_checkudata(L, 2, "ImGuiLuaFloatRef"));
    float step;
    if (ARGC < 3) {
        step = 0.0f;
    }
    else {
        step = static_cast<float>(luaL_checknumber(L, 3));
    }
    float step_fast;
    if (ARGC < 4) {
        step_fast = 0.0f;
    }
    else {
        step_fast = static_cast<float>(luaL_checknumber(L, 4));
    }
    const char *format;
    if (ARGC < 5) {
        format = "%.3f";
    }
    else {
        format = lua_tostring(L, 5);
    }
    int flags;
    if (ARGC < 6) {
        flags = 0;
    }
    else {
        flags = static_cast<int>(luaL_checkinteger(L, 6));
    }
    bool RETVAL = ImGui::InputFloat(label, v, step, step_fast, format, flags);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::InputFloat2

// bool ImGui_InputFloat2(const char*, float[2], const char*,
// ImGuiInputTextFlags): Unknown argument type 'float[2]' (float[2])


// function ImGui::InputFloat3

// bool ImGui_InputFloat3(const char*, float[3], const char*,
// ImGuiInputTextFlags): Unknown argument type 'float[3]' (float[3])


// function ImGui::InputFloat4

// bool ImGui_InputFloat4(const char*, float[4], const char*,
// ImGuiInputTextFlags): Unknown argument type 'float[4]' (float[4])


// function ImGui::InputInt

IMGUILUA_FUNCTION(ImGui::InputInt)
static int ImGui_InputInt(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(2, 5);
    const char *label;
    label = lua_tostring(L, 1);
    int *v;
    v = static_cast<int *>(luaL_checkudata(L, 2, "ImGuiLuaIntRef"));
    int step;
    if (ARGC < 3) {
        step = 1;
    }
    else {
        step = static_cast<int>(luaL_checkinteger(L, 3));
    }
    int step_fast;
    if (ARGC < 4) {
        step_fast = 100;
    }
    else {
        step_fast = static_cast<int>(luaL_checkinteger(L, 4));
    }
    int flags;
    if (ARGC < 5) {
        flags = 0;
    }
    else {
        flags = static_cast<int>(luaL_checkinteger(L, 5));
    }
    bool RETVAL = ImGui::InputInt(label, v, step, step_fast, flags);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::InputInt2

// bool ImGui_InputInt2(const char*, int[2], ImGuiInputTextFlags): Unknown
// argument type 'int[2]' (int[2])


// function ImGui::InputInt3

// bool ImGui_InputInt3(const char*, int[3], ImGuiInputTextFlags): Unknown
// argument type 'int[3]' (int[3])


// function ImGui::InputInt4

// bool ImGui_InputInt4(const char*, int[4], ImGuiInputTextFlags): Unknown
// argument type 'int[4]' (int[4])


// function ImGui::InputScalar

// bool ImGui_InputScalar(const char*, ImGuiDataType, void*, const void*, const
// void*, const char*, ImGuiInputTextFlags): Unknown argument type 'void*'
// (void*)


// function ImGui::InputScalarN

// bool ImGui_InputScalarN(const char*, ImGuiDataType, void*, int, const void*,
// const void*, const char*, ImGuiInputTextFlags): Unknown argument type 'void*'
// (void*)


// function ImGui::InputText

// bool ImGui_InputText(const char*, char*, size_t, ImGuiInputTextFlags,
// ImGuiInputTextCallback, void*): Unknown argument type 'char*' (char*)


// function ImGui::InputTextMultiline

// bool ImGui_InputTextMultiline(const char*, char*, size_t, const ImVec2,
// ImGuiInputTextFlags, ImGuiInputTextCallback, void*): Unknown argument type
// 'char*' (char*)


// function ImGui::InputTextWithHint

// bool ImGui_InputTextWithHint(const char*, const char*, char*, size_t,
// ImGuiInputTextFlags, ImGuiInputTextCallback, void*): Unknown argument type
// 'char*' (char*)


// function ImGui::InvisibleButton

IMGUILUA_FUNCTION(ImGui::InvisibleButton)
static int ImGui_InvisibleButton(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(2, 3);
    const char *str_id;
    str_id = lua_tostring(L, 1);
    ImVec2 size;
    size = ImGuiLua_ToImVec2(L, 2);
    int flags;
    if (ARGC < 3) {
        flags = 0;
    }
    else {
        flags = static_cast<int>(luaL_checkinteger(L, 3));
    }
    bool RETVAL = ImGui::InvisibleButton(str_id, size, flags);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::IsAnyItemActive

IMGUILUA_FUNCTION(ImGui::IsAnyItemActive)
static int ImGui_IsAnyItemActive(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    bool RETVAL = ImGui::IsAnyItemActive();
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::IsAnyItemFocused

IMGUILUA_FUNCTION(ImGui::IsAnyItemFocused)
static int ImGui_IsAnyItemFocused(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    bool RETVAL = ImGui::IsAnyItemFocused();
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::IsAnyItemHovered

IMGUILUA_FUNCTION(ImGui::IsAnyItemHovered)
static int ImGui_IsAnyItemHovered(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    bool RETVAL = ImGui::IsAnyItemHovered();
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::IsAnyMouseDown

IMGUILUA_FUNCTION(ImGui::IsAnyMouseDown)
static int ImGui_IsAnyMouseDown(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    bool RETVAL = ImGui::IsAnyMouseDown();
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::IsItemActivated

IMGUILUA_FUNCTION(ImGui::IsItemActivated)
static int ImGui_IsItemActivated(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    bool RETVAL = ImGui::IsItemActivated();
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::IsItemActive

IMGUILUA_FUNCTION(ImGui::IsItemActive)
static int ImGui_IsItemActive(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    bool RETVAL = ImGui::IsItemActive();
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::IsItemClicked

IMGUILUA_FUNCTION(ImGui::IsItemClicked)
static int ImGui_IsItemClicked(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(1);
    int mouse_button;
    if (ARGC < 1) {
        mouse_button = 0;
    }
    else {
        mouse_button = static_cast<int>(luaL_checkinteger(L, 1));
    }
    bool RETVAL = ImGui::IsItemClicked(mouse_button);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::IsItemDeactivated

IMGUILUA_FUNCTION(ImGui::IsItemDeactivated)
static int ImGui_IsItemDeactivated(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    bool RETVAL = ImGui::IsItemDeactivated();
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::IsItemDeactivatedAfterEdit

IMGUILUA_FUNCTION(ImGui::IsItemDeactivatedAfterEdit)
static int ImGui_IsItemDeactivatedAfterEdit(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    bool RETVAL = ImGui::IsItemDeactivatedAfterEdit();
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::IsItemEdited

IMGUILUA_FUNCTION(ImGui::IsItemEdited)
static int ImGui_IsItemEdited(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    bool RETVAL = ImGui::IsItemEdited();
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::IsItemFocused

IMGUILUA_FUNCTION(ImGui::IsItemFocused)
static int ImGui_IsItemFocused(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    bool RETVAL = ImGui::IsItemFocused();
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::IsItemHovered

IMGUILUA_FUNCTION(ImGui::IsItemHovered)
static int ImGui_IsItemHovered(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(1);
    int flags;
    if (ARGC < 1) {
        flags = 0;
    }
    else {
        flags = static_cast<int>(luaL_checkinteger(L, 1));
    }
    bool RETVAL = ImGui::IsItemHovered(flags);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::IsItemToggledOpen

IMGUILUA_FUNCTION(ImGui::IsItemToggledOpen)
static int ImGui_IsItemToggledOpen(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    bool RETVAL = ImGui::IsItemToggledOpen();
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::IsItemVisible

IMGUILUA_FUNCTION(ImGui::IsItemVisible)
static int ImGui_IsItemVisible(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    bool RETVAL = ImGui::IsItemVisible();
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::IsKeyDown

IMGUILUA_FUNCTION(ImGui::IsKeyDown)
static int ImGui_IsKeyDown(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    int key;
    key = static_cast<int>(luaL_checkinteger(L, 1));
    bool RETVAL = ImGui::IsKeyDown(key);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::IsKeyPressed

IMGUILUA_FUNCTION(ImGui::IsKeyPressed)
static int ImGui_IsKeyPressed(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(1, 2);
    int key;
    key = static_cast<int>(luaL_checkinteger(L, 1));
    bool repeat;
    if (ARGC < 2) {
        repeat = true;
    }
    else {
        repeat = lua_toboolean(L, 2);
    }
    bool RETVAL = ImGui::IsKeyPressed(key, repeat);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::IsKeyReleased

IMGUILUA_FUNCTION(ImGui::IsKeyReleased)
static int ImGui_IsKeyReleased(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    int key;
    key = static_cast<int>(luaL_checkinteger(L, 1));
    bool RETVAL = ImGui::IsKeyReleased(key);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::IsMouseClicked

IMGUILUA_FUNCTION(ImGui::IsMouseClicked)
static int ImGui_IsMouseClicked(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(1, 2);
    int button;
    button = static_cast<int>(luaL_checkinteger(L, 1));
    bool repeat;
    if (ARGC < 2) {
        repeat = false;
    }
    else {
        repeat = lua_toboolean(L, 2);
    }
    bool RETVAL = ImGui::IsMouseClicked(button, repeat);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::IsMouseDoubleClicked

IMGUILUA_FUNCTION(ImGui::IsMouseDoubleClicked)
static int ImGui_IsMouseDoubleClicked(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    int button;
    button = static_cast<int>(luaL_checkinteger(L, 1));
    bool RETVAL = ImGui::IsMouseDoubleClicked(button);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::IsMouseDown

IMGUILUA_FUNCTION(ImGui::IsMouseDown)
static int ImGui_IsMouseDown(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    int button;
    button = static_cast<int>(luaL_checkinteger(L, 1));
    bool RETVAL = ImGui::IsMouseDown(button);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::IsMouseDragging

IMGUILUA_FUNCTION(ImGui::IsMouseDragging)
static int ImGui_IsMouseDragging(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(1, 2);
    int button;
    button = static_cast<int>(luaL_checkinteger(L, 1));
    float lock_threshold;
    if (ARGC < 2) {
        lock_threshold = -1.0f;
    }
    else {
        lock_threshold = static_cast<float>(luaL_checknumber(L, 2));
    }
    bool RETVAL = ImGui::IsMouseDragging(button, lock_threshold);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::IsMouseHoveringRect

IMGUILUA_FUNCTION(ImGui::IsMouseHoveringRect)
static int ImGui_IsMouseHoveringRect(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(2, 3);
    ImVec2 r_min;
    r_min = ImGuiLua_ToImVec2(L, 1);
    ImVec2 r_max;
    r_max = ImGuiLua_ToImVec2(L, 2);
    bool clip;
    if (ARGC < 3) {
        clip = true;
    }
    else {
        clip = lua_toboolean(L, 3);
    }
    bool RETVAL = ImGui::IsMouseHoveringRect(r_min, r_max, clip);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::IsMousePosValid

// bool ImGui_IsMousePosValid(const ImVec2*): Unknown argument type 'const
// ImVec2*' (const ImVec2*)


// function ImGui::IsMouseReleased

IMGUILUA_FUNCTION(ImGui::IsMouseReleased)
static int ImGui_IsMouseReleased(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    int button;
    button = static_cast<int>(luaL_checkinteger(L, 1));
    bool RETVAL = ImGui::IsMouseReleased(button);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::IsPopupOpen

IMGUILUA_FUNCTION(ImGui::IsPopupOpen)
static int ImGui_IsPopupOpen(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(1, 2);
    const char *str_id;
    str_id = lua_tostring(L, 1);
    int flags;
    if (ARGC < 2) {
        flags = 0;
    }
    else {
        flags = static_cast<int>(luaL_checkinteger(L, 2));
    }
    bool RETVAL = ImGui::IsPopupOpen(str_id, flags);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::IsRectVisible

static int ImGui_IsRectVisible_v2(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    ImVec2 size;
    size = ImGuiLua_ToImVec2(L, 1);
    bool RETVAL = ImGui::IsRectVisible(size);
    lua_pushboolean(L, RETVAL);
    return 1;
}

static int ImGui_IsRectVisible_v2v2(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(2);
    ImVec2 rect_min;
    rect_min = ImGuiLua_ToImVec2(L, 1);
    ImVec2 rect_max;
    rect_max = ImGuiLua_ToImVec2(L, 2);
    bool RETVAL = ImGui::IsRectVisible(rect_min, rect_max);
    lua_pushboolean(L, RETVAL);
    return 1;
}

IMGUILUA_FUNCTION(ImGui::IsRectVisible)
static int ImGui_IsRectVisible(lua_State *L)
{
    int ARGC = lua_gettop(L);
    switch (ARGC) {
    case 1:
        return ImGui_IsRectVisible_v2(L);
    case 2:
        return ImGui_IsRectVisible_v2v2(L);
    default:
        return luaL_error(
            L, "Wrong number of arguments: got %d, wanted between %d and %d",
            ARGC, 1, 2);
    }
}


// function ImGui::IsWindowAppearing

IMGUILUA_FUNCTION(ImGui::IsWindowAppearing)
static int ImGui_IsWindowAppearing(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    bool RETVAL = ImGui::IsWindowAppearing();
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::IsWindowCollapsed

IMGUILUA_FUNCTION(ImGui::IsWindowCollapsed)
static int ImGui_IsWindowCollapsed(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    bool RETVAL = ImGui::IsWindowCollapsed();
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::IsWindowDocked

IMGUILUA_FUNCTION(ImGui::IsWindowDocked)
static int ImGui_IsWindowDocked(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    bool RETVAL = ImGui::IsWindowDocked();
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::IsWindowFocused

IMGUILUA_FUNCTION(ImGui::IsWindowFocused)
static int ImGui_IsWindowFocused(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(1);
    int flags;
    if (ARGC < 1) {
        flags = 0;
    }
    else {
        flags = static_cast<int>(luaL_checkinteger(L, 1));
    }
    bool RETVAL = ImGui::IsWindowFocused(flags);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::IsWindowHovered

IMGUILUA_FUNCTION(ImGui::IsWindowHovered)
static int ImGui_IsWindowHovered(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(1);
    int flags;
    if (ARGC < 1) {
        flags = 0;
    }
    else {
        flags = static_cast<int>(luaL_checkinteger(L, 1));
    }
    bool RETVAL = ImGui::IsWindowHovered(flags);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::LabelText is excluded


// function ImGui::LabelTextV is excluded


// function ImGui::ListBox

// bool ImGui_ListBox(const char*, int*, bool(*)(void* data,int idx,const char**
// out_text), void*, int, int): Unknown argument type 'bool(*)(void* data,int
// idx,const char** out_text)' (bool(*)(void* data,int idx,const char**
// out_text))

// bool ImGui_ListBox(const char*, int*, const char* const[], int, int): Unknown
// argument type 'const char* const[]' (const char* const[])


// function ImGui::LoadIniSettingsFromDisk

IMGUILUA_FUNCTION(ImGui::LoadIniSettingsFromDisk)
static int ImGui_LoadIniSettingsFromDisk(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    const char *ini_filename;
    ini_filename = lua_tostring(L, 1);
    ImGui::LoadIniSettingsFromDisk(ini_filename);
    return 0;
}


// function ImGui::LoadIniSettingsFromMemory

// void ImGui_LoadIniSettingsFromMemory(const char*, size_t): Unknown argument
// type 'size_t' (size_t)


// function ImGui::LogButtons

IMGUILUA_FUNCTION(ImGui::LogButtons)
static int ImGui_LogButtons(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImGui::LogButtons();
    return 0;
}


// function ImGui::LogFinish

IMGUILUA_FUNCTION(ImGui::LogFinish)
static int ImGui_LogFinish(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImGui::LogFinish();
    return 0;
}


// function ImGui::LogText

// void ImGui_LogText(const char*, ...): Unknown argument type '...' (...)


// function ImGui::LogTextV

// void ImGui_LogTextV(const char*, va_list): Unknown argument type 'va_list'
// (va_list)


// function ImGui::LogToClipboard

IMGUILUA_FUNCTION(ImGui::LogToClipboard)
static int ImGui_LogToClipboard(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(1);
    int auto_open_depth;
    if (ARGC < 1) {
        auto_open_depth = -1;
    }
    else {
        auto_open_depth = static_cast<int>(luaL_checkinteger(L, 1));
    }
    ImGui::LogToClipboard(auto_open_depth);
    return 0;
}


// function ImGui::LogToFile

IMGUILUA_FUNCTION(ImGui::LogToFile)
static int ImGui_LogToFile(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(2);
    int auto_open_depth;
    if (ARGC < 1) {
        auto_open_depth = -1;
    }
    else {
        auto_open_depth = static_cast<int>(luaL_checkinteger(L, 1));
    }
    const char *filename;
    if (ARGC < 2) {
        filename = NULL;
    }
    else {
        filename = lua_tostring(L, 2);
    }
    ImGui::LogToFile(auto_open_depth, filename);
    return 0;
}


// function ImGui::LogToTTY

IMGUILUA_FUNCTION(ImGui::LogToTTY)
static int ImGui_LogToTTY(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(1);
    int auto_open_depth;
    if (ARGC < 1) {
        auto_open_depth = -1;
    }
    else {
        auto_open_depth = static_cast<int>(luaL_checkinteger(L, 1));
    }
    ImGui::LogToTTY(auto_open_depth);
    return 0;
}


// function ImGui::MemAlloc

// void* ImGui_MemAlloc(size_t): Unknown return type 'void*' (void*)


// function ImGui::MemFree

// void ImGui_MemFree(void*): Unknown argument type 'void*' (void*)


// function ImGui::MenuItem

// bool ImGui_MenuItem(const char*, const char*, bool*, bool) is excluded

IMGUILUA_FUNCTION(ImGui::MenuItem)
static int ImGui_MenuItem(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(1, 4);
    const char *label;
    label = lua_tostring(L, 1);
    const char *shortcut;
    if (ARGC < 2) {
        shortcut = NULL;
    }
    else {
        shortcut = lua_tostring(L, 2);
    }
    bool selected;
    if (ARGC < 3) {
        selected = false;
    }
    else {
        selected = lua_toboolean(L, 3);
    }
    bool enabled;
    if (ARGC < 4) {
        enabled = true;
    }
    else {
        enabled = lua_toboolean(L, 4);
    }
    bool RETVAL = ImGui::MenuItem(label, shortcut, selected, enabled);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::NewFrame is excluded


// function ImGui::NewLine

IMGUILUA_FUNCTION(ImGui::NewLine)
static int ImGui_NewLine(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImGui::NewLine();
    return 0;
}


// function ImGui::NextColumn

IMGUILUA_FUNCTION(ImGui::NextColumn)
static int ImGui_NextColumn(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImGui::NextColumn();
    return 0;
}


// function ImGui::OpenPopup

static int ImGui_OpenPopup_ui(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(1, 2);
    unsigned int id;
    id = static_cast<unsigned int>(luaL_checkinteger(L, 1));
    int popup_flags;
    if (ARGC < 2) {
        popup_flags = 0;
    }
    else {
        popup_flags = static_cast<int>(luaL_checkinteger(L, 2));
    }
    ImGui::OpenPopup(id, popup_flags);
    return 0;
}

static int ImGui_OpenPopup_si(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(1, 2);
    const char *str_id;
    str_id = lua_tostring(L, 1);
    int popup_flags;
    if (ARGC < 2) {
        popup_flags = 0;
    }
    else {
        popup_flags = static_cast<int>(luaL_checkinteger(L, 2));
    }
    ImGui::OpenPopup(str_id, popup_flags);
    return 0;
}

IMGUILUA_FUNCTION(ImGui::OpenPopup)
static int ImGui_OpenPopup(lua_State *L)
{
    if (lua_type(L, 1) == LUA_TNUMBER) {
        return ImGui_OpenPopup_ui(L);
    }
    else {
        return ImGui_OpenPopup_si(L);
    }
}


// function ImGui::OpenPopupOnItemClick

IMGUILUA_FUNCTION(ImGui::OpenPopupOnItemClick)
static int ImGui_OpenPopupOnItemClick(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(2);
    const char *str_id;
    if (ARGC < 1) {
        str_id = NULL;
    }
    else {
        str_id = lua_tostring(L, 1);
    }
    int popup_flags;
    if (ARGC < 2) {
        popup_flags = 1;
    }
    else {
        popup_flags = static_cast<int>(luaL_checkinteger(L, 2));
    }
    ImGui::OpenPopupOnItemClick(str_id, popup_flags);
    return 0;
}


// function ImGui::PlotHistogram

// void ImGui_PlotHistogram(const char*, const float*, int, int, const char*,
// float, float, ImVec2, int): Unknown argument type 'const float*' (const
// float*)

// void ImGui_PlotHistogram(const char*, float(*)(void* data,int idx), void*,
// int, int, const char*, float, float, ImVec2): Unknown argument type
// 'float(*)(void* data,int idx)' (float(*)(void* data,int idx))


// function ImGui::PlotLines

// void ImGui_PlotLines(const char*, const float*, int, int, const char*, float,
// float, ImVec2, int): Unknown argument type 'const float*' (const float*)

// void ImGui_PlotLines(const char*, float(*)(void* data,int idx), void*, int,
// int, const char*, float, float, ImVec2): Unknown argument type
// 'float(*)(void* data,int idx)' (float(*)(void* data,int idx))


// function ImGui::PopAllowKeyboardFocus

IMGUILUA_FUNCTION(ImGui::PopAllowKeyboardFocus)
static int ImGui_PopAllowKeyboardFocus(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImGui::PopAllowKeyboardFocus();
    return 0;
}


// function ImGui::PopButtonRepeat

IMGUILUA_FUNCTION(ImGui::PopButtonRepeat)
static int ImGui_PopButtonRepeat(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImGui::PopButtonRepeat();
    return 0;
}


// function ImGui::PopClipRect

IMGUILUA_FUNCTION(ImGui::PopClipRect)
static int ImGui_PopClipRect(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImGui::PopClipRect();
    return 0;
}


// function ImGui::PopFont

IMGUILUA_FUNCTION(ImGui::PopFont)
static int ImGui_PopFont(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImGui::PopFont();
    return 0;
}


// function ImGui::PopID

IMGUILUA_FUNCTION(ImGui::PopID)
static int ImGui_PopID(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImGui::PopID();
    return 0;
}


// function ImGui::PopItemWidth

IMGUILUA_FUNCTION(ImGui::PopItemWidth)
static int ImGui_PopItemWidth(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImGui::PopItemWidth();
    return 0;
}


// function ImGui::PopStyleColor is excluded


// function ImGui::PopStyleVar is excluded


// function ImGui::PopTextWrapPos

IMGUILUA_FUNCTION(ImGui::PopTextWrapPos)
static int ImGui_PopTextWrapPos(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImGui::PopTextWrapPos();
    return 0;
}


// function ImGui::ProgressBar

IMGUILUA_FUNCTION(ImGui::ProgressBar)
static int ImGui_ProgressBar(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(1, 3);
    float fraction;
    fraction = static_cast<float>(luaL_checknumber(L, 1));
    ImVec2 size_arg;
    if (ARGC < 2) {
        size_arg = ImVec2(-FLT_MIN, 0);
    }
    else {
        size_arg = ImGuiLua_ToImVec2(L, 2);
    }
    const char *overlay;
    if (ARGC < 3) {
        overlay = NULL;
    }
    else {
        overlay = lua_tostring(L, 3);
    }
    ImGui::ProgressBar(fraction, size_arg, overlay);
    return 0;
}


// function ImGui::PushAllowKeyboardFocus

IMGUILUA_FUNCTION(ImGui::PushAllowKeyboardFocus)
static int ImGui_PushAllowKeyboardFocus(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    bool allow_keyboard_focus;
    allow_keyboard_focus = lua_toboolean(L, 1);
    ImGui::PushAllowKeyboardFocus(allow_keyboard_focus);
    return 0;
}


// function ImGui::PushButtonRepeat

IMGUILUA_FUNCTION(ImGui::PushButtonRepeat)
static int ImGui_PushButtonRepeat(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    bool repeat;
    repeat = lua_toboolean(L, 1);
    ImGui::PushButtonRepeat(repeat);
    return 0;
}


// function ImGui::PushClipRect

IMGUILUA_FUNCTION(ImGui::PushClipRect)
static int ImGui_PushClipRect(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(3);
    ImVec2 clip_rect_min;
    clip_rect_min = ImGuiLua_ToImVec2(L, 1);
    ImVec2 clip_rect_max;
    clip_rect_max = ImGuiLua_ToImVec2(L, 2);
    bool intersect_with_current_clip_rect;
    intersect_with_current_clip_rect = lua_toboolean(L, 3);
    ImGui::PushClipRect(clip_rect_min, clip_rect_max,
                        intersect_with_current_clip_rect);
    return 0;
}


// function ImGui::PushFont

// void ImGui_PushFont(ImFont*): Unknown argument type 'ImFont*' (ImFont*)


// function ImGui::PushID is excluded


// function ImGui::PushItemWidth

IMGUILUA_FUNCTION(ImGui::PushItemWidth)
static int ImGui_PushItemWidth(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    float item_width;
    item_width = static_cast<float>(luaL_checknumber(L, 1));
    ImGui::PushItemWidth(item_width);
    return 0;
}


// function ImGui::PushStyleColor is excluded


// function ImGui::PushStyleVar is excluded


// function ImGui::PushTextWrapPos

IMGUILUA_FUNCTION(ImGui::PushTextWrapPos)
static int ImGui_PushTextWrapPos(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(1);
    float wrap_local_pos_x;
    if (ARGC < 1) {
        wrap_local_pos_x = 0.0f;
    }
    else {
        wrap_local_pos_x = static_cast<float>(luaL_checknumber(L, 1));
    }
    ImGui::PushTextWrapPos(wrap_local_pos_x);
    return 0;
}


// function ImGui::RadioButton

static int ImGui_RadioButton_sb(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(2);
    const char *label;
    label = lua_tostring(L, 1);
    bool active;
    active = lua_toboolean(L, 2);
    bool RETVAL = ImGui::RadioButton(label, active);
    lua_pushboolean(L, RETVAL);
    return 1;
}

static int ImGui_RadioButton_sfPi(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(3);
    const char *label;
    label = lua_tostring(L, 1);
    int *v;
    v = static_cast<int *>(luaL_checkudata(L, 2, "ImGuiLuaIntRef"));
    int v_button;
    v_button = static_cast<int>(luaL_checkinteger(L, 3));
    bool RETVAL = ImGui::RadioButton(label, v, v_button);
    lua_pushboolean(L, RETVAL);
    return 1;
}

IMGUILUA_FUNCTION(ImGui::RadioButton)
static int ImGui_RadioButton(lua_State *L)
{
    int ARGC = lua_gettop(L);
    switch (ARGC) {
    case 2:
        return ImGui_RadioButton_sb(L);
    case 3:
        return ImGui_RadioButton_sfPi(L);
    default:
        return luaL_error(
            L, "Wrong number of arguments: got %d, wanted between %d and %d",
            ARGC, 2, 3);
    }
}


// function ImGui::Render is excluded


// function ImGui::RenderPlatformWindowsDefault

// void ImGui_RenderPlatformWindowsDefault(void*, void*): Unknown argument type
// 'void*' (void*)


// function ImGui::ResetMouseDragDelta

IMGUILUA_FUNCTION(ImGui::ResetMouseDragDelta)
static int ImGui_ResetMouseDragDelta(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(1);
    int button;
    if (ARGC < 1) {
        button = 0;
    }
    else {
        button = static_cast<int>(luaL_checkinteger(L, 1));
    }
    ImGui::ResetMouseDragDelta(button);
    return 0;
}


// function ImGui::SameLine

IMGUILUA_FUNCTION(ImGui::SameLine)
static int ImGui_SameLine(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(2);
    float offset_from_start_x;
    if (ARGC < 1) {
        offset_from_start_x = 0.0f;
    }
    else {
        offset_from_start_x = static_cast<float>(luaL_checknumber(L, 1));
    }
    float spacing;
    if (ARGC < 2) {
        spacing = -1.0f;
    }
    else {
        spacing = static_cast<float>(luaL_checknumber(L, 2));
    }
    ImGui::SameLine(offset_from_start_x, spacing);
    return 0;
}


// function ImGui::SaveIniSettingsToDisk

IMGUILUA_FUNCTION(ImGui::SaveIniSettingsToDisk)
static int ImGui_SaveIniSettingsToDisk(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    const char *ini_filename;
    ini_filename = lua_tostring(L, 1);
    ImGui::SaveIniSettingsToDisk(ini_filename);
    return 0;
}


// function ImGui::SaveIniSettingsToMemory

// const char* ImGui_SaveIniSettingsToMemory(size_t*): Unknown argument type
// 'size_t*' (size_t*)


// function ImGui::Selectable

// bool ImGui_Selectable(const char*, bool*, ImGuiSelectableFlags, const ImVec2)
// is excluded

IMGUILUA_FUNCTION(ImGui::Selectable)
static int ImGui_Selectable(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(1, 4);
    const char *label;
    label = lua_tostring(L, 1);
    bool selected;
    if (ARGC < 2) {
        selected = false;
    }
    else {
        selected = lua_toboolean(L, 2);
    }
    int flags;
    if (ARGC < 3) {
        flags = 0;
    }
    else {
        flags = static_cast<int>(luaL_checkinteger(L, 3));
    }
    ImVec2 size;
    if (ARGC < 4) {
        size = ImVec2(0, 0);
    }
    else {
        size = ImGuiLua_ToImVec2(L, 4);
    }
    bool RETVAL = ImGui::Selectable(label, selected, flags, size);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::Separator

IMGUILUA_FUNCTION(ImGui::Separator)
static int ImGui_Separator(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImGui::Separator();
    return 0;
}


// function ImGui::SetAllocatorFunctions

// void ImGui_SetAllocatorFunctions(ImGuiMemAllocFunc, ImGuiMemFreeFunc, void*):
// Unknown argument type 'ImGuiMemAllocFunc' (ImGuiMemAllocFunc)


// function ImGui::SetClipboardText

IMGUILUA_FUNCTION(ImGui::SetClipboardText)
static int ImGui_SetClipboardText(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    const char *text;
    text = lua_tostring(L, 1);
    ImGui::SetClipboardText(text);
    return 0;
}


// function ImGui::SetColorEditOptions

IMGUILUA_FUNCTION(ImGui::SetColorEditOptions)
static int ImGui_SetColorEditOptions(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    int flags;
    flags = static_cast<int>(luaL_checkinteger(L, 1));
    ImGui::SetColorEditOptions(flags);
    return 0;
}


// function ImGui::SetColumnOffset

IMGUILUA_FUNCTION(ImGui::SetColumnOffset)
static int ImGui_SetColumnOffset(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(2);
    int column_index;
    column_index = static_cast<int>(luaL_checkinteger(L, 1));
    float offset_x;
    offset_x = static_cast<float>(luaL_checknumber(L, 2));
    ImGui::SetColumnOffset(column_index, offset_x);
    return 0;
}


// function ImGui::SetColumnWidth

IMGUILUA_FUNCTION(ImGui::SetColumnWidth)
static int ImGui_SetColumnWidth(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(2);
    int column_index;
    column_index = static_cast<int>(luaL_checkinteger(L, 1));
    float width;
    width = static_cast<float>(luaL_checknumber(L, 2));
    ImGui::SetColumnWidth(column_index, width);
    return 0;
}


// function ImGui::SetCurrentContext

// void ImGui_SetCurrentContext(ImGuiContext*): Unknown argument type
// 'ImGuiContext*' (ImGuiContext*)


// function ImGui::SetCursorPos

IMGUILUA_FUNCTION(ImGui::SetCursorPos)
static int ImGui_SetCursorPos(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    ImVec2 local_pos;
    local_pos = ImGuiLua_ToImVec2(L, 1);
    ImGui::SetCursorPos(local_pos);
    return 0;
}


// function ImGui::SetCursorPosX

IMGUILUA_FUNCTION(ImGui::SetCursorPosX)
static int ImGui_SetCursorPosX(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    float local_x;
    local_x = static_cast<float>(luaL_checknumber(L, 1));
    ImGui::SetCursorPosX(local_x);
    return 0;
}


// function ImGui::SetCursorPosY

IMGUILUA_FUNCTION(ImGui::SetCursorPosY)
static int ImGui_SetCursorPosY(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    float local_y;
    local_y = static_cast<float>(luaL_checknumber(L, 1));
    ImGui::SetCursorPosY(local_y);
    return 0;
}


// function ImGui::SetCursorScreenPos

IMGUILUA_FUNCTION(ImGui::SetCursorScreenPos)
static int ImGui_SetCursorScreenPos(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    ImVec2 pos;
    pos = ImGuiLua_ToImVec2(L, 1);
    ImGui::SetCursorScreenPos(pos);
    return 0;
}


// function ImGui::SetDragDropPayload

// bool ImGui_SetDragDropPayload(const char*, const void*, size_t, ImGuiCond):
// Unknown argument type 'const void*' (const void*)


// function ImGui::SetItemAllowOverlap

IMGUILUA_FUNCTION(ImGui::SetItemAllowOverlap)
static int ImGui_SetItemAllowOverlap(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImGui::SetItemAllowOverlap();
    return 0;
}


// function ImGui::SetItemDefaultFocus

IMGUILUA_FUNCTION(ImGui::SetItemDefaultFocus)
static int ImGui_SetItemDefaultFocus(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImGui::SetItemDefaultFocus();
    return 0;
}


// function ImGui::SetKeyboardFocusHere

IMGUILUA_FUNCTION(ImGui::SetKeyboardFocusHere)
static int ImGui_SetKeyboardFocusHere(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(1);
    int offset;
    if (ARGC < 1) {
        offset = 0;
    }
    else {
        offset = static_cast<int>(luaL_checkinteger(L, 1));
    }
    ImGui::SetKeyboardFocusHere(offset);
    return 0;
}


// function ImGui::SetMouseCursor

IMGUILUA_FUNCTION(ImGui::SetMouseCursor)
static int ImGui_SetMouseCursor(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    int cursor_type;
    cursor_type = static_cast<int>(luaL_checkinteger(L, 1));
    ImGui::SetMouseCursor(cursor_type);
    return 0;
}


// function ImGui::SetNextItemOpen

IMGUILUA_FUNCTION(ImGui::SetNextItemOpen)
static int ImGui_SetNextItemOpen(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(1, 2);
    bool is_open;
    is_open = lua_toboolean(L, 1);
    int cond;
    if (ARGC < 2) {
        cond = 0;
    }
    else {
        cond = static_cast<int>(luaL_checkinteger(L, 2));
    }
    ImGui::SetNextItemOpen(is_open, cond);
    return 0;
}


// function ImGui::SetNextItemWidth

IMGUILUA_FUNCTION(ImGui::SetNextItemWidth)
static int ImGui_SetNextItemWidth(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    float item_width;
    item_width = static_cast<float>(luaL_checknumber(L, 1));
    ImGui::SetNextItemWidth(item_width);
    return 0;
}


// function ImGui::SetNextWindowBgAlpha

IMGUILUA_FUNCTION(ImGui::SetNextWindowBgAlpha)
static int ImGui_SetNextWindowBgAlpha(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    float alpha;
    alpha = static_cast<float>(luaL_checknumber(L, 1));
    ImGui::SetNextWindowBgAlpha(alpha);
    return 0;
}


// function ImGui::SetNextWindowClass

IMGUILUA_FUNCTION(ImGui::SetNextWindowClass)
static int ImGui_SetNextWindowClass(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    const ImGuiWindowClass *window_class;
    window_class = static_cast<ImGuiWindowClass *>(
        luaL_checkudata(L, 1, "ImGuiWindowClass"));
    ImGui::SetNextWindowClass(window_class);
    return 0;
}


// function ImGui::SetNextWindowCollapsed

IMGUILUA_FUNCTION(ImGui::SetNextWindowCollapsed)
static int ImGui_SetNextWindowCollapsed(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(1, 2);
    bool collapsed;
    collapsed = lua_toboolean(L, 1);
    int cond;
    if (ARGC < 2) {
        cond = 0;
    }
    else {
        cond = static_cast<int>(luaL_checkinteger(L, 2));
    }
    ImGui::SetNextWindowCollapsed(collapsed, cond);
    return 0;
}


// function ImGui::SetNextWindowContentSize

IMGUILUA_FUNCTION(ImGui::SetNextWindowContentSize)
static int ImGui_SetNextWindowContentSize(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    ImVec2 size;
    size = ImGuiLua_ToImVec2(L, 1);
    ImGui::SetNextWindowContentSize(size);
    return 0;
}


// function ImGui::SetNextWindowDockID

IMGUILUA_FUNCTION(ImGui::SetNextWindowDockID)
static int ImGui_SetNextWindowDockID(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(1, 2);
    unsigned int dock_id;
    dock_id = static_cast<unsigned int>(luaL_checkinteger(L, 1));
    int cond;
    if (ARGC < 2) {
        cond = 0;
    }
    else {
        cond = static_cast<int>(luaL_checkinteger(L, 2));
    }
    ImGui::SetNextWindowDockID(dock_id, cond);
    return 0;
}


// function ImGui::SetNextWindowFocus

IMGUILUA_FUNCTION(ImGui::SetNextWindowFocus)
static int ImGui_SetNextWindowFocus(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImGui::SetNextWindowFocus();
    return 0;
}


// function ImGui::SetNextWindowPos

IMGUILUA_FUNCTION(ImGui::SetNextWindowPos)
static int ImGui_SetNextWindowPos(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(1, 3);
    ImVec2 pos;
    pos = ImGuiLua_ToImVec2(L, 1);
    int cond;
    if (ARGC < 2) {
        cond = 0;
    }
    else {
        cond = static_cast<int>(luaL_checkinteger(L, 2));
    }
    ImVec2 pivot;
    if (ARGC < 3) {
        pivot = ImVec2(0, 0);
    }
    else {
        pivot = ImGuiLua_ToImVec2(L, 3);
    }
    ImGui::SetNextWindowPos(pos, cond, pivot);
    return 0;
}


// function ImGui::SetNextWindowSize

IMGUILUA_FUNCTION(ImGui::SetNextWindowSize)
static int ImGui_SetNextWindowSize(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(1, 2);
    ImVec2 size;
    size = ImGuiLua_ToImVec2(L, 1);
    int cond;
    if (ARGC < 2) {
        cond = 0;
    }
    else {
        cond = static_cast<int>(luaL_checkinteger(L, 2));
    }
    ImGui::SetNextWindowSize(size, cond);
    return 0;
}


// function ImGui::SetNextWindowSizeConstraints

// void ImGui_SetNextWindowSizeConstraints(const ImVec2, const ImVec2,
// ImGuiSizeCallback, void*): Unknown argument type 'ImGuiSizeCallback'
// (ImGuiSizeCallback)


// function ImGui::SetNextWindowViewport

IMGUILUA_FUNCTION(ImGui::SetNextWindowViewport)
static int ImGui_SetNextWindowViewport(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    unsigned int viewport_id;
    viewport_id = static_cast<unsigned int>(luaL_checkinteger(L, 1));
    ImGui::SetNextWindowViewport(viewport_id);
    return 0;
}


// function ImGui::SetScrollFromPosX

IMGUILUA_FUNCTION(ImGui::SetScrollFromPosX)
static int ImGui_SetScrollFromPosX(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(1, 2);
    float local_x;
    local_x = static_cast<float>(luaL_checknumber(L, 1));
    float center_x_ratio;
    if (ARGC < 2) {
        center_x_ratio = 0.5f;
    }
    else {
        center_x_ratio = static_cast<float>(luaL_checknumber(L, 2));
    }
    ImGui::SetScrollFromPosX(local_x, center_x_ratio);
    return 0;
}


// function ImGui::SetScrollFromPosY

IMGUILUA_FUNCTION(ImGui::SetScrollFromPosY)
static int ImGui_SetScrollFromPosY(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(1, 2);
    float local_y;
    local_y = static_cast<float>(luaL_checknumber(L, 1));
    float center_y_ratio;
    if (ARGC < 2) {
        center_y_ratio = 0.5f;
    }
    else {
        center_y_ratio = static_cast<float>(luaL_checknumber(L, 2));
    }
    ImGui::SetScrollFromPosY(local_y, center_y_ratio);
    return 0;
}


// function ImGui::SetScrollHereX

IMGUILUA_FUNCTION(ImGui::SetScrollHereX)
static int ImGui_SetScrollHereX(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(1);
    float center_x_ratio;
    if (ARGC < 1) {
        center_x_ratio = 0.5f;
    }
    else {
        center_x_ratio = static_cast<float>(luaL_checknumber(L, 1));
    }
    ImGui::SetScrollHereX(center_x_ratio);
    return 0;
}


// function ImGui::SetScrollHereY

IMGUILUA_FUNCTION(ImGui::SetScrollHereY)
static int ImGui_SetScrollHereY(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(1);
    float center_y_ratio;
    if (ARGC < 1) {
        center_y_ratio = 0.5f;
    }
    else {
        center_y_ratio = static_cast<float>(luaL_checknumber(L, 1));
    }
    ImGui::SetScrollHereY(center_y_ratio);
    return 0;
}


// function ImGui::SetScrollX

IMGUILUA_FUNCTION(ImGui::SetScrollX)
static int ImGui_SetScrollX(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    float scroll_x;
    scroll_x = static_cast<float>(luaL_checknumber(L, 1));
    ImGui::SetScrollX(scroll_x);
    return 0;
}


// function ImGui::SetScrollY

IMGUILUA_FUNCTION(ImGui::SetScrollY)
static int ImGui_SetScrollY(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    float scroll_y;
    scroll_y = static_cast<float>(luaL_checknumber(L, 1));
    ImGui::SetScrollY(scroll_y);
    return 0;
}


// function ImGui::SetStateStorage

// void ImGui_SetStateStorage(ImGuiStorage*): Unknown argument type
// 'ImGuiStorage*' (ImGuiStorage*)


// function ImGui::SetTabItemClosed

IMGUILUA_FUNCTION(ImGui::SetTabItemClosed)
static int ImGui_SetTabItemClosed(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    const char *tab_or_docked_window_label;
    tab_or_docked_window_label = lua_tostring(L, 1);
    ImGui::SetTabItemClosed(tab_or_docked_window_label);
    return 0;
}


// function ImGui::SetTooltip

// void ImGui_SetTooltip(const char*, ...): Unknown argument type '...' (...)


// function ImGui::SetTooltipV

// void ImGui_SetTooltipV(const char*, va_list): Unknown argument type 'va_list'
// (va_list)


// function ImGui::SetWindowCollapsed

static int ImGui_SetWindowCollapsed_bi(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(1, 2);
    bool collapsed;
    collapsed = lua_toboolean(L, 1);
    int cond;
    if (ARGC < 2) {
        cond = 0;
    }
    else {
        cond = static_cast<int>(luaL_checkinteger(L, 2));
    }
    ImGui::SetWindowCollapsed(collapsed, cond);
    return 0;
}

static int ImGui_SetWindowCollapsed_sbi(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(2, 3);
    const char *name;
    name = lua_tostring(L, 1);
    bool collapsed;
    collapsed = lua_toboolean(L, 2);
    int cond;
    if (ARGC < 3) {
        cond = 0;
    }
    else {
        cond = static_cast<int>(luaL_checkinteger(L, 3));
    }
    ImGui::SetWindowCollapsed(name, collapsed, cond);
    return 0;
}

IMGUILUA_FUNCTION(ImGui::SetWindowCollapsed)
static int ImGui_SetWindowCollapsed(lua_State *L)
{
    if (lua_isstring(L, 1)) {
        return ImGui_SetWindowCollapsed_sbi(L);
    }
    else {
        return ImGui_SetWindowCollapsed_bi(L);
    }
}


// function ImGui::SetWindowFocus

static int ImGui_SetWindowFocus_(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImGui::SetWindowFocus();
    return 0;
}

static int ImGui_SetWindowFocus_s(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    const char *name;
    name = lua_tostring(L, 1);
    ImGui::SetWindowFocus(name);
    return 0;
}

IMGUILUA_FUNCTION(ImGui::SetWindowFocus)
static int ImGui_SetWindowFocus(lua_State *L)
{
    int ARGC = lua_gettop(L);
    switch (ARGC) {
    case 0:
        return ImGui_SetWindowFocus_(L);
    case 1:
        return ImGui_SetWindowFocus_s(L);
    default:
        return luaL_error(
            L, "Wrong number of arguments: got %d, wanted between %d and %d",
            ARGC, 0, 1);
    }
}


// function ImGui::SetWindowFontScale

IMGUILUA_FUNCTION(ImGui::SetWindowFontScale)
static int ImGui_SetWindowFontScale(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    float scale;
    scale = static_cast<float>(luaL_checknumber(L, 1));
    ImGui::SetWindowFontScale(scale);
    return 0;
}


// function ImGui::SetWindowPos

static int ImGui_SetWindowPos_v2i(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(1, 2);
    ImVec2 pos;
    pos = ImGuiLua_ToImVec2(L, 1);
    int cond;
    if (ARGC < 2) {
        cond = 0;
    }
    else {
        cond = static_cast<int>(luaL_checkinteger(L, 2));
    }
    ImGui::SetWindowPos(pos, cond);
    return 0;
}

static int ImGui_SetWindowPos_sv2i(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(2, 3);
    const char *name;
    name = lua_tostring(L, 1);
    ImVec2 pos;
    pos = ImGuiLua_ToImVec2(L, 2);
    int cond;
    if (ARGC < 3) {
        cond = 0;
    }
    else {
        cond = static_cast<int>(luaL_checkinteger(L, 3));
    }
    ImGui::SetWindowPos(name, pos, cond);
    return 0;
}

IMGUILUA_FUNCTION(ImGui::SetWindowPos)
static int ImGui_SetWindowPos(lua_State *L)
{
    if (lua_isstring(L, 1)) {
        return ImGui_SetWindowPos_sv2i(L);
    }
    else {
        return ImGui_SetWindowPos_v2i(L);
    }
}


// function ImGui::SetWindowSize

static int ImGui_SetWindowSize_v2i(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(1, 2);
    ImVec2 size;
    size = ImGuiLua_ToImVec2(L, 1);
    int cond;
    if (ARGC < 2) {
        cond = 0;
    }
    else {
        cond = static_cast<int>(luaL_checkinteger(L, 2));
    }
    ImGui::SetWindowSize(size, cond);
    return 0;
}

static int ImGui_SetWindowSize_sv2i(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(2, 3);
    const char *name;
    name = lua_tostring(L, 1);
    ImVec2 size;
    size = ImGuiLua_ToImVec2(L, 2);
    int cond;
    if (ARGC < 3) {
        cond = 0;
    }
    else {
        cond = static_cast<int>(luaL_checkinteger(L, 3));
    }
    ImGui::SetWindowSize(name, size, cond);
    return 0;
}

IMGUILUA_FUNCTION(ImGui::SetWindowSize)
static int ImGui_SetWindowSize(lua_State *L)
{
    if (lua_isstring(L, 1)) {
        return ImGui_SetWindowSize_sv2i(L);
    }
    else {
        return ImGui_SetWindowSize_v2i(L);
    }
}


// function ImGui::ShowAboutWindow

IMGUILUA_FUNCTION(ImGui::ShowAboutWindow)
static int ImGui_ShowAboutWindow(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(1);
    bool *p_open;
    if (ARGC < 1) {
        p_open = NULL;
    }
    else {
        if (lua_type(L, 1) == LUA_TNIL) {
            p_open = NULL;
        }
        else {
            p_open =
                static_cast<bool *>(luaL_checkudata(L, 1, "ImGuiLuaBoolRef"));
        }
    }
    ImGui::ShowAboutWindow(p_open);
    return 0;
}


// function ImGui::ShowDemoWindow

IMGUILUA_FUNCTION(ImGui::ShowDemoWindow)
static int ImGui_ShowDemoWindow(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(1);
    bool *p_open;
    if (ARGC < 1) {
        p_open = NULL;
    }
    else {
        if (lua_type(L, 1) == LUA_TNIL) {
            p_open = NULL;
        }
        else {
            p_open =
                static_cast<bool *>(luaL_checkudata(L, 1, "ImGuiLuaBoolRef"));
        }
    }
    ImGui::ShowDemoWindow(p_open);
    return 0;
}


// function ImGui::ShowFontSelector

IMGUILUA_FUNCTION(ImGui::ShowFontSelector)
static int ImGui_ShowFontSelector(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    const char *label;
    label = lua_tostring(L, 1);
    ImGui::ShowFontSelector(label);
    return 0;
}


// function ImGui::ShowMetricsWindow

IMGUILUA_FUNCTION(ImGui::ShowMetricsWindow)
static int ImGui_ShowMetricsWindow(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(1);
    bool *p_open;
    if (ARGC < 1) {
        p_open = NULL;
    }
    else {
        if (lua_type(L, 1) == LUA_TNIL) {
            p_open = NULL;
        }
        else {
            p_open =
                static_cast<bool *>(luaL_checkudata(L, 1, "ImGuiLuaBoolRef"));
        }
    }
    ImGui::ShowMetricsWindow(p_open);
    return 0;
}


// function ImGui::ShowStackToolWindow

IMGUILUA_FUNCTION(ImGui::ShowStackToolWindow)
static int ImGui_ShowStackToolWindow(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(1);
    bool *p_open;
    if (ARGC < 1) {
        p_open = NULL;
    }
    else {
        if (lua_type(L, 1) == LUA_TNIL) {
            p_open = NULL;
        }
        else {
            p_open =
                static_cast<bool *>(luaL_checkudata(L, 1, "ImGuiLuaBoolRef"));
        }
    }
    ImGui::ShowStackToolWindow(p_open);
    return 0;
}


// function ImGui::ShowStyleEditor

// void ImGui_ShowStyleEditor(ImGuiStyle*): Unknown argument type 'ImGuiStyle*'
// (ImGuiStyle*)


// function ImGui::ShowStyleSelector

IMGUILUA_FUNCTION(ImGui::ShowStyleSelector)
static int ImGui_ShowStyleSelector(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    const char *label;
    label = lua_tostring(L, 1);
    bool RETVAL = ImGui::ShowStyleSelector(label);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::ShowUserGuide

IMGUILUA_FUNCTION(ImGui::ShowUserGuide)
static int ImGui_ShowUserGuide(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImGui::ShowUserGuide();
    return 0;
}


// function ImGui::SliderAngle

IMGUILUA_FUNCTION(ImGui::SliderAngle)
static int ImGui_SliderAngle(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(2, 6);
    const char *label;
    label = lua_tostring(L, 1);
    float *v_rad;
    v_rad = static_cast<float *>(luaL_checkudata(L, 2, "ImGuiLuaFloatRef"));
    float v_degrees_min;
    if (ARGC < 3) {
        v_degrees_min = -360.0f;
    }
    else {
        v_degrees_min = static_cast<float>(luaL_checknumber(L, 3));
    }
    float v_degrees_max;
    if (ARGC < 4) {
        v_degrees_max = +360.0f;
    }
    else {
        v_degrees_max = static_cast<float>(luaL_checknumber(L, 4));
    }
    const char *format;
    if (ARGC < 5) {
        format = "%.0f deg";
    }
    else {
        format = lua_tostring(L, 5);
    }
    int flags;
    if (ARGC < 6) {
        flags = 0;
    }
    else {
        flags = static_cast<int>(luaL_checkinteger(L, 6));
    }
    bool RETVAL = ImGui::SliderAngle(label, v_rad, v_degrees_min, v_degrees_max,
                                     format, flags);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::SliderFloat

IMGUILUA_FUNCTION(ImGui::SliderFloat)
static int ImGui_SliderFloat(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(4, 6);
    const char *label;
    label = lua_tostring(L, 1);
    float *v;
    v = static_cast<float *>(luaL_checkudata(L, 2, "ImGuiLuaFloatRef"));
    float v_min;
    v_min = static_cast<float>(luaL_checknumber(L, 3));
    float v_max;
    v_max = static_cast<float>(luaL_checknumber(L, 4));
    const char *format;
    if (ARGC < 5) {
        format = "%.3f";
    }
    else {
        format = lua_tostring(L, 5);
    }
    int flags;
    if (ARGC < 6) {
        flags = 0;
    }
    else {
        flags = static_cast<int>(luaL_checkinteger(L, 6));
    }
    bool RETVAL = ImGui::SliderFloat(label, v, v_min, v_max, format, flags);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::SliderFloat2

// bool ImGui_SliderFloat2(const char*, float[2], float, float, const char*,
// ImGuiSliderFlags): Unknown argument type 'float[2]' (float[2])


// function ImGui::SliderFloat3

// bool ImGui_SliderFloat3(const char*, float[3], float, float, const char*,
// ImGuiSliderFlags): Unknown argument type 'float[3]' (float[3])


// function ImGui::SliderFloat4

// bool ImGui_SliderFloat4(const char*, float[4], float, float, const char*,
// ImGuiSliderFlags): Unknown argument type 'float[4]' (float[4])


// function ImGui::SliderInt

IMGUILUA_FUNCTION(ImGui::SliderInt)
static int ImGui_SliderInt(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(4, 6);
    const char *label;
    label = lua_tostring(L, 1);
    int *v;
    v = static_cast<int *>(luaL_checkudata(L, 2, "ImGuiLuaIntRef"));
    int v_min;
    v_min = static_cast<int>(luaL_checkinteger(L, 3));
    int v_max;
    v_max = static_cast<int>(luaL_checkinteger(L, 4));
    const char *format;
    if (ARGC < 5) {
        format = "%d";
    }
    else {
        format = lua_tostring(L, 5);
    }
    int flags;
    if (ARGC < 6) {
        flags = 0;
    }
    else {
        flags = static_cast<int>(luaL_checkinteger(L, 6));
    }
    bool RETVAL = ImGui::SliderInt(label, v, v_min, v_max, format, flags);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::SliderInt2

// bool ImGui_SliderInt2(const char*, int[2], int, int, const char*,
// ImGuiSliderFlags): Unknown argument type 'int[2]' (int[2])


// function ImGui::SliderInt3

// bool ImGui_SliderInt3(const char*, int[3], int, int, const char*,
// ImGuiSliderFlags): Unknown argument type 'int[3]' (int[3])


// function ImGui::SliderInt4

// bool ImGui_SliderInt4(const char*, int[4], int, int, const char*,
// ImGuiSliderFlags): Unknown argument type 'int[4]' (int[4])


// function ImGui::SliderScalar

// bool ImGui_SliderScalar(const char*, ImGuiDataType, void*, const void*, const
// void*, const char*, ImGuiSliderFlags): Unknown argument type 'void*' (void*)


// function ImGui::SliderScalarN

// bool ImGui_SliderScalarN(const char*, ImGuiDataType, void*, int, const void*,
// const void*, const char*, ImGuiSliderFlags): Unknown argument type 'void*'
// (void*)


// function ImGui::SmallButton

IMGUILUA_FUNCTION(ImGui::SmallButton)
static int ImGui_SmallButton(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    const char *label;
    label = lua_tostring(L, 1);
    bool RETVAL = ImGui::SmallButton(label);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::Spacing

IMGUILUA_FUNCTION(ImGui::Spacing)
static int ImGui_Spacing(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImGui::Spacing();
    return 0;
}


// function ImGui::StyleColorsClassic

// void ImGui_StyleColorsClassic(ImGuiStyle*): Unknown argument type
// 'ImGuiStyle*' (ImGuiStyle*)


// function ImGui::StyleColorsDark

// void ImGui_StyleColorsDark(ImGuiStyle*): Unknown argument type 'ImGuiStyle*'
// (ImGuiStyle*)


// function ImGui::StyleColorsLight

// void ImGui_StyleColorsLight(ImGuiStyle*): Unknown argument type 'ImGuiStyle*'
// (ImGuiStyle*)


// function ImGui::TabItemButton

IMGUILUA_FUNCTION(ImGui::TabItemButton)
static int ImGui_TabItemButton(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(1, 2);
    const char *label;
    label = lua_tostring(L, 1);
    int flags;
    if (ARGC < 2) {
        flags = 0;
    }
    else {
        flags = static_cast<int>(luaL_checkinteger(L, 2));
    }
    bool RETVAL = ImGui::TabItemButton(label, flags);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::TableGetColumnCount

IMGUILUA_FUNCTION(ImGui::TableGetColumnCount)
static int ImGui_TableGetColumnCount(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    int RETVAL = ImGui::TableGetColumnCount();
    lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
    return 1;
}


// function ImGui::TableGetColumnFlags

IMGUILUA_FUNCTION(ImGui::TableGetColumnFlags)
static int ImGui_TableGetColumnFlags(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(1);
    int column_n;
    if (ARGC < 1) {
        column_n = -1;
    }
    else {
        column_n = static_cast<int>(luaL_checkinteger(L, 1));
    }
    int RETVAL = ImGui::TableGetColumnFlags(column_n);
    lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
    return 1;
}


// function ImGui::TableGetColumnIndex

IMGUILUA_FUNCTION(ImGui::TableGetColumnIndex)
static int ImGui_TableGetColumnIndex(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    int RETVAL = ImGui::TableGetColumnIndex();
    lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
    return 1;
}


// function ImGui::TableGetColumnName

IMGUILUA_FUNCTION(ImGui::TableGetColumnName)
static int ImGui_TableGetColumnName(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(1);
    int column_n;
    if (ARGC < 1) {
        column_n = -1;
    }
    else {
        column_n = static_cast<int>(luaL_checkinteger(L, 1));
    }
    const char *RETVAL = ImGui::TableGetColumnName(column_n);
    lua_pushstring(L, RETVAL);
    return 1;
}


// function ImGui::TableGetRowIndex

IMGUILUA_FUNCTION(ImGui::TableGetRowIndex)
static int ImGui_TableGetRowIndex(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    int RETVAL = ImGui::TableGetRowIndex();
    lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
    return 1;
}


// function ImGui::TableGetSortSpecs

// ImGuiTableSortSpecs* ImGui_TableGetSortSpecs(): Unknown return type
// 'ImGuiTableSortSpecs*' (ImGuiTableSortSpecs*)


// function ImGui::TableHeader

IMGUILUA_FUNCTION(ImGui::TableHeader)
static int ImGui_TableHeader(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    const char *label;
    label = lua_tostring(L, 1);
    ImGui::TableHeader(label);
    return 0;
}


// function ImGui::TableHeadersRow

IMGUILUA_FUNCTION(ImGui::TableHeadersRow)
static int ImGui_TableHeadersRow(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImGui::TableHeadersRow();
    return 0;
}


// function ImGui::TableNextColumn

IMGUILUA_FUNCTION(ImGui::TableNextColumn)
static int ImGui_TableNextColumn(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    bool RETVAL = ImGui::TableNextColumn();
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::TableNextRow

IMGUILUA_FUNCTION(ImGui::TableNextRow)
static int ImGui_TableNextRow(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(2);
    int row_flags;
    if (ARGC < 1) {
        row_flags = 0;
    }
    else {
        row_flags = static_cast<int>(luaL_checkinteger(L, 1));
    }
    float min_row_height;
    if (ARGC < 2) {
        min_row_height = 0.0f;
    }
    else {
        min_row_height = static_cast<float>(luaL_checknumber(L, 2));
    }
    ImGui::TableNextRow(row_flags, min_row_height);
    return 0;
}


// function ImGui::TableSetBgColor

IMGUILUA_FUNCTION(ImGui::TableSetBgColor)
static int ImGui_TableSetBgColor(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(2, 3);
    int target;
    target = static_cast<int>(luaL_checkinteger(L, 1));
    ImU32 color;
    color = static_cast<ImU32>(luaL_checkinteger(L, 2));
    int column_n;
    if (ARGC < 3) {
        column_n = -1;
    }
    else {
        column_n = static_cast<int>(luaL_checkinteger(L, 3));
    }
    ImGui::TableSetBgColor(target, color, column_n);
    return 0;
}


// function ImGui::TableSetColumnEnabled

IMGUILUA_FUNCTION(ImGui::TableSetColumnEnabled)
static int ImGui_TableSetColumnEnabled(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(2);
    int column_n;
    column_n = static_cast<int>(luaL_checkinteger(L, 1));
    bool v;
    v = lua_toboolean(L, 2);
    ImGui::TableSetColumnEnabled(column_n, v);
    return 0;
}


// function ImGui::TableSetColumnIndex

IMGUILUA_FUNCTION(ImGui::TableSetColumnIndex)
static int ImGui_TableSetColumnIndex(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    int column_n;
    column_n = static_cast<int>(luaL_checkinteger(L, 1));
    bool RETVAL = ImGui::TableSetColumnIndex(column_n);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::TableSetupColumn

IMGUILUA_FUNCTION(ImGui::TableSetupColumn)
static int ImGui_TableSetupColumn(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(1, 4);
    const char *label;
    label = lua_tostring(L, 1);
    int flags;
    if (ARGC < 2) {
        flags = 0;
    }
    else {
        flags = static_cast<int>(luaL_checkinteger(L, 2));
    }
    float init_width_or_weight;
    if (ARGC < 3) {
        init_width_or_weight = 0.0f;
    }
    else {
        init_width_or_weight = static_cast<float>(luaL_checknumber(L, 3));
    }
    unsigned int user_id;
    if (ARGC < 4) {
        user_id = 0;
    }
    else {
        user_id = static_cast<unsigned int>(luaL_checkinteger(L, 4));
    }
    ImGui::TableSetupColumn(label, flags, init_width_or_weight, user_id);
    return 0;
}


// function ImGui::TableSetupScrollFreeze

IMGUILUA_FUNCTION(ImGui::TableSetupScrollFreeze)
static int ImGui_TableSetupScrollFreeze(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(2);
    int cols;
    cols = static_cast<int>(luaL_checkinteger(L, 1));
    int rows;
    rows = static_cast<int>(luaL_checkinteger(L, 2));
    ImGui::TableSetupScrollFreeze(cols, rows);
    return 0;
}


// function ImGui::Text is excluded


// function ImGui::TextColored is excluded


// function ImGui::TextColoredV is excluded


// function ImGui::TextDisabled is excluded


// function ImGui::TextDisabledV is excluded


// function ImGui::TextUnformatted is excluded


// function ImGui::TextV is excluded


// function ImGui::TextWrapped is excluded


// function ImGui::TextWrappedV is excluded


// function ImGui::TreeNode

// bool ImGui_TreeNode(const char*, const char*, ...): Unknown argument type
// '...' (...)

// bool ImGui_TreeNode(const void*, const char*, ...): Unknown argument type
// 'const void*' (const void*)

IMGUILUA_FUNCTION(ImGui::TreeNode)
static int ImGui_TreeNode(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    const char *label;
    label = lua_tostring(L, 1);
    bool RETVAL = ImGui::TreeNode(label);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::TreeNodeEx

// bool ImGui_TreeNodeEx(const char*, ImGuiTreeNodeFlags, const char*, ...):
// Unknown argument type '...' (...)

// bool ImGui_TreeNodeEx(const void*, ImGuiTreeNodeFlags, const char*, ...):
// Unknown argument type 'const void*' (const void*)

IMGUILUA_FUNCTION(ImGui::TreeNodeEx)
static int ImGui_TreeNodeEx(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(1, 2);
    const char *label;
    label = lua_tostring(L, 1);
    int flags;
    if (ARGC < 2) {
        flags = 0;
    }
    else {
        flags = static_cast<int>(luaL_checkinteger(L, 2));
    }
    bool RETVAL = ImGui::TreeNodeEx(label, flags);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::TreeNodeExV

// bool ImGui_TreeNodeExV(const char*, ImGuiTreeNodeFlags, const char*,
// va_list): Unknown argument type 'va_list' (va_list)

// bool ImGui_TreeNodeExV(const void*, ImGuiTreeNodeFlags, const char*,
// va_list): Unknown argument type 'const void*' (const void*)


// function ImGui::TreeNodeV

// bool ImGui_TreeNodeV(const char*, const char*, va_list): Unknown argument
// type 'va_list' (va_list)

// bool ImGui_TreeNodeV(const void*, const char*, va_list): Unknown argument
// type 'const void*' (const void*)


// function ImGui::TreePop

IMGUILUA_FUNCTION(ImGui::TreePop)
static int ImGui_TreePop(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImGui::TreePop();
    return 0;
}


// function ImGui::TreePush

// void ImGui_TreePush(const void*): Unknown argument type 'const void*' (const
// void*)

IMGUILUA_FUNCTION(ImGui::TreePush)
static int ImGui_TreePush(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    const char *str_id;
    str_id = lua_tostring(L, 1);
    ImGui::TreePush(str_id);
    return 0;
}


// function ImGui::Unindent

IMGUILUA_FUNCTION(ImGui::Unindent)
static int ImGui_Unindent(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(1);
    float indent_w;
    if (ARGC < 1) {
        indent_w = 0.0f;
    }
    else {
        indent_w = static_cast<float>(luaL_checknumber(L, 1));
    }
    ImGui::Unindent(indent_w);
    return 0;
}


// function ImGui::UpdatePlatformWindows

IMGUILUA_FUNCTION(ImGui::UpdatePlatformWindows)
static int ImGui_UpdatePlatformWindows(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImGui::UpdatePlatformWindows();
    return 0;
}


// function ImGui::VSliderFloat

IMGUILUA_FUNCTION(ImGui::VSliderFloat)
static int ImGui_VSliderFloat(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(5, 7);
    const char *label;
    label = lua_tostring(L, 1);
    ImVec2 size;
    size = ImGuiLua_ToImVec2(L, 2);
    float *v;
    v = static_cast<float *>(luaL_checkudata(L, 3, "ImGuiLuaFloatRef"));
    float v_min;
    v_min = static_cast<float>(luaL_checknumber(L, 4));
    float v_max;
    v_max = static_cast<float>(luaL_checknumber(L, 5));
    const char *format;
    if (ARGC < 6) {
        format = "%.3f";
    }
    else {
        format = lua_tostring(L, 6);
    }
    int flags;
    if (ARGC < 7) {
        flags = 0;
    }
    else {
        flags = static_cast<int>(luaL_checkinteger(L, 7));
    }
    bool RETVAL =
        ImGui::VSliderFloat(label, size, v, v_min, v_max, format, flags);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::VSliderInt

IMGUILUA_FUNCTION(ImGui::VSliderInt)
static int ImGui_VSliderInt(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(5, 7);
    const char *label;
    label = lua_tostring(L, 1);
    ImVec2 size;
    size = ImGuiLua_ToImVec2(L, 2);
    int *v;
    v = static_cast<int *>(luaL_checkudata(L, 3, "ImGuiLuaIntRef"));
    int v_min;
    v_min = static_cast<int>(luaL_checkinteger(L, 4));
    int v_max;
    v_max = static_cast<int>(luaL_checkinteger(L, 5));
    const char *format;
    if (ARGC < 6) {
        format = "%d";
    }
    else {
        format = lua_tostring(L, 6);
    }
    int flags;
    if (ARGC < 7) {
        flags = 0;
    }
    else {
        flags = static_cast<int>(luaL_checkinteger(L, 7));
    }
    bool RETVAL =
        ImGui::VSliderInt(label, size, v, v_min, v_max, format, flags);
    lua_pushboolean(L, RETVAL);
    return 1;
}


// function ImGui::VSliderScalar

// bool ImGui_VSliderScalar(const char*, const ImVec2, ImGuiDataType, void*,
// const void*, const void*, const char*, ImGuiSliderFlags): Unknown argument
// type 'void*' (void*)


// function ImGui::Value is excluded


// method ImGuiIO::~ImGuiIO has no namespace


// method ImGuiIO::AddFocusEvent has no namespace


// method ImGuiIO::AddInputCharacter has no namespace


// method ImGuiIO::AddInputCharacterUTF16 has no namespace


// method ImGuiIO::AddInputCharactersUTF8 has no namespace


// method ImGuiIO::AddKeyAnalogEvent has no namespace


// method ImGuiIO::AddKeyEvent has no namespace


// method ImGuiIO::AddMouseButtonEvent has no namespace


// method ImGuiIO::AddMousePosEvent has no namespace


// method ImGuiIO::AddMouseViewportEvent has no namespace


// method ImGuiIO::AddMouseWheelEvent has no namespace


// method ImGuiIO::ClearInputCharacters has no namespace


// method ImGuiIO::ClearInputKeys has no namespace


// method ImGuiIO::ImGuiIO has no namespace


// method ImGuiIO::SetKeyEventNativeData has no namespace


// method ImGuiInputTextCallbackData::~ImGuiInputTextCallbackData has no
// namespace


// method ImGuiInputTextCallbackData::ClearSelection has no namespace


// method ImGuiInputTextCallbackData::DeleteChars has no namespace


// method ImGuiInputTextCallbackData::HasSelection has no namespace


// method ImGuiInputTextCallbackData::ImGuiInputTextCallbackData has no
// namespace


// method ImGuiInputTextCallbackData::InsertChars has no namespace


// method ImGuiInputTextCallbackData::SelectAll has no namespace


// method ImGuiListClipper::~ImGuiListClipper has no namespace


// method ImGuiListClipper::Begin has no namespace


// method ImGuiListClipper::End has no namespace


// method ImGuiListClipper::ForceDisplayRangeByIndices has no namespace


// method ImGuiListClipper::ImGuiListClipper has no namespace


// method ImGuiListClipper::Step has no namespace


// method ImGuiOnceUponAFrame::~ImGuiOnceUponAFrame has no namespace


// method ImGuiOnceUponAFrame::ImGuiOnceUponAFrame has no namespace


// method ImGuiPayload::~ImGuiPayload has no namespace


// method ImGuiPayload::Clear has no namespace


// method ImGuiPayload::ImGuiPayload has no namespace


// method ImGuiPayload::IsDataType has no namespace


// method ImGuiPayload::IsDelivery has no namespace


// method ImGuiPayload::IsPreview has no namespace


// method ImGuiPlatformIO::~ImGuiPlatformIO has no namespace


// method ImGuiPlatformIO::ImGuiPlatformIO has no namespace


// method ImGuiPlatformImeData::~ImGuiPlatformImeData has no namespace


// method ImGuiPlatformImeData::ImGuiPlatformImeData has no namespace


// method ImGuiPlatformMonitor::~ImGuiPlatformMonitor has no namespace


// method ImGuiPlatformMonitor::ImGuiPlatformMonitor has no namespace


// method ImGuiStorage::BuildSortByKey has no namespace


// method ImGuiStorage::Clear has no namespace


// method ImGuiStorage::GetBool has no namespace


// method ImGuiStorage::GetBoolRef has no namespace


// method ImGuiStorage::GetFloat has no namespace


// method ImGuiStorage::GetFloatRef has no namespace


// method ImGuiStorage::GetInt has no namespace


// method ImGuiStorage::GetIntRef has no namespace


// method ImGuiStorage::GetVoidPtr has no namespace


// method ImGuiStorage::GetVoidPtrRef has no namespace


// method ImGuiStorage::SetAllInt has no namespace


// method ImGuiStorage::SetBool has no namespace


// method ImGuiStorage::SetFloat has no namespace


// method ImGuiStorage::SetInt has no namespace


// method ImGuiStorage::SetVoidPtr has no namespace


// method ImGuiStoragePair::~ImGuiStoragePair has no namespace


// method ImGuiStoragePair::ImGuiStoragePair has no namespace


// method ImGuiStyle::~ImGuiStyle has no namespace


// method ImGuiStyle::ImGuiStyle has no namespace


// method ImGuiStyle::ScaleAllSizes has no namespace


// method ImGuiTableColumnSortSpecs::~ImGuiTableColumnSortSpecs has no namespace


// method ImGuiTableColumnSortSpecs::ImGuiTableColumnSortSpecs has no namespace


// method ImGuiTableSortSpecs::~ImGuiTableSortSpecs has no namespace


// method ImGuiTableSortSpecs::ImGuiTableSortSpecs has no namespace


// method ImGuiTextBuffer::~ImGuiTextBuffer has no namespace


// method ImGuiTextBuffer::ImGuiTextBuffer has no namespace


// method ImGuiTextBuffer::append has no namespace


// method ImGuiTextBuffer::appendf has no namespace


// method ImGuiTextBuffer::appendfv has no namespace


// method ImGuiTextBuffer::begin has no namespace


// method ImGuiTextBuffer::c_str has no namespace


// method ImGuiTextBuffer::clear has no namespace


// method ImGuiTextBuffer::empty has no namespace


// method ImGuiTextBuffer::end has no namespace


// method ImGuiTextBuffer::reserve has no namespace


// method ImGuiTextBuffer::size has no namespace


// method ImGuiTextFilter::~ImGuiTextFilter has no namespace


// method ImGuiTextFilter::Build has no namespace


// method ImGuiTextFilter::Clear has no namespace


// method ImGuiTextFilter::Draw has no namespace


// method ImGuiTextFilter::ImGuiTextFilter has no namespace


// method ImGuiTextFilter::IsActive has no namespace


// method ImGuiTextFilter::PassFilter has no namespace


// method ImGuiTextRange::~ImGuiTextRange has no namespace


// method ImGuiTextRange::ImGuiTextRange has no namespace


// method ImGuiTextRange::empty has no namespace


// method ImGuiTextRange::split has no namespace


// method ImGuiViewport::~ImGuiViewport has no namespace


// method ImGuiViewport::GetCenter has no namespace


// method ImGuiViewport::GetWorkCenter has no namespace


// method ImGuiViewport::ImGuiViewport has no namespace


// method ImGuiWindowClass::~ImGuiWindowClass has no namespace


// method ImGuiWindowClass::ImGuiWindowClass has no namespace


// method ImVec2::~ImVec2 has no namespace


// method ImVec2::ImVec2 has no namespace


// method ImVec4::~ImVec4 has no namespace


// method ImVec4::ImVec4 has no namespace


// method ImVector::~ImVector has no namespace


// method ImVector::ImVector has no namespace


// method ImVector::_grow_capacity has no namespace


// method ImVector::back has no namespace


// method ImVector::begin has no namespace


// method ImVector::capacity has no namespace


// method ImVector::clear has no namespace


// method ImVector::clear_delete has no namespace


// method ImVector::clear_destruct has no namespace


// method ImVector::contains has no namespace


// method ImVector::empty has no namespace


// method ImVector::end has no namespace


// method ImVector::erase has no namespace


// method ImVector::erase_unsorted has no namespace


// method ImVector::find has no namespace


// method ImVector::find_erase has no namespace


// method ImVector::find_erase_unsorted has no namespace


// method ImVector::front has no namespace


// method ImVector::index_from_ptr has no namespace


// method ImVector::insert has no namespace


// method ImVector::max_size has no namespace


// method ImVector::pop_back has no namespace


// method ImVector::push_back has no namespace


// method ImVector::push_front has no namespace


// method ImVector::reserve has no namespace


// method ImVector::resize has no namespace


// method ImVector::shrink has no namespace


// method ImVector::size has no namespace


// method ImVector::size_in_bytes has no namespace


// method ImVector::swap has no namespace


IMGUILUA_FUNCTION(ImGui::PushID)
static int ImGui_PushID(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    if (lua_type(L, 1) == LUA_TNUMBER) {
        int int_id = static_cast<int>(luaL_checkinteger(L, 1));
        ImGui::PushID(int_id);
    }
    else {
        size_t len;
        const char *str_id = luaL_checklstring(L, 1, &len);
        ImGui::PushID(str_id, str_id + len);
    }
    return 0;
}


IMGUILUA_FUNCTION(ImGui::GetID)
static int ImGui_GetID(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    size_t len;
    const char *str_id = luaL_checklstring(L, 1, &len);
    ImGuiID RETVAL = ImGui::GetID(str_id, str_id + len);
    lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));
    return 1;
}


IMGUILUA_FUNCTION(ImGui::GetIO)
static int ImGui_GetIO(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(0);
    ImGuiIO **u = static_cast<ImGuiIO **>(lua_newuserdatauv(L, sizeof(*u), 0));
    *u = &ImGui::GetIO();
    luaL_setmetatable(L, "ImGuiIO");
    return 1;
}


IMGUILUA_FUNCTION(ImGui::Text)
static int ImGui_Text(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    size_t len;
    const char *text = luaL_checklstring(L, 1, &len);
    ImGui::TextUnformatted(text, text + len);
    return 0;
}

IMGUILUA_FUNCTION(ImGui::TextColored)
static int ImGui_TextColored(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(2);
    ImVec4 col = ImGuiLua_ToImVec4(L, 1);
    const char *text = luaL_checkstring(L, 2);
    ImGui::TextColored(col, "%s", text);
    return 0;
}

IMGUILUA_FUNCTION(ImGui::TextDisabled)
static int ImGui_TextDisabled(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    const char *text = luaL_checkstring(L, 1);
    ImGui::TextDisabled("%s", text);
    return 0;
}

IMGUILUA_FUNCTION(ImGui::TextWrapped)
static int ImGui_TextWrapped(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(1);
    const char *text = luaL_checkstring(L, 1);
    ImGui::TextWrapped("%s", text);
    return 0;
}

IMGUILUA_FUNCTION(ImGui::LabelText)
static int ImGui_LabelText(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(2);
    const char *label = luaL_checkstring(L, 1);
    const char *text = luaL_checkstring(L, 2);
    ImGui::LabelText(label, "%s", text);
    return 0;
}

IMGUILUA_FUNCTION(ImGui::BulletText)
static int ImGui_BulletText(lua_State *L)
{
    int ARGC = lua_gettop(L);
    if (ARGC != 1) {
        return luaL_error(L, "Wrong number of arguments: got %d, wanted %d",
                          ARGC, 1);
    }
    const char *text = luaL_checkstring(L, 1);
    ImGui::BulletText("%s", text);
    return 0;
}

static int ImGuiLua_InputTextCallback(ImGuiInputTextCallbackData *data)
{
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        luaL_Buffer *pb = static_cast<luaL_Buffer *>(data->UserData);
        luaL_prepbuffsize(pb, static_cast<size_t>(data->BufTextLen));
        data->Buf = luaL_buffaddr(pb);
    }
    return 0;
}

IMGUILUA_FUNCTION(ImGui::InputText)
static int ImGui_InputText(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_BETWEEN(2, 3);

    const char *label = luaL_checkstring(L, 1);
    luaL_checkudata(L, 2, "ImGuiLuaStringRef");
    ImGuiInputTextFlags flags;
    if (ARGC >= 3) {
        flags = static_cast<ImGuiInputTextFlags>(luaL_checkinteger(L, 3));
    }
    else {
        flags = ImGuiInputTextFlags_None;
    }

    lua_getiuservalue(L, 2, 1);
    size_t len;
    const char *str = luaL_checklstring(L, -1, &len);
    size_t size = len + 1;

    luaL_Buffer buffer;
    memcpy(luaL_buffinitsize(L, &buffer, size), str, size);
    bool result = ImGui::InputText(label, luaL_buffaddr(&buffer), size,
                                   flags | ImGuiInputTextFlags_CallbackResize,
                                   ImGuiLua_InputTextCallback, &buffer);
    luaL_pushresultsize(&buffer, strlen(luaL_buffaddr(&buffer)));
    lua_setiuservalue(L, 2, 1);

    lua_pushboolean(L, result);
    return 1;
}


static int ImGuiLua_PushedStyleColors;
static int ImGuiLua_PushedStyleVars;

extern "C" int ImGuiLua_PopStyleColors()
{
    int count = ImGuiLua_PushedStyleColors;
    if (count > 0) {
        ImGui::PopStyleColor(count);
        ImGuiLua_PushedStyleColors = 0;
    }
    return count;
}

extern "C" int ImGuiLua_PopStyleVars()
{
    int count = ImGuiLua_PushedStyleVars;
    if (count > 0) {
        ImGui::PopStyleVar(count);
        ImGuiLua_PushedStyleVars = 0;
    }
    return count;
}

IMGUILUA_FUNCTION(ImGui::PushStyleColor)
static int ImGui_PushStyleColor(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(2);
    int idx = static_cast<int>(luaL_checkinteger(L, 1));
    if (lua_isnumber(L, 2)) {
        ImU32 col = static_cast<ImU32>(luaL_checkinteger(L, 2));
        ImGui::PushStyleColor(idx, col);
    }
    else {
        ImVec4 col = ImGuiLua_ToImVec4(L, 2);
        ImGui::PushStyleColor(idx, col);
    }
    ++ImGuiLua_PushedStyleColors;
    return 0;
}

IMGUILUA_FUNCTION(ImGui::PushStyleVar)
static int ImGui_PushStyleVar(lua_State *L)
{
    IMGUILUA_CHECK_ARGC(2);
    int idx = static_cast<int>(luaL_checkinteger(L, 1));
    if (lua_isnumber(L, 2)) {
        float val = static_cast<float>(luaL_checknumber(L, 2));
        ImGui::PushStyleVar(idx, val);
    }
    else {
        ImVec2 val = ImGuiLua_ToImVec2(L, 2);
        ImGui::PushStyleVar(idx, val);
    }
    ++ImGuiLua_PushedStyleVars;
    return 0;
}

IMGUILUA_FUNCTION(ImGui::PopStyleColor)
static int ImGui_PopStyleColor(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(1);
    int count;
    if (ARGC < 1) {
        count = 1;
    }
    else {
        count = static_cast<int>(luaL_checkinteger(L, 1));
    }
    if (count <= ImGuiLua_PushedStyleColors) {
        ImGui::PopStyleColor(count);
        ImGuiLua_PushedStyleColors -= count;
    }
    else {
        luaL_error(
            L, "Attempt to pop %d style colors, but only %d are on the stack",
            count, ImGuiLua_PushedStyleColors);
    }
    return 0;
}

IMGUILUA_FUNCTION(ImGui::PopStyleVar)
static int ImGui_PopStyleVar(lua_State *L)
{
    IMGUILUA_CHECK_ARGC_MAX(1);
    int count;
    if (ARGC < 1) {
        count = 1;
    }
    else {
        count = static_cast<int>(luaL_checkinteger(L, 1));
    }
    if (count <= ImGuiLua_PushedStyleVars) {
        ImGui::PopStyleVar(count);
        ImGuiLua_PushedStyleVars -= count;
    }
    else {
        luaL_error(L,
                   "Attempt to pop %d style vars, but only %d are on the stack",
                   count, ImGuiLua_PushedStyleColors);
    }
    return 0;
}


static luaL_Reg ImGuiLua_RegImGui[] = {
    {"Bool", ImGuiLua_Bool},
    {"Float", ImGuiLua_Float},
    {"Int", ImGuiLua_Int},
    {"String", ImGuiLua_String},
    {"AlignTextToFramePadding", ImGui_AlignTextToFramePadding},
    {"ArrowButton", ImGui_ArrowButton},
    {"Begin", ImGui_Begin},
    {"BeginChild", ImGui_BeginChild},
    {"BeginChildFrame", ImGui_BeginChildFrame},
    {"BeginCombo", ImGui_BeginCombo},
    {"BeginDisabled", ImGui_BeginDisabled},
    {"BeginDragDropSource", ImGui_BeginDragDropSource},
    {"BeginDragDropTarget", ImGui_BeginDragDropTarget},
    {"BeginGroup", ImGui_BeginGroup},
    {"BeginListBox", ImGui_BeginListBox},
    {"BeginMainMenuBar", ImGui_BeginMainMenuBar},
    {"BeginMenu", ImGui_BeginMenu},
    {"BeginMenuBar", ImGui_BeginMenuBar},
    {"BeginPopup", ImGui_BeginPopup},
    {"BeginPopupContextItem", ImGui_BeginPopupContextItem},
    {"BeginPopupContextVoid", ImGui_BeginPopupContextVoid},
    {"BeginPopupContextWindow", ImGui_BeginPopupContextWindow},
    {"BeginPopupModal", ImGui_BeginPopupModal},
    {"BeginTabBar", ImGui_BeginTabBar},
    {"BeginTabItem", ImGui_BeginTabItem},
    {"BeginTable", ImGui_BeginTable},
    {"BeginTooltip", ImGui_BeginTooltip},
    {"Bullet", ImGui_Bullet},
    {"BulletText", ImGui_BulletText},
    {"Button", ImGui_Button},
    {"CalcItemWidth", ImGui_CalcItemWidth},
    {"CalcTextSize", ImGui_CalcTextSize},
    {"CaptureKeyboardFromApp", ImGui_CaptureKeyboardFromApp},
    {"CaptureMouseFromApp", ImGui_CaptureMouseFromApp},
    {"Checkbox", ImGui_Checkbox},
    {"CheckboxFlags", ImGui_CheckboxFlags},
    {"CloseCurrentPopup", ImGui_CloseCurrentPopup},
    {"CollapsingHeader", ImGui_CollapsingHeader},
    {"ColorButton", ImGui_ColorButton},
    {"ColorConvertFloat4ToU32", ImGui_ColorConvertFloat4ToU32},
    {"ColorConvertHSVtoRGB", ImGui_ColorConvertHSVtoRGB},
    {"ColorConvertRGBtoHSV", ImGui_ColorConvertRGBtoHSV},
    {"ColorConvertU32ToFloat4", ImGui_ColorConvertU32ToFloat4},
    {"Columns", ImGui_Columns},
    {"Combo", ImGui_Combo},
    {"DestroyPlatformWindows", ImGui_DestroyPlatformWindows},
    {"DockSpace", ImGui_DockSpace},
    {"DockSpaceOverViewport", ImGui_DockSpaceOverViewport},
    {"DragFloat", ImGui_DragFloat},
    {"DragFloatRange2", ImGui_DragFloatRange2},
    {"DragInt", ImGui_DragInt},
    {"DragIntRange2", ImGui_DragIntRange2},
    {"Dummy", ImGui_Dummy},
    {"End", ImGui_End},
    {"EndChild", ImGui_EndChild},
    {"EndChildFrame", ImGui_EndChildFrame},
    {"EndCombo", ImGui_EndCombo},
    {"EndDisabled", ImGui_EndDisabled},
    {"EndDragDropSource", ImGui_EndDragDropSource},
    {"EndDragDropTarget", ImGui_EndDragDropTarget},
    {"EndGroup", ImGui_EndGroup},
    {"EndListBox", ImGui_EndListBox},
    {"EndMainMenuBar", ImGui_EndMainMenuBar},
    {"EndMenu", ImGui_EndMenu},
    {"EndMenuBar", ImGui_EndMenuBar},
    {"EndPopup", ImGui_EndPopup},
    {"EndTabBar", ImGui_EndTabBar},
    {"EndTabItem", ImGui_EndTabItem},
    {"EndTable", ImGui_EndTable},
    {"EndTooltip", ImGui_EndTooltip},
    {"FindViewportByID", ImGui_FindViewportByID},
    {"GetClipboardText", ImGui_GetClipboardText},
    {"GetColorU32", ImGui_GetColorU32},
    {"GetColumnIndex", ImGui_GetColumnIndex},
    {"GetColumnOffset", ImGui_GetColumnOffset},
    {"GetColumnWidth", ImGui_GetColumnWidth},
    {"GetColumnsCount", ImGui_GetColumnsCount},
    {"GetContentRegionAvail", ImGui_GetContentRegionAvail},
    {"GetContentRegionMax", ImGui_GetContentRegionMax},
    {"GetCursorPos", ImGui_GetCursorPos},
    {"GetCursorPosX", ImGui_GetCursorPosX},
    {"GetCursorPosY", ImGui_GetCursorPosY},
    {"GetCursorScreenPos", ImGui_GetCursorScreenPos},
    {"GetCursorStartPos", ImGui_GetCursorStartPos},
    {"GetFontSize", ImGui_GetFontSize},
    {"GetFontTexUvWhitePixel", ImGui_GetFontTexUvWhitePixel},
    {"GetFrameCount", ImGui_GetFrameCount},
    {"GetFrameHeight", ImGui_GetFrameHeight},
    {"GetFrameHeightWithSpacing", ImGui_GetFrameHeightWithSpacing},
    {"GetID", ImGui_GetID},
    {"GetIO", ImGui_GetIO},
    {"GetItemRectMax", ImGui_GetItemRectMax},
    {"GetItemRectMin", ImGui_GetItemRectMin},
    {"GetItemRectSize", ImGui_GetItemRectSize},
    {"GetKeyIndex", ImGui_GetKeyIndex},
    {"GetKeyName", ImGui_GetKeyName},
    {"GetKeyPressedAmount", ImGui_GetKeyPressedAmount},
    {"GetMainViewport", ImGui_GetMainViewport},
    {"GetMouseClickedCount", ImGui_GetMouseClickedCount},
    {"GetMouseCursor", ImGui_GetMouseCursor},
    {"GetMouseDragDelta", ImGui_GetMouseDragDelta},
    {"GetMousePos", ImGui_GetMousePos},
    {"GetMousePosOnOpeningCurrentPopup",
     ImGui_GetMousePosOnOpeningCurrentPopup},
    {"GetScrollMaxX", ImGui_GetScrollMaxX},
    {"GetScrollMaxY", ImGui_GetScrollMaxY},
    {"GetScrollX", ImGui_GetScrollX},
    {"GetScrollY", ImGui_GetScrollY},
    {"GetStyleColorName", ImGui_GetStyleColorName},
    {"GetTextLineHeight", ImGui_GetTextLineHeight},
    {"GetTextLineHeightWithSpacing", ImGui_GetTextLineHeightWithSpacing},
    {"GetTime", ImGui_GetTime},
    {"GetTreeNodeToLabelSpacing", ImGui_GetTreeNodeToLabelSpacing},
    {"GetVersion", ImGui_GetVersion},
    {"GetWindowContentRegionMax", ImGui_GetWindowContentRegionMax},
    {"GetWindowContentRegionMin", ImGui_GetWindowContentRegionMin},
    {"GetWindowDockID", ImGui_GetWindowDockID},
    {"GetWindowDpiScale", ImGui_GetWindowDpiScale},
    {"GetWindowHeight", ImGui_GetWindowHeight},
    {"GetWindowPos", ImGui_GetWindowPos},
    {"GetWindowSize", ImGui_GetWindowSize},
    {"GetWindowViewport", ImGui_GetWindowViewport},
    {"GetWindowWidth", ImGui_GetWindowWidth},
    {"Indent", ImGui_Indent},
    {"InputFloat", ImGui_InputFloat},
    {"InputInt", ImGui_InputInt},
    {"InputText", ImGui_InputText},
    {"InvisibleButton", ImGui_InvisibleButton},
    {"IsAnyItemActive", ImGui_IsAnyItemActive},
    {"IsAnyItemFocused", ImGui_IsAnyItemFocused},
    {"IsAnyItemHovered", ImGui_IsAnyItemHovered},
    {"IsAnyMouseDown", ImGui_IsAnyMouseDown},
    {"IsItemActivated", ImGui_IsItemActivated},
    {"IsItemActive", ImGui_IsItemActive},
    {"IsItemClicked", ImGui_IsItemClicked},
    {"IsItemDeactivated", ImGui_IsItemDeactivated},
    {"IsItemDeactivatedAfterEdit", ImGui_IsItemDeactivatedAfterEdit},
    {"IsItemEdited", ImGui_IsItemEdited},
    {"IsItemFocused", ImGui_IsItemFocused},
    {"IsItemHovered", ImGui_IsItemHovered},
    {"IsItemToggledOpen", ImGui_IsItemToggledOpen},
    {"IsItemVisible", ImGui_IsItemVisible},
    {"IsKeyDown", ImGui_IsKeyDown},
    {"IsKeyPressed", ImGui_IsKeyPressed},
    {"IsKeyReleased", ImGui_IsKeyReleased},
    {"IsMouseClicked", ImGui_IsMouseClicked},
    {"IsMouseDoubleClicked", ImGui_IsMouseDoubleClicked},
    {"IsMouseDown", ImGui_IsMouseDown},
    {"IsMouseDragging", ImGui_IsMouseDragging},
    {"IsMouseHoveringRect", ImGui_IsMouseHoveringRect},
    {"IsMouseReleased", ImGui_IsMouseReleased},
    {"IsPopupOpen", ImGui_IsPopupOpen},
    {"IsRectVisible", ImGui_IsRectVisible},
    {"IsWindowAppearing", ImGui_IsWindowAppearing},
    {"IsWindowCollapsed", ImGui_IsWindowCollapsed},
    {"IsWindowDocked", ImGui_IsWindowDocked},
    {"IsWindowFocused", ImGui_IsWindowFocused},
    {"IsWindowHovered", ImGui_IsWindowHovered},
    {"LabelText", ImGui_LabelText},
    {"LoadIniSettingsFromDisk", ImGui_LoadIniSettingsFromDisk},
    {"LogButtons", ImGui_LogButtons},
    {"LogFinish", ImGui_LogFinish},
    {"LogToClipboard", ImGui_LogToClipboard},
    {"LogToFile", ImGui_LogToFile},
    {"LogToTTY", ImGui_LogToTTY},
    {"MenuItem", ImGui_MenuItem},
    {"NewLine", ImGui_NewLine},
    {"NextColumn", ImGui_NextColumn},
    {"OpenPopup", ImGui_OpenPopup},
    {"OpenPopupOnItemClick", ImGui_OpenPopupOnItemClick},
    {"PopAllowKeyboardFocus", ImGui_PopAllowKeyboardFocus},
    {"PopButtonRepeat", ImGui_PopButtonRepeat},
    {"PopClipRect", ImGui_PopClipRect},
    {"PopFont", ImGui_PopFont},
    {"PopID", ImGui_PopID},
    {"PopItemWidth", ImGui_PopItemWidth},
    {"PopStyleColor", ImGui_PopStyleColor},
    {"PopStyleVar", ImGui_PopStyleVar},
    {"PopTextWrapPos", ImGui_PopTextWrapPos},
    {"ProgressBar", ImGui_ProgressBar},
    {"PushAllowKeyboardFocus", ImGui_PushAllowKeyboardFocus},
    {"PushButtonRepeat", ImGui_PushButtonRepeat},
    {"PushClipRect", ImGui_PushClipRect},
    {"PushID", ImGui_PushID},
    {"PushItemWidth", ImGui_PushItemWidth},
    {"PushStyleColor", ImGui_PushStyleColor},
    {"PushStyleVar", ImGui_PushStyleVar},
    {"PushTextWrapPos", ImGui_PushTextWrapPos},
    {"RadioButton", ImGui_RadioButton},
    {"ResetMouseDragDelta", ImGui_ResetMouseDragDelta},
    {"SameLine", ImGui_SameLine},
    {"SaveIniSettingsToDisk", ImGui_SaveIniSettingsToDisk},
    {"Selectable", ImGui_Selectable},
    {"Separator", ImGui_Separator},
    {"SetClipboardText", ImGui_SetClipboardText},
    {"SetColorEditOptions", ImGui_SetColorEditOptions},
    {"SetColumnOffset", ImGui_SetColumnOffset},
    {"SetColumnWidth", ImGui_SetColumnWidth},
    {"SetCursorPos", ImGui_SetCursorPos},
    {"SetCursorPosX", ImGui_SetCursorPosX},
    {"SetCursorPosY", ImGui_SetCursorPosY},
    {"SetCursorScreenPos", ImGui_SetCursorScreenPos},
    {"SetItemAllowOverlap", ImGui_SetItemAllowOverlap},
    {"SetItemDefaultFocus", ImGui_SetItemDefaultFocus},
    {"SetKeyboardFocusHere", ImGui_SetKeyboardFocusHere},
    {"SetMouseCursor", ImGui_SetMouseCursor},
    {"SetNextItemOpen", ImGui_SetNextItemOpen},
    {"SetNextItemWidth", ImGui_SetNextItemWidth},
    {"SetNextWindowBgAlpha", ImGui_SetNextWindowBgAlpha},
    {"SetNextWindowClass", ImGui_SetNextWindowClass},
    {"SetNextWindowCollapsed", ImGui_SetNextWindowCollapsed},
    {"SetNextWindowContentSize", ImGui_SetNextWindowContentSize},
    {"SetNextWindowDockID", ImGui_SetNextWindowDockID},
    {"SetNextWindowFocus", ImGui_SetNextWindowFocus},
    {"SetNextWindowPos", ImGui_SetNextWindowPos},
    {"SetNextWindowSize", ImGui_SetNextWindowSize},
    {"SetNextWindowViewport", ImGui_SetNextWindowViewport},
    {"SetScrollFromPosX", ImGui_SetScrollFromPosX},
    {"SetScrollFromPosY", ImGui_SetScrollFromPosY},
    {"SetScrollHereX", ImGui_SetScrollHereX},
    {"SetScrollHereY", ImGui_SetScrollHereY},
    {"SetScrollX", ImGui_SetScrollX},
    {"SetScrollY", ImGui_SetScrollY},
    {"SetTabItemClosed", ImGui_SetTabItemClosed},
    {"SetWindowCollapsed", ImGui_SetWindowCollapsed},
    {"SetWindowFocus", ImGui_SetWindowFocus},
    {"SetWindowFontScale", ImGui_SetWindowFontScale},
    {"SetWindowPos", ImGui_SetWindowPos},
    {"SetWindowSize", ImGui_SetWindowSize},
    {"ShowAboutWindow", ImGui_ShowAboutWindow},
    {"ShowDemoWindow", ImGui_ShowDemoWindow},
    {"ShowFontSelector", ImGui_ShowFontSelector},
    {"ShowMetricsWindow", ImGui_ShowMetricsWindow},
    {"ShowStackToolWindow", ImGui_ShowStackToolWindow},
    {"ShowStyleSelector", ImGui_ShowStyleSelector},
    {"ShowUserGuide", ImGui_ShowUserGuide},
    {"SliderAngle", ImGui_SliderAngle},
    {"SliderFloat", ImGui_SliderFloat},
    {"SliderInt", ImGui_SliderInt},
    {"SmallButton", ImGui_SmallButton},
    {"Spacing", ImGui_Spacing},
    {"TabItemButton", ImGui_TabItemButton},
    {"TableGetColumnCount", ImGui_TableGetColumnCount},
    {"TableGetColumnFlags", ImGui_TableGetColumnFlags},
    {"TableGetColumnIndex", ImGui_TableGetColumnIndex},
    {"TableGetColumnName", ImGui_TableGetColumnName},
    {"TableGetRowIndex", ImGui_TableGetRowIndex},
    {"TableHeader", ImGui_TableHeader},
    {"TableHeadersRow", ImGui_TableHeadersRow},
    {"TableNextColumn", ImGui_TableNextColumn},
    {"TableNextRow", ImGui_TableNextRow},
    {"TableSetBgColor", ImGui_TableSetBgColor},
    {"TableSetColumnEnabled", ImGui_TableSetColumnEnabled},
    {"TableSetColumnIndex", ImGui_TableSetColumnIndex},
    {"TableSetupColumn", ImGui_TableSetupColumn},
    {"TableSetupScrollFreeze", ImGui_TableSetupScrollFreeze},
    {"Text", ImGui_Text},
    {"TextColored", ImGui_TextColored},
    {"TextDisabled", ImGui_TextDisabled},
    {"TextWrapped", ImGui_TextWrapped},
    {"TreeNode", ImGui_TreeNode},
    {"TreeNodeEx", ImGui_TreeNodeEx},
    {"TreePop", ImGui_TreePop},
    {"TreePush", ImGui_TreePush},
    {"Unindent", ImGui_Unindent},
    {"UpdatePlatformWindows", ImGui_UpdatePlatformWindows},
    {"VSliderFloat", ImGui_VSliderFloat},
    {"VSliderInt", ImGui_VSliderInt},
    {NULL, NULL},
};

extern "C" int ImGuiLua_InitImGui(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_pushvalue(L, 1);
    ImGuiLua_GetOrCreateTable(L, "ImDrawFlags");
    ImGuiLua_RegisterEnumImDrawFlags(L);
    lua_pop(L, 1);
    ImGuiLua_GetOrCreateTable(L, "ImDrawListFlags");
    ImGuiLua_RegisterEnumImDrawListFlags(L);
    lua_pop(L, 1);
    ImGuiLua_GetOrCreateTable(L, "ImFontAtlasFlags");
    ImGuiLua_RegisterEnumImFontAtlasFlags(L);
    lua_pop(L, 1);
    ImGuiLua_GetOrCreateTable(L, "ImGuiBackendFlags");
    ImGuiLua_RegisterEnumImGuiBackendFlags(L);
    lua_pop(L, 1);
    ImGuiLua_GetOrCreateTable(L, "ImGuiButtonFlags");
    ImGuiLua_RegisterEnumImGuiButtonFlags(L);
    lua_pop(L, 1);
    ImGuiLua_GetOrCreateTable(L, "ImGuiCol");
    ImGuiLua_RegisterEnumImGuiCol(L);
    lua_pop(L, 1);
    ImGuiLua_GetOrCreateTable(L, "ImGuiColorEditFlags");
    ImGuiLua_RegisterEnumImGuiColorEditFlags(L);
    lua_pop(L, 1);
    ImGuiLua_GetOrCreateTable(L, "ImGuiComboFlags");
    ImGuiLua_RegisterEnumImGuiComboFlags(L);
    lua_pop(L, 1);
    ImGuiLua_GetOrCreateTable(L, "ImGuiCond");
    ImGuiLua_RegisterEnumImGuiCond(L);
    lua_pop(L, 1);
    ImGuiLua_GetOrCreateTable(L, "ImGuiConfigFlags");
    ImGuiLua_RegisterEnumImGuiConfigFlags(L);
    lua_pop(L, 1);
    ImGuiLua_GetOrCreateTable(L, "ImGuiDataType");
    ImGuiLua_RegisterEnumImGuiDataType(L);
    lua_pop(L, 1);
    ImGuiLua_GetOrCreateTable(L, "ImGuiDir");
    ImGuiLua_RegisterEnumImGuiDir(L);
    lua_pop(L, 1);
    ImGuiLua_GetOrCreateTable(L, "ImGuiDockNodeFlags");
    ImGuiLua_RegisterEnumImGuiDockNodeFlags(L);
    lua_pop(L, 1);
    ImGuiLua_GetOrCreateTable(L, "ImGuiDragDropFlags");
    ImGuiLua_RegisterEnumImGuiDragDropFlags(L);
    lua_pop(L, 1);
    ImGuiLua_GetOrCreateTable(L, "ImGuiFocusedFlags");
    ImGuiLua_RegisterEnumImGuiFocusedFlags(L);
    lua_pop(L, 1);
    ImGuiLua_GetOrCreateTable(L, "ImGuiHoveredFlags");
    ImGuiLua_RegisterEnumImGuiHoveredFlags(L);
    lua_pop(L, 1);
    ImGuiLua_GetOrCreateTable(L, "ImGuiInputTextFlags");
    ImGuiLua_RegisterEnumImGuiInputTextFlags(L);
    lua_pop(L, 1);
    ImGuiLua_GetOrCreateTable(L, "ImGuiKeyModFlags");
    ImGuiLua_RegisterEnumImGuiKeyModFlags(L);
    lua_pop(L, 1);
    ImGuiLua_GetOrCreateTable(L, "ImGuiKey");
    ImGuiLua_RegisterEnumImGuiKey(L);
    lua_pop(L, 1);
    ImGuiLua_GetOrCreateTable(L, "ImGuiMouseButton");
    ImGuiLua_RegisterEnumImGuiMouseButton(L);
    lua_pop(L, 1);
    ImGuiLua_GetOrCreateTable(L, "ImGuiMouseCursor");
    ImGuiLua_RegisterEnumImGuiMouseCursor(L);
    lua_pop(L, 1);
    ImGuiLua_GetOrCreateTable(L, "ImGuiNavInput");
    ImGuiLua_RegisterEnumImGuiNavInput(L);
    lua_pop(L, 1);
    ImGuiLua_GetOrCreateTable(L, "ImGuiPopupFlags");
    ImGuiLua_RegisterEnumImGuiPopupFlags(L);
    lua_pop(L, 1);
    ImGuiLua_GetOrCreateTable(L, "ImGuiSelectableFlags");
    ImGuiLua_RegisterEnumImGuiSelectableFlags(L);
    lua_pop(L, 1);
    ImGuiLua_GetOrCreateTable(L, "ImGuiSliderFlags");
    ImGuiLua_RegisterEnumImGuiSliderFlags(L);
    lua_pop(L, 1);
    ImGuiLua_GetOrCreateTable(L, "ImGuiSortDirection");
    ImGuiLua_RegisterEnumImGuiSortDirection(L);
    lua_pop(L, 1);
    ImGuiLua_GetOrCreateTable(L, "ImGuiStyleVar");
    ImGuiLua_RegisterEnumImGuiStyleVar(L);
    lua_pop(L, 1);
    ImGuiLua_GetOrCreateTable(L, "ImGuiTabBarFlags");
    ImGuiLua_RegisterEnumImGuiTabBarFlags(L);
    lua_pop(L, 1);
    ImGuiLua_GetOrCreateTable(L, "ImGuiTabItemFlags");
    ImGuiLua_RegisterEnumImGuiTabItemFlags(L);
    lua_pop(L, 1);
    ImGuiLua_GetOrCreateTable(L, "ImGuiTableBgTarget");
    ImGuiLua_RegisterEnumImGuiTableBgTarget(L);
    lua_pop(L, 1);
    ImGuiLua_GetOrCreateTable(L, "ImGuiTableColumnFlags");
    ImGuiLua_RegisterEnumImGuiTableColumnFlags(L);
    lua_pop(L, 1);
    ImGuiLua_GetOrCreateTable(L, "ImGuiTableFlags");
    ImGuiLua_RegisterEnumImGuiTableFlags(L);
    lua_pop(L, 1);
    ImGuiLua_GetOrCreateTable(L, "ImGuiTableRowFlags");
    ImGuiLua_RegisterEnumImGuiTableRowFlags(L);
    lua_pop(L, 1);
    ImGuiLua_GetOrCreateTable(L, "ImGuiTreeNodeFlags");
    ImGuiLua_RegisterEnumImGuiTreeNodeFlags(L);
    lua_pop(L, 1);
    ImGuiLua_GetOrCreateTable(L, "ImGuiViewportFlags");
    ImGuiLua_RegisterEnumImGuiViewportFlags(L);
    lua_pop(L, 1);
    ImGuiLua_GetOrCreateTable(L, "ImGuiWindowFlags");
    ImGuiLua_RegisterEnumImGuiWindowFlags(L);
    lua_pop(L, 1);
    ImGuiLua_RegisterStructImGuiIO(L);
    ImGuiLua_RegisterStructImGuiViewport(L);
    ImGuiLua_RegisterStructImGuiWindowClass(L);
    ImGuiLua_RegisterStructImVec2(L);
    ImGuiLua_RegisterStructImVec4(L);
    ImGuiLua_RegisterStructImGuiLuaBoolRef(L);
    ImGuiLua_RegisterStructImGuiLuaFloatRef(L);
    ImGuiLua_RegisterStructImGuiLuaIntRef(L);
    ImGuiLua_RegisterStructImGuiLuaStringRef(L);
    ImGuiLua_GetOrCreateTable(L, "ImGui");
    luaL_setfuncs(L, ImGuiLua_RegImGui, 0);
    lua_pop(L, 1);
    return 0;
}
