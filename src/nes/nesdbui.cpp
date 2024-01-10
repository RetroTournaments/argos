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

#include "nes/nesdbui.h"
#include "ext/nfdext/nfdext.h"
#include "game/gamedbui.h"

#include "util/string.h"
#include "util/file.h"

using namespace argos;
using namespace argos::nesui;

NESDBComponent::NESDBComponent(nes::NESDatabase* db)
{
    RegisterSubComponent(std::make_shared<game::ui::GameDBComponent>(db));
    RegisterSubComponent(std::make_shared<nesui::NESDBRomComponent>(db));
    RegisterSubComponent(std::make_shared<nesui::NESDBTASComponent>(db));
}

NESDBComponent::~NESDBComponent()
{
}

////////////////////////////////////////////////////////////////////////////////

NESDBRomComponent::NESDBRomComponent(nes::NESDatabase* db)
    : m_Database(db)
{
    Refresh(); // TODO if slow.. remove? or only size?
}

NESDBRomComponent::~NESDBRomComponent()
{
}

void NESDBRomComponent::SetIDToDelete(int id)
{
    m_RomToDelete = id;
}

void NESDBRomComponent::OnFrame()
{
    if (ImGui::Begin("nes_rom")) {
        if (!rgmui::IsAnyPopupOpen()) {
            m_RomToDelete = -1;
        }
        sqliteext::ui::DoDBViewTable<nes::db::nes_rom>("db::nes_rom", &m_Roms, nullptr,
                4, [](void*){
                    ImGui::TableSetupColumn("id    ", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("name         ", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("rom size ", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("         ", ImGuiTableColumnFlags_WidthFixed);
                }, 
                [](nes::db::nes_rom* rom,
                    bool* selected,
                    int* scroll,
                    void* nesdbromcomponent)
                {
                    using namespace sqliteext::ui;
                    bool changed = false;
                    IntColumn(0, &rom->id, selected, BasicColumnType::READONLY, &changed, scroll);
                    TextColumn(1, &rom->name, selected, BasicColumnType::READONLY, &changed, scroll);
                    std::string size = util::BytesFmt(rom->rom.size());
                    TextColumn(2, &size, selected, BasicColumnType::READONLY, &changed, scroll);
                    if (ButtonColumn(3, "delete", scroll)) {
                        NESDBRomComponent* comp = reinterpret_cast<NESDBRomComponent*>(nesdbromcomponent);
                        comp->SetIDToDelete(rom->id);
                    }
                
                    return false;
                }, 
                this);

        if (m_RomToDelete >= 0) {
            ImGui::OpenPopup("are_you_sure");
        }
        if (ImGui::BeginPopupModal("are_you_sure")) {
            ImGui::TextUnformatted("delete rom (and all associated TASes / everything)");
            ImGui::TextUnformatted("are you sure...?");
            if (rgmui::PopupYesNoHelper([&](){
                m_Database->DeleteROM(m_RomToDelete);
            })) {
                m_RomToDelete = -1;
            }
            ImGui::EndPopup();
        }

        if (ImGui::CollapsingHeader("insert new rom")) {
            DoInsertControls();
        }
    }
    ImGui::End();
}

void NESDBRomComponent::Refresh()
{
    m_Database->SelectAllROMs(&m_Roms);
}

void NESDBRomComponent::DoInsertControls()
{
    rgmui::InputText("pending name", &m_PendingName);
    if (m_RomPath.empty()) {
        if (nfdext::FileOpenButton("select rom path", &m_RomPath)) {
            try {
                util::ReadFileToVector(m_RomPath, &m_LoadedRom);
                if (m_LoadedRom.size() < 32) {
                    m_InsertMessage = "File too small: '" + util::BytesFmt(m_LoadedRom.size()) + "'";
                    m_RomPath.clear();
                } else if (
                        m_LoadedRom[0] != 0x4e ||
                        m_LoadedRom[1] != 0x45 ||
                        m_LoadedRom[2] != 0x53 ||
                        m_LoadedRom[3] != 0x1a) {
                    m_InsertMessage = "File does not start with 'NES<eof>': '" + m_RomPath + "'";
                    m_RomPath.clear();
                } else {
                    m_InsertMessage.clear();
                }
            } catch (std::invalid_argument except) {
                m_InsertMessage = "File ifstream not good: '" + m_RomPath + "'";
                m_RomPath.clear();
            }
        }
    } else {
        ImGui::TextUnformatted(m_RomPath.c_str());
        ImGui::TextUnformatted(util::BytesFmt(m_LoadedRom.size()).c_str());
    }
    if (!m_InsertMessage.empty()) {
        ImGui::TextUnformatted(m_InsertMessage.c_str());
    }

    ImGui::BeginDisabled(m_PendingName.empty() || m_LoadedRom.size() < 32);
    if (ImGui::Button("insert")) {
        m_Database->InsertROM(m_PendingName, m_LoadedRom);
        Refresh();
    }
    ImGui::EndDisabled();
}

////////////////////////////////////////////////////////////////////////////////


NESDBTASComponent::NESDBTASComponent(nes::NESDatabase* db)
    : m_Database(db)
    , m_SelectedTASID(-1)
    , m_TasToDelete(-1)
    , m_PendingROMID(1)
{
    Refresh();
    //m_NESTasComponent = std::make_shared<NESTASComponent>("selected_nes_tas");
    //RegisterSubComponent(m_NESTasComponent);
}

NESDBTASComponent::~NESDBTASComponent()
{
}

void NESDBTASComponent::Refresh()
{
    m_Database->SelectAllTasesLight(&m_NESTasInfo);
}

void NESDBTASComponent::SetIDToDelete(int id)
{
    m_TasToDelete = id;
}

void NESDBTASComponent::ChangeName(int id, const std::string& name)
{
    m_Database->UpdateTASName(id, name);
}

void NESDBTASComponent::OnFrame()
{
    if (ImGui::Begin("nes_tas")) {
        m_TasToDelete = -1;
        sqliteext::ui::DoDBViewTable<nes::db::nes_tas>("db::nes_tas", &m_NESTasInfo, &m_SelectedTASID,
            4, 
            [](void*){
                ImGui::TableSetupColumn("id    ", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("rom_id", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("name         ", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("         ", ImGuiTableColumnFlags_WidthFixed);
            }, 
            [](nes::db::nes_tas* tas,
                bool* selected,
                int* scroll,
                void* nesdbtascomp)
            {
                using namespace sqliteext::ui;
                bool changed = false;
                IntColumn(0, &tas->id, selected, BasicColumnType::READONLY, &changed, scroll);
                IntColumn(1, &tas->rom_id, selected, BasicColumnType::READONLY, &changed, scroll);
                TextColumn(2, &tas->name, selected, BasicColumnType::EDITABLE, &changed, scroll);
                if (ButtonColumn(3, "delete", scroll)) {
                    NESDBTASComponent* comp = reinterpret_cast<NESDBTASComponent*>(nesdbtascomp);
                    comp->SetIDToDelete(tas->id);
                }
                if (changed) {
                    NESDBTASComponent* comp = reinterpret_cast<NESDBTASComponent*>(nesdbtascomp);
                    comp->ChangeName(tas->id, tas->name);
                }
                return changed;
            }, 
            this);
        if (m_TasToDelete >= 0) {
            m_Database->DeleteTAS(m_TasToDelete);
            Refresh();
        }

        if (ImGui::CollapsingHeader("insert new tas")) {
            ImGui::InputInt("rom id", &m_PendingROMID);
            rgmui::InputText("name", &m_PendingName);
            if (ImGui::Button("insert")) {
                m_SelectedTASID = m_Database->InsertNewTAS(m_PendingROMID, m_PendingName);
                Refresh();
            }
        }
    }
    ImGui::End();

    OnSubComponentFrames();
}

////////////////////////////////////////////////////////////////////////////////
