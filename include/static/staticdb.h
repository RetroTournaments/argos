////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2023 Matthew Deutsch
//
// Static is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// Static is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Static; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
////////////////////////////////////////////////////////////////////////////////
//
// The staticdb contains information that is not generally meant to be human
// editable, but still should persist between runs. Such as window sizes /
// locations.
//
// Stored in a SQLite database which is great for structured data!
//
////////////////////////////////////////////////////////////////////////////////

#ifndef STATIC_STATIC_STATICDB_HEADER
#define STATIC_STATIC_STATICDB_HEADER

#include <string>

#include "ext/sqliteext/sqliteext.h"
#include "util/rect.h"

namespace sta
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

class StaticDB : public sqliteext::SQLiteExtDB
{
public:
    StaticDB(const std::string& path);
    ~StaticDB();

    bool LoadAppCache(db::AppCache* cache);
    void SaveAppCache(const db::AppCache& cache);

    static const char* AppCacheSchema();
};

}

#endif
