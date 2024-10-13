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

#ifndef ARGOS_SMB_SMBDBUI_HEADER
#define ARGOS_SMB_SMBDBUI_HEADER

#include "smb/smbdb.h"
#include "rgmui/rgmui.h"
#include "ext/sdlext/sdlext.h"

namespace argos::smbui
{

class SMBDatabaseApplication : public rgmui::IApplication
{
public:
    SMBDatabaseApplication(smb::SMBDatabase* db);
    ~SMBDatabaseApplication();
};

class SMBDBSoundComponent : public rgmui::IApplicationComponent
{
public:
    SMBDBSoundComponent(smb::SMBDatabase* db);
    ~SMBDBSoundComponent();

    void OnFrame();

private:
    void DoSoundEffectControls();
    std::shared_ptr<sdlext::SDLExtMixChunk> GetChunk(smb::SoundEffect effect);

private:
    smb::SMBDatabase* m_Database;
    std::unordered_map<smb::SoundEffect, std::shared_ptr<sdlext::SDLExtMixChunk>> m_Chunks;
};

class SMBDBMusicComponent : public rgmui::IApplicationComponent
{
public:
    SMBDBMusicComponent(smb::SMBDatabase* db);
    ~SMBDBMusicComponent();

    void OnFrame();

private:
    void DoMusicControls();
    std::shared_ptr<sdlext::SDLExtMixMusic> GetMusic(smb::MusicTrack track);

private:
    smb::SMBDatabase* m_Database;
    std::unordered_map<smb::MusicTrack, std::shared_ptr<sdlext::SDLExtMixMusic>> m_Musics;
};

class SMBDBNametableMiniComponent : public rgmui::IApplicationComponent
{
public:
    SMBDBNametableMiniComponent(smb::SMBDatabase* db);
    ~SMBDBNametableMiniComponent();

    void OnFrame();

private:
    smb::SMBDatabase* m_Database;
};

};

#endif

