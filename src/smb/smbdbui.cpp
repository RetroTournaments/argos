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
#include "ext/nfdext/nfdext.h"
#include "ext/sdlext/sdlextui.h"

using namespace argos;
using namespace argos::smbui;
using namespace argos::smb;

SMBDatabaseApplication::SMBDatabaseApplication(SMBDatabase* db)
{
    RegisterComponent(std::make_shared<nesui::NESDBComponent>(db));

    RegisterComponent(std::make_shared<sdlextui::SDLExtMixComponent>());
    RegisterComponent(std::make_shared<smbui::SMBDBSoundComponent>(db));
    RegisterComponent(std::make_shared<smbui::SMBDBMusicComponent>(db));
    RegisterComponent(std::make_shared<smbui::SMBDBNametableMiniComponent>(db));
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
        for (auto & track : smb::AllMusicTracks()) {
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

    for (auto & track : smb::AllMusicTracks()) {
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

    if (ImGui::CollapsingHeader("insert sound effects")) {
        for (auto & track : smb::AllMusicTracks()) {
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


SMBDBNametableMiniComponent::SMBDBNametableMiniComponent(smb::SMBDatabase* db)
    : m_Database(db)
{
}

SMBDBNametableMiniComponent::~SMBDBNametableMiniComponent()
{
}

void SMBDBNametableMiniComponent::OnFrame()
{
    if (ImGui::Begin("smb_nametable_mini")) {
    }
    ImGui::End();
}
