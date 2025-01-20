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

#ifndef SDL_WINDOWPOS_CENTERED_DISPLAY
#define SDL_WINDOWPOS_CENTERED_MASK    0x2FFF0000u
#define SDL_WINDOWPOS_CENTERED_DISPLAY(X)  (SDL_WINDOWPOS_CENTERED_MASK|(X))
#define SDL_WINDOWPOS_CENTERED         SDL_WINDOWPOS_CENTERED_DISPLAY(0)
#endif

#include "static/staticdb.h"

using namespace sta;
using namespace sta::db;

AppCache AppCache::Defaults(const char* name, int display)
{
    AppCache cache;
    if (name) {
        cache.Name = std::string(name);
    }
    cache.WindowX = SDL_WINDOWPOS_CENTERED_DISPLAY(display);
    cache.WindowY = SDL_WINDOWPOS_CENTERED_DISPLAY(display);
    cache.Width = 1920;
    cache.Height = 1080;
    cache.Display = display;

    return cache;
}

util::Rect2I AppCache::GetWinRect() const
{
    return util::Rect2I(WindowX, WindowY, Width, Height);
}

void AppCache::SetWinRect(const util::Rect2I& rect)
{
    WindowX = rect.X;
    WindowY = rect.Y;
    Width = rect.Width;
    Height = rect.Height;
}

////////////////////////////////////////////////////////////////////////////////

StaticDB::StaticDB(const std::string& path)
    : sqliteext::SQLiteExtDB(path)
{
    ExecOrThrow(AppCacheSchema());
}

StaticDB::~StaticDB()
{
}

bool StaticDB::LoadAppCache(AppCache* cache)
{
    if (!cache) return false;

    sqlite3_stmt* stmt;
    sqliteext::PrepareOrThrow(m_Database, R"(
        SELECT window_x, window_y, width, height, display, ini_data FROM app_cache WHERE name = ?;
    )", &stmt);
    sqliteext::BindStrOrThrow(stmt, 1, cache->Name);

    bool ret = false;
    while (sqlite3_step(stmt) == SQLITE_ROW) {

        cache->WindowX = sqlite3_column_int(stmt, 0);
        cache->WindowY = sqlite3_column_int(stmt, 1);
        cache->Width = sqlite3_column_int(stmt, 2);
        cache->Height = sqlite3_column_int(stmt, 3);
        cache->Display = sqlite3_column_int(stmt, 4);
        cache->IniData = sqliteext::column_str(stmt, 5);

        ret = true;
    }
    sqlite3_finalize(stmt);

    return ret;
}

void StaticDB::SaveAppCache(const AppCache& cache)
{
    sqlite3_stmt* stmt;
    sqliteext::PrepareOrThrow(m_Database, R"(
        UPDATE app_cache
        SET window_x=?, window_y=?, width=?, height=?, display=?, ini_data=? WHERE name = ?;
    )", &stmt);

    auto bindexec = [&](){
        sqliteext::BindIntOrThrow(stmt, 1, cache.WindowX);
        sqliteext::BindIntOrThrow(stmt, 2, cache.WindowY);
        sqliteext::BindIntOrThrow(stmt, 3, cache.Width);
        sqliteext::BindIntOrThrow(stmt, 4, cache.Height);
        sqliteext::BindIntOrThrow(stmt, 5, cache.Display);
        sqliteext::BindStrOrThrow(stmt, 6, cache.IniData);
        sqliteext::BindStrOrThrow(stmt, 7, cache.Name);
        sqliteext::StepAndFinalizeOrThrow(stmt);
    };
    bindexec();

    sqliteext::PrepareOrThrow(m_Database, R"(
    INSERT OR IGNORE INTO app_cache (window_x, window_y, width, height, display, ini_data, name)
    VALUES (?, ?, ?, ?, ?, ?, ?);
    )", &stmt);
    bindexec();
}

const char* StaticDB::AppCacheSchema()
{
    return R"(CREATE TABLE IF NOT EXISTS app_cache (
    name            TEXT PRIMARY KEY,
    window_x        INTEGER NOT NULL,
    window_y        INTEGER NOT NULL,
    width           INTEGER NOT NULL,
    height          INTEGER NOT NULL,
    display         INTEGER NOT NULL,
    ini_data        TEXT NOT NULL
);)";
}
