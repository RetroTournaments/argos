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

#include "fmt/fmt.h"

#include "smb/smbdb.h"
#include "util/file.h"
#include "ext/sqliteext/sqliteext.h"

using namespace argos;
using namespace argos::smb;

SMBDatabase::SMBDatabase(const std::string& path)
    : nes::NESDatabase(path)
{
    ExecOrThrow(SMBDatabase::SoundEffectSchema());
    ExecOrThrow(SMBDatabase::MusicTrackSchema());
    ExecOrThrow(SMBDatabase::NametablePageSchema());
    ExecOrThrow(SMBDatabase::MinimapPageSchema());
}

SMBDatabase::~SMBDatabase()
{
}

const char* SMBDatabase::SoundEffectSchema()
{
    return R"(CREATE TABLE IF NOT EXISTS sound_effect (
    effect          INTEGER PRIMARY KEY,
    wav_data        BLOB NOT NULL
);)";
}

const char* SMBDatabase::MusicTrackSchema()
{
    return R"(CREATE TABLE IF NOT EXISTS music_track (
    track           INTEGER PRIMARY KEY,
    wav_data        BLOB NOT NULL
);)";
}

const char* SMBDatabase::NametablePageSchema()
{
    return R"(CREATE TABLE IF NOT EXISTS nametable_page (
    area_id         INTEGER PRIMARY KEY,
    page            INTEGER NOT NULL,
    frame_palette   BLOB NOT NULL,
    data            BLOB NOT NULL
);)";
}

const char* SMBDatabase::MinimapPageSchema()
{
    return R"(CREATE TABLE IF NOT EXISTS minimap_page (
    area_id         INTEGER PRIMARY KEY,
    page            INTEGER NOT NULL,
    data            BLOB NOT NULL
);)";
}

static bool GetWav(SMBDatabase* db, const char* table, const char* nm, uint32_t v, std::vector<uint8_t>* data)
{
    sqlite3_stmt* stmt;
    sqliteext::PrepareOrThrow(db->m_Database, fmt::format(R"(
        SELECT wav_data FROM {} WHERE {} = ?;
    )", table, nm), &stmt);
    sqliteext::BindIntOrThrow(stmt, 1, v);

    bool ret = false;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int sz = sqlite3_column_bytes(stmt, 0);
        const uint8_t* dat = reinterpret_cast<const uint8_t*>(sqlite3_column_blob(stmt, 0));

        if (data) {
            data->assign(dat, dat + sz);
        }
        ret = true;
    }

    return ret;
}


bool SMBDatabase::GetSoundEffectWav(SoundEffect effect, std::vector<uint8_t>* data)
{
    return GetWav(this, "sound_effect", "effect", static_cast<uint32_t>(effect), data);
}

bool SMBDatabase::GetMusicTrackWav(MusicTrack track, std::vector<uint8_t>* data)
{
    return GetWav(this, "music_track", "track", static_cast<uint32_t>(track), data);
}

static bool InsertWav(SMBDatabase* db, const char* table, const char* nm, uint32_t v, const std::string& wavpath)
{
    std::vector<uint8_t> wav_data;
    if (!util::ReadFileToVector(wavpath, &wav_data)) {
        return false;
    }

    sqlite3_stmt* stmt;
    sqliteext::PrepareOrThrow(db->m_Database, fmt::format(R"(
        DELETE FROM {} WHERE {} = ?;
    )", table, nm), &stmt);
    sqliteext::BindIntOrThrow(stmt, 1, v);
    sqliteext::StepAndFinalizeOrThrow(stmt);

    sqliteext::PrepareOrThrow(db->m_Database, fmt::format(R"(
        INSERT INTO {} ({}, wav_data) VALUES (?, ?);
    )", table, nm), &stmt);

    sqliteext::BindIntOrThrow(stmt, 1, v);
    sqliteext::BindBlbOrThrow(stmt, 2, wav_data.data(), wav_data.size());
    sqliteext::StepAndFinalizeOrThrow(stmt);

    return true;
}

bool smb::InsertSoundEffect(SMBDatabase* database, SoundEffect effect, const std::string& wavpath)
{
    return InsertWav(database, "sound_effect", "effect", static_cast<uint32_t>(effect), wavpath);
}

bool smb::InsertMusicTrack(SMBDatabase* database, MusicTrack track, const std::string& wavpath)
{
    return InsertWav(database, "music_track", "track", static_cast<uint32_t>(track), wavpath);
}

bool SMBDatabase::GetNametablePage(AreaID area_id, uint8_t page, db::nametable_page* nt_page)
{
    if (!nt_page) return false;

    return false;
}

bool SMBDatabase::GetMinimapPage(AreaID area_id, uint8_t page, db::minimap_page* mini_page)
{
    if (!mini_page) return false;

    return false;
}
