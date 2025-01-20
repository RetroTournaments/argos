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

#include <numeric>

#include "nes/ppux.h"
#include "nes/nesui.h"
#include "game/gamedbui.h"
#include "ext/opencvext/opencvext.h"
#include "ext/nfdext/nfdext.h"
#include "util/bitmanip.h"

using namespace sta;
using namespace sta::nesui;

NESPaletteComponentOptions NESPaletteComponentOptions::Defaults()
{
    NESPaletteComponentOptions options;
    options.BGROrder = true;
    options.AllowEdits = true;

    return options;
}

NESPaletteComponent::NESPaletteComponent(const std::string& windowName)
    : NESPaletteComponent(windowName,
            NESPaletteComponentOptions::Defaults(),
            nes::DefaultPaletteBGR())
{
}

NESPaletteComponent::NESPaletteComponent(const std::string& windowName,
                        NESPaletteComponentOptions options,
                        nes::Palette initialPalette)
    : m_WindowName(windowName)
    , m_Options(options)
    , m_Palette(initialPalette)
{
}

NESPaletteComponent::~NESPaletteComponent()
{
}

void NESPaletteComponent::OnFrame()
{
    if (ImGui::Begin(m_WindowName.c_str())) {
        NESPaletteComponent::Controls(&m_Palette, m_Options);
    }
    ImGui::End();
}

bool NESPaletteComponent::Controls(nes::Palette* palette, const NESPaletteComponentOptions& options)
{
    bool changed = false;
    int colorIndex = 0;
    if (ImGui::BeginTable("palette", 16, ImGuiTableFlags_SizingFixedSame
                | ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_NoPadInnerX)) {
        for (int row = 0; row < 4; row++) {
            ImGui::TableNextRow();
            for (int col = 0; col < 16; col++) {
                ImGui::TableNextColumn();

                float rgb[3];
                for (int i = 0; i < 3; i++) {
                    rgb[i] = static_cast<float>((*palette)[colorIndex * 3 + i]) / 255.0f;
                }
                if (options.BGROrder) {
                    std::swap(rgb[0], rgb[2]);
                }

                ImGuiColorEditFlags flags =
                    ImGuiColorEditFlags_NoAlpha |
                    ImGuiColorEditFlags_NoInputs |
                    ImGuiColorEditFlags_NoLabel;
                if (!options.AllowEdits) {
                    flags |= ImGuiColorEditFlags_NoPicker;
                }
                std::string label = fmt::format("0x{:02x}", colorIndex);

                if (ImGui::ColorEdit3(label.c_str(), rgb, flags)) {
                    if (options.BGROrder) {
                        std::swap(rgb[0], rgb[2]);
                    }

                    for (int i = 0; i < 3; i++) {
                        (*palette)[colorIndex * 3 + i] = static_cast<uint8_t>(std::round(rgb[i] * 255.0f));
                    }

                    changed = true;
                }

                colorIndex++;
            }
        }
        ImGui::EndTable();
    }


    return changed;
}

nes::Palette& NESPaletteComponent::GetPalette()
{
    return m_Palette;
}

const nes::Palette& NESPaletteComponent::GetPalette() const
{
    return m_Palette;
}

NESPaletteComponentOptions& NESPaletteComponent::GetOptions()
{
    return m_Options;
}

const NESPaletteComponentOptions& NESPaletteComponent::GetOptions() const
{
    return m_Options;
}

////////////////////////////////////////////////////////////////////////////////

FramePaletteComponentOptions FramePaletteComponentOptions::Defaults()
{
    FramePaletteComponentOptions options;
    options.BGROrder = true;
    options.AllowEdits = true;
    options.NesPalette = nes::DefaultPaletteBGR();
    options.NesPaletteP = nullptr;
    return options;
}

FramePaletteComponent::FramePaletteComponent(const std::string& windowName)
    : FramePaletteComponent(windowName, FramePaletteComponentOptions::Defaults(),
            {})
{
    std::iota(m_FramePalette.begin(), m_FramePalette.end(), 0);
}

FramePaletteComponent::FramePaletteComponent(const std::string& windowName,
        FramePaletteComponentOptions options, nes::FramePalette framePalette)
    : m_WindowName(windowName)
    , m_Options(options)
    , m_FramePalette(framePalette)
{
}

FramePaletteComponent::~FramePaletteComponent()
{
}

void FramePaletteComponent::OnFrame()
{
    if (ImGui::Begin(m_WindowName.c_str())) {
        FramePaletteComponent::Controls(&m_FramePalette, m_Options, &m_PopupPaletteIndex);
    }
    ImGui::End();
}

nes::FramePalette& FramePaletteComponent::GetFramePalette()
{
    return m_FramePalette;
}

const nes::FramePalette& FramePaletteComponent::GetFramePalette() const
{
    return m_FramePalette;
}

FramePaletteComponentOptions& FramePaletteComponent::GetOptions()
{
    return m_Options;
}

const FramePaletteComponentOptions& FramePaletteComponent::GetOptions() const
{
    return m_Options;
}

static ImVec4 GetFPalColor(int pal, const FramePaletteComponentOptions& options)
{
    ImVec4 color;
    if (options.NesPaletteP) {
        color.x = static_cast<float>((*options.NesPaletteP)[(pal * 3) + 0]) / 255.0f;
        color.y = static_cast<float>((*options.NesPaletteP)[(pal * 3) + 1]) / 255.0f;
        color.z = static_cast<float>((*options.NesPaletteP)[(pal * 3) + 2]) / 255.0f;
        color.w = 1.0f;
    } else {
        color.x = static_cast<float>(options.NesPalette[(pal * 3) + 0]) / 255.0f;
        color.y = static_cast<float>(options.NesPalette[(pal * 3) + 1]) / 255.0f;
        color.z = static_cast<float>(options.NesPalette[(pal * 3) + 2]) / 255.0f;
        color.w = 1.0f;
    }

    if (options.BGROrder) {
        std::swap(color.x, color.z);
    }

    return color;
}

static bool FramePaletteComponentPopup(uint8_t* fpalEntry, const FramePaletteComponentOptions& options)
{
    bool changed = false;
    int v = static_cast<int>(*fpalEntry);
    if (rgmui::SliderIntExt("Palette Entry", &v, 0, nes::PALETTE_ENTRIES - 1)) {
        *fpalEntry = static_cast<uint8_t>(v);
        changed = true;
    }

    if (ImGui::BeginTable("p2", 16, ImGuiTableFlags_SizingFixedSame
                | ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_NoPadInnerX)) {
        int paletteIndex = 0;
        for (int row = 0; row < 4; row++) {
            ImGui::TableNextRow();
            for (int col = 0; col < 16; col++) {
                ImGui::TableNextColumn();

                ImVec4 color = GetFPalColor(paletteIndex, options);
                std::string label = fmt::format("0x{:02x}", paletteIndex);
                ImGuiColorEditFlags flags = ImGuiColorEditFlags_None;
                if (paletteIndex != static_cast<int>(*fpalEntry)) {
                    flags = ImGuiColorEditFlags_NoBorder;
                }

                if (ImGui::ColorButton(label.c_str(), color, flags)) {
                    *fpalEntry = static_cast<uint8_t>(paletteIndex);
                    changed = true;
                }

                paletteIndex++;
            }
        }
        ImGui::EndTable();
    }

    return changed;
}


bool FramePaletteComponent::Controls(nes::FramePalette* framePalette, const FramePaletteComponentOptions& options, int* popupPaletteIndex)
{
    bool changed = false;

    if (ImGui::BeginTable("palette", 16, ImGuiTableFlags_SizingFixedSame
                | ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_NoPadInnerX)) {
        int paletteIndex = 0;
        for (int row = 0; row < 2; row++) {
            ImGui::TableNextRow();
            for (int col = 0; col < 16; col++) {
                ImGui::TableNextColumn();

                int pal = static_cast<int>((*framePalette)[paletteIndex]);
                std::string label = fmt::format("0x{:02x}: 0x{:02x}", paletteIndex, pal);

                ImVec4 color = GetFPalColor(pal, options);
                if (ImGui::ColorButton(label.c_str(), color)) {
                    if (popupPaletteIndex) {
                        *popupPaletteIndex = paletteIndex;
                        ImGui::OpenPopup("fpal");
                    }
                }

                if (popupPaletteIndex && *popupPaletteIndex == paletteIndex && ImGui::BeginPopup("fpal")) {
                    FramePaletteComponentPopup(&(*framePalette)[*popupPaletteIndex], options);
                    rgmui::TextFmt("0x{:02x}", *popupPaletteIndex);


                    ImGui::EndPopup();
                }

                paletteIndex++;
            }
        }
        ImGui::EndTable();

    }

    return changed;
}

////////////////////////////////////////////////////////////////////////////////

ControllerColors ControllerColors::Defaults()
{
    ControllerColors c;
    c.Base      = ImVec4(0.800f, 0.800f, 0.800f, 1.000f);
    c.BaseDark  = ImVec4(0.650f, 0.650f, 0.650f, 1.000f);
    c.Dark      = ImVec4(0.100f, 0.100f, 0.100f, 1.000f);
    c.Accent    = ImVec4(0.400f, 0.400f, 0.400f, 1.000f);
    c.Black     = ImVec4(0.000f, 0.000f, 0.000f, 1.000f);

    c.ABButton  = ImVec4(0.800f, 0.000f, 0.000f, 1.000f);
    c.STButton  = ImVec4(0.100f, 0.100f, 0.100f, 1.000f);
    c.DPad      = ImVec4(0.100f, 0.100f, 0.100f, 1.000f);
    c.Pressed   = ImVec4(0.000f, 1.000f, 0.000f, 1.000f);

    return c;
}

ControllerGeometry ControllerGeometry::Defaults()
{
    ControllerGeometry g;
    g.Boundary.Width = 123.5f;
    g.Boundary.Height = 53.7f;
    g.Boundary.Radius = 1.6f;

    g.Boundary.ControllerPort.CenterX = 34.5f;
    g.Boundary.ControllerPort.Width = 4.5f;
    g.Boundary.ControllerPort.Height = 1.8f;

    g.DarkRegion.Offset = 3.7f;
    g.DarkRegion.Height = 40.8f;
    g.DarkRegion.Radius = 1.3f;

    g.DPad.CenterX = 21.3f;
    g.DPad.CenterY = 32.3f;
    g.DPad.PadDiameter = 20.6;
    g.DPad.PadWidth = 6.7;
    g.DPad.Radius = 1.0f;

    g.DPad.Outline.Thickness = 1.4f;
    g.DPad.Outline.Radius = 1.3f;

    g.SelectStart.CenterX = (74.5f + 40.2f) / 2.0f;
    g.SelectStart.Width = 74.5f - 40.2f;
    g.SelectStart.AccentRadius = 1.5f;

    g.SelectStart.Accent[0].TY = 0.0f;
    g.SelectStart.Accent[0].Height = 5.0f;
    g.SelectStart.Accent[0].Rounding = ImDrawFlags_RoundCornersBottom;

    g.SelectStart.Accent[1].TY = 7.0f;
    g.SelectStart.Accent[1].Height = 5.7f;
    g.SelectStart.Accent[1].Rounding = ImDrawFlags_RoundCornersAll;

    g.SelectStart.Accent[2].TY = 14.7f;
    g.SelectStart.Accent[2].Height = 5.7f;
    g.SelectStart.Accent[2].Rounding = ImDrawFlags_RoundCornersAll;

    g.SelectStart.Accent[3].TY = 36.9f;
    g.SelectStart.Accent[3].Height = -1.0f;
    g.SelectStart.Accent[3].Rounding = ImDrawFlags_RoundCornersTop;

    g.SelectStart.ButtonInset.CenterY = (36.9f + (14.7f + 5.7f)) / 2;
    g.SelectStart.ButtonInset.Height = 11.9f;
    g.SelectStart.ButtonInset.Button.OffsetX = (25.3f / 2.0f) - 10.0f / 2;
    g.SelectStart.ButtonInset.Button.Width = 10.0f;
    g.SelectStart.ButtonInset.Button.Height = 4.6f;

    g.ABButtons.CenterX = (96.6f + 93.4f) / 2.0f;
    g.ABButtons.CenterY = g.SelectStart.ButtonInset.CenterY;
    g.ABButtons.OutsideWidth = 30.5f;
    g.ABButtons.ButtonDiameter = 10.3f;
    g.ABButtons.Accent.Width = 13.5f;
    g.ABButtons.Accent.Radius = 1.4f;

    return g;
}

DogboneGeometry DogboneGeometry::Defaults()
{
    DogboneGeometry g;

    g.Boundary.Width = 131.7f;
    g.Boundary.Height = 57.9f;
    g.Boundary.BetweenCentersWidth = 74.5f;
    g.Boundary.InnerHeight = 44.6f;

    g.Boundary.ControllerPort.Width = 4.5f;
    g.Boundary.ControllerPort.Height = 1.8f;

    g.DPad.PadDiameter = 23.6;
    g.DPad.PadWidth = 7.7;
    g.DPad.Radius = 1.0f;

    g.DPad.Outline.Thickness = 1.4f;
    g.DPad.Outline.Radius = 1.3f;

    g.SelectStart.Button.Width = 9.83f;
    g.SelectStart.Button.Height = 3.87f;
    g.SelectStart.Button.Angle = -0.436f;
    g.SelectStart.InterDistance = 16.0f;

    g.ABButtons.Angle = -0.436f;
    g.ABButtons.InterDistance = 16.0f;
    g.ABButtons.ButtonDiameter = 12.0f;

    return g;
}

/*
 * (0, 0)
 *    +-------------------------------------+
 *    |                                     |
 *    |      _                              |
 *    |    _| |_                  __   __   |
 *    |   |_   _|   |==| |==|    |()| |()|  |
 *    |     |_|                   ''   ''   |
 *    |                                     |
 *    +-------------------------------------+
 *
 */

ImVec2 nesui::DrawController(
        nes::ControllerState state,
        const ControllerColors& colors,
        const ControllerGeometry& g,
        float ox, float oy, float scale,
        ImDrawList* list, ButtonLocations* buttons)
{
    auto C = [&](float x, float y) {
        return ImVec2(static_cast<int>(std::round(ox + (x * scale))),
                      static_cast<int>(std::round(oy + (y * scale))));
    };
    auto V = [&](float v) {
        return v * scale;
    };
    auto CL = [&](const ImVec4 color) {
        return ImGui::GetColorU32(color);
    };

    ButtonLocation ButtonA, ButtonB, ButtonSelect, ButtonStart, ButtonUp, ButtonDown, ButtonLeft, ButtonRight;

    // The Boundary
    {
        list->AddRectFilled(C(0, 0), C(g.Boundary.Width, g.Boundary.Height),
                CL(colors.Base), V(g.Boundary.Radius));

        auto& cp = g.Boundary.ControllerPort;
        list->AddRectFilled(
                C(cp.CenterX - cp.Width / 2, 0),
                C(cp.CenterX + cp.Width / 2, cp.Height),
                CL(colors.BaseDark), 0.0f);

    }
    // The DarkRegion
    {
        auto o = g.DarkRegion.Offset;
        list->AddRectFilled(
                C(o, g.Boundary.Height - o - g.DarkRegion.Height),
                C(g.Boundary.Width - o, g.Boundary.Height - o),
                CL(colors.Dark), V(g.DarkRegion.Radius));
    }

    // The DPad
    {
        float cx = g.DPad.CenterX;
        float cy = g.DPad.CenterY;
        float pw = g.DPad.PadWidth;
        float pd = g.DPad.PadDiameter;
        float tw = g.DPad.PadWidth + g.DPad.Outline.Thickness;
        float td = g.DPad.PadDiameter + g.DPad.Outline.Thickness;

        list->AddRectFilled(
                C(cx - td / 2, cy - tw / 2),
                C(cx + td / 2, cy + tw / 2),
                CL(colors.Base), V(g.DPad.Outline.Radius));
        list->AddRectFilled(
                C(cx - tw / 2, cy - td / 2),
                C(cx + tw / 2, cy + td / 2),
                CL(colors.Base), V(g.DPad.Outline.Radius));

        list->AddRectFilled(
                C(cx - pd / 2, cy - pw / 2),
                C(cx + pd / 2, cy + pw / 2),
                CL(colors.DPad), V(g.DPad.Radius));
        list->AddRectFilled(
                C(cx - pw / 2, cy - pd / 2),
                C(cx + pw / 2, cy + pd / 2),
                CL(colors.DPad), V(g.DPad.Radius));

        ButtonUp.first = C(cx - pw / 2, cy - pd / 2);
        ButtonUp.second = C(cx + pw / 2, cy - pd / 2 + pw);
        if (state & nes::Button::UP) {
            list->AddRectFilled(C(cx - pw / 2, cy - pd / 2),
                    C(cx + pw / 2, cy - pd / 2 + pw),
                    CL(colors.Pressed), V(g.DPad.Radius));
            list->AddRect(C(cx - pw / 2, cy - pd / 2),
                    C(cx + pw / 2, cy - pd / 2 + pw),
                    CL(colors.Black), V(g.DPad.Radius));
        }

        ButtonDown.first = C(cx - pw / 2, cy + pd / 2 - pw);
        ButtonDown.second = C(cx + pw / 2, cy + pd / 2);
        if (state & nes::Button::DOWN) {
            list->AddRectFilled(C(cx - pw / 2, cy + pd / 2 - pw),
                    C(cx + pw / 2, cy + pd / 2),
                    CL(colors.Pressed), V(g.DPad.Radius));
            list->AddRect(C(cx - pw / 2, cy + pd / 2 - pw),
                    C(cx + pw / 2, cy + pd / 2),
                    CL(colors.Black), V(g.DPad.Radius));
        }

        ButtonLeft.first = C(cx - pd / 2, cy - pw / 2);
        ButtonLeft.second = C(cx - pd / 2 + pw, cy + pw / 2);
        if (state & nes::Button::LEFT) {
            list->AddRectFilled(C(cx - pd / 2, cy - pw / 2),
                    C(cx - pd / 2 + pw, cy + pw / 2),
                    CL(colors.Pressed), V(g.DPad.Radius));
            list->AddRect(C(cx - pd / 2, cy - pw / 2),
                    C(cx - pd / 2 + pw, cy + pw / 2),
                    CL(colors.Black), V(g.DPad.Radius));
        }

        ButtonRight.first = C(cx + pd / 2 - pw, cy - pw / 2);
        ButtonRight.second = C(cx + pd / 2, cy + pw / 2);
        if (state & nes::Button::RIGHT) {
            list->AddRectFilled(C(cx + pd / 2 - pw, cy - pw / 2),
                    C(cx + pd / 2, cy + pw / 2),
                    CL(colors.Pressed), V(g.DPad.Radius));
            list->AddRect(C(cx + pd / 2 - pw, cy - pw / 2),
                    C(cx + pd / 2, cy + pw / 2),
                    CL(colors.Black), V(g.DPad.Radius));
        }
    }

    // The select start region
    {
        auto ss = g.SelectStart;
        auto oy = g.Boundary.Height - g.DarkRegion.Offset - g.DarkRegion.Height;
        auto ey = g.Boundary.Height - g.DarkRegion.Offset;


        for (int i = 0; i < NUM_ACCENTS; i++) {
            auto& a = ss.Accent[i];
            auto ty = oy + a.TY;
            auto tey = ty + a.Height;
            if (i == NUM_ACCENTS - 1) {
                tey = ey;
            }
            list->AddRectFilled(
                    C(ss.CenterX - ss.Width / 2, ty),
                    C(ss.CenterX + ss.Width / 2, tey),
                    CL(colors.Accent), V(ss.AccentRadius), a.Rounding);
        }

        auto& bi = ss.ButtonInset;
        list->AddRectFilled(
                C(ss.CenterX - ss.Width / 2, oy + bi.CenterY - bi.Height / 2),
                C(ss.CenterX + ss.Width / 2, oy + bi.CenterY + bi.Height / 2),
                CL(colors.Base), V(ss.AccentRadius));

        float b1x = ss.CenterX - bi.Button.OffsetX;
        float b2x = ss.CenterX + bi.Button.OffsetX;
        float by = oy + bi.CenterY;
        float bw = bi.Button.Width;
        float bh = bi.Button.Height;

        auto selectC = CL(colors.STButton);
        auto startC = CL(colors.STButton);
        if (state & nes::Button::SELECT) {
            selectC = CL(colors.Pressed);
        }
        if (state & nes::Button::START) {
            startC = CL(colors.Pressed);
        }

        list->AddRectFilled(
                C(b1x - bw / 2, by - bh / 2),
                C(b1x + bw / 2, by + bh / 2),
                selectC, V(bh / 2));
        list->AddRectFilled(
                C(b2x - bw / 2, by - bh / 2),
                C(b2x + bw / 2, by + bh / 2),
                startC, V(bh / 2));

        ButtonSelect.first = C(b1x - bw / 2, by - bh / 2);
        ButtonSelect.second = C(b1x + bw / 2, by + bh / 2);
        if (state & nes::Button::SELECT) {
            list->AddRect(
                    C(b1x - bw / 2, by - bh / 2),
                    C(b1x + bw / 2, by + bh / 2),
                    CL(colors.Black), V(bh / 2));
        }

        ButtonStart.first = C(b2x - bw / 2, by - bh / 2);
        ButtonStart.second = C(b2x + bw / 2, by + bh / 2);
        if (state & nes::Button::START) {
            list->AddRect(
                    C(b2x - bw / 2, by - bh / 2),
                    C(b2x + bw / 2, by + bh / 2),
                    CL(colors.Black), V(bh / 2));
        }
    }

    // ABButtons
    {
        auto oy = g.Boundary.Height - g.DarkRegion.Offset - g.DarkRegion.Height;
        auto& ab = g.ABButtons;
        list->AddRectFilled(
                C(ab.CenterX - ab.OutsideWidth / 2, oy + ab.CenterY - ab.Accent.Width / 2),
                C(ab.CenterX - ab.OutsideWidth / 2 + ab.Accent.Width, oy + ab.CenterY + ab.Accent.Width / 2),
                CL(colors.Base), V(ab.Accent.Radius));
        list->AddRectFilled(
                C(ab.CenterX + ab.OutsideWidth / 2 - ab.Accent.Width, oy + ab.CenterY - ab.Accent.Width / 2),
                C(ab.CenterX + ab.OutsideWidth / 2, oy + ab.CenterY + ab.Accent.Width / 2),
                CL(colors.Base), V(ab.Accent.Radius));

        float x1 = ab.CenterX - ab.OutsideWidth / 2 + ab.Accent.Width / 2;
        float x2 = ab.CenterX + ab.OutsideWidth / 2 - ab.Accent.Width / 2;

        auto bc = CL(colors.ABButton);
        auto ac = CL(colors.ABButton);
        if (state & nes::Button::B) {
            bc = CL(colors.Pressed);
        }
        if (state & nes::Button::A) {
            ac = CL(colors.Pressed);
        }

        ButtonB.first = C(x1 - ab.ButtonDiameter / 2, oy+ab.CenterY - ab.ButtonDiameter / 2);
        ButtonB.second = C(x1 + ab.ButtonDiameter / 2, oy+ab.CenterY + ab.ButtonDiameter / 2);
        ButtonA.first = C(x2 - ab.ButtonDiameter / 2, oy+ab.CenterY - ab.ButtonDiameter / 2);
        ButtonA.second = C(x2 + ab.ButtonDiameter / 2, oy+ab.CenterY + ab.ButtonDiameter / 2);

        list->AddCircleFilled(C(x1, oy + ab.CenterY), V(ab.ButtonDiameter / 2), bc);
        list->AddCircleFilled(C(x2, oy + ab.CenterY), V(ab.ButtonDiameter / 2), ac);

        list->AddCircle(C(x1, oy + ab.CenterY), V(ab.ButtonDiameter / 2), CL(colors.Black));
        list->AddCircle(C(x2, oy + ab.CenterY), V(ab.ButtonDiameter / 2), CL(colors.Black));
    }

    if (buttons) {
        *buttons = {ButtonA, ButtonB, ButtonSelect, ButtonStart, ButtonUp, ButtonDown, ButtonLeft, ButtonRight};
    }

    return C(g.Boundary.Width, g.Boundary.Height);
}

ImVec2 nesui::DrawDogbone(
        nes::ControllerState state,
        const ControllerColors& colors,
        const DogboneGeometry& g,
        float ox, float oy, float scale,
        ImDrawList* list, ButtonLocations* buttons)
{
    auto C = [&](float x, float y) {
        return ImVec2(static_cast<int>(std::round(ox + (x * scale))),
                      static_cast<int>(std::round(oy + (y * scale))));
    };
    auto V = [&](float v) {
        return v * scale;
    };
    auto CL = [&](const ImVec4 color) {
        return ImGui::GetColorU32(color);
    };


    float tcx = g.Boundary.Width / 2;
    float tcy = g.Boundary.Height / 2;
    float r = g.Boundary.Height / 2;

    ButtonLocation ButtonA, ButtonB, ButtonSelect, ButtonStart, ButtonUp, ButtonDown, ButtonLeft, ButtonRight;

    // The Boundary
    {
        list->AddRectFilled(
                C(r, tcy - g.Boundary.InnerHeight / 2),
                C(g.Boundary.Width - r, tcy + g.Boundary.InnerHeight / 2),
                CL(colors.Base));
        list->AddCircleFilled(C(r, tcy), V(r),
                CL(colors.Base));
        list->AddCircleFilled(C(g.Boundary.Width - r, tcy), V(r),
                CL(colors.Base));
    }

    // The DPad (TODO copy paste)
    {
        float cx = r;
        float cy = r;
        float pw = g.DPad.PadWidth;
        float pd = g.DPad.PadDiameter;
        float tw = g.DPad.PadWidth + g.DPad.Outline.Thickness;
        float td = g.DPad.PadDiameter + g.DPad.Outline.Thickness;

        list->AddRectFilled(
                C(cx - td / 2, cy - tw / 2),
                C(cx + td / 2, cy + tw / 2),
                CL(colors.Base), V(g.DPad.Outline.Radius));
        list->AddRectFilled(
                C(cx - tw / 2, cy - td / 2),
                C(cx + tw / 2, cy + td / 2),
                CL(colors.Base), V(g.DPad.Outline.Radius));

        list->AddRectFilled(
                C(cx - pd / 2, cy - pw / 2),
                C(cx + pd / 2, cy + pw / 2),
                CL(colors.DPad), V(g.DPad.Radius));
        list->AddRectFilled(
                C(cx - pw / 2, cy - pd / 2),
                C(cx + pw / 2, cy + pd / 2),
                CL(colors.DPad), V(g.DPad.Radius));

        ButtonUp.first = C(cx - pw / 2, cy - pd / 2);
        ButtonUp.second = C(cx + pw / 2, cy - pd / 2 + pw);
        if (state & nes::Button::UP) {
            list->AddRectFilled(C(cx - pw / 2, cy - pd / 2),
                    C(cx + pw / 2, cy - pd / 2 + pw),
                    CL(colors.Pressed), V(g.DPad.Radius));
            list->AddRect(C(cx - pw / 2, cy - pd / 2),
                    C(cx + pw / 2, cy - pd / 2 + pw),
                    CL(colors.Black), V(g.DPad.Radius));
        }

        ButtonDown.first = C(cx - pw / 2, cy + pd / 2 - pw);
        ButtonDown.second = C(cx + pw / 2, cy + pd / 2);
        if (state & nes::Button::DOWN) {
            list->AddRectFilled(C(cx - pw / 2, cy + pd / 2 - pw),
                    C(cx + pw / 2, cy + pd / 2),
                    CL(colors.Pressed), V(g.DPad.Radius));
            list->AddRect(C(cx - pw / 2, cy + pd / 2 - pw),
                    C(cx + pw / 2, cy + pd / 2),
                    CL(colors.Black), V(g.DPad.Radius));
        }

        ButtonLeft.first = C(cx - pd / 2, cy - pw / 2);
        ButtonLeft.second = C(cx - pd / 2 + pw, cy + pw / 2);
        if (state & nes::Button::LEFT) {
            list->AddRectFilled(C(cx - pd / 2, cy - pw / 2),
                    C(cx - pd / 2 + pw, cy + pw / 2),
                    CL(colors.Pressed), V(g.DPad.Radius));
            list->AddRect(C(cx - pd / 2, cy - pw / 2),
                    C(cx - pd / 2 + pw, cy + pw / 2),
                    CL(colors.Black), V(g.DPad.Radius));
        }

        ButtonRight.first = C(cx + pd / 2 - pw, cy - pw / 2);
        ButtonRight.second = C(cx + pd / 2, cy + pw / 2);
        if (state & nes::Button::RIGHT) {
            list->AddRectFilled(C(cx + pd / 2 - pw, cy - pw / 2),
                    C(cx + pd / 2, cy + pw / 2),
                    CL(colors.Pressed), V(g.DPad.Radius));
            list->AddRect(C(cx + pd / 2 - pw, cy - pw / 2),
                    C(cx + pd / 2, cy + pw / 2),
                    CL(colors.Black), V(g.DPad.Radius));
        }
    }


    {
        auto& b = g.SelectStart.Button;

        auto selectC = CL(colors.STButton);
        auto startC = CL(colors.STButton);
        if (state & nes::Button::SELECT) {
            selectC = CL(colors.Pressed);
        }
        if (state & nes::Button::START) {
            startC = CL(colors.Pressed);
        }

        float dx = g.SelectStart.InterDistance / 2;

        float minx, maxx;
        float miny, maxy;
        auto PR = [&](float tdx){
            list->PathRect(ImVec2(-b.Width/2, -b.Height/2),
                    ImVec2(b.Width/2, b.Height/2),
                    b.Height / 2);
            float ca = std::cos(b.Angle);
            float sa = std::sin(b.Angle);


            for (int i = 0; i < list->_Path.Size; i++) {
                float x = list->_Path[i].x;
                float y = list->_Path[i].y;

                float xp = x * ca - y * sa + tcx + tdx;
                float yp = x * sa + y * ca + tcy;

                list->_Path[i] = C(xp, yp);
                if (i == 0 || list->_Path[i].x < minx) minx = list->_Path[i].x;
                if (i == 0 || list->_Path[i].x > maxx) maxx = list->_Path[i].x;
                if (i == 0 || list->_Path[i].y < miny) miny = list->_Path[i].y;
                if (i == 0 || list->_Path[i].y > maxy) maxy = list->_Path[i].y;
            }
        };

        PR(-dx); list->PathFillConvex(selectC);
        ButtonSelect.first.x = minx;
        ButtonSelect.first.y = miny;
        ButtonSelect.second.x = maxx;
        ButtonSelect.second.y = maxy;
        //std::cout << minx << " " << miny << "  " << maxx << " " << maxy << std::endl;
        if (state & nes::Button::SELECT) {
            PR(-dx); list->PathStroke(CL(colors.Black), ImDrawFlags_Closed);
        }

        PR(dx); list->PathFillConvex(startC);
        ButtonStart.first.x = minx;
        ButtonStart.first.y = miny;
        ButtonStart.second.x = maxx;
        ButtonStart.second.y = maxy;
        if (state & nes::Button::START) {
            PR(dx); list->PathStroke(CL(colors.Black), ImDrawFlags_Closed);
        }
    }

    {
        auto& ab = g.ABButtons;

        float cx = g.Boundary.Width - r;
        float cy = r;

        float ca = std::cos(ab.Angle);
        float sa = std::sin(ab.Angle);

        auto bc = CL(colors.ABButton);
        auto ac = CL(colors.ABButton);
        if (state & nes::Button::B) {
            bc = CL(colors.Pressed);
        }
        if (state & nes::Button::A) {
            ac = CL(colors.Pressed);
        }

        float dx = ca * ab.InterDistance / 2;
        float dy = sa * ab.InterDistance / 2;

        float x1 = cx - dx;
        float x2 = cx + dx;
        float y1 = cy - dy;
        float y2 = cy + dy;

        ButtonB.first = C(x1 - ab.ButtonDiameter / 2, y1 - ab.ButtonDiameter / 2);
        ButtonB.second = C(x1 + ab.ButtonDiameter / 2, y1 + ab.ButtonDiameter / 2);
        ButtonA.first = C(x2 - ab.ButtonDiameter / 2, y2 - ab.ButtonDiameter / 2);
        ButtonA.second = C(x2 + ab.ButtonDiameter / 2, y2 + ab.ButtonDiameter / 2);

        list->AddCircleFilled(C(x1, y1), V(ab.ButtonDiameter / 2), bc);
        list->AddCircleFilled(C(x2, y2), V(ab.ButtonDiameter / 2), ac);

        list->AddCircle(C(x1, y1), V(ab.ButtonDiameter / 2), CL(colors.Black));
        list->AddCircle(C(x2, y2), V(ab.ButtonDiameter / 2), CL(colors.Black));
    }

    if (buttons) {
        *buttons = {ButtonA, ButtonB, ButtonSelect, ButtonStart, ButtonUp, ButtonDown, ButtonLeft, ButtonRight};
    }

    return C(g.Boundary.Width, g.Boundary.Height);
}

////////////////////////////////////////////////////////////////////////////////

NESControllerComponentOptions NESControllerComponentOptions::Defaults()
{
    NESControllerComponentOptions options;
    options.Colors = ControllerColors::Defaults();
    options.Geometry = ControllerGeometry::Defaults();
    options.DogGeometry = DogboneGeometry::Defaults();
    options.IsDogbone = false;
    options.Scale = 1.9f;
    options.AllowEdits = true;
    options.ButtonPad = 2.0f;
    return options;
}


////////////////////////////////////////////////////////////////////////////////



NESControllerComponent::NESControllerComponent(const std::string& windowName)
    : m_WindowName(windowName)
    , m_Options(NESControllerComponentOptions::Defaults())
    , m_ControllerState(0)
{
}

NESControllerComponent::~NESControllerComponent()
{
}

void NESControllerComponent::OnFrame()
{
    if (ImGui::Begin(m_WindowName.c_str())) {
        NESControllerComponent::Controls(&m_ControllerState, &m_Options);
    }
    ImGui::End();
}

bool NESControllerComponent::Controls(nes::ControllerState* state, NESControllerComponentOptions* options)
{
    ImDrawList* list = ImGui::GetWindowDrawList();
    ImVec2 ul = ImGui::GetCursorScreenPos();
    ImVec2 br;
    ButtonLocations buttonLocs;
    if (options->IsDogbone) {
        br = DrawDogbone(*state, options->Colors, options->DogGeometry,
                ul.x, ul.y, options->Scale, list, &buttonLocs);
    } else {
        br = DrawController(*state, options->Colors, options->Geometry,
                ul.x, ul.y, options->Scale, list, &buttonLocs);
    }

    ImVec2 curpos = ImGui::GetCursorScreenPos();
    uint8_t b = 1;
    for (auto & [bul, bbr] : buttonLocs) {
        ImGui::SetCursorScreenPos(ImVec2(bul.x - options->ButtonPad, bul.y - options->ButtonPad));
        ImGui::PushID(static_cast<int>(b));
        float w = bbr.x - bul.x + options->ButtonPad * 2;
        float h = bbr.y - bul.y + options->ButtonPad * 2;

        if (ImGui::InvisibleButton("button", ImVec2(w, h))) {
            nes::ToggleControllerStateButton(state, static_cast<nes::Button>(b));
        }

        b <<= 1;
    }
    ImGui::SetCursorScreenPos(curpos);

    if (ImGui::InvisibleButton("controller_invis", ImVec2(br.x - ul.x, br.y - ul.y), ImGuiButtonFlags_MouseButtonRight)) {
        ImGui::OpenPopup("cont_options");
    }
    if (ImGui::BeginPopup("cont_options")) {
        rgmui::SliderFloatExt("scale", &options->Scale, 0.1f, 5.0f);
        rgmui::SliderFloatExt("button pad", &options->ButtonPad, 0.0f, 5.0f);
        ImGui::Checkbox("dogbone", &options->IsDogbone);

        ImGui::EndPopup();
    }

    return false;
}

nes::ControllerState& NESControllerComponent::GetState()
{
    return m_ControllerState;
}

const nes::ControllerState& NESControllerComponent::GetState() const
{
    return m_ControllerState;
}

NESControllerComponentOptions& NESControllerComponent::GetOptions()
{
    return m_Options;
}

const NESControllerComponentOptions& NESControllerComponent::GetOptions() const
{
    return m_Options;
}

////////////////////////////////////////////////////////////////////////////////

NESInputsComponentOptions NESInputsComponentOptions::Defaults()
{
    NESInputsComponentOptions options;

    options.ColumnPadding = 4;
    options.ChevronColumnWidth = 18;
    options.FrameTextColumnWidth = 44;
    options.ButtonWidth = 29;
    options.FrameTextNumDigits = 6;
    options.TextColor = ImVec4(1.0f, 1.0f, 1.0f, 0.5f);
    options.HighlightTextColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    options.ButtonColor = ImVec4(215.0f / 255.0f,  25.0f / 255.0f,  25.0f / 255.0f, 1.0f);
    options.MarkerColor = ImVec4( 25.0f / 255.0f,  25.0f / 255.0f, 215.0f / 255.0f, 1.0f);
    options.DisallowLROrUD = false;
    //options.StickyAutoScroll = true;
    options.VisibleButtons = 0b11111111;
    options.AllowTargetChange = true;
    options.KeepTargetVisible = true;

    return options;
}

NESInputsComponent::NESInputsComponent(const std::string& windowName,
        NESInputsComponentOptions options)
    : m_WindowName(windowName)
    , m_Options(options)
{
    m_State.Reset();
}

NESInputsComponent::~NESInputsComponent()
{
}

void NESInputsComponent::OnFrame()
{
    if (ImGui::Begin(m_WindowName.c_str())) {
        NESInputsComponent::Controls(&m_Inputs, &m_Options, &m_State);
    }
    ImGui::End();
}

void NESInputsComponent::SetInputChangeCallback(std::function<void(int frameIndex, nes::ControllerState newState)> cback)
{
    m_State.OnInputChangeCallback = cback;
}

std::string GetInputText(int inputIndex, const NESInputsComponentOptions* options)
{
    std::ostringstream os;
    os << std::setw(options->FrameTextNumDigits) << std::setfill('0') << inputIndex;
    return os.str();
}

static ImU32 TextColor(bool highlighted, const NESInputsComponentOptions* options)
{
    return ImGui::ColorConvertFloat4ToU32((highlighted) ? options->HighlightTextColor : options->TextColor);
}

static bool DoInputChevrons(ImVec2 screenPos, int inputIndex, const NESInputsComponentOptions* options,
        const NESInputLineSize& size, NESInputsComponentState* state)
{


    ImDrawList* list = ImGui::GetWindowDrawList();
    ImVec2 a(screenPos.x + size.ChevronX, screenPos.y);
    ImVec2 b(screenPos.x + size.ChevronX + options->ChevronColumnWidth, screenPos.y + size.Height / 2);
    ImVec2 c(screenPos.x + size.ChevronX, screenPos.y + size.Height);

    ImVec2 ul(screenPos.x + size.ChevronX - options->ColumnPadding, screenPos.y);
    ImVec2 dr(screenPos.x + size.ChevronX + options->ChevronColumnWidth, screenPos.y + size.Height);

    if (ImGui::IsMouseHoveringRect(ul, dr)) {
        state->MouseWasHoveringChevron = true;
    }

    if (options->AllowTargetChange) {
        if (ImGui::IsMouseHoveringRect(ul, dr) && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            state->IsDraggingTarget = true;
        }

        if (state->IsDraggingTarget) {
            ImVec2 p = ImGui::GetMousePos();
            if (p.y >= ul.y && p.y < dr.y) {
                state->TargetIndex = inputIndex;
            }
        }
    }
    if (inputIndex == state->TargetIndex) {
        list->AddTriangleFilled(a, b, c, ImGui::ColorConvertFloat4ToU32(options->ButtonColor));
    }
    if (inputIndex == state->EmulatorIndex) {
        list->AddTriangle(a, b, c, IM_COL32_WHITE, 1.0f);
    }

    return false;
}

static void DrawShrinkedRect(ImDrawList* list, ImVec2 ul, ImVec2 lr, bool stroked, bool filled, ImVec4 fillcolor)
{
    if (!stroked && !filled) {
        return;
    }

    float rounding = (lr.y - ul.y) * 0.2;
    float shrink = rounding * 0.5;

    ImVec2 ul2(ul.x + shrink, ul.y + shrink);
    ImVec2 lr2(lr.x - shrink, lr.y - shrink);

    if (stroked) {
        list->AddRect(ul, lr, IM_COL32_WHITE, rounding, 0, 2.0f);
    }

    if (filled) {
        list->AddRectFilled(ul2, lr2, ImGui::ColorConvertFloat4ToU32(fillcolor), rounding);
    }
}

static bool DoInputText(ImVec2 screenPos, int inputIndex, const NESInputsComponentOptions* options,
        const NESInputLineSize& size, NESInputsComponentState* state, nes::ControllerState input)
{
    bool changed = false;
    ImDrawList* list = ImGui::GetWindowDrawList();
    std::string inputText = GetInputText(inputIndex, options);
    int ox = screenPos.x + size.FrameTextX;
    ImVec2 txtPos(ox, screenPos.y + 1);
    bool isHovered = false;

    ImVec2 ul(ox, screenPos.y);
    ImVec2 lr(ox + options->FrameTextColumnWidth, screenPos.y + size.Height);

    if (!rgmui::IsAnyPopupOpen() && ImGui::IsMouseHoveringRect(ul, lr)) {
        isHovered = true;

        if (ImGui::IsWindowFocused() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            state->BeginInputRowDrag(inputIndex, input);
        }
    }

    bool textHighlighted = state->HighlightInputRow(inputIndex, isHovered);
    list->AddText(txtPos, TextColor(textHighlighted, options), inputText.c_str());
    if (textHighlighted) {
        ImVec2 bul(screenPos.x + size.ButtonsX, screenPos.y);
        ImVec2 blr(screenPos.x + size.Width - options->ColumnPadding, screenPos.y + size.Height);

        DrawShrinkedRect(list, bul, blr, true, false, {});
    }

    return changed;
}

static std::string ButtonText(uint8_t button) {
    std::string bt = "A";
    if (button == nes::Button::B) {
        bt = "B";
    } else if (button == nes::Button::SELECT) {
        bt = "S";
    } else if (button == nes::Button::START) {
        bt = "T";
    } else if (button == nes::Button::UP) {
        bt = "U";
    } else if (button == nes::Button::DOWN) {
        bt = "D";
    } else if (button == nes::Button::LEFT) {
        bt = "L";
    } else if (button == nes::Button::RIGHT) {
        bt = "R";
    }
    return bt;
}


static void DrawButton(ImDrawList* list, ImVec2 ul, ImVec2 lr, uint8_t button, bool buttonOn, bool highlighted,
        const NESInputsComponentOptions* options)
{
    DrawShrinkedRect(list, ul, lr, highlighted, buttonOn, options->ButtonColor);
    std::string buttonText = ButtonText(button);
    ImVec2 txtsiz = ImGui::CalcTextSize(buttonText.c_str());
    ImVec2 txtpos(ul.x + ((lr.x - ul.x) - txtsiz.x) / 2, ul.y + 1);
    list->AddText(txtpos, TextColor(highlighted || buttonOn, options), buttonText.c_str());
}

static bool DoInputButtons(ImVec2 screenPos, int inputIndex, nes::ControllerState* input, const NESInputsComponentOptions* options,
        const NESInputLineSize& size, NESInputsComponentState* state)
{
    bool changed = false;
    ImDrawList* list = ImGui::GetWindowDrawList();

    uint8_t thisInput = *input;

    int ox = screenPos.x + size.ButtonsX;
    int oy = screenPos.y;

    int buttonCount = 0;
    for (uint8_t button = 0x01; button != 0; button <<= 1) {
        if (!(button & options->VisibleButtons)) {
            continue;
        }
        bool buttonOn = thisInput & button;

        int tx = ox + buttonCount * options->ButtonWidth;
        ImVec2 ul(tx, oy);
        ImVec2 lr(tx + options->ButtonWidth, oy + size.Height);

        bool isHovered = false;

        if (!rgmui::IsAnyPopupOpen() && ImGui::IsMouseHoveringRect(ul, lr)) {
            isHovered = true;
            if (ImGui::IsWindowFocused() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                state->BeginButtonDrag(inputIndex, button, *input);
            }
        }

        bool highlighted = state->HighlightButton(inputIndex, button, isHovered);
        DrawButton(list, ul, lr, button, buttonOn, highlighted, options);
        buttonCount++;
    }

    return changed;
}


bool NESInputsComponent::DoInputLine(int inputIndex, std::vector<nes::ControllerState>* inputs,
            NESInputsComponentOptions* options,
            const NESInputLineSize& size,
            NESInputsComponentState* state)
{
    bool changed = false;
    ImVec2 screenPos = ImGui::GetCursorScreenPos();

    ImGui::PushID(inputIndex);
    ImGui::InvisibleButton("invis", ImVec2(size.Width, size.Height));
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly)) {
        changed = state->DragTo(inputIndex, inputs) || changed;
    }

    changed = DoInputChevrons(screenPos, inputIndex, options, size, state) || changed;
    changed = DoInputText(screenPos, inputIndex, options, size, state, inputs->at(inputIndex)) || changed;
    changed = DoInputButtons(screenPos, inputIndex, &inputs->at(inputIndex), options, size, state) || changed;

    ImGui::PopID();
    return changed;
}

static void InitInputLineSize(const NESInputsComponentOptions& options, int frameHeight, NESInputLineSize* size)
{
    size->Height = frameHeight - 2;

    size->Width = 0;
    size->Width += options.ColumnPadding;
    size->ChevronX = size->Width;
    size->Width += options.ChevronColumnWidth;
    size->Width += options.ColumnPadding;
    size->FrameTextX = size->Width;
    size->Width += options.FrameTextColumnWidth;
    size->Width += options.ColumnPadding;
    size->ButtonsX = size->Width;
    size->Width += util::BitCount(options.VisibleButtons) * options.ButtonWidth;
    size->Width += options.ColumnPadding;
}

static void EnsureInputSize(std::vector<nes::ControllerState>* inputs,
        const NESInputsComponentOptions* options, NESInputsComponentState* state)
{
    size_t orig = inputs->size();
    if ((state->TargetIndex + 1) >= inputs->size()) {
        inputs->resize(state->TargetIndex + 2);
    }
    if (state->DragState != DraggingState::NOTHING &&
            (state->DraggedToIndex + 1) >= inputs->size()) {
        inputs->resize(state->DraggedToIndex + 2);
    }
    if (state->Scroll.AtBottom()) {
        inputs->insert(inputs->end(), 50, 0x00);
    }

    bool hit = false;
    bool emptyFromFiftyOnwards = true;
    int fifty = std::max(state->Scroll.LastVisibleTarget, state->TargetIndex) + 50;
    for (int i = fifty; i < static_cast<int>(inputs->size()); i++) {
        hit = true;
        if (inputs->at(i) != 0x00) {
            emptyFromFiftyOnwards = false;
            break;
        }
    }

    if (hit && emptyFromFiftyOnwards) {
        inputs->resize(fifty);
    }
    if (inputs->size() != orig && state->Scroll.AutoScroll) {
        state->Scroll.ForceAutoOn = true;
    }
}


bool NESInputsComponent::Controls(std::vector<nes::ControllerState>* inputs,
            NESInputsComponentOptions* options,
            NESInputsComponentState* state)
{
    NESInputLineSize size;
    InitInputLineSize(*options, ImGui::GetFrameHeight(), &size);

    bool changed = false;

    state->Scroll.DoScrollControls(options);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove;
    if (options->AllowTargetChange && state->MouseWasHoveringChevron) {
        flags |= ImGuiWindowFlags_NoScrollWithMouse;

        auto& io = ImGui::GetIO();
        if (io.MouseWheel != 0) {
            state->TargetIndex -= io.MouseWheel;
            if (state->TargetIndex < 0) {
                state->TargetIndex = 0;
            }
        }
    }
    ImGui::BeginChild("buttons", ImVec2(0, 0), true, flags);

    EnsureInputSize(inputs, options, state);
    state->Scroll.UpdateScroll(options, state->TargetIndex, size.Height);

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        changed = state->EndDrag(inputs) || changed;
        state->IsDraggingTarget = false;
    }

    state->MouseWasHoveringChevron = false;
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(inputs->size()));
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
            changed = NESInputsComponent::DoInputLine(i, inputs, options, size, state) || changed;
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);


    return changed;
}

void NESInputsComponentState::Reset()
{
    DraggedFromIndex = -1;
    DraggedFromIndex = -1;
    DragState = DraggingState::NOTHING;
    Button = 0x00;
    State = 0x00;
    TargetIndex = 0;
    EmulatorIndex = -1;
    IsDraggingTarget = false;
    MouseWasHoveringChevron = false;

    Scroll.Reset();
}

void NESInputsComponentState::BeginButtonDrag(int inputIndex, uint8_t button, nes::ControllerState origState)
{
    DraggedFromIndex = inputIndex;
    DraggedToIndex = inputIndex;
    DragState = DraggingState::BUTTON;
    Button = button;
    State = origState;
}

void NESInputsComponentState::BeginInputRowDrag(int inputIndex, nes::ControllerState origState)
{
    DraggedFromIndex = inputIndex;
    DraggedToIndex = inputIndex;
    DragState = DraggingState::INPUT_ROW;
    Button = 0x00;
    State = origState;
}

bool NESInputsComponentState::DragTo(int inputIndex, std::vector<nes::ControllerState>* inputs)
{
    bool changed = false;
    if (DragState == DraggingState::NOTHING) return changed;

    int changeStart = -1;
    int changeEnd = -1;
    if (inputIndex < DraggedFromIndex) {
        changeStart = inputIndex;
        changeEnd = DraggedFromIndex;
        if (DraggedFromIndex == DraggedToIndex) changeEnd++;
        DraggedFromIndex = inputIndex;
    }
    if (inputIndex > DraggedToIndex) {
        changeStart = DraggedToIndex + 1;
        changeEnd = inputIndex + 1;
        if (DraggedFromIndex == DraggedToIndex) changeStart--;
        DraggedToIndex = inputIndex;
    }

    for (int i = changeStart; i < changeEnd; i++) {
        nes::ControllerState input = inputs->at(i);

        if (DragState == DraggingState::BUTTON) {
            bool set = !(State & Button);

            if (set && !(input & Button)) {
                input |= Button;
                SetState(i, inputs, input);
                changed = true;
            } else if (!set && (input & Button)) {
                input &= ~Button;
                SetState(i, inputs, input);
                changed = true;
            }
        } else if (DragState == DraggingState::INPUT_ROW) {
            if (input != State) {
                SetState(i, inputs, State);
                changed = true;
            }
        }
    }

    return changed;
}

bool NESInputsComponentState::SetState(int inputIndex, std::vector<nes::ControllerState>* inputs,
        nes::ControllerState newState)
{
    if (!Locked && inputs->at(inputIndex) != newState) {
        // TODO undo/redo
        inputs->at(inputIndex) = newState;

        if (OnInputChangeCallback) {
            OnInputChangeCallback(inputIndex, newState);
        }


        return true;
    }
    return false;
}

bool NESInputsComponentState::EndDrag(std::vector<nes::ControllerState>* inputs)
{
    bool changed = false;
    if (DragState == DraggingState::NOTHING) return changed;

    // Handle single button toggle
    if ((DragState == DraggingState::BUTTON) && (DraggedFromIndex == DraggedToIndex)) {
        SetState(DraggedFromIndex, inputs, inputs->at(DraggedFromIndex) ^ Button);
        changed = true;
    }

    DragState = DraggingState::NOTHING;
    return changed;
}

bool NESInputsComponentState::HighlightButton(int inputIndex, uint8_t button, bool isHovered)
{
    if (DragState == DraggingState::NOTHING) return isHovered;
    if (DragState == DraggingState::BUTTON) {
        return button == Button && inputIndex >= DraggedFromIndex && inputIndex <= DraggedToIndex;
    }

    return false;
}

bool NESInputsComponentState::HighlightInputRow(int inputIndex, bool isHovered)
{
    if (DragState == DraggingState::NOTHING) return isHovered;
    if (DragState == DraggingState::INPUT_ROW) {
        return inputIndex >= DraggedFromIndex && inputIndex <= DraggedToIndex;
    }
    return false;
}

void NESInputsComponentScrollState::Reset()
{
    ForceAutoOn = false;
    AutoScroll = true;
    LastScroll = 0;
    FirstVisibleTarget = 0;
    LastVisibleTarget = 0;
}

void NESInputsComponentScrollState::DoScrollControls(const NESInputsComponentOptions* options)
{
    if (ImGui::Checkbox("auto scroll", &AutoScroll) && AutoScroll) {
        ForceAutoOn = true;
    }
}

bool NESInputsComponentScrollState::AtBottom()
{
    return ImGui::GetScrollY() == ImGui::GetScrollMaxY();
    //float y = ImGui::GetScrollY();
    //float smax = ImGui::GetScrollMaxY();

    //if (y == smax) {
    //    if (inputs->size() < options->MaxInputSize) {
    //        inputs->insert(inputs->end(), 50, 0x00);
    //    }
    //    if (inputs->size() > options->MaxInputSize) {
    //        inputs->resize(options->MaxInputSize);
    //    }
    //}

}

void NESInputsComponentScrollState::UpdateScroll(const NESInputsComponentOptions* options, int target, int height)
{
    float y = ImGui::GetScrollY();
    float y2 = y + ImGui::GetContentRegionAvail().y;
    FirstVisibleTarget = static_cast<int>(y / static_cast<float>(height));
    LastVisibleTarget = static_cast<int>(y2 / static_cast<float>(height));

    if (ForceAutoOn) {
        AutoScroll = true;
    } else if (y != LastScroll) { // User has scrolled
        AutoScroll = false;
    }
    ForceAutoOn = false;
    LastScroll = y;

    int SCROLL_BUFFER = 8;

    if (AutoScroll) {
        float ty = -1;
        if (target == 0) {
            ty = 0;
        } else if (target < (FirstVisibleTarget + SCROLL_BUFFER)) {
            ty = (target - SCROLL_BUFFER) * height;
        } else if (target > (LastVisibleTarget - SCROLL_BUFFER)) {
            ty = (target - (LastVisibleTarget - FirstVisibleTarget) + SCROLL_BUFFER) * height;
        }

        if (ty >= 0) {
            ImGui::SetScrollY(ty);
            LastScroll = ty;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

NESEmuFrameComponentOptions NESEmuFrameComponentOptions::Defaults()
{
    NESEmuFrameComponentOptions options;
    options.AllowScrollingTarget = true;
    options.AllowEditOptions = false;
    options.Scale = 2.0f;
    options.NesPalette = nes::DefaultPaletteBGR();
    options.NesPaletteP = nullptr;
    return options;
}

bool NESEmuFrameComponent::Controls(const nes::Frame& frame, NESEmuFrameComponentOptions* options, int* targetIndex)
{
    static NESEmuFrameComponentOptions defaults = NESEmuFrameComponentOptions::Defaults();
    if (!options) {
        options = &defaults;
    }

    const uint8_t* palette = options->NesPalette.data();
    if (options->NesPaletteP) {
        palette = options->NesPaletteP->data();
    }

    cv::Mat m = opencvext::ConstructPaletteImage(
            frame.data(), nes::FRAME_WIDTH, nes::FRAME_HEIGHT, palette,
            opencvext::PaletteDataOrder::BGR);
    m = opencvext::ResizePrefNearest(m, options->Scale);

    bool ret = false;
    rgmui::MatAnnotator anno("m", m);
    if (anno.IsHovered(false)) {
        if (options->AllowScrollingTarget && targetIndex) {
            auto& io = ImGui::GetIO();
            if (io.MouseWheel != 0) {
                *targetIndex -= io.MouseWheel;
                if (*targetIndex < 0) {
                    *targetIndex = 0;
                }
                ret = true;
            }
        }
    }
    return ret;
}

NESEmuNametableComponentOptions NESEmuNametableComponentOptions::Defaults()
{
    NESEmuNametableComponentOptions options;
    options.AllowEditOptions = false;
    options.Scale = 1.0f;
    options.NesPalette = nes::DefaultPaletteBGR();
    options.NesPaletteP = nullptr;
    return options;
}

bool NESEmuNametableComponent::Controls(const nes::NameTable& nt,
        const nes::PatternTable& pt, const nes::FramePalette& fp,
        NESEmuNametableComponentOptions* options)
{
    static NESEmuNametableComponentOptions defaults = NESEmuNametableComponentOptions::Defaults();
    if (!options) {
        options = &defaults;
    }

    const uint8_t* palette = options->NesPalette.data();
    if (options->NesPaletteP) {
        palette = options->NesPaletteP->data();
    }

    nes::EffectInfo effects = nes::EffectInfo::Defaults();

    nes::PPUx ppux(nes::FRAME_WIDTH, nes::FRAME_HEIGHT, nes::PPUxPriorityStatus::ENABLED);
    ppux.RenderNametable(0, 0, nes::NAMETABLE_WIDTH_BYTES, nes::NAMETABLE_HEIGHT_BYTES,
            nt.data(), nt.data() + nes::NAMETABLE_ATTRIBUTE_OFFSET,
            pt.data(), fp.data(), palette, 1, effects);
    cv::Mat m(ppux.GetHeight(), ppux.GetWidth(), CV_8UC3, ppux.GetBGROut());
    m = opencvext::ResizePrefNearest(m, options->Scale);

    rgmui::MatAnnotator anno("m", m);

    return false;
}

////////////////////////////////////////////////////////////////////////////////

nesui::smb::InputAction nesui::smb::GetActionFromKeysPressed()
{
    InputAction action = InputAction::NO_ACTON;
    if (rgmui::ShiftIsDown()) {
        if (ImGui::IsKeyPressed(ImGuiKey_2, false) || ImGui::IsKeyPressed(ImGuiKey_4, false)) {
            action = InputAction::SMB_REMOVE_LAST_JUMP;
        } else if (ImGui::IsKeyPressed(ImGuiKey_3)) {
            action = InputAction::SMB_FULL_JUMP;
        }
    } else if (ImGui::IsKeyPressed(ImGuiKey_1)) {
        action = InputAction::SMB_JUMP_EARLIER;
    } else if (ImGui::IsKeyPressed(ImGuiKey_2)) {
        action = InputAction::SMB_JUMP_LATER;
    } else if (ImGui::IsKeyPressed(ImGuiKey_3)) {
        action = InputAction::SMB_JUMP;
    } else if (ImGui::IsKeyPressed(ImGuiKey_4)) {
        action = InputAction::SMB_JUMP_SHORTER;
    } else if (ImGui::IsKeyPressed(ImGuiKey_5)) {
        action = InputAction::SMB_JUMP_LONGER;
    } else if (ImGui::IsKeyPressed(ImGuiKey_T)) {
        action = InputAction::SMB_START;
    }

    return action;
}

std::pair<int, int> nesui::smb::FindPreviousJump(int targetindex, const std::vector<nes::ControllerState>& inputs)
{
    bool foundEnd = false;
    int start = 0;
    int end = 0;
    for (int i = targetindex; i >= 0; i--) {
        if (!foundEnd && inputs[i] & nes::Button::A) {
            end = i;
            foundEnd = true;
        } else if (foundEnd && !(inputs[i] & nes::Button::A)) {
            start = i + 1;
            break;
        }
    }
    if (foundEnd) {
        while (end < (inputs.size()) && inputs[end] & nes::Button::A) {
            end++;
        }
    }
    return std::make_pair(start, end);
}


bool nesui::smb::DoAction(nesui::smb::InputAction action,
        std::vector<nes::ControllerState>* inputs,
        NESInputsComponentState* state
        )
{
    bool changed = false;
    int target = state->TargetIndex;
    auto ChangeInputTo = [&](int index, nes::ControllerState newState) {
        changed = true;
        state->SetState(index, inputs, newState);
    };
    switch(action) {
        case InputAction::SMB_JUMP_EARLIER: {
            auto [from, to] = FindPreviousJump(target, *inputs);
            if (from != to && from > 0) {
                ChangeInputTo(from - 1, (*inputs)[from - 1] | nes::Button::A);
            }
            break;
        }
        case InputAction::SMB_JUMP_LATER: {
            auto [from, to] = FindPreviousJump(target, *inputs);
            if (from != to && to > (from + 1)) {
                ChangeInputTo(from, (*inputs)[from] & ~nes::Button::A);
            }
            break;
        }
        case InputAction::SMB_JUMP: {
            if (target >= 2) {
                if ((*inputs)[target - 2] & nes::Button::A) {
                    auto [from, to] = FindPreviousJump(target, *inputs);
                    ChangeInputTo(to, (*inputs)[to] | nes::Button::A);
                } else {
                    ChangeInputTo(target - 2, (*inputs)[target - 2] | nes::Button::A);
                }
            }
            break;
        }
        case InputAction::SMB_JUMP_SHORTER: {
            auto [from, to] = FindPreviousJump(target, *inputs);
            if (from != to && to > 0 && to > (from + 1)) {
                ChangeInputTo(to - 1, (*inputs)[to - 1] & ~nes::Button::A);
            }
            break;
        }
        case InputAction::SMB_JUMP_LONGER: {
            auto [from, to] = FindPreviousJump(target, *inputs);
            if (from != to) {
                ChangeInputTo(to, (*inputs)[to] | nes::Button::A);
            }
            break;
        }
        case InputAction::SMB_START: {
            if (target >= 2) {
                int t = target - 2;
                if (!((*inputs)[t] & nes::Button::START)) {
                    ChangeInputTo(t, (*inputs)[t] | nes::Button::START);
                }
            }
            break;
        }
        case InputAction::SMB_REMOVE_LAST_JUMP: {
            auto [from, to] = FindPreviousJump(target, *inputs);
            if (from != to) {
                for (int i = from; i < to; i++) {
                    ChangeInputTo(i, (*inputs)[i] & ~nes::Button::A);
                }
                //ConsolidateLast(to - from); TODO undo/redo
            }
            break;
        }
        case InputAction::SMB_FULL_JUMP: {
            if (target >= 2 && (*inputs).size() > (target + 36)) {
                // This would be properly based on the subspeed in $0057,
                // if frame s - 1, ram[$0057] <= 15 or >= 25:
                //   jump 32
                // else:
                //   jump 35
                // (according to slither)
                // But I don't really have access to the emulator here
                int s = target - 2;
                if ((*inputs)[s] & nes::Button::A) {
                    auto [from, to] = FindPreviousJump(target, *inputs);
                    s = from;
                }

                int nchanges = 0;
                for (int i = 0; i < 35; i++) {
                    if (!((*inputs)[s + i] & nes::Button::A)) {
                        nchanges++;
                        ChangeInputTo(s + i, (*inputs)[s + i] | nes::Button::A);
                    }
                }
                //m_UndoRedo.ConsolidateLast(nchanges); TODO undo/redo
            }
            break;
        }
        default:
            break;
    }
    return changed;
}

NESTASComponent::NESTASComponent(const char* name)
    : m_Name(name)
    , m_UserData(nullptr)
{
    m_InputsState.Locked = true;
    m_InputsState.Reset();
    m_InputsState.OnInputChangeCallback = [&](int frame_index, nes::ControllerState new_state){
        if (m_StateThread) {
            m_StateThread->InputChange(frame_index, new_state);
        }
    };
    m_FrameOptions = NESEmuFrameComponentOptions::Defaults();
    m_FrameOptions.Scale = 1.0;
    m_InputsOptions = NESInputsComponentOptions::Defaults();
}

NESTASComponent::~NESTASComponent()
{
}

void NESTASComponent::OnFrame()
{
    if (ImGui::Begin(m_Name.c_str())) {
        if (!m_StateThread) {
            ImGui::TextUnformatted("no thread initialized");
        } else {
            std::string new_state;
            m_StateThread->TargetChange(m_InputsState.TargetIndex);
            if (m_StateThread->HasNewState(nullptr, &new_state)) {
                if (!new_state.empty()) {
                    m_Emulator.LoadStateString(new_state);
                    m_Emulator.ScreenPeekFrame(&m_Frame);
                }
            }
            NESEmuFrameComponent::Controls(m_Frame, &m_FrameOptions, &m_InputsState.TargetIndex);
            ImGui::Checkbox("Locked", &m_InputsState.Locked);
            ImGui::SameLine();
            NESInputsComponent::Controls(&m_Inputs, &m_InputsOptions, &m_InputsState);
        }
    }
    ImGui::End();
}

void NESTASComponent::ClearTAS() {
    m_InputsState.Reset();
    m_StateThread = nullptr;
}

void NESTASComponent::SetTAS(void* user_data,
                const uint8_t* rom, size_t rom_size,
                std::string start_string,
                const std::vector<nes::ControllerState>& init_states)
{
    m_Inputs = init_states;
    m_InputsState.Reset();
    m_Emulator.LoadINESData(rom, rom_size);
    auto p = std::make_unique<nes::NestopiaNESEmulator>();
    p->LoadINESData(rom, rom_size);
    if (!start_string.empty()) {
        p->LoadStateString(start_string);
    }
    m_StateThread = std::make_unique<nes::StateSequenceThread>(
            nes::StateSequenceThreadConfig::Defaults(),
            std::move(p), init_states);
}
