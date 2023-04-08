# Copyright (c) 2022 askmeaboutloom
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
package ImGuiLua::Overloads;
use 5.0020;
use warnings;
use experimental qw(signatures);


my %overloads;

sub define(@args) {
    my $resolution = pop @args;
    $overloads{join ':', sort @args} = $resolution;
}

sub get(@signatures) {
    return $overloads{join ':', @signatures};
}


for my $pair (
        ['bool ImGui_BeginChild(ImGuiID, const ImVec2, bool, ImGuiWindowFlags)',
         'bool ImGui_BeginChild(const char*, const ImVec2, bool, ImGuiWindowFlags)'],
        ['void ImGui_OpenPopup(ImGuiID, ImGuiPopupFlags)',
         'void ImGui_OpenPopup(const char*, ImGuiPopupFlags)'],
        ['bool ImGui_IsPopupOpen(ImGuiID, ImGuiPopupFlags)',
         'bool ImGui_IsPopupOpen(const char*, ImGuiPopupFlags)'],
 ) {
    define @$pair,
        sub ($id_fn, $string_fn) {
            return qq/if (lua_type(L, 1) == LUA_TNUMBER) {\n/
                 . qq/    return $id_fn(L);\n/
                 . qq/}\n/
                 . qq/else {\n/
                 . qq/    return $string_fn(L);\n/
                 . qq/}\n/;
        };
}


define
    'bool ImGui_CollapsingHeader(const char*, ImGuiTreeNodeFlags)',
    'bool ImGui_CollapsingHeader(const char*, bool*, ImGuiTreeNodeFlags)',
    sub ($two_arg_fn, $three_arg_fn) {
        return qq/int ARGC = lua_gettop(L);\n/
             . qq/if (ARGC == 1 || (ARGC == 2 && lua_type(L, 2) == LUA_TNUMBER)) {\n/
             . qq/    return $two_arg_fn(L);\n/
             . qq/}\n/
             . qq/else if (ARGC == 2 || ARGC == 3) {\n/
             . qq/    return $three_arg_fn(L);\n/
             . qq/}\n/
             . qq/else {\n/
             . qq/    return luaL_error(L, "Wrong number of arguments: got %d, wanted between %d and %d", ARGC, 1, 3);\n/
             . qq/}\n/;
    };


for my $pair (
        ['void ImGui_SetWindowCollapsed(bool, ImGuiCond)',
         'void ImGui_SetWindowCollapsed(const char*, bool, ImGuiCond)'],
        ['void ImGui_SetWindowPos(const ImVec2, ImGuiCond)',
         'void ImGui_SetWindowPos(const char*, const ImVec2, ImGuiCond)'],
        ['void ImGui_SetWindowSize(const ImVec2, ImGuiCond)',
         'void ImGui_SetWindowSize(const char*, const ImVec2, ImGuiCond)']) {
    define @$pair,
        sub ($two_arg_fn, $three_arg_fn) {
            return qq/if (lua_isstring(L, 1)) {\n/
                 . qq/    return $three_arg_fn(L);\n/
                 . qq/}\n/
                 . qq/else {\n/
                 . qq/    return $two_arg_fn(L);\n/
                 . qq/}\n/;
        };
}


define
    'ImU32 ImGui_GetColorU32(ImGuiCol, float)',
    'ImU32 ImGui_GetColorU32(ImU32)',
    'ImU32 ImGui_GetColorU32(const ImVec4)',
    sub ($two_arg_fn, $u32_fn, $vec4_fn) {
        return qq/int ARGC = lua_gettop(L);\n/
             . qq/if (ARGC == 1) {\n/
             . qq/    if (lua_isnumber(L, 1)) {\n/
             . qq/        return $u32_fn(L);\n/
             . qq/    }\n/
             . qq/    else {\n/
             . qq/        return $vec4_fn(L);\n/
             . qq/    }\n/
             . qq/}\n/
             . qq/else if (ARGC == 2) {\n/
             . qq/    return $two_arg_fn(L);\n/
             . qq/}\n/
             . qq/else {\n/
             . qq/    return luaL_error(L, "Wrong number of arguments: got %d, wanted between %d and %d", ARGC, 1, 2);\n/
             . qq/}\n/;
    };


1;
