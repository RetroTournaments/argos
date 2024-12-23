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

bool AreaIDCombo(const char* label, smb::AreaID* id);

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

class SMBDBNametablePageComponent : public rgmui::IApplicationComponent
{
public:
    SMBDBNametablePageComponent(smb::SMBDatabase* db);
    ~SMBDBNametablePageComponent();

    void OnFrame();

private:
    void RefreshPage();

private:
    nes::NESDatabase::RomSPtr m_Rom;
    smb::SMBNametableCachePtr m_Cache;
    smb::AreaID m_AreaID;
    uint8_t m_Page;

    cv::Mat m_NametableMat;
    cv::Mat m_MinimapMat;
};

class SMBDBRouteComponent : public rgmui::IApplicationComponent
{
public:
    SMBDBRouteComponent(smb::SMBDatabase* db);
    ~SMBDBRouteComponent();

    void OnFrame();

private:
    void RefreshView();

private:
    smb::SMBDatabase* m_Database;
    smb::SMBNametableCachePtr m_Cache;

    std::vector<std::string> m_RouteNames;
    smb::db::route m_Route;
    int m_XLoc;

    cv::Mat m_ViewMat;
};

};

#endif

