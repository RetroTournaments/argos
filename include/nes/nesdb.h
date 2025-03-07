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

#ifndef STATIC_NES_NESDB_HEADER
#define STATIC_NES_NESDB_HEADER

#include "game/gamedb.h"
#include "nes/nes.h"

namespace sta::nes
{

void column_inputs(sqlite3_stmt* stmt, int column, std::vector<nes::ControllerState>* inputs);
void column_frame_palette(sqlite3_stmt* stmt, int column, nes::FramePalette* palette);
void column_nametable(sqlite3_stmt* stmt, int column, nes::NameTable* nametable);
void column_pattern_table(sqlite3_stmt* stmt, int column, nes::PatternTable* pattern_table);

namespace db
{

struct nes_rom
{
    int                         id;
    std::string                 name;
    std::vector<uint8_t>        rom;
    std::array<uint8_t, 16>     header;
};

struct nes_tas
{
    int                                 id;
    int                                 rom_id;
    std::string                         name;
    std::string                         start_string;
    std::vector<nes::ControllerState>   inputs;
};

struct nes_pattern_table
{
    int                 id;
    std::string         name;
    nes::PatternTable   pattern_table;
};

}

class NESDatabase : public game::GameDatabase
{
public:
    NESDatabase(const std::string& path);
    ~NESDatabase();

    // Recall GameDB key value store and SQLiteExtDB ExecOrThrow

    // NES games have some ROMs.
    int InsertROM(const std::string& name, const std::vector<uint8_t>& rom);
    void SelectAllROMs(std::vector<db::nes_rom>* roms);
    void DeleteROM(int id);

    typedef std::shared_ptr<const std::vector<uint8_t>> RomSPtr;
    RomSPtr GetRomCached(int rom_id);
    bool GetRomByName(const std::string& name, std::vector<uint8_t>* rom);

    // And TASes
    void SelectAllTasesLight(std::vector<db::nes_tas>* tases);
    bool SelectTAS(int id, db::nes_tas* tas,
            std::vector<nes::ControllerState>* inputs = nullptr);
    void DeleteTAS(int id);
    void UpdateTASName(int id, const std::string& name);
    int InsertNewTAS(int rom_id, const std::string& name);
    int InsertNewTAS(int rom_id, const std::string& name, const std::vector<nes::ControllerState>& inputs);

    // And pattern tables
    bool GetPatternTableByName(const std::string& name,
            nes::PatternTable* pattern_table);


    static const char* ROMSchema();
    static const char* TASSchema();
    static const char* PatternTableSchema();

private:
    std::unordered_map<int, RomSPtr> m_CachedRoms;
};

}

#endif
