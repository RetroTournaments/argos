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

#ifndef ARGOS_SMB_SMBDB_HEADER
#define ARGOS_SMB_SMBDB_HEADER

#include "smb/smb.h"
#include "nes/nesdb.h"

namespace argos::smb
{

AreaID column_area_id(sqlite3_stmt* stmt, int column);
void column_minimap(sqlite3_stmt* stmt, int column, smb::MinimapImage* img);
const uint8_t* rom_chr0(nes::NESDatabase::RomSPtr rom);
const uint8_t* rom_chr1(nes::NESDatabase::RomSPtr rom);

namespace db
{

struct sound_effect
{
    SoundEffect effect;
    std::vector<uint8_t> wav_data;
};

struct music_track
{
    MusicTrack track;
    std::vector<uint8_t> wav_data;
};

struct nametable_page
{
    int id;
    AreaID area_id;
    int page;
    nes::FramePalette frame_palette;
    nes::NameTable nametable;
};

struct minimap_page
{
    int id;
    AreaID area_id;
    int page;
    MinimapImage minimap;
};

struct nt_extract_record
{
    int id;
    int nes_tas_id;
    int frame;
    AreaID area_id;
    int page;
    int nt_index;
};

struct route
{
    int id;
    std::string name;
    Route route;
};

}

class SMBNametableCache;
typedef std::shared_ptr<SMBNametableCache> SMBNametableCachePtr;

class SMBDatabase : public nes::NESDatabase
{
public:
    SMBDatabase(const std::string& path);
    ~SMBDatabase();

    bool IsInit();

    RomSPtr GetBaseRom();
    SMBNametableCachePtr GetNametableCache();

    bool GetSoundEffectWav(SoundEffect effect, std::vector<uint8_t>* data);
    bool GetMusicTrackWav(MusicTrack track, std::vector<uint8_t>* data);
    bool GetMinimapPage(AreaID area_id, int page, db::minimap_page* mini_page);
    bool GetAllNametablePages(std::vector<db::nametable_page>* pages);
    bool GetAllMinimapPages(std::vector<db::minimap_page>* pages);
    bool GetAllNTExtractTASIDs(std::vector<int>* ids);
    bool GetAllNTExtractRecords(int nes_tas_id, std::vector<db::nt_extract_record>* records);
    bool GetRouteNames(std::vector<std::string>* names);
    bool GetRoute(const std::string& name, db::route* route);

    static const char* SoundEffectSchema();
    static const char* MusicTrackSchema();
    static const char* NametablePageSchema();
    static const char* MinimapPageSchema();
    static const char* NTExtractRecordSchema();
    static const char* RouteSchema();
    static const char* RouteSectionSchema();

private:
    SMBNametableCachePtr m_NametableCache;
};

class SMBNametableCache : public INametableCache
{
public:
    SMBNametableCache(SMBDatabase* database);
    ~SMBNametableCache();

    bool KnownNametable(AreaID id, int page) const;
    const nes::NameTable* Nametable(AreaID id, int page) const final;
    const db::nametable_page& GetNametable(AreaID id, int page) const;
    const db::nametable_page* MaybeGetNametable(AreaID id, int page) const;

    bool KnownMinimap(AreaID id, int page) const;
    const MinimapImage* Minimap(AreaID id, int page) const final;
    const db::minimap_page& GetMinimap(AreaID id, int page) const;
    const db::minimap_page* MaybeGetMinimap(AreaID id, int page) const;

private:
    std::unordered_map<AreaID, std::vector<db::nametable_page>> m_nametables;
    std::unordered_map<AreaID, std::vector<db::minimap_page>> m_minimaps;
};

//
bool InitializeSMBDatabase(SMBDatabase* database, const std::string& smb_data_path,
        const std::vector<uint8_t>& smb_rom);

//
bool InsertSoundEffect(SMBDatabase* database, SoundEffect effect, const std::string& wavpath);
bool InsertMusicTrack(SMBDatabase* database, MusicTrack track, const std::string& wavpath);
bool InsertNametablePage(SMBDatabase* database, const db::nametable_page& nt);

}

#endif
