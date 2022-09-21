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
#include "lua_util.h"
#include <dpcommon/common.h>
#include <dpmsg/disconnect_reason.h>
#include <dpmsg/message.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>


DP_LUA_DEFINE_CHECK(DP_Message, check_message)
DP_LUA_DEFINE_GC(DP_Message, message_gc, DP_message_decref)

static int message_index_type(lua_State *L, DP_Message *msg)
{
    lua_pushinteger(L, (lua_Integer)DP_message_type(msg));
    return 1;
}

static int message_index_context_id(lua_State *L, DP_Message *msg)
{
    lua_pushinteger(L, (lua_Integer)DP_message_context_id(msg));
    return 1;
}

static int message_index_server_command_message(lua_State *L, DP_Message *msg)
{
    DP_MsgServerCommand *mc = DP_message_internal(msg);
    size_t len;
    const char *message = (const char *)DP_msg_server_command_msg(mc, &len);
    lua_pushlstring(L, message, len);
    return 1;
}

static int message_index_disconnect_reason(lua_State *L, DP_Message *msg)
{
    DP_MsgDisconnect *md = DP_message_internal(msg);
    lua_pushinteger(L, (int)DP_msg_disconnect_reason(md));
    return 1;
}

static int message_index_ping_is_pong(lua_State *L, DP_Message *msg)
{
    DP_MsgPing *mp = DP_message_internal(msg);
    lua_pushboolean(L, DP_msg_ping_is_pong(mp));
    return 1;
}

static int message_index_join_name(lua_State *L, DP_Message *msg)
{
    DP_MsgJoin *mj = DP_message_internal(msg);
    size_t len;
    const char *name = DP_msg_join_name(mj, &len);
    lua_pushlstring(L, name, len);
    return 1;
}

static int message_index_chat_text(lua_State *L, DP_Message *msg)
{
    DP_MsgChat *mc = DP_message_internal(msg);
    size_t len;
    const char *message = DP_msg_chat_message(mc, &len);
    lua_pushlstring(L, message, len);
    return 1;
}

static int message_index_chat_is_shout(lua_State *L, DP_Message *msg)
{
    DP_MsgChat *mc = DP_message_internal(msg);
    lua_pushboolean(L, DP_msg_chat_oflags(mc) & DP_MSG_CHAT_OFLAGS_SHOUT);
    return 1;
}

static int message_index_chat_is_action(lua_State *L, DP_Message *msg)
{
    DP_MsgChat *mc = DP_message_internal(msg);
    lua_pushboolean(L, DP_msg_chat_oflags(mc) & DP_MSG_CHAT_OFLAGS_ACTION);
    return 1;
}

static int message_index_chat_is_pin(lua_State *L, DP_Message *msg)
{
    DP_MsgChat *mc = DP_message_internal(msg);
    lua_pushboolean(L, DP_msg_chat_oflags(mc) & DP_MSG_CHAT_OFLAGS_PIN);
    return 1;
}


typedef struct DP_LuaMessageIndexFunc {
    int (*value)(lua_State *L, DP_Message *msg);
} DP_LuaMessageIndexFunc;

typedef struct DP_LuaMessageIndexEntry {
    const char *name;
    const DP_LuaMessageIndexFunc func;
} DP_LuaMessageIndexReg;

typedef struct DP_LuaMessageIndexTable {
    DP_MessageType type;
    const DP_LuaMessageIndexReg *reg;
} DP_LuaMessageIndexTable;

static int message_index(lua_State *L)
{
    DP_Message *msg = check_message(L, 1);
    DP_MessageType type = DP_message_type(msg);
    if (lua_geti(L, lua_upvalueindex(1), (lua_Integer)type) == LUA_TNIL) {
        lua_pop(L, 1); // Unknown types, use fallback table.
        lua_geti(L, lua_upvalueindex(1), (lua_Integer)DP_MSG_TYPE_COUNT);
    }
    lua_pushvalue(L, 2);
    if (lua_gettable(L, -2) != LUA_TNIL) {
        const DP_LuaMessageIndexFunc *func = lua_touserdata(L, -1);
        return func->value(L, msg);
    }
    else {
        lua_pushnil(L);
        return 1;
    }
}

static const DP_LuaMessageIndexReg index_common[] = {
    {"type", {message_index_type}},
    {"context_id", {message_index_context_id}},
    {NULL, {NULL}},
};

static const DP_LuaMessageIndexReg index_server_command[] = {
    {"message", {message_index_server_command_message}},
    {NULL, {NULL}},
};

static const DP_LuaMessageIndexReg index_disconnect[] = {
    {"reason", {message_index_disconnect_reason}},
    {NULL, {NULL}},
};

static const DP_LuaMessageIndexReg index_ping[] = {
    {"is_pong", {message_index_ping_is_pong}},
    {NULL, {NULL}},
};

static const DP_LuaMessageIndexReg index_join[] = {
    {"name", {message_index_join_name}},
    {NULL, {NULL}},
};

static const DP_LuaMessageIndexReg index_chat[] = {
    {"text", {message_index_chat_text}},
    {"is_shout", {message_index_chat_is_shout}},
    {"is_action", {message_index_chat_is_action}},
    {"is_pin", {message_index_chat_is_pin}},
    {NULL, {NULL}},
};

static const DP_LuaMessageIndexTable index_tables[] = {
    {DP_MSG_SERVER_COMMAND, index_server_command},
    {DP_MSG_DISCONNECT, index_disconnect},
    {DP_MSG_PING, index_ping},
    {DP_MSG_JOIN, index_join},
    {DP_MSG_CHAT, index_chat},
};

static void register_indexes(lua_State *L, const DP_LuaMessageIndexReg *reg)
{
    for (int i = 0; reg[i].name && reg[i].func.value; ++i) {
        lua_pushlightuserdata(L, (void *)&reg[i].func);
        lua_setfield(L, -2, reg[i].name);
    }
}

static void create_index_table(lua_State *L)
{
    lua_newtable(L);
    for (size_t i = 0; i < DP_ARRAY_LENGTH(index_tables); ++i) {
        const DP_LuaMessageIndexTable *table = &index_tables[i];
        lua_newtable(L);
        register_indexes(L, index_common);
        register_indexes(L, table->reg);
        lua_seti(L, -2, (lua_Integer)table->type);
    }
    // Fallback for unknown types.
    lua_newtable(L);
    register_indexes(L, index_common);
    lua_seti(L, -2, (lua_Integer)DP_MSG_TYPE_COUNT);
}

void DP_lua_message_push_noinc(lua_State *L, DP_Message *msg)
{
    DP_Message **pp = lua_newuserdatauv(L, sizeof(msg), 0);
    *pp = msg;
    luaL_setmetatable(L, "DP_Message");
}

static int push_new_message(lua_State *L, DP_Message *msg)
{
    if (msg) {
        DP_lua_message_push_noinc(L, msg);
        return 1;
    }
    else {
        return luaL_error(L, "%s", DP_error());
    }
}

static int message_command_new(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    size_t length;
    const char *message = luaL_checklstring(L, 2, &length);
    return push_new_message(L, DP_msg_server_command_new(0, message, length));
}

static int message_disconnect_new(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    return push_new_message(
        L,
        DP_msg_disconnect_new(0, DP_MSG_DISCONNECT_REASON_SHUTDOWN, NULL, 0));
}

static int message_ping_new(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checkany(L, 2);
    return push_new_message(L, DP_msg_ping_new(0, lua_toboolean(L, 2)));
}

static int message_chat_new(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    size_t text_length;
    const char *text = luaL_checklstring(L, 2, &text_length);
    return push_new_message(L, DP_msg_chat_new(0, 0, 0, text, text_length));
}

static const luaL_Reg constructors[] = {
    {"Command", message_command_new},
    {"Disconnect", message_disconnect_new},
    {"Ping", message_ping_new},
    {"Chat", message_chat_new},
    {NULL, NULL},
};


int DP_lua_message_init(lua_State *L)
{
    luaL_newmetatable(L, "DP_Message");
    lua_pushcfunction(L, message_gc);
    lua_setfield(L, -2, "__gc");
    create_index_table(L);
    lua_pushcclosure(L, message_index, 1);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

    lua_pushglobaltable(L);
    luaL_getsubtable(L, -1, "DP");
    luaL_getsubtable(L, -1, "Message");

    for (int i = 0; constructors[i].name && constructors[i].func; ++i) {
        luaL_getsubtable(L, -1, constructors[i].name);
        lua_pushcfunction(L, constructors[i].func);
        lua_setfield(L, -2, "new");
        lua_pop(L, 1);
    }

    luaL_getsubtable(L, -1, "Type");
    DP_LUA_SET_ENUM(DP_MSG_, SERVER_COMMAND);
    DP_LUA_SET_ENUM(DP_MSG_, DISCONNECT);
    DP_LUA_SET_ENUM(DP_MSG_, PING);
    DP_LUA_SET_ENUM(DP_MSG_, INTERNAL);
    DP_LUA_SET_ENUM(DP_MSG_, JOIN);
    DP_LUA_SET_ENUM(DP_MSG_, LEAVE);
    DP_LUA_SET_ENUM(DP_MSG_, SESSION_OWNER);
    DP_LUA_SET_ENUM(DP_MSG_, CHAT);
    DP_LUA_SET_ENUM(DP_MSG_, TRUSTED_USERS);
    DP_LUA_SET_ENUM(DP_MSG_, SOFT_RESET);
    DP_LUA_SET_ENUM(DP_MSG_, PRIVATE_CHAT);
    DP_LUA_SET_ENUM(DP_MSG_, INTERVAL);
    DP_LUA_SET_ENUM(DP_MSG_, LASER_TRAIL);
    DP_LUA_SET_ENUM(DP_MSG_, MOVE_POINTER);
    DP_LUA_SET_ENUM(DP_MSG_, MARKER);
    DP_LUA_SET_ENUM(DP_MSG_, USER_ACL);
    DP_LUA_SET_ENUM(DP_MSG_, LAYER_ACL);
    DP_LUA_SET_ENUM(DP_MSG_, FEATURE_ACCESS_LEVELS);
    DP_LUA_SET_ENUM(DP_MSG_, DEFAULT_LAYER);
    DP_LUA_SET_ENUM(DP_MSG_, FILTERED);
    DP_LUA_SET_ENUM(DP_MSG_, EXTENSION);
    DP_LUA_SET_ENUM(DP_MSG_, UNDO_POINT);
    DP_LUA_SET_ENUM(DP_MSG_, CANVAS_RESIZE);
    DP_LUA_SET_ENUM(DP_MSG_, LAYER_CREATE);
    DP_LUA_SET_ENUM(DP_MSG_, LAYER_ATTRIBUTES);
    DP_LUA_SET_ENUM(DP_MSG_, LAYER_RETITLE);
    DP_LUA_SET_ENUM(DP_MSG_, LAYER_ORDER);
    DP_LUA_SET_ENUM(DP_MSG_, LAYER_DELETE);
    DP_LUA_SET_ENUM(DP_MSG_, LAYER_VISIBILITY);
    DP_LUA_SET_ENUM(DP_MSG_, PUT_IMAGE);
    DP_LUA_SET_ENUM(DP_MSG_, FILL_RECT);
    DP_LUA_SET_ENUM(DP_MSG_, TOOL_CHANGE);
    DP_LUA_SET_ENUM(DP_MSG_, PEN_MOVE);
    DP_LUA_SET_ENUM(DP_MSG_, PEN_UP);
    DP_LUA_SET_ENUM(DP_MSG_, ANNOTATION_CREATE);
    DP_LUA_SET_ENUM(DP_MSG_, ANNOTATION_RESHAPE);
    DP_LUA_SET_ENUM(DP_MSG_, ANNOTATION_EDIT);
    DP_LUA_SET_ENUM(DP_MSG_, ANNOTATION_DELETE);
    DP_LUA_SET_ENUM(DP_MSG_, MOVE_REGION);
    DP_LUA_SET_ENUM(DP_MSG_, PUT_TILE);
    DP_LUA_SET_ENUM(DP_MSG_, CANVAS_BACKGROUND);
    DP_LUA_SET_ENUM(DP_MSG_, DRAW_DABS_CLASSIC);
    DP_LUA_SET_ENUM(DP_MSG_, DRAW_DABS_PIXEL);
    DP_LUA_SET_ENUM(DP_MSG_, DRAW_DABS_PIXEL_SQUARE);
    DP_LUA_SET_ENUM(DP_MSG_, DRAW_DABS_MYPAINT);
    DP_LUA_SET_ENUM(DP_MSG_, MOVE_RECT);
    DP_LUA_SET_ENUM(DP_MSG_, SET_METADATA_INT);
    DP_LUA_SET_ENUM(DP_MSG_, SET_METADATA_STR);
    DP_LUA_SET_ENUM(DP_MSG_, SET_TIMELINE_FRAME);
    DP_LUA_SET_ENUM(DP_MSG_, REMOVE_TIMELINE_FRAME);
    DP_LUA_SET_ENUM(DP_MSG_, UNDO);
    lua_pop(L, 1);

    luaL_getsubtable(L, -1, "Disconnect");
    luaL_getsubtable(L, -1, "Reason");
    DP_LUA_SET_ENUM(DP_MSG_DISCONNECT_REASON_, ERROR);
    DP_LUA_SET_ENUM(DP_MSG_DISCONNECT_REASON_, KICK);
    DP_LUA_SET_ENUM(DP_MSG_DISCONNECT_REASON_, SHUTDOWN);
    DP_LUA_SET_ENUM(DP_MSG_DISCONNECT_REASON_, OTHER);
    lua_pop(L, 2);

    return 0;
}
