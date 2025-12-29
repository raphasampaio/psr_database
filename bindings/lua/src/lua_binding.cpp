extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include <psr_database/database.h>
#include <psr_database/result.h>

#include <cstring>
#include <memory>
#include <string>

// Metatable names
static const char* DATABASE_MT = "PsrDatabase.Database";
static const char* RESULT_MT = "PsrDatabase.Result";

// Userdata wrappers
struct LuaDatabase {
    psr::Database* db;
};

struct LuaResult {
    psr::Result* result;
};

// Helper to check and get Database userdata
static LuaDatabase* check_database(lua_State* L, int index) {
    return static_cast<LuaDatabase*>(luaL_checkudata(L, index, DATABASE_MT));
}

// Helper to check and get Result userdata
static LuaResult* check_result(lua_State* L, int index) {
    return static_cast<LuaResult*>(luaL_checkudata(L, index, RESULT_MT));
}

// Push a Value to Lua stack
static void push_value(lua_State* L, const psr::Value& value) {
    if (std::holds_alternative<std::nullptr_t>(value)) {
        lua_pushnil(L);
    } else if (std::holds_alternative<int64_t>(value)) {
        lua_pushinteger(L, std::get<int64_t>(value));
    } else if (std::holds_alternative<double>(value)) {
        lua_pushnumber(L, std::get<double>(value));
    } else if (std::holds_alternative<std::string>(value)) {
        const auto& s = std::get<std::string>(value);
        lua_pushlstring(L, s.c_str(), s.size());
    } else if (std::holds_alternative<std::vector<uint8_t>>(value)) {
        const auto& blob = std::get<std::vector<uint8_t>>(value);
        lua_pushlstring(L, reinterpret_cast<const char*>(blob.data()), blob.size());
    } else {
        lua_pushnil(L);
    }
}

// ============================================================================
// Database methods
// ============================================================================

static int l_database_new(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);

    LuaDatabase* ud = static_cast<LuaDatabase*>(lua_newuserdata(L, sizeof(LuaDatabase)));
    ud->db = nullptr;

    try {
        ud->db = new psr::Database(path);
    } catch (const std::exception& e) {
        lua_pushnil(L);
        lua_pushstring(L, e.what());
        return 2;
    }

    luaL_getmetatable(L, DATABASE_MT);
    lua_setmetatable(L, -2);

    return 1;
}

static int l_database_close(lua_State* L) {
    LuaDatabase* ud = check_database(L, 1);
    if (ud->db) {
        ud->db->close();
    }
    return 0;
}

static int l_database_is_open(lua_State* L) {
    LuaDatabase* ud = check_database(L, 1);
    lua_pushboolean(L, ud->db && ud->db->is_open());
    return 1;
}

static int l_database_execute(lua_State* L) {
    LuaDatabase* ud = check_database(L, 1);
    const char* sql = luaL_checkstring(L, 2);

    if (!ud->db || !ud->db->is_open()) {
        lua_pushnil(L);
        lua_pushstring(L, "Database is not open");
        return 2;
    }

    try {
        psr::Result cpp_result = ud->db->execute(sql);

        LuaResult* result =
            static_cast<LuaResult*>(lua_newuserdata(L, sizeof(LuaResult)));
        result->result = new psr::Result(std::move(cpp_result));

        luaL_getmetatable(L, RESULT_MT);
        lua_setmetatable(L, -2);

        return 1;
    } catch (const std::exception& e) {
        lua_pushnil(L);
        lua_pushstring(L, e.what());
        return 2;
    }
}

static int l_database_last_insert_rowid(lua_State* L) {
    LuaDatabase* ud = check_database(L, 1);
    if (ud->db) {
        lua_pushinteger(L, ud->db->last_insert_rowid());
    } else {
        lua_pushinteger(L, 0);
    }
    return 1;
}

static int l_database_changes(lua_State* L) {
    LuaDatabase* ud = check_database(L, 1);
    if (ud->db) {
        lua_pushinteger(L, ud->db->changes());
    } else {
        lua_pushinteger(L, 0);
    }
    return 1;
}

static int l_database_begin_transaction(lua_State* L) {
    LuaDatabase* ud = check_database(L, 1);
    try {
        ud->db->begin_transaction();
        lua_pushboolean(L, 1);
        return 1;
    } catch (const std::exception& e) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, e.what());
        return 2;
    }
}

static int l_database_commit(lua_State* L) {
    LuaDatabase* ud = check_database(L, 1);
    try {
        ud->db->commit();
        lua_pushboolean(L, 1);
        return 1;
    } catch (const std::exception& e) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, e.what());
        return 2;
    }
}

static int l_database_rollback(lua_State* L) {
    LuaDatabase* ud = check_database(L, 1);
    try {
        ud->db->rollback();
        lua_pushboolean(L, 1);
        return 1;
    } catch (const std::exception& e) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, e.what());
        return 2;
    }
}

static int l_database_gc(lua_State* L) {
    LuaDatabase* ud = check_database(L, 1);
    delete ud->db;
    ud->db = nullptr;
    return 0;
}

static int l_database_tostring(lua_State* L) {
    LuaDatabase* ud = check_database(L, 1);
    if (ud->db) {
        lua_pushfstring(L, "Database(%s)", ud->db->path().c_str());
    } else {
        lua_pushstring(L, "Database(closed)");
    }
    return 1;
}

// ============================================================================
// Result methods
// ============================================================================

static int l_result_row_count(lua_State* L) {
    LuaResult* ud = check_result(L, 1);
    lua_pushinteger(L, ud->result ? ud->result->row_count() : 0);
    return 1;
}

static int l_result_column_count(lua_State* L) {
    LuaResult* ud = check_result(L, 1);
    lua_pushinteger(L, ud->result ? ud->result->column_count() : 0);
    return 1;
}

static int l_result_columns(lua_State* L) {
    LuaResult* ud = check_result(L, 1);
    if (!ud->result) {
        lua_newtable(L);
        return 1;
    }

    const auto& cols = ud->result->columns();
    lua_createtable(L, static_cast<int>(cols.size()), 0);
    for (size_t i = 0; i < cols.size(); ++i) {
        lua_pushstring(L, cols[i].c_str());
        lua_rawseti(L, -2, static_cast<int>(i + 1));
    }
    return 1;
}

static int l_result_get_row(lua_State* L) {
    LuaResult* ud = check_result(L, 1);
    lua_Integer row = luaL_checkinteger(L, 2) - 1;  // Lua is 1-indexed

    if (!ud->result || row < 0 || static_cast<size_t>(row) >= ud->result->row_count()) {
        lua_pushnil(L);
        return 1;
    }

    const auto& r = (*ud->result)[row];
    const auto& cols = ud->result->columns();

    lua_createtable(L, 0, static_cast<int>(cols.size()));
    for (size_t i = 0; i < cols.size(); ++i) {
        lua_pushstring(L, cols[i].c_str());
        push_value(L, r[i]);
        lua_settable(L, -3);
    }
    return 1;
}

static int l_result_is_empty(lua_State* L) {
    LuaResult* ud = check_result(L, 1);
    lua_pushboolean(L, !ud->result || ud->result->empty());
    return 1;
}

static int l_result_gc(lua_State* L) {
    LuaResult* ud = check_result(L, 1);
    delete ud->result;
    ud->result = nullptr;
    return 0;
}

static int l_result_len(lua_State* L) {
    LuaResult* ud = check_result(L, 1);
    lua_pushinteger(L, ud->result ? ud->result->row_count() : 0);
    return 1;
}

static int l_result_index(lua_State* L) {
    LuaResult* ud = check_result(L, 1);

    // Check if it's a number (row access) or string (method access)
    if (lua_isnumber(L, 2)) {
        lua_Integer row = lua_tointeger(L, 2) - 1;
        if (!ud->result || row < 0 || static_cast<size_t>(row) >= ud->result->row_count()) {
            lua_pushnil(L);
            return 1;
        }

        const auto& r = (*ud->result)[row];
        const auto& cols = ud->result->columns();

        lua_createtable(L, 0, static_cast<int>(cols.size()));
        for (size_t i = 0; i < cols.size(); ++i) {
            lua_pushstring(L, cols[i].c_str());
            push_value(L, r[i]);
            lua_settable(L, -3);
        }
        return 1;
    }

    // Method access - look up in metatable
    luaL_getmetatable(L, RESULT_MT);
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);
    return 1;
}

// ============================================================================
// Module registration
// ============================================================================

static const luaL_Reg database_methods[] = {
    {"close", l_database_close},
    {"is_open", l_database_is_open},
    {"execute", l_database_execute},
    {"last_insert_rowid", l_database_last_insert_rowid},
    {"changes", l_database_changes},
    {"begin_transaction", l_database_begin_transaction},
    {"commit", l_database_commit},
    {"rollback", l_database_rollback},
    {"__gc", l_database_gc},
    {"__tostring", l_database_tostring},
    {nullptr, nullptr}
};

static const luaL_Reg result_methods[] = {
    {"row_count", l_result_row_count},
    {"column_count", l_result_column_count},
    {"columns", l_result_columns},
    {"get_row", l_result_get_row},
    {"is_empty", l_result_is_empty},
    {"__gc", l_result_gc},
    {"__len", l_result_len},
    {"__index", l_result_index},
    {nullptr, nullptr}
};

static const luaL_Reg module_functions[] = {
    {"open", l_database_new},
    {nullptr, nullptr}
};

extern "C" int luaopen_psr_database(lua_State* L) {
    // Create Database metatable
    luaL_newmetatable(L, DATABASE_MT);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, database_methods, 0);
    lua_pop(L, 1);

    // Create Result metatable
    luaL_newmetatable(L, RESULT_MT);
    luaL_setfuncs(L, result_methods, 0);
    lua_pop(L, 1);

    // Create module table
    luaL_newlib(L, module_functions);

    // Add version
    lua_pushstring(L, "1.0.0");
    lua_setfield(L, -2, "version");

    return 1;
}
