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
    ExecOrThrow(SMBDatabase::NTExtractInputsSchema());
    ExecOrThrow(SMBDatabase::NTExtractRecordSchema());
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

const char* SMBDatabase::NTExtractInputsSchema()
{
    return R"(CREATE TABLE IF NOT EXISTS nt_extract_inputs (
    id              INTEGER PRIMARY KEY,
    name            STRING,
    inputs          BLOB
);)";
}

const char* SMBDatabase::NTExtractRecordSchema()
{
    return R"(CREATE TABLE IF NOT EXISTS nt_extract_record (
    id                      INTEGER PRIMARY KEY,
    nt_extract_id           INTEGER REFERENCES nt_extract_inputs(id) ON DELETE RESTRICT NOT NULL,
    frame                   INTEGER NOT NULL,
    nt_index                INTEGER NOT NULL,
    area_data_low           INTEGER,
    area_data_high          INTEGER,
    screenedge_pageloc      INTEGER,
    screenedge_x_pos        INTEGER,
    block_buffer_84_disc    INTEGER,
    frame_palette           BLOB
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
    throw std::runtime_error("not implemented");
    return false;
}

bool SMBDatabase::GetMinimapPage(AreaID area_id, uint8_t page, db::minimap_page* mini_page)
{
    if (!mini_page) return false;
    throw std::runtime_error("not implemented");
    return false;
}

bool SMBDatabase::GetAllNTExtractInputs(std::vector<db::nt_extract_inputs>* inputs)
{
    if (!inputs) {
        return false;
    }
    sqlite3_stmt* stmt;
    sqliteext::PrepareOrThrow(m_Database, R"(
        SELECT id, name, inputs FROM nt_extract_inputs ORDER BY id ASC;
    )", &stmt);

    inputs->clear();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        inputs->emplace_back();
        inputs->back().id = sqlite3_column_int(stmt, 0);
        inputs->back().name = sqliteext::column_str(stmt, 1);

        int sz = sqlite3_column_bytes(stmt, 2);
        const uint8_t* dat = reinterpret_cast<const uint8_t*>(sqlite3_column_blob(stmt, 2));
        inputs->back().inputs.assign(dat, dat + sz);
    }
    sqlite3_finalize(stmt);
    return true;
}

bool SMBDatabase::GetAllNTExtractRecords(int input_id, std::vector<db::nt_extract_record>* records)
{
    if (!records) {
        return false;
    }
    sqlite3_stmt* stmt;
    sqliteext::PrepareOrThrow(m_Database, R"(
        SELECT * FROM nt_extract_record WHERE nt_extract_id = ? ORDER BY frame ASC;
    )", &stmt);
    sqliteext::BindIntOrThrow(stmt, 1, input_id);

    records->clear();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        records->emplace_back();
        records->back().id = sqlite3_column_int(stmt, 0);
        records->back().nt_extract_id = sqlite3_column_int(stmt, 1);
        records->back().area_data_low = sqlite3_column_int(stmt, 2);
        records->back().area_data_high = sqlite3_column_int(stmt, 3);
        records->back().screenedge_pageloc = sqlite3_column_int(stmt, 4);
        records->back().screenedge_x_pos = sqlite3_column_int(stmt, 5);
        records->back().block_buffer_84_disc = sqlite3_column_int(stmt, 6);
        nes::column_frame_palette(stmt, 7, &records->back().frame_palette);
    }
    sqlite3_finalize(stmt);
    return true;
}
