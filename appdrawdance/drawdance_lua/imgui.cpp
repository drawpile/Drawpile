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
#include <imgui.h>
extern "C" {
#define DP_INSIDE_EXTERN_C
#include "lua_util.h"
#include <dpcommon/common.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#undef DP_INSIDE_EXTERN_C
}


struct BeginStackEntry {
    const char *begin_name;
    int end_id;
    void (*end_fn)(void);
};

static ImVector<BeginStackEntry> begin_stack;

extern "C" void DP_lua_imgui_begin(const char *begin_name, int end_id,
                                   void (*end_fn)(void))
{
    BeginStackEntry entry = {begin_name, end_id, end_fn};
    begin_stack.push_back(entry);
}

extern "C" void DP_lua_imgui_end(lua_State *L, const char *end_name, int end_id)
{
    if (begin_stack.empty()) {
        DP_LUA_DIE(L, "Unexpected %s, there's nothing to end", end_name);
    }
    const BeginStackEntry &entry = begin_stack.back();
    if (entry.end_id == end_id) {
        begin_stack.pop_back();
    }
    else {
        DP_LUA_DIE(L, "Unexpected %s to end %s", end_name, entry.begin_name);
    }
}

extern "C" int ImGuiLua_PopStyleColors();
extern "C" int ImGuiLua_PopStyleVars();

extern "C" void DP_lua_imgui_stack_clear(void)
{
    for (int i = begin_stack.size(); i > 0; --i) {
        const BeginStackEntry &entry = begin_stack[i - 1];
        DP_warn("Closing %s", entry.begin_name);
        entry.end_fn();
    }
    begin_stack.clear();

    int popped_style_colors = ImGuiLua_PopStyleColors();
    if (popped_style_colors != 0) {
        DP_warn("Popped %d style color(s)", popped_style_colors);
    }

    int popped_style_vars = ImGuiLua_PopStyleVars();
    if (popped_style_vars != 0) {
        DP_warn("Popped %d style var(s)", popped_style_vars);
    }
}
