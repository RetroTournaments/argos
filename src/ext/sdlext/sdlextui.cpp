////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2024 Matthew Deutsch
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

#include "ext/sdlext/sdlextui.h"

using namespace sdlextui;

SDLExtMixComponent::SDLExtMixComponent()
{
}

SDLExtMixComponent::~SDLExtMixComponent()
{
}

void SDLExtMixComponent::OnFrame()
{
    if (ImGui::Begin("sdlext_mix")) {
        DoMixControls();
    }
    ImGui::End();
}

void SDLExtMixComponent::DoMixControls()
{
    int vol = Mix_MasterVolume(-1);
    if (sta::rgmui::SliderIntExt("Mix_MasterVolume", &vol, 0, MIX_MAX_VOLUME)) {
        Mix_MasterVolume(vol);
    }

    vol = Mix_VolumeMusic(-1);
    if (sta::rgmui::SliderIntExt("Mix_VolumeMusic", &vol, 0, MIX_MAX_VOLUME)) {
        Mix_VolumeMusic(vol);
    }

    if (!ImGui::CollapsingHeader("extra")) return;

    if (ImGui::Button("Mix_PauseAudio(0)")) {
        Mix_PauseAudio(0);
    }
    ImGui::SameLine();
    if (ImGui::Button("Mix_PauseAudio(1)")) {
        Mix_PauseAudio(1);
    }
    sta::rgmui::TextFmt("Mix_AllocateChannels(-1): {}", Mix_AllocateChannels(-1));

    if (ImGui::Button("Mix_HaltChannel(-1)")) {
        Mix_HaltChannel(-1);
    }
    if (ImGui::Button("Mix_HaltMusic()")) {
        Mix_HaltMusic();
    }
    if (ImGui::Button("Mix_Pause(-1)")) {
        Mix_Pause(-1);
    }
    ImGui::SameLine();
    if (ImGui::Button("Mix_Resume(-1)")) {
        Mix_Resume(-1);
    }
    if (ImGui::Button("Mix_PauseMusic()")) {
        Mix_PauseMusic();
    }
    ImGui::SameLine();
    if (ImGui::Button("Mix_ResumeMusic()")) {
        Mix_ResumeMusic();
    }
}

