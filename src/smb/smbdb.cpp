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

#include "nes/nestopiaimpl.h"
#include "nes/ppux.h"
#include "smb/smbdb.h"
#include "util/file.h"
#include "ext/sqliteext/sqliteext.h"
#include "ext/opencvext/opencvext.h"

using namespace argos;
using namespace argos::smb;

AreaID argos::smb::column_area_id(sqlite3_stmt* stmt, int column)
{
    int v = sqlite3_column_int(stmt, column);
    if (v < 0 || v > std::numeric_limits<uint16_t>::max()) {
        throw std::runtime_error("out of bounds data in db for sqliteext::column_area_id");
    }
    return static_cast<AreaID>(v);
}

void argos::smb::column_minimap(sqlite3_stmt* stmt, int column, smb::MinimapImage* minimap)
{
    int sz = sqlite3_column_bytes(stmt, column);
    if (sz != smb::MINIMAP_NUM_BYTES) {
        throw std::runtime_error("not a minimap?");
    }
    const uint8_t* dat = reinterpret_cast<const uint8_t*>(sqlite3_column_blob(stmt, column));
    for (int i = 0; i < smb::MINIMAP_NUM_BYTES; i++) {
        (*minimap)[i] = dat[i];
    }
}

const uint8_t* argos::smb::rom_chr0(nes::NESDatabase::RomSPtr rom)
{
    return rom->data() + 0x8010;
}

const uint8_t* argos::smb::rom_chr1(nes::NESDatabase::RomSPtr rom)
{
    return rom->data() + 0x9010;
}

SMBDatabase::SMBDatabase(const std::string& path)
    : nes::NESDatabase(path)
    , m_NametableCache(nullptr)
{
    ExecOrThrow(SMBDatabase::SoundEffectSchema());
    ExecOrThrow(SMBDatabase::MusicTrackSchema());
    ExecOrThrow(SMBDatabase::NametablePageSchema());
    ExecOrThrow(SMBDatabase::MinimapPageSchema());
    ExecOrThrow(SMBDatabase::NTExtractRecordSchema());
    ExecOrThrow(SMBDatabase::RouteSchema());
    ExecOrThrow(SMBDatabase::RouteSectionSchema());
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
    id              INTEGER PRIMARY KEY,
    area_id         INTEGER NOT NULL,
    page            INTEGER NOT NULL,
    frame_palette   BLOB NOT NULL,
    nametable       BLOB NOT NULL
);)";
}

const char* SMBDatabase::MinimapPageSchema()
{
    return R"(CREATE TABLE IF NOT EXISTS minimap_page (
    id              INTEGER PRIMARY KEY,
    area_id         INTEGER NOT NULL,
    page            INTEGER NOT NULL,
    minimap         BLOB NOT NULL
);)";
}

const char* SMBDatabase::NTExtractRecordSchema()
{
    return R"(CREATE TABLE IF NOT EXISTS nt_extract_record (
    id                      INTEGER PRIMARY KEY,
    nes_tas_id              INTEGER REFERENCES nes_tas(id) ON DELETE RESTRICT NOT NULL,
    frame                   INTEGER NOT NULL,
    area_id                 INTEGER NOT NULL,
    page                    INTEGER NOT NULL,
    nt_index                INTEGER NOT NULL
);)";
}

const char* SMBDatabase::RouteSchema()
{
    return R"(CREATE TABLE IF NOT EXISTS route (
    id                      INTEGER PRIMARY KEY,
    name                    TEXT NOT NULL UNIQUE
);)";
}

const char* SMBDatabase::RouteSectionSchema()
{
    return R"(CREATE TABLE IF NOT EXISTS route_section (
    id                      INTEGER PRIMARY KEY,
    route_id                INTEGER REFERENCES route(id) ON DELETE RESTRICT NOT NULL,
    area_id                 INTEGER NOT NULL,
    world                   INTEGER NOT NULL,
    level                   INTEGER NOT NULL,
    left                    INTEGER NOT NULL,
    right                   INTEGER NOT NULL,
    xloc                    INTEGER NOT NULL
);)";
}

bool SMBDatabase::IsInit()
{
    if (!GetBaseRom()) {
        return false;
    }

    return true;
}

argos::nes::NESDatabase::RomSPtr SMBDatabase::GetBaseRom()
{
    return GetRomCached(1);
}

argos::smb::SMBNametableCachePtr SMBDatabase::GetNametableCache()
{
    if (!m_NametableCache) {
        m_NametableCache = std::make_shared<SMBNametableCache>(this);
    }
    return m_NametableCache;
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

bool smb::InsertNametablePage(SMBDatabase* database, const db::nametable_page& nt)
{
    sqlite3_stmt* stmt;
    sqliteext::PrepareOrThrow(database->m_Database,
            "INSERT INTO nametable_page (area_id, page, frame_palette, nametable) VALUES (?, ?, ?, ?);",
            &stmt);

    sqliteext::BindIntOrThrow(stmt, 1, static_cast<int>(nt.area_id));
    sqliteext::BindIntOrThrow(stmt, 2, nt.page);
    sqliteext::BindBlbOrThrow(stmt, 3, nt.frame_palette.data(), nt.frame_palette.size());
    sqliteext::BindBlbOrThrow(stmt, 4, nt.nametable.data(), nt.nametable.size());
    sqliteext::StepAndFinalizeOrThrow(stmt);
    return true;
}

bool SMBDatabase::GetAllNametablePages(std::vector<db::nametable_page>* nt_pages)
{
    if (!nt_pages) return false;

    sqlite3_stmt* stmt;
    sqliteext::PrepareOrThrow(m_Database, R"(
        SELECT * FROM nametable_page;
    )", &stmt);

    nt_pages->clear();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        db::nametable_page page;
        page.id = sqlite3_column_int(stmt, 0);
        page.area_id = column_area_id(stmt, 1);
        page.page = sqlite3_column_int(stmt, 2);
        nes::column_frame_palette(stmt, 3, &page.frame_palette);
        nes::column_nametable(stmt, 4, &page.nametable);
        nt_pages->push_back(page);
    }
    sqlite3_finalize(stmt);
    return true;
}

bool SMBDatabase::GetAllMinimapPages(std::vector<db::minimap_page>* mini_pages)
{
    if (!mini_pages) return false;

    sqlite3_stmt* stmt;
    sqliteext::PrepareOrThrow(m_Database, R"(
        SELECT * FROM minimap_page;
    )", &stmt);

    mini_pages->clear();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        db::minimap_page page;
        page.id = sqlite3_column_int(stmt, 0);
        page.area_id = column_area_id(stmt, 1);
        page.page = sqlite3_column_int(stmt, 2);
        smb::column_minimap(stmt, 3, &page.minimap);
        mini_pages->push_back(page);
    }
    sqlite3_finalize(stmt);
    return true;
}

bool SMBDatabase::GetMinimapPage(AreaID area_id, int page, db::minimap_page* mini_page)
{
    if (!mini_page) return false;
    throw std::runtime_error("not implemented");
    return false;
}

bool SMBDatabase::GetAllNTExtractRecords(int nes_tas_id, std::vector<db::nt_extract_record>* records)
{
    if (!records) {
        return false;
    }
    sqlite3_stmt* stmt;
    sqliteext::PrepareOrThrow(m_Database, R"(
        SELECT * FROM nt_extract_record WHERE nes_tas_id = ? ORDER BY frame ASC;
    )", &stmt);
    sqliteext::BindIntOrThrow(stmt, 1, nes_tas_id);

    records->clear();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        records->emplace_back();
        records->back().id = sqlite3_column_int(stmt, 0);
        records->back().nes_tas_id = sqlite3_column_int(stmt, 1);
        records->back().frame = sqlite3_column_int(stmt, 2);
        records->back().area_id = smb::column_area_id(stmt, 3);
        records->back().page = sqlite3_column_int(stmt, 4);
        records->back().nt_index = sqlite3_column_int(stmt, 5);
    }
    sqlite3_finalize(stmt);
    return true;
}

bool SMBDatabase::GetAllNTExtractTASIDs(std::vector<int>* ids)
{
    if (!ids) {
        return false;
    }
    sqlite3_stmt* stmt;
    sqliteext::PrepareOrThrow(m_Database, R"(
        SELECT DISTINCT nes_tas_id FROM nt_extract_record;
    )", &stmt);
    ids->clear();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int v = sqlite3_column_int(stmt, 0);
        ids->push_back(v);
    }
    sqlite3_finalize(stmt);
    return true;
}

bool SMBDatabase::GetRouteNames(std::vector<std::string>* names)
{
    if (!names) {
        return false;
    }

    sqlite3_stmt* stmt;
    sqliteext::PrepareOrThrow(m_Database, R"(
        SELECT name FROM route;
    )", &stmt);
    names->clear();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        names->push_back(sqliteext::column_str(stmt, 0));
    }
    sqlite3_finalize(stmt);
    return true;
}

bool SMBDatabase::GetRoute(const std::string& name, db::route* route)
{
    bool ret = false;
    sqlite3_stmt* stmt;
    sqliteext::PrepareOrThrow(m_Database, R"(
        SELECT id FROM route WHERE name = ?;
    )", &stmt);
    sqliteext::BindStrOrThrow(stmt, 1, name);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (route) {
            route->id = sqlite3_column_int(stmt, 0);
            route->name = name;
        }
        ret = true;
    }
    sqlite3_finalize(stmt);

    if (route) {
        route->route.clear();
    }

    if (route && ret) {
        sqliteext::PrepareOrThrow(m_Database, R"(
            SELECT area_id, world, level, left, right, xloc FROM route_section
            WHERE route_id = ?;
        )", &stmt);
        sqliteext::BindIntOrThrow(stmt, 1, route->id);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            route->route.emplace_back();
            auto& sec = route->route.back();

            sec.AID = smb::column_area_id(stmt, 0);
            sec.World = sqlite3_column_int(stmt, 1);
            sec.Level = sqlite3_column_int(stmt, 2);
            sec.Left = sqlite3_column_int(stmt, 3);
            sec.Right = sqlite3_column_int(stmt, 4);
            sec.XLoc = sqlite3_column_int(stmt, 5);
        }
        sqlite3_finalize(stmt);

        std::sort(route->route.begin(), route->route.end(),
        [&](const WorldSection& l, const WorldSection& r){
            return l.XLoc < r.XLoc;
        });
        for (size_t i = 0; i < route->route.size(); i++) {
            route->route[i].SectionIndex = i;
        }
    }

    return ret;
}

////////////////////////////////////////////////////////////////////////////////

SMBNametableCache::SMBNametableCache(SMBDatabase* database)
    : INametableCache(rom_chr1(database->GetBaseRom()))
{
    std::vector<db::nametable_page> nametables;
    database->GetAllNametablePages(&nametables);
    for (auto & nametable : nametables) {
        auto& v = m_nametables[nametable.area_id];
        if (v.size() <= nametable.page) {
            v.resize(nametable.page + 1);
        }
        v[nametable.page] = nametable;
    }

    std::vector<db::minimap_page> minimaps;
    database->GetAllMinimapPages(&minimaps);
    for (auto & minimap : minimaps) {
        auto& v = m_minimaps[minimap.area_id];
        if (v.size() <= minimap.page) {
            v.resize(minimap.page + 1);
        }
        v[minimap.page] = minimap;
    }
}

SMBNametableCache::~SMBNametableCache()
{
}

bool SMBNametableCache::KnownNametable(AreaID id, int page) const
{
    auto it = m_nametables.find(id);
    if (it == m_nametables.end()) {
        return false;
    }
    if (static_cast<size_t>(page) >= it->second.size()) {
        return false;
    }
    return true;
}

const db::nametable_page& SMBNametableCache::GetNametable(AreaID id, int page) const
{
    auto it = m_nametables.find(id);
    if (it == m_nametables.end()) {
        throw std::runtime_error(fmt::format("no nametable? {:04x} {:d}",
                    static_cast<int>(id), page));
    }

    if (static_cast<size_t>(page) >= it->second.size()) {
        throw std::runtime_error(fmt::format("no nametable? {:04x} {:d}",
                    static_cast<int>(id), page));
    }

    return it->second[page];
}

const db::nametable_page* SMBNametableCache::MaybeGetNametable(AreaID id, int page) const
{
    if (!KnownNametable(id, page)) {
        return nullptr;
    }
    return &GetNametable(id, page);
}

bool SMBNametableCache::KnownMinimap(AreaID id, int page) const
{
    auto it = m_minimaps.find(id);
    if (it == m_minimaps.end()) {
        return false;
    }
    if (static_cast<size_t>(page) >= it->second.size()) {
        return false;
    }
    return true;
}

const db::minimap_page& SMBNametableCache::GetMinimap(AreaID id, int page) const
{
    auto it = m_minimaps.find(id);
    if (it == m_minimaps.end()) {
        throw std::runtime_error(fmt::format("no minimap? {:04x} {:d}",
                    static_cast<int>(id), page));
    }

    if (static_cast<size_t>(page) >= it->second.size()) {
        throw std::runtime_error(fmt::format("no minimap? {:04x} {:d}",
                    static_cast<int>(id), page));
    }

    return it->second[page];
}

const db::minimap_page* SMBNametableCache::MaybeGetMinimap(AreaID id, int page) const
{
    if (!KnownMinimap(id, page)) {
        return nullptr;
    }
    return &GetMinimap(id, page);
}

const nes::NameTable* SMBNametableCache::Nametable(AreaID id, int page) const
{
    auto* p = MaybeGetNametable(id, page);
    if (p) {
        return &p->nametable;
    }
    return nullptr;
}

const MinimapImage* SMBNametableCache::Minimap(AreaID id, int page) const
{
    auto* p = MaybeGetMinimap(id, page);
    if (p) {
        return &p->minimap;
    }
    return nullptr;
}

////////////////////////////////////////////////////////////////////////////////

static bool DoInitializeNametablePages(SMBDatabase* database)
{
    sqliteext::SQLiteExtTransaction transaction(database);

    auto rom = database->GetBaseRom();
    if (!rom) {
        return false;
    }
    // TODO
    // const uint8_t* chr0 = rom->data() + 0x8010;
    // const uint8_t* chr1 = rom->data() + 0x9010;

    std::vector<int> tas_ids;
    if (!database->GetAllNTExtractTASIDs(&tas_ids)) {
        return false;
    }

    for (auto & tas_id : tas_ids) {
        nes::db::nes_tas tas;
        if (!database->SelectTAS(tas_id, &tas)) {
            std::cerr << "failed selected tas " << tas_id << "?" << std::endl;
            return false;
        }
        std::cout << "execute tas: " << tas.name << " [" << tas.inputs.size() << " frames]" << std::endl;

        nes::NestopiaNESEmulator emu;
        emu.LoadINESData(rom->data(), rom->size());

        std::vector<db::nt_extract_record> records;
        if (!database->GetAllNTExtractRecords(tas_id, &records)) {
            std::cerr << "failed getting nt extract records?" << std::endl;
            return false;
        }
        std::sort(records.begin(), records.end(), [&]
                (const db::nt_extract_record& a, const db::nt_extract_record& b){
            return a.frame < b.frame;
        });
        size_t records_index = 0;
        for (auto & input : tas.inputs) {
            if (records_index >= records.size()) {
                break;
            }
            emu.Execute(input);
            while (emu.CurrentFrame() ==
                    static_cast<uint64_t>(records[records_index].frame)) {
                const auto& record = records[records_index];


                smb::db::nametable_page my_page;

                my_page.area_id = smb::AreaIDFromRAM(emu.CPUPeek(smb::RamAddress::AREA_DATA_LOW),
                                                     emu.CPUPeek(smb::RamAddress::AREA_DATA_HIGH));
                my_page.page = record.page;

                emu.PPUPeekNameTable(record.nt_index, &my_page.nametable);

                // Wipe the top 4 rows (SMB specific)
                int i = 0;
                for (int row = 0; row < 4; row++) {
                    for (int col = 0; col < nes::NAMETABLE_WIDTH_BYTES; col++) {
                        my_page.nametable[i] = 36;
                        i++;
                    }
                }

                emu.PPUPeekFramePalette(&my_page.frame_palette);

                InsertNametablePage(database, my_page);

                //nes::EffectInfo info = nes::EffectInfo::Defaults();
                //nes::PPUx ppux(nes::FRAME_WIDTH, nes::FRAME_HEIGHT,
                //        nes::PPUxPriorityStatus::DISABLED);
                //ppux.RenderNametable(0, 0, nes::NAMETABLE_WIDTH_BYTES, nes::NAMETABLE_HEIGHT_BYTES,
                //        my_page.nametable.data(),
                //        my_page.nametable.data() + nes::NAMETABLE_ATTRIBUTE_OFFSET,
                //        chr1, my_page.frame_palette.data(), nes::DefaultPaletteBGR().data(),
                //        1, info);

                //cv::Mat m2(ppux.GetHeight(), ppux.GetWidth(), CV_8UC3, ppux.GetBGROut());
                //cv::imwrite(fmt::format("nt_{}_{}.png",
                //            static_cast<int>(my_page.area_id),
                //            static_cast<int>(my_page.page)), m2);

                //nes::Frame f;
                //emu.ScreenPeekFrame(&f);
                //cv::Mat m = opencvext::ConstructPaletteImage(
                //        f.data(), nes::FRAME_WIDTH, nes::FRAME_HEIGHT,
                //        nes::DefaultPaletteBGR().data(),
                //        opencvext::PaletteDataOrder::BGR);
                //cv::imwrite(fmt::format("{}_{}_{}.png", tas_id, emu.CurrentFrame(), record.id), m);
                records_index++;
            }
        }
        std::cout << "  " << records_index << " nametables extracted" << std::endl;
    }
    return true;
}

bool argos::smb::InitializeSMBDatabase(SMBDatabase* database,
        const std::string& smb_data_path,
        const std::vector<uint8_t>& smb_rom)
{
    std::cout << "insert base rom" << std::endl;
    int smb_id = database->InsertROM("SMB.nes", smb_rom);
    if (smb_id != 1) {
        std::cerr << "database not cleared?\n";
        return false;
    }

    util::fs::path base(smb_data_path);

    for (auto & sound_effect : smb::AudibleSoundEffects()) {
        if (sound_effect == smb::SoundEffect::DEATH_SOUND) {
            continue;
        }
        std::string path = base / util::fs::path(fmt::format("SMB_SOUND_{:06x}.flac",
                static_cast<uint32_t>(sound_effect)));
        std::cout << "insert sound effect: " << smb::ToString(sound_effect) << " " << path << std::endl;
        if (!InsertSoundEffect(database, sound_effect, path)) {
            std::cerr << "Failed adding sound effect?\n";
            return false;
        }
    }

    for (auto & music_track : smb::AudibleMusicTracks()) {
        std::string path = base / util::fs::path(fmt::format("SMB_MUSIC_{:06x}.flac",
                static_cast<uint32_t>(music_track)));
        std::cout << "insert music track: " << smb::ToString(music_track) << " " << path << std::endl;
        if (!InsertMusicTrack(database, music_track, path)) {
            std::cerr << "Failed adding music track?\n";
            return false;
        }
        if (music_track == smb::MusicTrack::DEATH_MUSIC) {
            auto sound_effect = smb::SoundEffect::DEATH_SOUND;
            std::cout << "insert sound effect: " << smb::ToString(sound_effect) << " " << path << std::endl;
            if (!InsertSoundEffect(database, sound_effect, path)) {
                std::cerr << "Failed adding sound effect?\n";
                return false;
            }
        }
    }

    auto ExecAndLog = [&](const char* file){
        std::string sql_path = base / util::fs::path(file);
        std::cout << "execute sql file: " << sql_path << std::endl;
        database->ExecFileOrThrow(sql_path);
    };

    ExecAndLog("nt_extract_tas.sql");
    ExecAndLog("nt_extract_record.sql");

    if (!DoInitializeNametablePages(database)) {
        std::cerr << "Failed initializing nametable pages\n";
        return false;
    }

    ExecAndLog("minimap.sql");
    ExecAndLog("pattern_tables.sql");
    ExecAndLog("routes.sql");

    return true;
}
