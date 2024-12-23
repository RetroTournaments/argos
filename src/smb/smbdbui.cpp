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

#include "smb/smbdbui.h"
#include "nes/nesdbui.h"
#include "nes/ppux.h"
#include "ext/nfdext/nfdext.h"
#include "ext/sdlext/sdlextui.h"

using namespace argos;
using namespace argos::smbui;
using namespace argos::smb;

bool argos::smbui::AreaIDCombo(const char* label, smb::AreaID* id)
{
    bool changed = false;
    if (!id) {
        throw std::invalid_argument("null id disallowed");
    }
    const auto& area_ids = KnownAreaIDs();
    size_t idx = 0;
    for (auto & aid : area_ids) {
        if (aid == *id) {
            break;
        }
        idx++;
    }

    if (ImGui::BeginCombo(label, ToString(*id).c_str())) {
        for (auto & aid : area_ids) {
            bool selected = *id == aid;
            if (ImGui::Selectable(ToString(aid).c_str(), selected)) {
                *id = aid;
                changed = true;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    } else {
        if (rgmui::HandleCombo4Scroll(&idx, area_ids.size())) {
            *id = area_ids[idx];
            changed = true;
        }
    }
    return changed;
}

SMBDatabaseApplication::SMBDatabaseApplication(SMBDatabase* db)
{
    RegisterComponent(std::make_shared<nesui::NESDBComponent>(db));

    RegisterComponent(std::make_shared<sdlextui::SDLExtMixComponent>());
    RegisterComponent(std::make_shared<smbui::SMBDBSoundComponent>(db));
    RegisterComponent(std::make_shared<smbui::SMBDBMusicComponent>(db));
    RegisterComponent(std::make_shared<smbui::SMBDBNametablePageComponent>(db));
    RegisterComponent(std::make_shared<smbui::SMBDBRouteComponent>(db));
}

SMBDatabaseApplication::~SMBDatabaseApplication()
{
}

////////////////////////////////////////////////////////////////////////////////

SMBDBSoundComponent::SMBDBSoundComponent(smb::SMBDatabase* db)
    : m_Database(db)
{
}

SMBDBSoundComponent::~SMBDBSoundComponent()
{
}

void SMBDBSoundComponent::OnFrame()
{
    if (ImGui::Begin("smb_sound")) {
        DoSoundEffectControls();
    }
    ImGui::End();
}

std::shared_ptr<sdlext::SDLExtMixChunk> SMBDBSoundComponent::GetChunk(smb::SoundEffect effect)
{
    std::shared_ptr<sdlext::SDLExtMixChunk> ptr;

    std::vector<uint8_t> wav_data;
    if (m_Database->GetSoundEffectWav(effect, &wav_data)) {
        ptr = std::make_shared<sdlext::SDLExtMixChunk>(wav_data);
    }

    return ptr;
}


void SMBDBSoundComponent::DoSoundEffectControls()
{
    if (ImGui::Button("init chunks")) {
        for (auto & effect : smb::AudibleSoundEffects()) {
            auto ptr = GetChunk(effect);
            if (ptr) {
                m_Chunks[effect] = ptr;
            }
        }
    }

    for (auto & effect : smb::AudibleSoundEffects()) {
        auto it = m_Chunks.find(effect);
        if (it != m_Chunks.end()) {
            rgmui::TextFmt("{:32s} {:08x}", smb::ToString(effect), static_cast<uint32_t>(effect));
            ImGui::SameLine();
            ImGui::PushID(static_cast<uint32_t>(effect));
            if (ImGui::SmallButton("play")) {
                Mix_PlayChannel(-1, it->second->Chunk, 0);
            }
            ImGui::PopID();
        }
    }

    if (ImGui::CollapsingHeader("insert sound effects")) {
        for (auto & effect : smb::AudibleSoundEffects()) {
            rgmui::TextFmt("{:32s} {:08x}", smb::ToString(effect), static_cast<uint32_t>(effect));
            ImGui::SameLine();
            ImGui::PushID(static_cast<uint32_t>(effect));
            if (ImGui::SmallButton("replace")) {
                std::string path;
                if (nfdext::FileOpenDialog(&path)) {
                    InsertSoundEffect(m_Database, effect, path);
                    m_Chunks[effect] = GetChunk(effect);
                }
            }
            ImGui::PopID();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

SMBDBMusicComponent::SMBDBMusicComponent(smb::SMBDatabase* db)
    : m_Database(db)
{
}

SMBDBMusicComponent::~SMBDBMusicComponent()
{
}

void SMBDBMusicComponent::OnFrame()
{
    if (ImGui::Begin("smb_music")) {
        DoMusicControls();
    }
    ImGui::End();
}

std::shared_ptr<sdlext::SDLExtMixMusic> SMBDBMusicComponent::GetMusic(smb::MusicTrack track)
{
    std::shared_ptr<sdlext::SDLExtMixMusic> ptr;

    std::vector<uint8_t> wav_data;
    if (m_Database->GetMusicTrackWav(track, &wav_data)) {
        ptr = std::make_shared<sdlext::SDLExtMixMusic>(wav_data);
    }

    return ptr;
}

void SMBDBMusicComponent::DoMusicControls()
{
    if (ImGui::Button("init music")) {
        for (auto & track : smb::AudibleMusicTracks()) {
            auto ptr = GetMusic(track);
            if (ptr) {
                m_Musics[track] = ptr;
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("stop music")) {
        Mix_HaltMusic();
    }

    for (auto & track : smb::AudibleMusicTracks()) {
        auto it = m_Musics.find(track);
        if (it != m_Musics.end()) {
            rgmui::TextFmt("{:32s} {:08x}", smb::ToString(track), static_cast<uint32_t>(track));
            ImGui::SameLine();
            ImGui::PushID(static_cast<uint32_t>(track));
            if (ImGui::SmallButton("play")) {
                Mix_HaltMusic();
                Mix_PlayMusic(it->second->Music, 0);
            }
            ImGui::PopID();
        }
    }

    if (ImGui::CollapsingHeader("insert music track")) {
        for (auto & track : smb::AudibleMusicTracks()) {
            rgmui::TextFmt("{:32s} {:08x}", smb::ToString(track), static_cast<uint32_t>(track));
            ImGui::SameLine();
            ImGui::PushID(static_cast<uint32_t>(track));
            if (ImGui::SmallButton("replace")) {
                std::string path;
                if (nfdext::FileOpenDialog(&path)) {
                    InsertMusicTrack(m_Database, track, path);
                    m_Musics[track] = GetMusic(track);
                }
            }
            ImGui::PopID();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

SMBDBNametablePageComponent::SMBDBNametablePageComponent(smb::SMBDatabase* db)
    : m_Rom(db->GetBaseRom())
    , m_Cache(db->GetNametableCache())
    , m_AreaID(smb::AreaID::GROUND_AREA_6)
    , m_Page(0x01)
{
    RefreshPage();
}

SMBDBNametablePageComponent::~SMBDBNametablePageComponent()
{
}

void SMBDBNametablePageComponent::OnFrame()
{
    if (ImGui::Begin("smb_nametable_page")) {
        if (smbui::AreaIDCombo("Area ID", &m_AreaID)) {
            RefreshPage();
        }
        if (rgmui::SliderUint8Ext("Page", &m_Page, 0x00, 0x20)) {
            RefreshPage();
        }
        rgmui::MatAnnotator anno("nametable_pages", m_NametableMat);
        if (anno.IsHovered()) {
            if (rgmui::HoverScroll<uint8_t>(&m_Page, 0x00, 0x20)) {
                RefreshPage();
            }
        }
        rgmui::MatAnnotator anno2("minimap_pages", m_MinimapMat);
        if (anno.IsHovered()) {
            if (rgmui::HoverScroll<uint8_t>(&m_Page, 0x00, 0x20)) {
                RefreshPage();
            }
        }
    }
    ImGui::End();
}

void SMBDBNametablePageComponent::RefreshPage()
{
    m_NametableMat = cv::Mat::zeros(nes::FRAME_HEIGHT, nes::FRAME_WIDTH * 3, CV_8UC3);
    nes::PPUx ppux(m_NametableMat.cols, m_NametableMat.rows, m_NametableMat.data,
            nes::PPUxPriorityStatus::DISABLED);
    for (int i = 0; i < 3; i++) {
        if (i == 0 && m_Page == 0) {
            continue;
        }

        uint8_t p = m_Page + i - 1;
        if (m_Cache->KnownNametable(m_AreaID, p)) {
            const auto& ntpage = m_Cache->GetNametable(m_AreaID, p);

            ppux.RenderNametable(nes::FRAME_WIDTH * i, 0,
                    nes::NAMETABLE_WIDTH_BYTES, nes::NAMETABLE_HEIGHT_BYTES,
                    ntpage.nametable.data(),
                    ntpage.nametable.data() + nes::NAMETABLE_ATTRIBUTE_OFFSET,
                    smb::rom_chr1(m_Rom), ntpage.frame_palette.data(),
                    nes::DefaultPaletteBGR().data(), 1,
                    nes::EffectInfo::Defaults());
        }
    }

    m_MinimapMat = cv::Mat::zeros(nes::FRAME_HEIGHT, nes::FRAME_WIDTH * 3, CV_8UC3);
    nes::PPUx ppux2(m_MinimapMat.cols, m_MinimapMat.rows, m_MinimapMat.data,
            nes::PPUxPriorityStatus::DISABLED);
    ppux2.FillBackground(nes::PALETTE_ENTRY_WHITE,
            nes::DefaultPaletteBGR().data());
    for (int i = 0; i < 3; i++) {
        if (i == 0 && m_Page == 0) {
            continue;
        }

        uint8_t p = m_Page + i - 1;
        if (m_Cache->KnownMinimap(m_AreaID, p)) {
            const auto& minipage = m_Cache->GetMinimap(m_AreaID, p);

            smb::RenderMinimapToPPUx(nes::FRAME_WIDTH * i, 0, 0, nes::FRAME_WIDTH,
                    minipage.minimap, smb::DefaultMinimapPalette(),
                    nes::DefaultPaletteBGR(), &ppux2);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

SMBDBRouteComponent::SMBDBRouteComponent(smb::SMBDatabase* db)
    : m_Database(db)
    , m_Cache(db->GetNametableCache())
{
    m_Database->GetRouteNames(&m_RouteNames);
    if (!m_RouteNames.empty()) {
        m_Database->GetRoute(m_RouteNames.front(), &m_Route);
    }
    m_XLoc = 0;
    RefreshView();
}

SMBDBRouteComponent::~SMBDBRouteComponent()
{
}

void SMBDBRouteComponent::OnFrame()
{
    if (ImGui::Begin("smb_route")) {
        if (rgmui::Combo4("route", &m_Route.name, m_RouteNames, m_RouteNames)) {
            m_Database->GetRoute(m_Route.name, &m_Route);
            RefreshView();
        }
        int xmx = m_Route.route.last_x(m_ViewMat.cols);
        if (rgmui::SliderIntExt("XLoc", &m_XLoc, 0, xmx)) {
            RefreshView();
        }
        rgmui::MatAnnotator anno("route", m_ViewMat);

    }
    ImGui::End();
}

void SMBDBRouteComponent::RefreshView()
{
    m_ViewMat = cv::Mat::zeros(nes::FRAME_HEIGHT, nes::FRAME_WIDTH * 3, CV_8UC3);
    nes::PPUx ppux(m_ViewMat.cols, m_ViewMat.rows, m_ViewMat.data,
            nes::PPUxPriorityStatus::DISABLED);
    ppux.FillBackground(nes::PALETTE_ENTRY_WHITE,
            nes::DefaultPaletteBGR().data());

    std::vector<WorldSection> visible;
    m_Route.route.GetVisibleSections(m_XLoc, m_ViewMat.cols, &visible);

    for (auto & vsec : visible) {
        uint8_t lp = vsec.LeftPage();
        for (uint8_t p = lp; p < vsec.RightPage(); p++) {
            const auto* minipage = m_Cache->MaybeGetMinimap(vsec.AID, p);
            if (minipage) {
                smb::RenderMinimapToPPUx(vsec.XLoc + 256 * (p - lp), 0,
                        vsec.PageLeft(p), vsec.PageRight(p),
                        minipage->minimap, smb::DefaultMinimapPalette(),
                        nes::DefaultPaletteBGR(), &ppux);
            }
        }
    }
}
