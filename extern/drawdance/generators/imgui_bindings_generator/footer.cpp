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
        return luaL_error(L, "Wrong number of arguments: got %d, wanted %d", ARGC, 1);
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
        luaL_error(L, "Attempt to pop %d style colors, but only %d are on the stack", count, ImGuiLua_PushedStyleColors);
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
        luaL_error(L, "Attempt to pop %d style vars, but only %d are on the stack", count, ImGuiLua_PushedStyleColors);
    }
    return 0;
}
