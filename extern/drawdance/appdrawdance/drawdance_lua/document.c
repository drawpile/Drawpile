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
#include <dpclient/document.h>
#include <dpcommon/common.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>


static int document_new(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);

    DP_Document *doc = DP_document_new();
    if (!doc) {
        return luaL_error(L, "Error creating document: %s", DP_error());
    }

    DP_Document **pp = lua_newuserdatauv(L, sizeof(doc), 0);
    *pp = doc;
    luaL_setmetatable(L, "DP_Document");
    return 1;
}

DP_LUA_DEFINE_GC(DP_Document, document_gc, DP_document_free)

static const luaL_Reg document_methods[] = {
    {"__gc", document_gc},
    {NULL, NULL},
};


int DP_lua_document_init(lua_State *L)
{
    lua_pushglobaltable(L);
    luaL_getsubtable(L, -1, "DP");
    luaL_getsubtable(L, -1, "Document");
    lua_pushcfunction(L, document_new);
    lua_setfield(L, -2, "new");
    lua_pop(L, 2);

    luaL_newmetatable(L, "DP_Document");
    luaL_setfuncs(L, document_methods, 0);
    return 0;
}
