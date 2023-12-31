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
//
// The argosdb contains information that is not generally meant to be human
// editable, but still should persist between runs. Such as window sizes /
// locations.
//
// Stored in a SQLite database which is great for structured data!
//
////////////////////////////////////////////////////////////////////////////////

#ifndef ARGOS_ARGOS_ARGOSDB_HEADER
#define ARGOS_ARGOS_ARGOSDB_HEADER

#include <string>

#include "ext/sqliteext/sqliteext.h"
#include "util/rect.h"

namespace argos
{

namespace db
{

struct AppCache
{
    std::string Name;

    int WindowX;
    int WindowY;
    int Width;
    int Height;

    int Display;

    std::string IniData;

    //
    util::Rect2I GetWinRect() const;
    void SetWinRect(const util::Rect2I& rect);
    static AppCache Defaults(const char* name = nullptr, int display = 0);
};

}

class ArgosDB : public sqliteext::SQLiteExtDB
{
public:
    ArgosDB(const std::string& path);
    ~ArgosDB();

    bool LoadAppCache(db::AppCache* cache);
    void SaveAppCache(const db::AppCache& cache);

    static const char* AppCacheSchema();
};

}

#endif
