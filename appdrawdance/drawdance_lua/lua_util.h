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
#ifndef DRAWDANCE_LUA_UTIL_H
#define DRAWDANCE_LUA_UTIL_H


#define DP_LUA_DIE(...)          \
    do {                         \
        luaL_error(__VA_ARGS__); \
        DP_UNREACHABLE();        \
    } while (0)

#define DP_LUA_SET_ENUM(PREFIX, TYPE)                  \
    do {                                               \
        lua_pushinteger(L, (lua_Integer)PREFIX##TYPE); \
        lua_setfield(L, -2, #TYPE);                    \
        lua_pushstring(L, #TYPE);                      \
        lua_seti(L, -2, (lua_Integer)PREFIX##TYPE);    \
    } while (0)

#define DP_LUA_DEFINE_CHECK(TYPE, NAME)               \
    static TYPE *NAME(lua_State *L, int index)        \
    {                                                 \
        TYPE **pp = luaL_checkudata(L, index, #TYPE); \
        if (*pp) {                                    \
            return *pp;                               \
        }                                             \
        else {                                        \
            DP_LUA_DIE(L, #TYPE " is null");          \
        }                                             \
    }

#define DP_LUA_DEFINE_GC(TYPE, NAME, FREE)        \
    static int NAME(lua_State *L)                 \
    {                                             \
        TYPE **pp = luaL_checkudata(L, 1, #TYPE); \
        if (*pp) {                                \
            FREE(*pp);                            \
            *pp = NULL;                           \
        }                                         \
        return 0;                                 \
    }


#endif
