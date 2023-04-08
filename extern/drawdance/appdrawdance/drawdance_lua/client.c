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
#include "../drawdance/app.h"
#include "lua_bindings.h"
#include "lua_util.h"
#include <dpclient/client.h>
#include <dpclient/document.h>
#include <dpclient/ext_auth.h>
#include <dpcommon/common.h>
#include <dpcommon/worker.h>
#include <dpmsg/message.h>
#include <dpmsg/msg_internal.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <parson.h>


typedef struct DP_LuaClientData {
    lua_State *L;
    DP_Document *doc;
    int doc_ref;
} DP_LuaClientData;


static int client_supported_schemes(lua_State *L)
{
    const char **supported_schemes = DP_client_supported_schemes();
    lua_newtable(L);
    for (lua_Integer i = 0; supported_schemes[i]; ++i) {
        lua_pushstring(L, supported_schemes[i]);
        lua_seti(L, -2, i + 1);
    }
    return 1;
}

static int client_default_scheme(lua_State *L)
{
    lua_pushstring(L, DP_client_default_scheme());
    return 1;
}

static int client_url_valid(lua_State *L)
{
    const char *url = luaL_checkstring(L, 1);
    DP_ClientUrlValidationResult result = DP_client_url_valid(url);
    lua_pushboolean(L, result == DP_CLIENT_URL_VALID);
    switch (result) {
    case DP_CLIENT_URL_VALID:
        lua_pushnil(L);
        break;
    case DP_CLIENT_URL_PARSE_ERROR:
        lua_pushliteral(L, "parse_error");
        break;
    case DP_CLIENT_URL_SCHEME_UNSUPPORTED:
        lua_pushliteral(L, "scheme_unsupported");
        break;
    case DP_CLIENT_URL_HOST_MISSING:
        lua_pushliteral(L, "host_missing");
        break;
    }
    return 2;
}


static void on_event(void *data, DP_Client *client, DP_ClientEventType type,
                     const char *message_or_null)
{
    DP_LuaClientData *lcd = data;
    lua_State *L = lcd->L;
    DP_LuaAppState *state = DP_lua_app_state(L);

    DP_lua_app_state_client_event_push(state, DP_client_id(client), type,
                                       message_or_null);

    if (type == DP_CLIENT_EVENT_FREE) {
        int doc_ref = lcd->doc_ref;
        if (doc_ref != LUA_NOREF) {
            luaL_unref(L, LUA_REGISTRYINDEX, doc_ref);
        }
        DP_free(lcd);
    }
}

static void push_drawing_command(DP_LuaClientData *lcd, DP_Message *msg,
                                 void (*push)(DP_Document *, DP_Message *))
{
    DP_Document *doc = lcd->doc;
    if (doc) {
        push(doc, msg);
    }
    else {
        DP_warn("Got drawing command, but no document is set up");
    }
}

static void push_reset_command(DP_LuaClientData *lcd, DP_Message *msg,
                               bool soft)
{
    unsigned int context_id = DP_message_context_id(msg);
    DP_Message *reset_msg = soft ? DP_msg_internal_soft_reset_new(context_id)
                                 : DP_msg_internal_reset_new(context_id);
    push_drawing_command(lcd, reset_msg, DP_document_command_push_noinc);
}

static void handle_command(DP_LuaClientData *lcd, DP_Message *msg)
{
    DP_MsgServerCommand *msc = DP_msg_server_command_cast(msg);
    const char *string = DP_msg_server_command_msg(msc, NULL);
    DP_debug("Handling command %s", string);
    JSON_Value *value = json_parse_string(string);
    if (!value) {
        DP_warn("Parsing command body failed: %s", string);
        return;
    }

    JSON_Object *object = json_value_get_object(value);
    if (!object) {
        DP_warn("Command body is not an object: %s", string);
        json_value_free(value);
        return;
    }

    const char *type = json_object_get_string(object, "type");
    if (type && strcmp(type, "reset") == 0) {
        push_reset_command(lcd, msg, false);
    }

    json_value_free(value);
}

static void on_message(void *data, DP_Client *client, DP_Message *msg)
{
    DP_LuaClientData *lcd = data;
    DP_MessageType type = DP_message_type(msg);
    if (DP_message_type_command(type)) {
        push_drawing_command(lcd, msg, DP_document_command_push_inc);
    }
    else if (type == DP_MSG_SOFT_RESET) {
        push_reset_command(lcd, msg, true);
    }
    else {
        if (type == DP_MSG_SERVER_COMMAND) {
            handle_command(lcd, msg);
        }
        int client_id = DP_client_id(client);
        DP_LuaAppState *state = DP_lua_app_state(lcd->L);
        DP_lua_app_state_client_message_push(state, client_id, msg);
    }
}

static const DP_ClientCallbacks client_callbacks = {on_event, on_message};

static int client_new(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    const char *url = luaL_checkstring(L, 2);

    lua_getiuservalue(L, lua_upvalueindex(1), 1);
    int id = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);
    lua_pushinteger(L, id + 1);
    lua_setiuservalue(L, lua_upvalueindex(1), 1);

    DP_LuaClientData *lcd = DP_malloc(sizeof(*lcd));
    *lcd = (DP_LuaClientData){L, NULL, LUA_NOREF};
    DP_Client *client = DP_client_new(id, url, &client_callbacks, lcd);
    if (!client) {
        return luaL_error(L, "Error creating client: %s", DP_error());
    }

    DP_Client **pp = lua_newuserdatauv(L, sizeof(client), 0);
    *pp = client;
    luaL_setmetatable(L, "DP_Client");
    return 1;
}


DP_LUA_DEFINE_CHECK(DP_Client, check_client)

static void free_client_on_worker(void *user)
{
    DP_client_free(user);
}

static void free_client(lua_State *L, DP_Client *client)
{
    DP_App *app = DP_lua_app(L);
    if (DP_app_worker_ready(app)) {
        DP_debug("Freeing client %d on worker", DP_client_id(client));
        DP_app_worker_push(app, free_client_on_worker, client);
    }
    else {
        DP_debug("Freeing client %d synchronously", DP_client_id(client));
        DP_client_free(client);
    }
}

#define CALL_FREE_CLIENT(PTR) free_client(L, (PTR))

DP_LUA_DEFINE_GC(DP_Client, client_gc, CALL_FREE_CLIENT)

static int client_index(lua_State *L)
{
    DP_Client *client = check_client(L, 1);
    if (lua_type(L, 2) == LUA_TSTRING) {
        const char *key = lua_tostring(L, 2);
        if (strcmp(key, "id") == 0) {
            lua_pushinteger(L, DP_client_id(client));
            return 1;
        }
        else if (strcmp(key, "document") == 0) {
            DP_LuaClientData *lcd = DP_client_callback_data(client);
            lua_rawgeti(L, LUA_REGISTRYINDEX, lcd->doc_ref);
            return 1;
        }
    }
    lua_getmetatable(L, 1);
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);
    return 1;
}

static int client_send(lua_State *L)
{
    DP_Client *client = check_client(L, 1);
    DP_Message **pp = luaL_checkudata(L, 2, "DP_Message");
    DP_client_send_inc(client, *pp);
    return 0;
}

static int client_attach_document(lua_State *L)
{
    DP_Client *client = check_client(L, 1);
    DP_Document **pp = luaL_checkudata(L, 2, "DP_Document");
    DP_LuaClientData *lcd = DP_client_callback_data(client);
    if (lcd->doc) {
        return luaL_error(L, "Client already has a document attached");
    }
    else {
        lua_pushvalue(L, 2);
        lcd->doc_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        lcd->doc = *pp;
    }
    return 0;
}

static int client_start_ping_timer(lua_State *L)
{
    DP_Client *client = check_client(L, 1);
    if (DP_client_ping_timer_start(client)) {
        return 0;
    }
    else {
        lua_pushstring(L, DP_error());
        return lua_error(L);
    }
}

static void push_ext_auth(void *user, DP_ClientExtAuthExecFn exec_fn,
                          void *ext_auth_params)
{
    DP_App *app = user;
    DP_app_worker_push(app, exec_fn, ext_auth_params);
}

static int client_ext_auth(lua_State *L)
{
    DP_Client *client = check_client(L, 1);
    const char *url = luaL_checkstring(L, 2);
    const char *body = luaL_checkstring(L, 3);
    long timeout_seconds = (long)luaL_checkinteger(L, 4);
    DP_App *app = DP_lua_app(L);
    if (DP_app_worker_ready(app)) {
        DP_client_ext_auth(client, url, body, timeout_seconds, push_ext_auth,
                           app);
        return 0;
    }
    else {
        return luaL_error(L, "No worker available");
    }
}

static const luaL_Reg client_methods[] = {
    {"__gc", client_gc},
    {"__index", client_index},
    {"terminate", client_gc},
    {"send", client_send},
    {"attach_document", client_attach_document},
    {"start_ping_timer", client_start_ping_timer},
    {"ext_auth", client_ext_auth},
    {NULL, NULL},
};


int DP_lua_client_init(lua_State *L)
{
    lua_pushglobaltable(L);
    luaL_getsubtable(L, -1, "DP");
    luaL_getsubtable(L, -1, "Client");

    lua_pushcfunction(L, client_supported_schemes);
    lua_setfield(L, -2, "supported_schemes");
    lua_pushcfunction(L, client_default_scheme);
    lua_setfield(L, -2, "default_scheme");
    lua_pushcfunction(L, client_url_valid);
    lua_setfield(L, -2, "url_valid");

    lua_newuserdatauv(L, 0, 1);
    lua_pushinteger(L, 1);
    lua_setiuservalue(L, -2, 1);
    lua_pushcclosure(L, client_new, 1);
    lua_setfield(L, -2, "new");

    luaL_getsubtable(L, -1, "Event");
    DP_LUA_SET_ENUM(DP_CLIENT_EVENT_, RESOLVE_ADDRESS_START);
    DP_LUA_SET_ENUM(DP_CLIENT_EVENT_, RESOLVE_ADDRESS_SUCCESS);
    DP_LUA_SET_ENUM(DP_CLIENT_EVENT_, RESOLVE_ADDRESS_ERROR);
    DP_LUA_SET_ENUM(DP_CLIENT_EVENT_, CONNECT_SOCKET_START);
    DP_LUA_SET_ENUM(DP_CLIENT_EVENT_, CONNECT_SOCKET_ERROR);
    DP_LUA_SET_ENUM(DP_CLIENT_EVENT_, CONNECT_SOCKET_SUCCESS);
    DP_LUA_SET_ENUM(DP_CLIENT_EVENT_, SPAWN_RECV_THREAD_ERROR);
    DP_LUA_SET_ENUM(DP_CLIENT_EVENT_, NETWORK_ERROR);
    DP_LUA_SET_ENUM(DP_CLIENT_EVENT_, SEND_ERROR);
    DP_LUA_SET_ENUM(DP_CLIENT_EVENT_, RECV_ERROR);
    DP_LUA_SET_ENUM(DP_CLIENT_EVENT_, CONNECTION_ESTABLISHED);
    DP_LUA_SET_ENUM(DP_CLIENT_EVENT_, CONNECTION_CLOSING);
    DP_LUA_SET_ENUM(DP_CLIENT_EVENT_, CONNECTION_CLOSED);
    DP_LUA_SET_ENUM(DP_CLIENT_EVENT_, EXT_AUTH_RESULT);
    DP_LUA_SET_ENUM(DP_CLIENT_EVENT_, EXT_AUTH_ERROR);
    DP_LUA_SET_ENUM(DP_CLIENT_EVENT_, FREE);
    DP_LUA_SET_ENUM(DP_CLIENT_EVENT_, COUNT);

    lua_pop(L, 2);
    luaL_newmetatable(L, "DP_Client");
    luaL_setfuncs(L, client_methods, 0);
    return 0;
}
