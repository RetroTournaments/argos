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

#include <sstream>
#include <cassert>
#include <iostream>
#include <iomanip>

#include "ext/sqliteext/sqliteext.h"

using namespace argos::sqliteext;

void argos::sqliteext::OpenOrThrow(const std::string& path, sqlite3** database)
{
    int ret = sqlite3_open(path.c_str(), database);
    if (ret != SQLITE_OK) {
        std::ostringstream os;
        os << "unable to open database '" << path << "': " << sqlite3_errmsg(*database);
        sqlite3_close(*database);
        throw std::runtime_error(os.str());
    }
}

void argos::sqliteext::PrepareOrThrow(sqlite3* db, const std::string& query, sqlite3_stmt** stmt)
{
    if (sqlite3_prepare_v2(db, query.c_str(), -1, stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Error preparing statement: " + std::string(sqlite3_errmsg(db)));
    }
}

void argos::sqliteext::BindIntOrThrow(sqlite3_stmt* stmt, int pos, int value)
{
    if (sqlite3_bind_int(stmt, pos, value)) {
        throw std::runtime_error("bind failed? TODO get error info from sqlite");
    }
}

void argos::sqliteext::BindBlbOrThrow(sqlite3_stmt* stmt, int pos, const void* data, size_t size)
{
    if (sqlite3_bind_blob(stmt, pos, data, size, SQLITE_STATIC)) {
        throw std::runtime_error("bind failed? TODO get error info from sqlite");
    }
}

void argos::sqliteext::BindStrOrThrow(sqlite3_stmt* stmt, int pos, const std::string& str)
{
    if (sqlite3_bind_text(stmt, pos, str.c_str(), str.size(), SQLITE_TRANSIENT)) {
        throw std::runtime_error("bind failed? TODO get error info from sqlite");
    }
}

static int MyCallback(void* cback, int argc, char** data, char** columns)
{
    if (cback) {
        auto funcp = reinterpret_cast<std::function<bool(int, char**, char**)>*>(cback);
        if (!(*funcp)(argc, data, columns)) {
            return 1;
        }
    }
    return 0;
}

int argos::sqliteext::ExecOrThrow(sqlite3* db, const std::string& query,
        std::function<bool(int argc, char** data, char** columns)> cback)
{
    char* errmsg = nullptr;
    void* arg = nullptr;
    if (cback) {
        arg = &cback;
    }
    int ret = sqlite3_exec(db, query.c_str(), MyCallback, arg, &errmsg);
    if (ret == SQLITE_ABORT) {
        assert(errmsg == nullptr);
        return 0;
    }
    if (ret != SQLITE_OK) {
        assert(errmsg != nullptr);
        std::ostringstream os;
        os << "sqlite3_exec failed: " << errmsg << std::endl;
        sqlite3_free(errmsg);
        throw std::runtime_error(os.str());
    }
    return sqlite3_changes(db);
}

bool argos::sqliteext::SimpleSQLitePrint(int argc, char** data, char** columns)
{
    for (int i = 0; i < argc; i++) {
        std::string tdat;
        if (data[i]) {
            tdat = std::string(data[i]);
        } else {
            tdat = "<NULL>";
        }
        std::cout << std::setw(3) << i << " " << columns[i] << " : " << tdat << std::endl;
    }
    return true;
}

int argos::sqliteext::ExecPrintOrThrow(sqlite3* db, const std::string& query)
{
    return ExecOrThrow(db, query, SimpleSQLitePrint);
}


bool argos::sqliteext::ExecForSingleNullableString(sqlite3* db, const std::string& query, std::string* str)
{
    std::string v;
    bool fnd = false;
    ExecOrThrow(db, query, [&](int argc, char** data, char**){
        if (fnd) {
            std::ostringstream os;
            os << "ExecForSingle '" << query << "' returned more than one row.";
            throw std::runtime_error(os.str());
        }
        if (argc != 1) {
            std::ostringstream os;
            os << "ExecForSingle '" << query << "' returned not exactly one column.";
            throw std::runtime_error(os.str());
        }

        v = std::string(data[0]);
        fnd = true;
        return true;
    });
    if (str) {
        *str = v;
    }
    return fnd;
}

std::string argos::sqliteext::ExecForSingleString(sqlite3* db, const std::string& query)
{
    std::string str;
    if (!ExecForSingleNullableString(db, query, &str)) {
        std::ostringstream os;
        os << "ExecForSingle '" << query << "' returned nothing.";
        throw std::runtime_error(os.str());
    }
    return str;
}

int argos::sqliteext::ExecForSingleInt(sqlite3* db, const std::string& query)
{
    std::string v = ExecForSingleString(db, query);
    return std::stoi(v);
}

bool argos::sqliteext::ExecForSingleNullableInt(sqlite3* db, const std::string& query, int* v)
{
    std::string str;
    if (ExecForSingleNullableString(db, query, &str)) {
        if (v) {
            *v = std::stoi(str);
        }
        return true;
    }
    return false;
}

void argos::sqliteext::StepAndFinalizeOrThrow(sqlite3_stmt* stmt)
{
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        throw std::runtime_error("step failed? TODO get error from sqlite");
    }
    sqlite3_finalize(stmt);
}

std::string argos::sqliteext::column_str(sqlite3_stmt* stmt, int column)
{
    // TODO error handling?
    return std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, column)));
}

////////////////////////////////////////////////////////////////////////////////

SQLiteExtDB::SQLiteExtDB(const std::string& path)
    : m_DatabasePath(path)
{
    sqliteext::OpenOrThrow(m_DatabasePath, &m_Database);
}

SQLiteExtDB::~SQLiteExtDB()
{
    sqlite3_close(m_Database);
}

//void SQLiteExtDB::TableSchema(const char* table, const char* schema)
//{
//    ExecOrThrow(std::string(schema)); // todo register the table / schema somewhere for later query?
//}

int SQLiteExtDB::ExecOrThrow(const std::string& query,
        std::function<bool(int argc, char** data, char** columns)> cback)
{
    return sqliteext::ExecOrThrow(m_Database, query, cback);
}


int SQLiteExtDB::SystemLaunchSQLite3WithExamples()
{
    std::string cmd = "sqlite3 " + m_DatabasePath;
    std::cout << cmd << "\n";
    std::cout << R"(examples:
    .tables
    .schema TABLENAME
    .exit
)";
    return system(cmd.c_str());
}



