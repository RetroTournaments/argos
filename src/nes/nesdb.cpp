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

#include "nes/nesdb.h"

using namespace sta::nes;

void sta::nes::column_inputs(sqlite3_stmt* stmt, int column, std::vector<nes::ControllerState>* inputs) {
    if (!inputs) return;

    int sz = sqlite3_column_bytes(stmt, column);
    const uint8_t* dat = reinterpret_cast<const uint8_t*>(sqlite3_column_blob(stmt, column));
    inputs->assign(dat, dat + sz);
}

void sta::nes::column_frame_palette(sqlite3_stmt* stmt, int column, nes::FramePalette* palette)
{
    int sz = sqlite3_column_bytes(stmt, column);
    if (sz != nes::FRAMEPALETTE_SIZE) {
        throw std::runtime_error("not a frame palette?");
    }
    const uint8_t* dat = reinterpret_cast<const uint8_t*>(sqlite3_column_blob(stmt, column));
    for (int i = 0; i < nes::FRAMEPALETTE_SIZE; i++) {
        (*palette)[i] = dat[i];
    }
}

void sta::nes::column_nametable(sqlite3_stmt* stmt, int column, nes::NameTable* nametable)
{
    int sz = sqlite3_column_bytes(stmt, column);
    if (sz != nes::NAMETABLE_SIZE) {
        throw std::runtime_error("not a nametable?");
    }
    const uint8_t* dat = reinterpret_cast<const uint8_t*>(sqlite3_column_blob(stmt, column));
    for (int i = 0; i < nes::NAMETABLE_SIZE; i++) {
        (*nametable)[i] = dat[i];
    }
}

void sta::nes::column_pattern_table(sqlite3_stmt* stmt, int column, nes::PatternTable* pattern_table)
{
    int sz = sqlite3_column_bytes(stmt, 0);
    if (sz != nes::PATTERNTABLE_SIZE) {
        throw std::runtime_error("not a pattern table?");
    }
    const uint8_t* dat = reinterpret_cast<const uint8_t*>(sqlite3_column_blob(stmt, 0));
    for (int i = 0; i < nes::PATTERNTABLE_SIZE; i++) {
        (*pattern_table)[i] = dat[i];
    }
}

NESDatabase::NESDatabase(const std::string& path)
    : game::GameDatabase(path)
{
    ExecOrThrow(NESDatabase::ROMSchema());
    ExecOrThrow(NESDatabase::TASSchema());
    ExecOrThrow(NESDatabase::PatternTableSchema());
}

NESDatabase::~NESDatabase()
{
}

const char* NESDatabase::ROMSchema()
{
    return R"(CREATE TABLE IF NOT EXISTS nes_rom (
    id              INTEGER PRIMARY KEY,
    name            TEXT,
    rom             BLOB NOT NULL,
    header          BLOB NOT NULL
);)";
}

const char* NESDatabase::TASSchema()
{
    return R"(CREATE TABLE IF NOT EXISTS nes_tas (
    id              INTEGER PRIMARY KEY,
    rom_id          INTEGER REFERENCES rom(id) ON DELETE CASCADE NOT NULL,
    name            TEXT,
    start_string    BLOB,
    inputs          BLOB NOT NULL
);)";
}

const char* NESDatabase::PatternTableSchema()
{
    return R"(CREATE TABLE IF NOT EXISTS nes_pattern_table (
    id              INTEGER PRIMARY KEY,
    name            TEXT,
    pattern_table   BLOB NOT NULL
);)";
}

int NESDatabase::InsertROM(const std::string& name, const std::vector<uint8_t>& rom)
{
    if (rom.size() < 16) {
        return -1;
    }
    sqlite3_stmt* stmt;
    sqliteext::PrepareOrThrow(m_Database, R"(
        INSERT INTO nes_rom (name, rom, header) VALUES (?, ?, ?);
    )", &stmt);
    sqliteext::BindStrOrThrow(stmt, 1, name);
    sqliteext::BindBlbOrThrow(stmt, 2, rom.data(), rom.size());
    sqliteext::BindBlbOrThrow(stmt, 3, rom.data(), 16);
    sqliteext::StepAndFinalizeOrThrow(stmt);
    return sqlite3_last_insert_rowid(m_Database);
}

void NESDatabase::SelectAllROMs(std::vector<db::nes_rom>* roms)
{
    assert(roms);
    roms->clear();

    sqlite3_stmt* stmt;
    sqliteext::PrepareOrThrow(m_Database, R"(
        SELECT id, name, rom, header FROM nes_rom ORDER BY id ASC;
    )", &stmt);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        roms->emplace_back();
        auto& rom = roms->back();

        rom.id = sqlite3_column_int(stmt, 0);
        rom.name = sqliteext::column_str(stmt, 1);
        int sz = sqlite3_column_bytes(stmt, 2);
        const uint8_t* dat = reinterpret_cast<const uint8_t*>(sqlite3_column_blob(stmt, 2));
        rom.rom.assign(dat, dat + sz);

        sz = sqlite3_column_bytes(stmt, 3);
        assert(sz == 16);
        dat = reinterpret_cast<const uint8_t*>(sqlite3_column_blob(stmt, 3));
        for (int i = 0; i < sz; i++) {
            rom.header[i] = dat[i];
        }
    }
    sqlite3_finalize(stmt);
}

void NESDatabase::DeleteROM(int id)
{
    sqlite3_stmt* stmt;
    sqliteext::PrepareOrThrow(m_Database, R"(
        DELETE FROM nes_rom WHERE id = ?;
    )", &stmt);
    sqliteext::BindIntOrThrow(stmt, 1, id);
    sqliteext::StepAndFinalizeOrThrow(stmt);
}

NESDatabase::RomSPtr NESDatabase::GetRomCached(int rom_id)
{
    auto it = m_CachedRoms.find(rom_id);
    if (it != m_CachedRoms.end()) {
        return it->second;
    }

    sqlite3_stmt* stmt;
    sqliteext::PrepareOrThrow(m_Database, R"(
        SELECT rom FROM nes_rom WHERE id = ?;
    )", &stmt);
    sqliteext::BindIntOrThrow(stmt, 1, rom_id);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int sz = sqlite3_column_bytes(stmt, 0);
        const uint8_t* dat = reinterpret_cast<const uint8_t*>(sqlite3_column_blob(stmt, 0));

        std::vector<uint8_t> rom(dat, dat + sz);
        RomSPtr ptr = std::make_shared<const std::vector<uint8_t>>(std::move(rom));
        m_CachedRoms[rom_id] = ptr;
        return ptr;
    }
    return nullptr;
}

void NESDatabase::SelectAllTasesLight(std::vector<db::nes_tas>* tases)
{
    assert(tases);
    tases->clear();

    sqlite3_stmt* stmt;
    sqliteext::PrepareOrThrow(m_Database, R"(
        SELECT id, rom_id, name FROM nes_tas ORDER BY id ASC;
    )", &stmt);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        tases->emplace_back();
        tases->back().id = sqlite3_column_int(stmt, 0);
        tases->back().rom_id = sqlite3_column_int(stmt, 1);
        tases->back().name = sqliteext::column_str(stmt, 2);
    }
    sqlite3_finalize(stmt);

}

bool NESDatabase::SelectTAS(int tas_id, db::nes_tas* tas,
        std::vector<nes::ControllerState>* inputs)
{
    sqlite3_stmt* stmt;
    sqliteext::PrepareOrThrow(m_Database, R"(
        SELECT id, rom_id, name, start_string, inputs FROM nes_tas WHERE id = ?;
    )", &stmt);
    sqliteext::BindIntOrThrow(stmt, 1, tas_id);

    bool ret = false;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (tas) {
            if (!inputs) {
                inputs = &tas->inputs;
            }
            tas->id = sqlite3_column_int(stmt, 0);
            tas->rom_id = sqlite3_column_int(stmt, 1);
            tas->name = sqliteext::column_str(stmt, 2);
            tas->start_string = sqliteext::column_str(stmt, 3);
        }
        if (inputs) {
            column_inputs(stmt, 4, inputs);
        }
        ret = true;
    }
    sqlite3_finalize(stmt);
    return ret;
}

void NESDatabase::DeleteTAS(int id)
{
    sqlite3_stmt* stmt;
    sqliteext::PrepareOrThrow(m_Database, R"(
        DELETE FROM nes_tas WHERE id = ?;
    )", &stmt);
    sqliteext::BindIntOrThrow(stmt, 1, id);
    sqliteext::StepAndFinalizeOrThrow(stmt);
}

void NESDatabase::UpdateTASName(int id, const std::string& name)
{
    sqlite3_stmt* stmt;
    sqliteext::PrepareOrThrow(m_Database, R"(
        UPDATE nes_tas SET name = ? WHERE id = ?;
    )", &stmt);
    sqliteext::BindStrOrThrow(stmt, 1, name);
    sqliteext::BindIntOrThrow(stmt, 2, id);
    sqliteext::StepAndFinalizeOrThrow(stmt);
}

int NESDatabase::InsertNewTAS(int rom_id, const std::string& name, const std::vector<nes::ControllerState>& inputs)
{
    sqlite3_stmt* stmt;
    sqliteext::PrepareOrThrow(m_Database, R"(
        INSERT INTO nes_tas (rom_id, name, inputs) VALUES (?, ?, ?);
    )", &stmt);
    sqliteext::BindIntOrThrow(stmt, 1, rom_id);
    sqliteext::BindStrOrThrow(stmt, 2, name);
    sqliteext::BindBlbOrThrow(stmt, 3, inputs.data(), inputs.size());
    sqliteext::StepAndFinalizeOrThrow(stmt);
    return sqlite3_last_insert_rowid(m_Database);
}

int NESDatabase::InsertNewTAS(int rom_id, const std::string& name)
{
    std::vector<nes::ControllerState> inputs;
    return InsertNewTAS(rom_id, name, inputs);
}

bool NESDatabase::GetRomByName(const std::string& name, std::vector<uint8_t>* rom)
{
    sqlite3_stmt* stmt;
    sqliteext::PrepareOrThrow(m_Database, R"(
        SELECT rom FROM nes_rom WHERE name = ?;
    )", &stmt);
    sqliteext::BindStrOrThrow(stmt, 1, name);

    bool ret = false;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int sz = sqlite3_column_bytes(stmt, 0);
        const uint8_t* dat = reinterpret_cast<const uint8_t*>(sqlite3_column_blob(stmt, 0));
        if (rom) {
            rom->assign(dat, dat + sz);
        }
        ret = true;
    }
    sqlite3_finalize(stmt);
    return ret;
}

bool NESDatabase::GetPatternTableByName(const std::string& name,
        nes::PatternTable* pattern_table) {
    sqlite3_stmt* stmt;
    sqliteext::PrepareOrThrow(m_Database, R"(
        SELECT pattern_table FROM nes_pattern_table WHERE name = ?;
    )", &stmt);
    sqliteext::BindStrOrThrow(stmt, 1, name);

    bool ret = false;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        column_pattern_table(stmt, 0, pattern_table);
        ret = true;
    }
    sqlite3_finalize(stmt);
    return ret;
}
