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
#include "../drawdance/ui.h"
#include "../drawdance/app.h"
#include "lua_bindings.h"
#include "lua_util.h"
#include <dpcommon/common.h>
#include <SDL.h>
#include <SDL_keycode.h>
#include <SDL_mouse.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>


static int get_delta_time(lua_State *L)
{
    DP_App *app = DP_lua_app(L);
    DP_UserInputs *inputs = DP_app_inputs(app);
    lua_pushnumber(L, inputs->delta_time);
    return 1;
}

static int get_view_width_height(lua_State *L)
{
    DP_App *app = DP_lua_app(L);
    DP_UserInputs *inputs = DP_app_inputs(app);
    lua_pushinteger(L, inputs->view_width);
    lua_pushinteger(L, inputs->view_height);
    return 2;
}

static int get_view_width(lua_State *L)
{
    DP_App *app = DP_lua_app(L);
    DP_UserInputs *inputs = DP_app_inputs(app);
    lua_pushinteger(L, inputs->view_width);
    return 1;
}

static int get_view_height(lua_State *L)
{
    DP_App *app = DP_lua_app(L);
    DP_UserInputs *inputs = DP_app_inputs(app);
    lua_pushinteger(L, inputs->view_height);
    return 1;
}

static int scan_masked(lua_State *L, uint8_t mask)
{
    lua_Integer index = luaL_checkinteger(L, 1);
    if (index >= 0 && index < SDL_NUM_SCANCODES) {
        DP_App *app = DP_lua_app(L);
        DP_UserInputs *inputs = DP_app_inputs(app);
        lua_pushboolean(L, inputs->scan_codes[index] & mask);
        return 1;
    }
    else {
        return luaL_error(L, "Scan code out of bounds: %I", index);
    }
}

static int scan_held(lua_State *L)
{
    return scan_masked(L, DP_UI_HELD);
}

static int scan_pressed(lua_State *L)
{
    return scan_masked(L, DP_UI_PRESSED);
}

static int scan_released(lua_State *L)
{
    return scan_masked(L, DP_UI_RELEASED);
}

static int get_key_mods(lua_State *L)
{
    SDL_Keymod mods = SDL_GetModState();
    lua_Integer result = KMOD_NONE;
    if (mods & KMOD_SHIFT) {
        result |= KMOD_SHIFT;
    }
    if (mods & KMOD_CTRL) {
        result |= KMOD_CTRL;
    }
    if (mods & KMOD_ALT) {
        result |= KMOD_ALT;
    }
    if (mods & KMOD_GUI) {
        result |= KMOD_GUI;
    }
    lua_pushinteger(L, result);
    return 1;
}

static int mouse_masked(lua_State *L, uint8_t mask)
{
    lua_Integer index = luaL_checkinteger(L, 1);
    if (index >= DP_UI_MOUSE_BUTTON_MIN && index <= DP_UI_MOUSE_BUTTON_MAX) {
        DP_App *app = DP_lua_app(L);
        DP_UserInputs *inputs = DP_app_inputs(app);
        lua_pushboolean(L, inputs->mouse_buttons[index - 1] & mask);
        return 1;
    }
    else {
        return luaL_error(L, "Mouse button out of bounds: %I", index);
    }
}

static int mouse_held(lua_State *L)
{
    return mouse_masked(L, DP_UI_HELD);
}

static int mouse_pressed(lua_State *L)
{
    return mouse_masked(L, DP_UI_PRESSED);
}

static int mouse_released(lua_State *L)
{
    return mouse_masked(L, DP_UI_RELEASED);
}

static int get_mouse_delta_xy(lua_State *L)
{
    DP_App *app = DP_lua_app(L);
    DP_UserInputs *inputs = DP_app_inputs(app);
    lua_pushinteger(L, inputs->mouse_delta_x);
    lua_pushinteger(L, inputs->mouse_delta_y);
    return 2;
}

static int get_mouse_delta_x(lua_State *L)
{
    DP_App *app = DP_lua_app(L);
    DP_UserInputs *inputs = DP_app_inputs(app);
    lua_pushinteger(L, inputs->mouse_delta_x);
    return 1;
}

static int get_mouse_delta_y(lua_State *L)
{
    DP_App *app = DP_lua_app(L);
    DP_UserInputs *inputs = DP_app_inputs(app);
    lua_pushinteger(L, inputs->mouse_delta_y);
    return 1;
}

static int get_mouse_wheel_xy(lua_State *L)
{
    DP_App *app = DP_lua_app(L);
    DP_UserInputs *inputs = DP_app_inputs(app);
    lua_pushinteger(L, inputs->mouse_wheel_x);
    lua_pushinteger(L, inputs->mouse_wheel_y);
    return 2;
}

static int get_mouse_wheel_x(lua_State *L)
{
    DP_App *app = DP_lua_app(L);
    DP_UserInputs *inputs = DP_app_inputs(app);
    lua_pushinteger(L, inputs->mouse_wheel_x);
    return 1;
}

static int get_mouse_wheel_y(lua_State *L)
{
    DP_App *app = DP_lua_app(L);
    DP_UserInputs *inputs = DP_app_inputs(app);
    lua_pushinteger(L, inputs->mouse_wheel_y);
    return 1;
}

static int get_finger_delta_xy(lua_State *L)
{
    DP_App *app = DP_lua_app(L);
    DP_UserInputs *inputs = DP_app_inputs(app);
    lua_pushnumber(L, inputs->finger_delta_x);
    lua_pushnumber(L, inputs->finger_delta_y);
    return 2;
}

static int get_finger_delta_x(lua_State *L)
{
    DP_App *app = DP_lua_app(L);
    DP_UserInputs *inputs = DP_app_inputs(app);
    lua_pushnumber(L, inputs->finger_delta_x);
    return 1;
}

static int get_finger_delta_y(lua_State *L)
{
    DP_App *app = DP_lua_app(L);
    DP_UserInputs *inputs = DP_app_inputs(app);
    lua_pushnumber(L, inputs->finger_delta_y);
    return 1;
}

static int get_finger_pinch(lua_State *L)
{
    DP_App *app = DP_lua_app(L);
    DP_UserInputs *inputs = DP_app_inputs(app);
    lua_pushnumber(L, inputs->finger_pinch);
    return 1;
}

static int set_cursor(lua_State *L)
{
    DP_App *app = DP_lua_app(L);
    DP_UserInputs *inputs = DP_app_inputs(app);
    lua_Integer id = luaL_checkinteger(L, 1);
    if (id >= 0 && id < SDL_NUM_SYSTEM_CURSORS) {
        DP_user_inputs_cursor_set(inputs, (SDL_SystemCursor)id);
        return 0;
    }
    else {
        return luaL_error(L, "Cursor id bounds: %I", index);
    }
}

static luaL_Reg ui_funcs[] = {
    {"get_delta_time", get_delta_time},
    {"get_view_width_height", get_view_width_height},
    {"get_view_width", get_view_width},
    {"get_view_height", get_view_height},
    {"scan_held", scan_held},
    {"scan_pressed", scan_pressed},
    {"scan_released", scan_released},
    {"get_key_mods", get_key_mods},
    {"mouse_held", mouse_held},
    {"mouse_pressed", mouse_pressed},
    {"mouse_released", mouse_released},
    {"get_mouse_delta_xy", get_mouse_delta_xy},
    {"get_mouse_delta_x", get_mouse_delta_x},
    {"get_mouse_delta_y", get_mouse_delta_y},
    {"get_mouse_wheel_xy", get_mouse_wheel_xy},
    {"get_mouse_wheel_x", get_mouse_wheel_x},
    {"get_mouse_wheel_y", get_mouse_wheel_y},
    {"get_finger_delta_xy", get_finger_delta_xy},
    {"get_finger_delta_x", get_finger_delta_x},
    {"get_finger_delta_y", get_finger_delta_y},
    {"get_finger_pinch", get_finger_pinch},
    {"set_cursor", set_cursor},
    {NULL, NULL},
};

int DP_lua_ui_init(lua_State *L)
{
    lua_pushglobaltable(L);
    luaL_getsubtable(L, -1, "DP");
    luaL_getsubtable(L, -1, "UI");

    luaL_setfuncs(L, ui_funcs, 0);

    luaL_getsubtable(L, -1, "Scancode");
    DP_LUA_SET_ENUM(SDL_SCANCODE_, UNKNOWN);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, A);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, B);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, C);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, D);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, E);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, F);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, G);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, H);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, I);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, J);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, K);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, L);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, M);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, N);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, O);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, P);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, Q);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, R);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, S);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, T);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, U);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, V);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, W);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, X);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, Y);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, Z);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, 1);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, 2);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, 3);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, 4);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, 5);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, 6);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, 7);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, 8);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, 9);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, 0);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, RETURN);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, ESCAPE);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, BACKSPACE);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, TAB);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, SPACE);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, MINUS);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, EQUALS);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, LEFTBRACKET);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, RIGHTBRACKET);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, BACKSLASH);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, NONUSHASH);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, SEMICOLON);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, APOSTROPHE);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, GRAVE);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, COMMA);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, PERIOD);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, SLASH);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, CAPSLOCK);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, F1);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, F2);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, F3);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, F4);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, F5);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, F6);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, F7);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, F8);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, F9);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, F10);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, F11);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, F12);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, PRINTSCREEN);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, SCROLLLOCK);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, PAUSE);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, INSERT);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, HOME);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, PAGEUP);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, DELETE);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, END);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, PAGEDOWN);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, RIGHT);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, LEFT);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, DOWN);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, UP);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, NUMLOCKCLEAR);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_DIVIDE);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_MULTIPLY);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_MINUS);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_PLUS);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_ENTER);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_1);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_2);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_3);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_4);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_5);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_6);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_7);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_8);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_9);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_0);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_PERIOD);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, NONUSBACKSLASH);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, APPLICATION);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, POWER);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_EQUALS);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, F13);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, F14);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, F15);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, F16);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, F17);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, F18);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, F19);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, F20);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, F21);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, F22);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, F23);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, F24);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, EXECUTE);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, HELP);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, MENU);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, SELECT);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, STOP);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, AGAIN);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, UNDO);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, CUT);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, COPY);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, PASTE);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, FIND);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, MUTE);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, VOLUMEUP);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, VOLUMEDOWN);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_COMMA);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_EQUALSAS400);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, INTERNATIONAL1);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, INTERNATIONAL2);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, INTERNATIONAL3);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, INTERNATIONAL4);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, INTERNATIONAL5);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, INTERNATIONAL6);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, INTERNATIONAL7);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, INTERNATIONAL8);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, INTERNATIONAL9);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, LANG1);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, LANG2);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, LANG3);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, LANG4);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, LANG5);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, LANG6);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, LANG7);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, LANG8);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, LANG9);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, ALTERASE);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, SYSREQ);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, CANCEL);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, CLEAR);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, PRIOR);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, RETURN2);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, SEPARATOR);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, OUT);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, OPER);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, CLEARAGAIN);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, CRSEL);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, EXSEL);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_00);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_000);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, THOUSANDSSEPARATOR);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, DECIMALSEPARATOR);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, CURRENCYUNIT);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, CURRENCYSUBUNIT);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_LEFTPAREN);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_RIGHTPAREN);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_LEFTBRACE);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_RIGHTBRACE);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_TAB);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_BACKSPACE);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_A);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_B);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_C);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_D);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_E);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_F);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_XOR);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_POWER);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_PERCENT);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_LESS);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_GREATER);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_AMPERSAND);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_DBLAMPERSAND);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_VERTICALBAR);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_DBLVERTICALBAR);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_COLON);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_HASH);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_SPACE);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_AT);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_EXCLAM);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_MEMSTORE);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_MEMRECALL);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_MEMCLEAR);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_MEMADD);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_MEMSUBTRACT);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_MEMMULTIPLY);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_MEMDIVIDE);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_PLUSMINUS);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_CLEAR);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_CLEARENTRY);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_BINARY);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_OCTAL);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_DECIMAL);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KP_HEXADECIMAL);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, LCTRL);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, LSHIFT);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, LALT);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, LGUI);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, RCTRL);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, RSHIFT);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, RALT);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, RGUI);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, MODE);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, AUDIONEXT);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, AUDIOPREV);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, AUDIOSTOP);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, AUDIOPLAY);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, AUDIOMUTE);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, MEDIASELECT);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, WWW);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, MAIL);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, CALCULATOR);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, COMPUTER);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, AC_SEARCH);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, AC_HOME);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, AC_BACK);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, AC_FORWARD);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, AC_STOP);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, AC_REFRESH);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, AC_BOOKMARKS);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, BRIGHTNESSDOWN);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, BRIGHTNESSUP);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, DISPLAYSWITCH);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KBDILLUMTOGGLE);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KBDILLUMDOWN);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, KBDILLUMUP);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, EJECT);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, SLEEP);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, APP1);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, APP2);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, AUDIOREWIND);
    DP_LUA_SET_ENUM(SDL_SCANCODE_, AUDIOFASTFORWARD);
    lua_pop(L, 1);

    luaL_getsubtable(L, -1, "KeyMod");
    DP_LUA_SET_ENUM(KMOD_, NONE);
    DP_LUA_SET_ENUM(KMOD_, LSHIFT);
    DP_LUA_SET_ENUM(KMOD_, RSHIFT);
    DP_LUA_SET_ENUM(KMOD_, LCTRL);
    DP_LUA_SET_ENUM(KMOD_, RCTRL);
    DP_LUA_SET_ENUM(KMOD_, LALT);
    DP_LUA_SET_ENUM(KMOD_, RALT);
    DP_LUA_SET_ENUM(KMOD_, LGUI);
    DP_LUA_SET_ENUM(KMOD_, RGUI);
    DP_LUA_SET_ENUM(KMOD_, NUM);
    DP_LUA_SET_ENUM(KMOD_, CAPS);
    DP_LUA_SET_ENUM(KMOD_, MODE);
#if SDL_VERSION_ATLEAST(2, 0, 22)
    DP_LUA_SET_ENUM(KMOD_, SCROLL);
#endif
    DP_LUA_SET_ENUM(KMOD_, CTRL);
    DP_LUA_SET_ENUM(KMOD_, SHIFT);
    DP_LUA_SET_ENUM(KMOD_, ALT);
    DP_LUA_SET_ENUM(KMOD_, GUI);
    lua_pop(L, 1);

    luaL_getsubtable(L, -1, "MouseButton");
    DP_LUA_SET_ENUM(SDL_BUTTON_, LEFT);
    DP_LUA_SET_ENUM(SDL_BUTTON_, MIDDLE);
    DP_LUA_SET_ENUM(SDL_BUTTON_, RIGHT);
    DP_LUA_SET_ENUM(SDL_BUTTON_, X1);
    DP_LUA_SET_ENUM(SDL_BUTTON_, X2);
    lua_pop(L, 1);

    luaL_getsubtable(L, -1, "SystemCursor");
    DP_LUA_SET_ENUM(SDL_SYSTEM_CURSOR_, ARROW);
    DP_LUA_SET_ENUM(SDL_SYSTEM_CURSOR_, IBEAM);
    DP_LUA_SET_ENUM(SDL_SYSTEM_CURSOR_, WAIT);
    DP_LUA_SET_ENUM(SDL_SYSTEM_CURSOR_, CROSSHAIR);
    DP_LUA_SET_ENUM(SDL_SYSTEM_CURSOR_, WAITARROW);
    DP_LUA_SET_ENUM(SDL_SYSTEM_CURSOR_, SIZENWSE);
    DP_LUA_SET_ENUM(SDL_SYSTEM_CURSOR_, SIZENESW);
    DP_LUA_SET_ENUM(SDL_SYSTEM_CURSOR_, SIZEWE);
    DP_LUA_SET_ENUM(SDL_SYSTEM_CURSOR_, SIZENS);
    DP_LUA_SET_ENUM(SDL_SYSTEM_CURSOR_, SIZEALL);
    DP_LUA_SET_ENUM(SDL_SYSTEM_CURSOR_, NO);
    DP_LUA_SET_ENUM(SDL_SYSTEM_CURSOR_, HAND);
    lua_pop(L, 1);

    return 0;
}
