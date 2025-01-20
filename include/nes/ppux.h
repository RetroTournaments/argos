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
//
// The PPUx is a means by which we can display NES information and build up the
// combined images that we want. Given the importance:
//
//  I am a dancer, that is to say a conduit.
//  I don't define music; music defines me.
//  Every dance tells a story.
//  A human mind.
//  Exploration.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef STATIC_NES_PPUX_HEADER
#define STATIC_NES_PPUX_HEADER

#include "nes/nes.h"
#include "util/rect.h"

namespace sta::nes
{

struct PPUxPriorityInfo;

struct OAMxEntry
{
    int X, Y; // In 'Game' Pixels
    uint8_t TileIndex;
    uint8_t Attributes;
    int PatternTableIndex; // Into RenderInfo::PatternTables
    std::array<uint8_t, 4> TilePalette;
};
// Helper to update the oamx in a specific way
void NextOAMx(OAMxEntry* oamx, int x8, int y8, uint8_t tileIndex, uint8_t attributes);

struct Nametablex
{
    int X, Y; // In 'Game' Pixels
    const nes::NameTable* NametableP;
    nes::FramePalette FramePalette; // Hmmm
    int PatternTableIndex; // Into RenderInfo::PatternTables
};

// The mostly 'static' things, associated with rendering
struct RenderInfo
{
    int OffX, OffY; // In PPUxPixels
    int Scale;
    // So tx = oamx.X * render.Scale + render.OffX : for example

    std::vector<const uint8_t*> PatternTables; // All 0x1000 in size
    const uint8_t* PaletteBGR;   // 0x00C0 in size
};

// Per item specific rendering options
struct EffectInfo
{
    float Opacity;
    bool CropWithin;
    util::Rect2I Crop;

    static EffectInfo Defaults();
};

// Priority status is to allow sprites to be behind backgrounds (and other
// sprites!) as a real NES PPU does.
// For simple images err on the side of disabled. However if you want a background
// and sprites behind it then you need to enable it and manage the priority via
// 'ResetPriority' on a new frame.
enum class PPUxPriorityStatus
{
    DISABLED,
    ENABLED
};

class PPUx {
public:
    // When putting into a pre-existing opencv mat: PPUx ppux(mat.cols, mat.rows, mat.data);
    PPUx(int width, int height, uint8_t* bgrout, PPUxPriorityStatus priorityStatus); // your data
    PPUx(int width, int height, PPUxPriorityStatus priorityStatus); // my data
    ~PPUx();

    // example: cv::Mat m(ppux.GetHeight(), ppux.GetWidth(), CV_8UC3, ppux.GetBGROut());
    int GetWidth() const;
    int GetHeight() const;
    uint8_t* GetBGROut();
    const uint8_t* GetBGROut() const;

    PPUxPriorityStatus GetPriorityStatus() const;
    void SetPriorityStatus(PPUxPriorityStatus status);

    // If priority enabled, then
    // Reset the priority either (ResetPriority or FillBackground)
    // Draw your nametables: RenderNametableEntry
    // Then the OAM: RenderOAMEntry
    void ResetPriority();
    void ResetSpritePriorityOnly();
    void CopyPriorityTo(PPUx* other);
    std::vector<uint8_t>& GetPriorityInfo(); // be careful
    void FillBackground(uint8_t paletteIndex, const uint8_t* paletteBGR);
    void FillBackground(uint8_t paletteIndex, const RenderInfo& render);

    // Note conventional order is y, tile, attributes, x
    void RenderOAMEntry(
            int x, int y, // In screen pixels
            uint8_t tileIndex, uint8_t oamAttributes,
            const uint8_t* patternTable, // 0x1000 in size
            const uint8_t* framePalette, // 0x0020 in size
            const uint8_t* paletteBGR,   // 0x00C0 in size
            int scale, const EffectInfo& effects);

    void RenderNametableEntry(
            int x, int y, uint8_t tileIndex, uint8_t attributes, // 2 bits
            const uint8_t* patternTable, // 0x1000 in size
            const uint8_t* framePalette, // 0x0020 in size
            const uint8_t* paletteBGR,   // 0x00C0 in size
            int scale, const EffectInfo& effects);

    void RenderNametable(
            int x, int y, // In screen pixels
            int width, int height, // in 8x8 pixel tiles
            const uint8_t* tileStart, // will read width * height bytes each as a tile index
            const uint8_t* attrStart, // will read (width * height) / 16 bytes using 2 bits at a time as attributes for 2x2 tiles as per usual.
            // often: tileStart + nes::NAMETABLE_ATTRIBUTE_OFFSET (if width = nes::NAMETABLE_WIDTH_BYTES, height = nes::NAMETABLE_HEIGHT_BYTES)
            const uint8_t* patternTable, // 0x1000 in size
            const uint8_t* framePalette, // 0x0020 in size
            const uint8_t* paletteBGR,   // 0x00C0 in size
            int scale, const EffectInfo& effects);


    void RenderOAMxEntry(
            const OAMxEntry& oamx,
            const RenderInfo& render,
            const EffectInfo& effects);
    void RenderNametableX(
            const Nametablex& ntx,
            const RenderInfo& render,
            const EffectInfo& effects);

    // Render a string (as a sprite) with in front of background priority
    void RenderString(int x, int y, // In screen pixels
            const std::string& str,
            const uint8_t* patternTable, // 0x1000 in size
            const uint8_t* tilePalette,  // 0x0004 in size!!
            const uint8_t* paletteBGR,   // 0x00C0 in size
            int scale, const EffectInfo& effects);

    void RenderStringX(int x, int y, // In screen pixels
            const std::string& strX,
            const uint8_t* patternTable, // 0x1000 in size
            const uint8_t* tilePalette,  // 0x0004 in size!!
            const uint8_t* paletteBGR,   // 0x00C0 in size
            int scalx, int scaly, const EffectInfo& effects);


    void RenderHardcodedSprite(int x, int y,
            std::vector<std::vector<uint8_t>> pixels,
            const uint8_t* paletteBGR,
            const EffectInfo& effects);



    // To render palettized data direct to the canvas, accounting for some stuff
    enum RenderPaletteDataFlags : uint8_t
    {
        RPD_PLACE_PIXELS_DIRECT = 0b00000000, // just puts pixels directly (doesn't set / change priority info)
        RPD_AS_SPRITE           = 0b00000001, // zero is see through, will check / update priority (if enabled)
        RPD_SPRITE_PRIORITY     = 0b00000010, // 0 in front, 1 behind background
        RPD_AS_NAMETABLE        = 0b00000100, // will update priority information
    };
    void RenderPaletteData(
            int x, int y, // in screen pixels
            int width, int height,
            const uint8_t* data, // each byte is taken as an index into paletteBGR, will read width * height bytes
            const uint8_t* paletteBGR, // max value in data * 3 in size (at most 0xff * 3)
            RenderPaletteDataFlags flags,
            const EffectInfo& effects);

    // To 'outline' things we have a rather unfortunate problem. The sprite
    // entries are rendered in 8x8 tiles that might share edges etc. So! I have
    // this nice method..
    void BeginOutline();
    // In between use things like: RenderOAMEntry, and RenderOAMxEntry
    void StrokeOutlineO(float outlineRadius, uint8_t paletteIndex, const uint8_t* paletteBGR); // rounded
    void StrokeOutlineX(float outlineRadius, uint8_t paletteIndex, const uint8_t* paletteBGR); // squared outline


    //
    static size_t RequiredBGROutSize(int width, int height);

    // The ppu will draw a sprite behind the background, but still update it as
    // if a sprite is there so you get weird glitches. This is the default
    // (correct) behaviour. Use this function to turn it off if you're picky.
    void SetSpritePriorityGlitch(bool value);

    void DrawBorderedBox(
            int x, int y, int w, int h,
            std::array<uint8_t, 9> paletteEntries,
            const uint8_t* paletteBGR, int outlineWidth = 1);

private:
    enum Render88Flags : uint8_t
    {
        R88_NAMETABLE       = 0b00000000,
        R88_IS_SPRITE       = 0b00000001, // Will skip palette entries of zero
        R88_FLIP_VERTICAL   = 0b00000010,
        R88_FLIP_HORIZONTAL = 0b00000100,
        R88_SPRITE_PRIORITY = 0b00001000, // 0 in front, 1 behind background
    };
    Render88Flags GetSpriteRenderFlags(uint8_t attributes);

    void RenderTile88(
            int x, int y, Render88Flags flags,
            const uint8_t* tilePalette,
            const uint8_t* tileData,
            const uint8_t* paletteBGR,
            int scalx, int scaly, const EffectInfo& effects);

    void PutPixel(int x, int y, const uint8_t* bgr, // blue green red
            bool overridePriority, bool isSprite, bool spritePriority, bool isBackground,
            const EffectInfo& effects);

    enum PPUxPriorityInfoEntry : uint8_t
    {
        PPUPRI_NONE         = 0b00000000,
        PPUPRI_BG           = 0b00000001,
        PPUPRI_SPR          = 0b00000010,
        PPUPRI_TO_OUTLINE   = 0b01000000,
        PPUPRI_OUTLINED     = 0b10000000,
        PPUPRI_AFTER_OUTLINE= 0b00111111,
    };
    static size_t RequiredPriorityInfoDataSize(int width, int height);
    bool PriorityEnabled() const;

    PPUxPriorityInfoEntry GetPriority(int tx, int ty);
    void SetPriority(int tx, int ty, PPUxPriorityInfoEntry entry);

    struct Offset {
        int dx;
        int dy;
        int ix;
    };
    void DoStrokeOutline(const std::vector<Offset>& offsets, uint8_t paletteIndex, const uint8_t* paletteBGR);
    void ClearStrokePriority();


private:
    int m_Width, m_Height;
    std::vector<uint8_t> m_MyBGR;
    uint8_t* m_BGROut;

    PPUxPriorityStatus m_PriorityStatus;
    std::vector<uint8_t> m_PriorityInfo;
    bool m_Outlining;

    bool m_SpritePriorityGlitch;
};

}

#endif

