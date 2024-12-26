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

#include "smb/smb.h"
#include "fmt/fmt.h"

using namespace argos;
using namespace argos::smb;

AreaID argos::smb::AreaIDFromRAM(uint8_t area_data_low, uint8_t area_data_high)
{
    uint16_t area_id = static_cast<uint16_t>(area_data_high);
    area_id <<= 8;
    area_id += static_cast<uint16_t>(area_data_low);
    return static_cast<AreaID>(area_id);
}

const std::vector<AreaID>& argos::smb::KnownAreaIDs()
{
    static std::vector<AreaID> s_AreaIDs = {
        AreaID::WATER_AREA_1,
        AreaID::WATER_AREA_2,
        AreaID::WATER_AREA_3,
        AreaID::GROUND_AREA_1,
        AreaID::GROUND_AREA_2,
        AreaID::GROUND_AREA_3,
        AreaID::GROUND_AREA_4,
        AreaID::GROUND_AREA_5,
        AreaID::GROUND_AREA_6,
        AreaID::GROUND_AREA_7,
        AreaID::GROUND_AREA_8,
        AreaID::GROUND_AREA_9,
        AreaID::GROUND_AREA_10,
        AreaID::GROUND_AREA_11,
        AreaID::GROUND_AREA_12,
        AreaID::GROUND_AREA_13,
        AreaID::GROUND_AREA_14,
        AreaID::GROUND_AREA_15,
        AreaID::GROUND_AREA_16,
        AreaID::GROUND_AREA_17,
        AreaID::GROUND_AREA_18,
        AreaID::GROUND_AREA_19,
        AreaID::GROUND_AREA_20,
        AreaID::GROUND_AREA_21,
        AreaID::GROUND_AREA_22,
        AreaID::UNDERGROUND_AREA_1,
        AreaID::UNDERGROUND_AREA_2,
        AreaID::UNDERGROUND_AREA_3,
        AreaID::CASTLE_AREA_1,
        AreaID::CASTLE_AREA_2,
        AreaID::CASTLE_AREA_3,
        AreaID::CASTLE_AREA_4,
        AreaID::CASTLE_AREA_5,
        AreaID::CASTLE_AREA_6,
    };
    return s_AreaIDs;
}

std::string argos::smb::ToString(smb::AreaID area_id)
{
    static std::unordered_map<smb::AreaID, std::string> s_AreaStrings = {
        {smb::AreaID::WATER_AREA_1        , "WATER_AREA_1        0xAE08"},
        {smb::AreaID::WATER_AREA_2        , "WATER_AREA_2        0xAE47"},
        {smb::AreaID::WATER_AREA_3        , "WATER_AREA_3        0xAEC2"},
        {smb::AreaID::GROUND_AREA_1       , "GROUND_AREA_1       0xA46D"},
        {smb::AreaID::GROUND_AREA_2       , "GROUND_AREA_2       0xA4D0"},
        {smb::AreaID::GROUND_AREA_3       , "GROUND_AREA_3       0xA539"},
        {smb::AreaID::GROUND_AREA_4       , "GROUND_AREA_4       0xA58C"},
        {smb::AreaID::GROUND_AREA_5       , "GROUND_AREA_5       0xA61B"},
        {smb::AreaID::GROUND_AREA_6       , "GROUND_AREA_6       0xA690"},
        {smb::AreaID::GROUND_AREA_7       , "GROUND_AREA_7       0xA6F5"},
        {smb::AreaID::GROUND_AREA_8       , "GROUND_AREA_8       0xA74A"},
        {smb::AreaID::GROUND_AREA_9       , "GROUND_AREA_9       0xA7CF"},
        {smb::AreaID::GROUND_AREA_10      , "GROUND_AREA_10      0xA834"},
        {smb::AreaID::GROUND_AREA_11      , "GROUND_AREA_11      0xA83D"},
        {smb::AreaID::GROUND_AREA_12      , "GROUND_AREA_12      0xA87C"},
        {smb::AreaID::GROUND_AREA_13      , "GROUND_AREA_13      0xA891"},
        {smb::AreaID::GROUND_AREA_14      , "GROUND_AREA_14      0xA8F8"},
        {smb::AreaID::GROUND_AREA_15      , "GROUND_AREA_15      0xA95D"},
        {smb::AreaID::GROUND_AREA_16      , "GROUND_AREA_16      0xA9D0"},
        {smb::AreaID::GROUND_AREA_17      , "GROUND_AREA_17      0xAA01"},
        {smb::AreaID::GROUND_AREA_18      , "GROUND_AREA_18      0xAA94"},
        {smb::AreaID::GROUND_AREA_19      , "GROUND_AREA_19      0xAB07"},
        {smb::AreaID::GROUND_AREA_20      , "GROUND_AREA_20      0xAB80"},
        {smb::AreaID::GROUND_AREA_21      , "GROUND_AREA_21      0xABD9"},
        {smb::AreaID::GROUND_AREA_22      , "GROUND_AREA_22      0xAC04"},
        {smb::AreaID::UNDERGROUND_AREA_1  , "UNDERGROUND_AREA_1  0xAC37"},
        {smb::AreaID::UNDERGROUND_AREA_2  , "UNDERGROUND_AREA_2  0xACDA"},
        {smb::AreaID::UNDERGROUND_AREA_3  , "UNDERGROUND_AREA_3  0xAD7B"},
        {smb::AreaID::CASTLE_AREA_1       , "CASTLE_AREA_1       0xA1B1"},
        {smb::AreaID::CASTLE_AREA_2       , "CASTLE_AREA_2       0xA212"},
        {smb::AreaID::CASTLE_AREA_3       , "CASTLE_AREA_3       0xA291"},
        {smb::AreaID::CASTLE_AREA_4       , "CASTLE_AREA_4       0xA304"},
        {smb::AreaID::CASTLE_AREA_5       , "CASTLE_AREA_5       0xA371"},
        {smb::AreaID::CASTLE_AREA_6       , "CASTLE_AREA_6       0xA3FC"},
    };
    auto it = s_AreaStrings.find(area_id);
    if (it == s_AreaStrings.end()) {
        return fmt::format("UNKNOWN             {:04x}", static_cast<uint16_t>(area_id));
    }
    return it->second;
}

bool argos::smb::AreaIDEnd(AreaID area_id, int* end) {
    static std::unordered_map<uint16_t, int> s_KnownEnds = {
        {0xAE47, 3072},
        {0xA46D, 2608},
        {0xA4D0, 3664},
        {0xA539, 3808},
        {0xA58C, 3664},
        {0xA61B, 3408},
        {0xA690, 3376},
        {0xA6F5, 2624},
        {0xA74A, 3792},
        {0xA7CF, 3408},
        {0xA83D, 3392},
        {0xA891, 2544},
        {0xA8F8, 2864},
        {0xA95D, 3216},
        {0xA9D0, 1024},
        {0xAA01, 6224},
        {0xAA94, 3408},
        {0xAB07, 3664},
        {0xAB80, 3072},
        {0xAC04, 3552},
        {0xACDA, 3136},
        {0xAC37, 3056},
        {0xAEC2, 1136},
        {0xA1B1, 2560},
        {0xA212, 3072},
        {0xA291, 2560},
        {0xA304, 2560},
        {0xA371, 3584},
    };
    auto it = s_KnownEnds.find(static_cast<uint16_t>(area_id));
    if (it != s_KnownEnds.end()) {
        if (end) {
            *end = it->second;
        }
        return true;
    }
    return false;
}

const std::vector<SoundEffect>& argos::smb::AudibleSoundEffects()
{
    static std::vector<SoundEffect> s_Effects = {
        SoundEffect::SMALL_JUMP,
        SoundEffect::FLAGPOLE,
        SoundEffect::FIREBALL,
        SoundEffect::PIPEDOWN_INJURY,
        SoundEffect::ENEMY_SMACK,
        SoundEffect::ENEMY_STOMP_SWIM,
        SoundEffect::BUMP,
        SoundEffect::BIG_JUMP,
        SoundEffect::BOWSER_FALL,
        SoundEffect::EXTRA_LIFE,
        SoundEffect::GRAB_POWER_UP,
        SoundEffect::TIMERTICK,
        SoundEffect::BLAST,
        SoundEffect::GROW_VINE,
        SoundEffect::GROW_POWER_UP,
        SoundEffect::GRAB_COIN,
        SoundEffect::BOWSER_FLAME,
        SoundEffect::BRICK_SHATTER,
    };
    return s_Effects;
}

std::string argos::smb::ToString(SoundEffect effect)
{
    switch (effect) {
        case(SoundEffect::SILENCE): return "SoundEffect::SILENCE";
        case(SoundEffect::SMALL_JUMP): return "SoundEffect::SMALL_JUMP";
        case(SoundEffect::FLAGPOLE): return "SoundEffect::FLAGPOLE";
        case(SoundEffect::FIREBALL): return "SoundEffect::FIREBALL";
        case(SoundEffect::PIPEDOWN_INJURY): return "SoundEffect::PIPEDOWN_INJURY";
        case(SoundEffect::ENEMY_SMACK): return "SoundEffect::ENEMY_SMACK";
        case(SoundEffect::ENEMY_STOMP_SWIM): return "SoundEffect::ENEMY_STOMP_SWIM";
        case(SoundEffect::BUMP): return "SoundEffect::BUMP";
        case(SoundEffect::BIG_JUMP): return "SoundEffect::BIG_JUMP";
        case(SoundEffect::BOWSER_FALL): return "SoundEffect::BOWSER_FALL";
        case(SoundEffect::EXTRA_LIFE): return "SoundEffect::EXTRA_LIFE";
        case(SoundEffect::GRAB_POWER_UP): return "SoundEffect::GRAB_POWER_UP";
        case(SoundEffect::TIMERTICK): return "SoundEffect::TIMERTICK";
        case(SoundEffect::BLAST): return "SoundEffect::BLAST";
        case(SoundEffect::GROW_VINE): return "SoundEffect::GROW_VINE";
        case(SoundEffect::GROW_POWER_UP): return "SoundEffect::GROW_POWER_UP";
        case(SoundEffect::GRAB_COIN): return "SoundEffect::GRAB_COIN";
        case(SoundEffect::BOWSER_FLAME): return "SoundEffect::BOWSER_FLAME";
        case(SoundEffect::BRICK_SHATTER): return "SoundEffect::BRICK_SHATTER";
        default: break;
    };

    return "SoundEffect::UNKNOWN";
}


const std::vector<MusicTrack>& argos::smb::AudibleMusicTracks()
{
    static std::vector<MusicTrack> s_Tracks = {
        MusicTrack::STAR_POWER,
        MusicTrack::PIPE_INTRO,
        MusicTrack::CLOUD,
        MusicTrack::CASTLE,
        MusicTrack::UNDERGROUND,
        MusicTrack::WATER,
        MusicTrack::GROUND,
        MusicTrack::TIME_RUNNING_OUT,
        MusicTrack::END_OF_LEVEL,
        MusicTrack::ALT_GAME_OVER,
        MusicTrack::END_OF_CASTLE,
        MusicTrack::VICTORY,
        MusicTrack::GAME_OVER,
        MusicTrack::DEATH_MUSIC,
    };

    return s_Tracks;
}

std::string argos::smb::ToString(MusicTrack track)
{
    switch(track) {
        case(MusicTrack::SILENCE): return "MusicTrack::SILENCE";
        case(MusicTrack::STAR_POWER): return "MusicTrack::STAR_POWER";
        case(MusicTrack::PIPE_INTRO): return "MusicTrack::PIPE_INTRO";
        case(MusicTrack::CLOUD): return "MusicTrack::CLOUD";
        case(MusicTrack::CASTLE): return "MusicTrack::CASTLE";
        case(MusicTrack::UNDERGROUND): return "MusicTrack::UNDERGROUND";
        case(MusicTrack::WATER): return "MusicTrack::WATER";
        case(MusicTrack::GROUND): return "MusicTrack::GROUND";
        case(MusicTrack::TIME_RUNNING_OUT): return "MusicTrack::TIME_RUNNING_OUT";
        case(MusicTrack::END_OF_LEVEL): return "MusicTrack::END_OF_LEVEL";
        case(MusicTrack::ALT_GAME_OVER): return "MusicTrack::ALT_GAME_OVER";
        case(MusicTrack::END_OF_CASTLE): return "MusicTrack::END_OF_CASTLE";
        case(MusicTrack::VICTORY): return "MusicTrack::VICTORY";
        case(MusicTrack::GAME_OVER): return "MusicTrack::GAME_OVER";
        case(MusicTrack::DEATH_MUSIC): return "MusicTrack::DEATH_MUSIC";
        default: break;
    };

    return "MusicTrack::UNKNOWN";
}

const MinimapPalette& argos::smb::DefaultMinimapPalette()
{
    static MinimapPalette p = {
        0x00, 0x3d, 0x2d, 0x1d
    };
    return p;
}

const MinimapPaletteBGR& argos::smb::DefaultMinimapPaletteBGR()
{
    static MinimapPaletteBGR b = {
        0xff, 0xff, 0xff,
        0xa9, 0xa9, 0xa9,
        0x3c, 0x3c, 0x3c,
        0x00, 0x00, 0x00
    };
    return b;
}

void argos::smb::RenderMinimapToPPUx(int x, int y, int sx, int ex,
        const MinimapImage& img, const MinimapPalette& miniPal, const nes::Palette& nesPal, nes::PPUx* ppux)
{
    if (!ppux) return;

    nes::EffectInfo effects = nes::EffectInfo::Defaults();
    effects.CropWithin = true;
    effects.Crop = util::Rect2I(x + sx, 0, ex - sx, 240);

    RenderMinimapToPPUx(x, y, effects, img, miniPal, nesPal, ppux);
}

void argos::smb::RenderMinimapToPPUx(int x, int y, const nes::EffectInfo& effects,
        const MinimapImage& img, const MinimapPalette& miniPal,
        const nes::Palette& nesPal, nes::PPUx* ppux)
{
    if (!ppux) return;
    int rem = 0;
    uint8_t v = 0x00;

    const uint8_t* in = img.data();
    for (int iny = 0; iny < nes::FRAME_HEIGHT; iny++) {
        for (int inx = 0; inx < nes::FRAME_WIDTH; inx++) {
            if (rem == 0) {
                v = *in;
                rem = 4;
                in++;
            }

            uint8_t q = v & 0b11;
            if (q) {
                ppux->RenderPaletteData(x + inx, y + iny, 1, 1,
                        &miniPal[q], nesPal.data(),
                        nes::PPUx::RPD_AS_NAMETABLE, effects);
            }

            v >>= 2;
            rem--;
        }
    }
}

void Route::GetVisibleSections(int xloc, int width, std::vector<WorldSection>* sections) const
{
    if (!sections || width <= 0) {
        return;
    }
    sections->clear();

    int rxloc = xloc + width;

    for (auto & sec : m_Route) {
        int rx = sec.XLoc + sec.Width();

        if (!(rx < xloc || sec.XLoc > rxloc)) {
            WorldSection vsec = sec;
            vsec.XLoc = sec.XLoc - xloc;
            vsec.SectionIndex = sections->size();
            sections->push_back(vsec);
        }
        if (sec.XLoc > rxloc) {
            break;
        }
    }
}

void Route::RenderMinimapTo(nes::PPUx* ppux, int categoryX,
        const MinimapPalette& minimapPalette,
        const INametableCache* nametables,
        std::vector<WorldSection>* visibleSections) const
{
    GetVisibleSections(categoryX, ppux->GetWidth(), visibleSections);

    int tlx = categoryX;
    int trx = tlx + ppux->GetWidth();
    int lx = 0;

    for (auto & sec : m_Route) {
        int qw = sec.Right - sec.Left - 1;
        int rx = lx + qw;

        if (!(rx < tlx || lx > trx)) {
            int ttx = lx - tlx;
            nametables->RenderTo(sec.AID,
                    sec.Left, qw, ppux, ttx, nes::DefaultPaletteBGR(), nullptr, &minimapPalette);
        }

        lx = rx + 16;
    }

}

bool Route::InCategory(AreaID id, int apx, int world, int level, int* categoryX, int* sectionIndex) const
{
    int lx = 0;
    int i = 0;
    for (auto & sec : m_Route) {
        int qw = sec.Right - sec.Left - 1;
        int rx = lx + qw;

        if (sec.AID == id) {
            bool skipWorldCheck = false;
            if (sec.AID == AreaID::UNDERGROUND_AREA_1 || sec.AID == AreaID::GROUND_AREA_16) {
                skipWorldCheck = true;
            }
            if (skipWorldCheck || (sec.World == world && sec.Level == level)) {
                if (apx >= sec.Left && apx < sec.Right) {
                    if (categoryX) *categoryX = lx + apx - sec.Left;
                    if (sectionIndex) *sectionIndex = i;
                    return true;
                }
            }
        }

        lx = rx + 16;
        i++;
    }
    return false;

}

bool argos::smb::IsMarioTile(uint8_t tileIndex)
{
    return (tileIndex >= 0x00 && tileIndex <= 0x4f ||
            tileIndex >= 0x58 && tileIndex <= 0x5a ||
            tileIndex >= 0x5c && tileIndex <= 0x5f ||
            tileIndex >= 0x90 && tileIndex <= 0x93 ||
            tileIndex == 0x9e || tileIndex == 0x9f);
}


void INametableCache::RenderTo(AreaID id, int apx, int width, nes::PPUx* ppux,
        int x, const nes::Palette& pal, const uint8_t* pt,
        const MinimapPalette* minimap, const uint8_t* fpal,
        const std::vector<SMBNametableDiff>* diffs) const
{
    if (apx < 0) {
        width += apx;
        x -= apx;
        apx = 0;
    }
    if (width <= 0) return;
    if ((!pt && !minimap) || !ppux) return;

    nes::RenderInfo render;
    render.OffX = 0;
    render.OffY = 0;
    render.Scale = 1;
    render.PatternTables.push_back(pt);
    render.PaletteBGR = pal.data();

    nes::EffectInfo effects = nes::EffectInfo::Defaults();
    effects.CropWithin = true;
    effects.Crop = util::Rect2I(x, 0, width, 240);

    int page = apx / 256;
    int xoff = apx % 256;

    int sx = x - xoff;
    int wr = -xoff;

    while (wr < width) {
        auto* nt = Nametable(id, page);
        if (nt) {
            if (minimap) {
                auto* mp = Minimap(id, page);
                if (mp) {
                    RenderMinimapToPPUx(sx, 0, effects,
                            *mp, *minimap, pal, ppux);
                }
            } else {
                nes::Nametablex ntx;
                ntx.X = sx;
                ntx.Y = 0;
                ntx.NametableP = nt;
                if (fpal) {
                    for (int i = 0; i < nes::FRAMEPALETTE_SIZE; i++) {
                        ntx.FramePalette[i] = fpal[i];
                    }
                } else {
                    throw std::runtime_error("not supported anymore");
                }
                ntx.PatternTableIndex = 0;

                if (diffs) {
                    for (auto & diff : *diffs) {
                        if (diff.NametablePage == page) {
                            if (ntx.NametableP != &BufferTable) {
                                ntx.NametableP = &BufferTable;
                                BufferTable = *nt;
                            }

                            BufferTable[diff.Offset] = diff.Value;
                        }
                    }
                }

                ppux->RenderNametableX(ntx, render, effects);
            }
        }

        page++;
        wr += 256;
        sx += 256;
    }
}
