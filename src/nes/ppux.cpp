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

#include <cassert>

#include "nes/ppux.h"
#include "util/bitmanip.h"

using namespace argos::nes;

EffectInfo EffectInfo::Defaults()
{
    EffectInfo ei;
    ei.Opacity = 1.0f;
    ei.CropWithin = false;
    ei.Crop = {0, 0, 0, 0};
    return ei;
}

void argos::nes::NextOAMx(OAMxEntry* oamx, int x8, int y8, uint8_t tileIndex, uint8_t attributes)
{
    oamx->X += x8 * 8;
    oamx->Y += y8 * 8;
    oamx->TileIndex = tileIndex;
    oamx->Attributes = attributes;
}


PPUx::PPUx(int width, int height, uint8_t* bgrout, PPUxPriorityStatus priorityStatus)
    : m_Width(width)
    , m_Height(height)
    , m_BGROut(bgrout)
    , m_PriorityStatus(priorityStatus)
    , m_Outlining(false)
    , m_SpritePriorityGlitch(true)
{
    if (m_Width <= 0 || m_Height <= 0) {
        throw std::invalid_argument("invalid size for ppux");
    }
    if (!m_BGROut) {
        throw std::invalid_argument("invalid output for ppux");
    }
}

PPUx::PPUx(int width, int height, PPUxPriorityStatus priorityStatus)
    : m_Width(width)
    , m_Height(height)
    , m_MyBGR(PPUx::RequiredBGROutSize(width, height), 0x00)
    , m_BGROut(m_MyBGR.data())
    , m_PriorityStatus(priorityStatus)
    , m_Outlining(false)
{
    if (m_Width <= 0 || m_Height <= 0) {
        throw std::invalid_argument("invalid size for ppux");
    }
}

PPUx::~PPUx()
{
}

int PPUx::GetWidth() const
{
    return m_Width;
}

int PPUx::GetHeight() const
{
    return m_Height;
}

uint8_t* PPUx::GetBGROut()
{
    return m_BGROut;
}

const uint8_t* PPUx::GetBGROut() const
{
    return m_BGROut;
}

size_t PPUx::RequiredBGROutSize(int width, int height)
{
    return static_cast<size_t>(width * height * 3);
}

size_t PPUx::RequiredPriorityInfoDataSize(int width, int height)
{
    return static_cast<size_t>(width * height);
}

void PPUx::RenderTile88(int x, int y, Render88Flags flags,
        const uint8_t* tilePalette, const uint8_t* tileData,
        const uint8_t* paletteBGR,
        int scalx, int scaly, const EffectInfo& effects)
{
    if (scalx <= 0 || scaly <= 0) {
        throw std::runtime_error("invalid scale to PPUx::RenderTile88");
    }


    const uint8_t* p1 = tileData;
    const uint8_t* p2 = tileData + 8;

    int ty = y;

    // Loop over the eight 'rows' of pixels
    for (int iy = 0; iy < 8; iy++) {
        int oy = (flags & Render88Flags::R88_FLIP_VERTICAL) ? 7 - iy : iy;

        // For the rows of this individual pixel (because of scal)
        for (int yc = 0; yc < scaly; yc++, ty++) {
            if (effects.CropWithin) {
                auto& crop = effects.Crop;
                if (ty < crop.Y) continue;
                if (ty > (crop.Y + crop.Height)) break;
            }
            if (ty < 0) continue;
            if (ty >= m_Height) break;

            uint8_t a = *(p1 + oy);
            uint8_t b = *(p2 + oy);
            if (!(flags & Render88Flags::R88_FLIP_HORIZONTAL)) {
                a = util::Reverse(a);
                b = util::Reverse(b);
            }

            int tx = x;
            // For the columns of this row
            for (int ix = 0; ix < 8; ix++) {
                uint8_t paletteIndex = (a & 0x01) + (b & 0x01) * 2;
                a >>= 1; b >>= 1;

                bool skipThis = false;

                if ((flags & Render88Flags::R88_IS_SPRITE)) {
                    if (paletteIndex == 0) {
                        skipThis = true;
                    } 
                }

                if (skipThis) {
                    tx += scalx;
                    continue;
                }

                if (tx >= m_Width) {
                    break;
                }

                // for the columns of this individual pixel
                const uint8_t* bgr = paletteBGR + tilePalette[paletteIndex] * 3;
                for (int xc = 0; xc < scalx; xc++, tx++) {
                    // WRITING the pixel
                    PutPixel(tx, ty, bgr, false,
                            flags & Render88Flags::R88_IS_SPRITE,
                            flags & Render88Flags::R88_SPRITE_PRIORITY,
                            effects.Opacity == 1.0f && paletteIndex != 0,
                            effects);
                }
            }
        }

        if (ty >= m_Height) break;
    }
}

PPUx::PPUxPriorityInfoEntry PPUx::GetPriority(int tx, int ty) {
    int v = ty * m_Width + tx;
    if (v < 0 || v >= m_PriorityInfo.size()) {
        return PPUxPriorityInfoEntry::PPUPRI_NONE;
    }
    return static_cast<PPUxPriorityInfoEntry>(m_PriorityInfo[v]);
}

void PPUx::SetPriority(int tx, int ty, PPUxPriorityInfoEntry entry) {
    if (m_PriorityInfo.size() != RequiredPriorityInfoDataSize(m_Width, m_Height)) {
        ResetPriority();
    }

    int v = ty * m_Width + tx;
    if (v < 0 || v >= m_PriorityInfo.size()) {
        throw std::invalid_argument("invalid pixel to set priority info!?");
    }

    m_PriorityInfo[v] = entry;
}

bool PPUx::PriorityEnabled() const
{
    return m_PriorityStatus == PPUxPriorityStatus::ENABLED;
}

void PPUx::FillBackground(uint8_t paletteIndex, const RenderInfo& render)
{
    FillBackground(paletteIndex, render.PaletteBGR);
}
void PPUx::FillBackground(uint8_t paletteIndex, const uint8_t* paletteBGR)
{
    ResetPriority();

    uint8_t* out = m_BGROut;
    const uint8_t* in = paletteBGR + paletteIndex * 3;
    for (int y = 0; y < m_Height; y++) {
        for (int x = 0; x < m_Width; x++) {
            for (int i = 0; i < 3; i++) {
                *(out + i) = *(in + i);
            }
            out += 3;
        }
    }
}

PPUx::Render88Flags PPUx::GetSpriteRenderFlags(uint8_t attributes)
{
    uint8_t flags = Render88Flags::R88_IS_SPRITE;
    if (attributes & OAMAttributeBits::OAM_FLIP_HORIZONTAL) {
        flags = flags | Render88Flags::R88_FLIP_HORIZONTAL;
    }
    if (attributes & OAMAttributeBits::OAM_FLIP_VERTICAL) {
        flags = flags | Render88Flags::R88_FLIP_VERTICAL;
    }
    if (attributes & OAMAttributeBits::OAM_PRIORITY) {
        flags = flags | Render88Flags::R88_SPRITE_PRIORITY;
    }
    return static_cast<Render88Flags>(flags);
}


void PPUx::RenderOAMEntry(int x, int y, uint8_t tileIndex, uint8_t attributes,
            const uint8_t* patternTable, // 0x1000 in size
            const uint8_t* framePalette, // 0x0020 in size
            const uint8_t* palette,
            int scal, const EffectInfo& effects)
{
    const uint8_t* tileData = patternTable + static_cast<int>(tileIndex) * 16;

    std::array<uint8_t, 4> tilePalette;
    tilePalette[0] = framePalette[0];

    const uint8_t* spritePalette = framePalette + 16 + (attributes & OAMAttributeBits::OAM_PALETTE) * 4;
    for (int i = 1; i < 4; i++) {
        tilePalette[i] = spritePalette[i];
    }

    RenderTile88(x, y, GetSpriteRenderFlags(attributes),
            tilePalette.data(), tileData, palette, scal, scal, effects);
}


void PPUx::ResetPriority()
{
    if (m_PriorityStatus == PPUxPriorityStatus::ENABLED) {
        m_PriorityInfo.assign(RequiredPriorityInfoDataSize(m_Width, m_Height), 
                PPUxPriorityInfoEntry::PPUPRI_NONE);
    }
}

void PPUx::ResetSpritePriorityOnly()
{
    if (m_PriorityStatus == PPUxPriorityStatus::ENABLED) {
        for (auto & entry : m_PriorityInfo) {
            entry &= ~PPUPRI_SPR;
        }
    }
}

void PPUx::RenderNametableEntry(
        int x, int y, uint8_t tileIndex, uint8_t attributes,
        const uint8_t* patternTable,
        const uint8_t* framePalette,
        const uint8_t* paletteBGR,
        int scale, const EffectInfo& effects)
{
    const uint8_t* tileData = patternTable + static_cast<int>(tileIndex) * 16;

    std::array<uint8_t, 4> tilePalette;
    tilePalette[0] = framePalette[0];
    const uint8_t* thisPalette = framePalette + (attributes & 0b00000011) * 4;
    for (int i = 1; i < 4; i++) {
        tilePalette[i] = thisPalette[i];
    }

    RenderTile88(x, y, R88_NAMETABLE,
            tilePalette.data(), tileData, paletteBGR, scale, scale, effects);
}

void PPUx::RenderOAMxEntry(
        const OAMxEntry& oamx, const RenderInfo& render, const EffectInfo& effects)
{
    int tx = oamx.X * render.Scale + render.OffX;
    int ty = oamx.Y * render.Scale + render.OffY;

    const uint8_t* tileData = 
        render.PatternTables.at(oamx.PatternTableIndex) + static_cast<int>(oamx.TileIndex) * 16;

    RenderTile88(tx, ty, GetSpriteRenderFlags(oamx.Attributes),
            oamx.TilePalette.data(), tileData, render.PaletteBGR,
            render.Scale, render.Scale, effects);
}

void PPUx::RenderNametableX(
        const Nametablex& ntx, const RenderInfo& render, const EffectInfo& effects)
{
    int tx = ntx.X * render.Scale + render.OffX;
    int ty = ntx.Y * render.Scale + render.OffY;

    RenderNametable(tx, ty, nes::NAMETABLE_WIDTH_BYTES, nes::NAMETABLE_HEIGHT_BYTES,
            ntx.NametableP->data(), ntx.NametableP->data() + nes::NAMETABLE_ATTRIBUTE_OFFSET,
            render.PatternTables.at(ntx.PatternTableIndex), ntx.FramePalette.data(),
            render.PaletteBGR, render.Scale, effects);
}

void PPUx::RenderNametable(int x, int y, int width, int height,
        const uint8_t* tileStart, const uint8_t* attrStart,
        const uint8_t* patternTable, 
        const uint8_t* framePalette,
        const uint8_t* paletteBGR,
        int scale, const EffectInfo& effects)
{
//    if (overrides) {
//        if (width != nes::NAMETABLE_WIDTH_BYTES || height !=  nes::NAMETABLE_HEIGHT_BYTES ||
//                attrStart != tileStart + nes::NAMETABLE_ATTRIBUTE_OFFSET) {
//            throw std::invalid_argument("unfortunately you need to change how nametableoverridesx offsets are used");
//        }
//    }

    for (int iy = 0; iy < height; iy++) {
        int ty = y + iy * 8 * scale;
        for (int ix = 0; ix < width; ix++) {
            int tx = x + ix * 8 * scale;

            int cy = iy / 4;
            int cx = ix / 4;

            int by = (iy % 4) / 2;
            int bx = (ix % 4) / 2;

            int tileOffset = iy * width + ix;
            int attrOffset = cy * (width / 4) + cx;

            uint8_t attr = *(attrStart + attrOffset);
            attr >>= (bx + by * 2) * 2;

            uint8_t tile = *(tileStart + tileOffset);

//            if (overrides) {
//                auto attrOversIt = overrides->find(tileOffset + nes::NAMETABLE_ATTRIBUTE_OFFSET);
//                if (attrOversIt != overrides->end()) {
//                    auto it = std::max_element(attrOversIt->begin(), attrOversIt->end(), 
//                        [&](const std::pair<uint8_t, float>& l, const std::pair<uint8_t, float>& r){
//                            return l.second < r.second;
//                        });
//
//
//                }
//
//            }

            RenderNametableEntry(tx, ty, tile, attr,
                    patternTable, framePalette, paletteBGR, scale, effects);
        }
    }
}

void PPUx::BeginOutline()
{
    if (!PriorityEnabled()) {
        throw std::runtime_error("PPUx::BeginOutline, Priority must be enabled in order to use outlining!");
    }
    m_Outlining = true;
}

void PPUx::ClearStrokePriority()
{
    m_Outlining = false;
    int ix = 0;
    for (int y = 0; y < m_Height; y++) {
        for (int x = 0; x < m_Width; x++) {
            m_PriorityInfo[ix] &= PPUPRI_AFTER_OUTLINE;
            ix++;
        }
    }
}


void PPUx::DoStrokeOutline(const std::vector<Offset>& offsets, uint8_t paletteIndex, const uint8_t* paletteBGR)
{
    if (m_PriorityInfo.empty()) return;

    // TODO: this could be optimized... (especially with 'x'
    const uint8_t* bgr = paletteBGR + paletteIndex * 3;

    int ix = 0;
    for (int y = 0; y < m_Height; y++) {
        for (int x = 0; x < m_Width; x++) {
            if (m_PriorityInfo[ix] & PPUPRI_TO_OUTLINE) {
                for (auto [dx, dy, off] : offsets) {
                    int tx = x + dx;
                    int ty = y + dy;

                    if (tx >= 0 && tx < m_Width &&
                        ty >= 0 && ty < m_Height) {
                        int tx = ix + off;
                        uint8_t* out = m_BGROut + tx * 3;
                        if (((m_PriorityInfo[tx] == PPUPRI_NONE) || (m_PriorityInfo[tx] & PPUPRI_BG)) &&
                            !(m_PriorityInfo[tx] & PPUPRI_OUTLINED)) {

                            m_PriorityInfo[tx] |= PPUPRI_OUTLINED;
                            for (int i = 0; i < 3; i++) {
                                *(out + i) = *(bgr + i);
                            }
                        }
                    }
                }
            }

            ix++;
        }
    }
    ClearStrokePriority();
}

void PPUx::StrokeOutlineX(float outlineRadius, uint8_t paletteIndex, const uint8_t* paletteBGR)
{
    int mr = static_cast<int>(std::round(outlineRadius));
    if (mr >= 1) {
        std::vector<Offset> offsets;
        offsets.reserve((mr * 2 + 1) * (mr * 2 + 1));
        Offset off;
        for (off.dy = -mr; off.dy <= mr; off.dy++) {
            for (off.dx = -mr; off.dx <= mr; off.dx++) {
                if (off.dx == 0 && off.dy == 0) continue;
                off.ix = off.dy * m_Width + off.dx;
                offsets.push_back(off);
            }
        }
        DoStrokeOutline(offsets, paletteIndex, paletteBGR);
    }
}

void PPUx::StrokeOutlineO(float outlineRadius, uint8_t paletteIndex, const uint8_t* paletteBGR)
{
    int mr = std::max(static_cast<int>(outlineRadius) + 1, 1);
    std::vector<Offset> offsets;
    
    float f = outlineRadius * outlineRadius;
    for (int dy = -mr; dy <= mr; dy++) {
        float yf = static_cast<float>(dy) * static_cast<float>(dy);
        for (int dx = -mr; dx <= mr; dx++) {
            float xf = static_cast<float>(dx) * static_cast<float>(dx);

            if ((xf + yf) <= f) {
                if (!(dx == 0 && dy == 0)) {
                    Offset off;
                    off.dx = dx;
                    off.dy = dy;
                    off.ix = dy * m_Width + dx;
                    offsets.push_back(off);
                }
            }
        }
    }

    DoStrokeOutline(offsets, paletteIndex, paletteBGR);
}

void PPUx::RenderHardcodedSprite(int x, int y, std::vector<std::vector<uint8_t>> pixels,
        const uint8_t* paletteBGR, const EffectInfo& effects)
{
    int r = 0;
    for (auto & row : pixels) {
        int c = 0;
        for (auto & pix : row) {
            if (pix != 0x00) {
                const uint8_t* bgr = paletteBGR + (pix * 3);
                PutPixel(x + c, y + r, bgr, false, true, false, false, effects);
            }

            c++;
        }
        r++;
    }

}

void PPUx::RenderPaletteData(int x, int y, int width, int height, const uint8_t* data,
        const uint8_t* paletteBGR, RenderPaletteDataFlags flags,
        const EffectInfo& effects)
{

    for (int dy = 0; dy < height; dy++) {
        for (int dx = 0; dx < width; dx++) {
            const uint8_t* bgr = paletteBGR + (*data * 3);
            if ((flags & RPD_AS_SPRITE) || (flags & RPD_AS_NAMETABLE)) {
                if (*data == 0x00) continue;
            }

            // todo i think there are bugs here
            PutPixel(x + dx, y + dy, bgr, flags == RPD_PLACE_PIXELS_DIRECT,
                    flags & RPD_AS_SPRITE,
                    flags & RPD_SPRITE_PRIORITY,
                    flags & RPD_AS_NAMETABLE, effects);
            data++;
        }
    }
}

void PPUx::PutPixel(int x, int y, const uint8_t* bgr,
        bool overridePriority, bool isSprite, bool spritePriority, bool isBackground, const EffectInfo& effects)
{
    if (y < 0 || y >= m_Height) return;
    if (x < 0 || x >= m_Width) return;
    if (effects.CropWithin) {
        auto& crop = effects.Crop;
        if (x < crop.X) return;
        if (x > (crop.X + crop.Width)) return;
    }

    if (!overridePriority) {
        if (PriorityEnabled()) {
            auto entry = GetPriority(x, y);

            if (isSprite) {
                if (effects.Opacity == 1.0f) {
                    if (!m_SpritePriorityGlitch && spritePriority && entry != PPUPRI_NONE) {
                        return;
                    } else {
                        SetPriority(x, y, PPUPRI_SPR);
                    }
                }

                if (spritePriority) { // behind background
                    if (entry != PPUPRI_NONE) {
                        return;
                    }
                } else {
                    if (entry & PPUPRI_SPR) {
                        return;
                    }
                }
            } else {
                if (isBackground) {
                    SetPriority(x, y, PPUPRI_BG);
                }
            }
        }

        if (m_Outlining && PriorityEnabled()) {
            int it = y * m_Width + x;
            m_PriorityInfo[it] |= PPUPRI_TO_OUTLINE;
        }
    }

    if (effects.Opacity != 1.0f) {
        throw std::invalid_argument("not handled yet");
    }
    const uint8_t* t = bgr;
    uint8_t* dat = m_BGROut + (y * m_Width * 3) + (x * 3);
    for (int v = 0; v < 3; v++) {
        *(dat + v) = *(t + v);
    }
}

void PPUx::RenderString(int x, int y, 
        const std::string& str, const uint8_t* patternTable, const uint8_t* tilePalette,
        const uint8_t* paletteBGR, int scale, const EffectInfo& effects)
{
    Render88Flags flags = Render88Flags::R88_IS_SPRITE;

    for (auto c : str) {
        const uint8_t* tileData = patternTable + static_cast<int>(c) * 16;
        RenderTile88(x, y, flags, tilePalette, tileData, paletteBGR, scale, scale, effects);

        x += 8 * scale;
    }
}

void PPUx::RenderStringX(int x, int y, 
        const std::string& strx, const uint8_t* patternTable, const uint8_t* tilePalette,
        const uint8_t* paletteBGR, int scalx, int scaly, const EffectInfo& effects)
{
    Render88Flags flags = Render88Flags::R88_IS_SPRITE;


    int sx = x;
    for (auto c : strx) {
        if (static_cast<int>(c) == -61) {
            static std::array<uint8_t, 16> dat = {
                0b11000110,
                0b00000000,
                0b11000110,
                0b11000110,
                0b11000110,
                0b11000110,
                0b01111100,
                0b00000000,
                0b00000000,
                0b00000000,
                0b00000000,
                0b00000000,
                0b00000000,
                0b00000000,
                0b00000000,
                0b00000000,
            };
            RenderTile88(x, y, flags, tilePalette, dat.data(), paletteBGR, scalx, scaly, effects);
            x += 8 * scalx;
        } 
        if (static_cast<int>(c) < 0) {
            continue;
        }
        
        if (c == '\n') {
            y += 8 * scaly;
            x = sx;
        } else {
            const uint8_t* tileData = patternTable + static_cast<int>(c) * 16;
            RenderTile88(x, y, flags, tilePalette, tileData, paletteBGR, scalx, scaly, effects);

            x += 8 * scalx;
        }
    }
}

void PPUx::SetSpritePriorityGlitch(bool value)
{
    m_SpritePriorityGlitch = value;
}

void PPUx::DrawBorderedBox(
        int x, int y, int w, int h,
        std::array<uint8_t, 9> paletteEntries,
        const uint8_t* paletteBGR, int outlineWidth)
{
    nes::EffectInfo effects = nes::EffectInfo::Defaults();
    for (int iy = 0; iy < h; iy++) {
        for (int ix = 0; ix < w; ix++) {
            uint8_t t = paletteEntries[4];
            if (ix < outlineWidth) {
                if (iy < outlineWidth) {
                    t = paletteEntries[0];
                } else if (iy >= (h - outlineWidth)) {
                    t = paletteEntries[6];
                } else {
                    t = paletteEntries[3];
                }
            } else if (ix >= (w - outlineWidth)) {
                if (iy < outlineWidth) {
                    t = paletteEntries[2];
                } else if (iy >= (h - outlineWidth)) {
                    t = paletteEntries[8];
                } else {
                    t = paletteEntries[5];
                }
            } else if (iy < outlineWidth) {
                t = paletteEntries[1];
            } else if (iy >= (h - outlineWidth)) {
                t = paletteEntries[7];
            }


            const uint8_t* bgr = paletteBGR + (t * 3);
            PutPixel(x + ix, y + iy, bgr, false, true, false, false, effects);
        }
    }
}


std::vector<uint8_t>& PPUx::GetPriorityInfo()
{
    return m_PriorityInfo;
}

void PPUx::CopyPriorityTo(PPUx* other)
{
    assert(other->GetWidth() == GetWidth());
    assert(other->GetHeight() == GetHeight());

    other->GetPriorityInfo() = GetPriorityInfo();
}

