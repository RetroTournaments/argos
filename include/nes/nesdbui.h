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

#ifndef ARGOS_NES_NESDBUI_HEADER
#define ARGOS_NES_NESDBUI_HEADER

#include <vector>
#include <string>

#include "nes/nesui.h"
#include "nes/nesdb.h"
#include "rgmui/rgmui.h"

namespace argos::nesui
{

class NESDBComponent : public rgmui::IApplicationComponent
{
public:
    NESDBComponent(nes::NESDatabase* db);
    ~NESDBComponent();
};

class NESDBRomComponent : public rgmui::IApplicationComponent
{
public:
    NESDBRomComponent(nes::NESDatabase* db);
    ~NESDBRomComponent();

    void OnFrame();

private:
    void Refresh();
    void DoInsertControls();
    void SetIDToDelete(int id);

private:
    nes::NESDatabase* m_Database;

    std::vector<nes::db::nes_rom> m_Roms;
    int m_RomToDelete;

    std::string m_PendingName;
    std::string m_InsertMessage;
    std::string m_RomPath;
    std::vector<uint8_t> m_LoadedRom;
};

class NESDBTASComponent : public rgmui::IApplicationComponent
{
public:
    NESDBTASComponent(nes::NESDatabase* db);
    ~NESDBTASComponent();

    void OnFrame();

private:
    void Refresh();
    void SetIDToDelete(int id);
    void ChangeName(int id, const std::string& name);
    bool Locked();

    void SetSubComponentTAS();
    bool SetSubComponentTAS(const nes::db::nes_tas& tas); 

private:
    nes::NESDatabase* m_Database;
    std::shared_ptr<NESTASComponent> m_NESTasComponent;
    int m_IDForComponent;

    int m_TasToDelete;
    int m_SelectedTASID;
    std::vector<nes::db::nes_tas> m_NESTasInfo;

    int m_PendingROMID;
    std::string m_PendingName;
    bool m_Locked;
};

}

#endif
