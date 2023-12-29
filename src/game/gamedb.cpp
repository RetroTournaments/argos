////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2023 Matthew Deutsch
//
// Argos is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// Argos is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Argos; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
////////////////////////////////////////////////////////////////////////////////

#include "game/gamedb.h"

using namespace argos;
using namespace argos::game;

GameDatabase::GameDatabase(const std::string& path)
    : SQLiteExtDB(path)
{
    ExecOrThrow(KVIntSchema());
    ExecOrThrow(KVDoubleSchema());
    ExecOrThrow(KVStringSchema());
    ExecOrThrow(KVBlobSchema());
}

GameDatabase::~GameDatabase()
{
}

const char* GameDatabase::KVIntSchema()
{
    return R"(CREATE TABLE IF NOT EXISTS kv_int (
    key                 TEXT PRIMARY KEY,
    value               INTEGER NOT NULL
);)";
}

const char* GameDatabase::KVDoubleSchema()
{
    return R"(CREATE TABLE IF NOT EXISTS kv_real (
    key                 TEXT PRIMARY KEY,
    value               REAL NOT NULL
);)";
}

const char* GameDatabase::KVStringSchema()
{
    return R"(CREATE TABLE IF NOT EXISTS kv_text (
    key                 TEXT PRIMARY KEY,
    value               TEXT NOT NULL
);)";
}

enum KVBlobType : int {
    BASIC_BLOB = 0,
    PNG_BLOB = 1
};

const char* GameDatabase::KVBlobSchema()
{
    return R"(CREATE TABLE IF NOT EXISTS kv_blob (
    key                 TEXT PRIMARY KEY,
    value               BLOB NOT NULL,
    type                INTEGER
);)";
}

int GameDatabase::SystemLaunchSQLite3WithExamples()
{
    std::string cmd = "sqlite3 " + m_DatabasePath;
    std::cout << cmd << "\n";
    std::cout << R"(examples:
    SELECT name FROM sqlite_schema WHERE type='table' ORDER BY name;
    .exit
    .schema TABLENAME
)";
    return system(cmd.c_str());
}

template<typename T> 
static bool DoMySelect(GameDatabase* db, const char* table, const char* key, std::function<T(sqlite3_stmt*, int)> func, T* value)
{
    sqlite3_stmt* stmt;
    sqliteext::PrepareOrThrow(db->m_Database, 
            "SELECT value FROM " + std::string(table) + " WHERE key = ?", &stmt);
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        *value = func(stmt, 0);
        sqlite3_finalize(stmt);
        return true;
    }
    sqlite3_finalize(stmt);
    return false;
}

template<typename T> 
static T DoMySelect2(GameDatabase* db, const char* table, const char* key, std::function<T(sqlite3_stmt*, int)> func)
{
    T ret;
    if (!DoMySelect(db, table, key, func, &ret)) {
        throw std::runtime_error("Unknown key: '" + std::string(key) + "' for table '" + std::string(table) + "'");
    }
    return ret;
}

static void DoMyUpdate(GameDatabase* db, const char* table, const char* key, std::function<void(sqlite3_stmt*, int)> bind)
{
    sqlite3_stmt* stmt;
    sqliteext::PrepareOrThrow(db->m_Database, 
            "INSERT OR IGNORE INTO " + std::string(table) + " (key, value) VALUES (?, ?)", &stmt);
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);
    bind(stmt, 2);
    sqliteext::StepAndFinalizeOrThrow(stmt);

    sqliteext::PrepareOrThrow(db->m_Database, 
            "UPDATE " + std::string(table) + " SET value = ? WHERE key = ?", &stmt);
    bind(stmt, 1);
    sqlite3_bind_text(stmt, 2, key, -1, SQLITE_STATIC);
    sqliteext::StepAndFinalizeOrThrow(stmt);
}

int GameDatabase::GetInt(const char* key)
{
    return DoMySelect2<int>(this, "kv_int", key, sqlite3_column_int);
}

int GameDatabase::GetInt(const char* key, int default_value)
{
    DoMySelect<int>(this, "kv_int", key, sqlite3_column_int, &default_value);
    return default_value;
}

void GameDatabase::SetInt(const char* key, int value)
{
    DoMyUpdate(this, "kv_int", key, [&](sqlite3_stmt* stmt, int pos){
        sqlite3_bind_int(stmt, pos, value);
    });
}

double GameDatabase::GetReal(const char* key)
{
    return DoMySelect2<double>(this, "kv_real", key, sqlite3_column_double);
}

double GameDatabase::GetReal(const char* key, double default_value)
{
    DoMySelect<double>(this, "kv_real", key, sqlite3_column_double, &default_value);
    return default_value;
}

void GameDatabase::SetReal(const char* key, double value)
{
    DoMyUpdate(this, "kv_real", key, [&](sqlite3_stmt* stmt, int pos){
        sqlite3_bind_double(stmt, pos, value);
    });
}

std::string GameDatabase::GetText(const char* key)
{
    return DoMySelect2<std::string>(this, "kv_text", key, sqliteext::column_str);
}

std::string GameDatabase::GetText(const char* key, std::string default_value)
{
    DoMySelect<std::string>(this, "kv_text", key, sqliteext::column_str, &default_value);
    return default_value;
}

void GameDatabase::SetText(const char* key, const std::string& value)
{
    DoMyUpdate(this, "kv_text", key, [&](sqlite3_stmt* stmt, int pos){
        sqliteext::BindStrOrThrow(stmt, pos, value);
    });
}
