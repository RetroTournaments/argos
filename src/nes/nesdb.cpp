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

#include "nes/nesdb.h"

using namespace argos::nes;

NESDatabase::NESDatabase(const std::string& path)
    : game::GameDatabase(path)
{
    ExecOrThrow(NESDatabase::ROMSchema());
    ExecOrThrow(NESDatabase::TASSchema());
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

        int sz = sqlite3_column_bytes(stmt, 1);
        const uint8_t* dat = reinterpret_cast<const uint8_t*>(sqlite3_column_blob(stmt, 1));

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

int NESDatabase::InsertNewTAS(int rom_id, const std::string& name)
{
    sqlite3_stmt* stmt;
    sqliteext::PrepareOrThrow(m_Database, R"(
        INSERT INTO nes_tas (rom_id, name) VALUES (?, ?);
    )", &stmt);
    sqliteext::BindIntOrThrow(stmt, 1, rom_id);
    sqliteext::BindStrOrThrow(stmt, 2, name);
    try {
        sqliteext::StepAndFinalizeOrThrow(stmt);
    } catch (std::runtime_error& err) {
        std::cout << err.what() << std::endl;
        return -1;
    }
    return sqlite3_last_insert_rowid(m_Database);
}

bool NESDatabase::GetRomByName(const std::string& name, std::vector<uint8_t>* rom)
{
    assert(rom);
    sqlite3_stmt* stmt;
    sqliteext::PrepareOrThrow(m_Database, R"(
        SELECT rom FROM nes_rom WHERE name = ?;
    )", &stmt);
    sqliteext::BindStrOrThrow(stmt, 1, name);

    bool ret = false;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int sz = sqlite3_column_bytes(stmt, 0);
        const uint8_t* dat = reinterpret_cast<const uint8_t*>(sqlite3_column_blob(stmt, 0));
        rom->assign(dat, dat + sz);
        ret = true;
    }
    return ret;
}
