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

#include "argos/argosdb.h"

using namespace argos;

ArgosDB::ArgosDB(const std::string& path)
    : sqliteext::SQLiteExtDB(path)
{
    ExecOrThrow(AppCacheSchema());
}

ArgosDB::~ArgosDB()
{
}

const char* ArgosDB::AppCacheSchema()
{
    return R"(CREATE TABLE IF NOT EXISTS app_cache (
    name            TEXT PRIMARY KEY,
    width           INTEGER NOT NULL,
    height          INTEGER NOT NULL,
    display         INTEGER NOT NULL,
    ini_data        TEXT NOT NULL
);)";
}
