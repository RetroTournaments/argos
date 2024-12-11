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

#ifndef ARGOS_EXT_SQLITEEXT_HEADER
#define ARGOS_EXT_SQLITEEXT_HEADER

#include <string>
#include <functional>
#include <cstdint>

#include <sqlite3.h>

namespace argos::sqliteext {

void OpenOrThrow(const std::string& path, sqlite3** database);
int ExecOrThrow(sqlite3* db, const std::string& query,
        std::function<bool(int argc, char** data, char** columns)> cback = nullptr);
bool SimpleSQLitePrint(int argc, char** data, char** columns);
int ExecPrintOrThrow(sqlite3* db, const std::string& query);
void PrepareOrThrow(sqlite3* db, const std::string& query, sqlite3_stmt** stmt);
void BindIntOrThrow(sqlite3_stmt* stmt, int pos, int value);
void BindStrOrThrow(sqlite3_stmt* stmt, int pos, const std::string& str);
void BindBlbOrThrow(sqlite3_stmt* stmt, int pos, const void* data, size_t size); // SQLITE_STATIC only
void StepAndFinalizeOrThrow(sqlite3_stmt* stmt);

bool ExecForSingleNullableString(sqlite3* db, const std::string& query, std::string* str);
bool ExecForSingleNullableInt(sqlite3* db, const std::string& query, int* v);
std::string ExecForSingleString(sqlite3* db, const std::string& query);
int ExecForSingleInt(sqlite3* db, const std::string& query);

// to align with:
// int                  sqlite3_column_int(sqlite3_stmt*, int column)
// const unsigned char* sqlite3_column_text(sqlite3_stmt*, int column)
// int                  sqlite3_column_bytes(sqlite3_stmt*, int column)
// const void*          sqlite3_column_blob(sqlite3_stmt*, int column)
std::string column_str(sqlite3_stmt* stmt, int column);

////////////////////////////////////////////////////////////////////////////////

class SQLiteExtDB
{
public:
    SQLiteExtDB(const std::string& path);
    ~SQLiteExtDB();

    void Open();
    void Close();

    // calls 'system', so not suitable for most situations.
    int SystemLaunchSQLite3WithExamples();

    int ExecOrThrow(const std::string& query,
            std::function<bool(int argc, char** data, char** columns)> cback = nullptr);
    sqlite3* m_Database;
    std::string m_DatabasePath;
};

}

#endif

