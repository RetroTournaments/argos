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

#include <fstream>

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
    , m_IDForComponent(-1)
    , m_PendingROMID(1)
    , m_Locked(true)
{
    Refresh();
    m_NESTasComponent = std::make_shared<NESTASComponent>("selected_nes_tas");
    RegisterSubComponent(m_NESTasComponent);
}

NESDBTASComponent::~NESDBTASComponent()
{
}

void NESDBTASComponent::Refresh()
{
    m_Database->SelectAllTasesLight(&m_NESTasInfo);
    bool fnd = false;
    for (auto & v : m_NESTasInfo) {
        if (v.id == m_SelectedTASID) {
            fnd = true;
            break;
        }
    }
    if (!fnd) {
        m_SelectedTASID = -1;
    }
}

void NESDBTASComponent::SetIDToDelete(int id)
{
    m_TasToDelete = id;
}

void NESDBTASComponent::ChangeName(int id, const std::string& name)
{
    m_Database->UpdateTASName(id, name);
}

bool NESDBTASComponent::Locked()
{
    return m_Locked;
}

void NESDBTASComponent::OnFrame()
{
    if (ImGui::Begin("nes_tas")) {
        if (ImGui::Button("Refresh")) Refresh();
        ImGui::SameLine();
        ImGui::Checkbox("Locked", &m_Locked);
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
                NESDBTASComponent* comp = reinterpret_cast<NESDBTASComponent*>(nesdbtascomp);

                bool changed = false;
                IntColumn(0, &tas->id, selected, BasicColumnType::READONLY, &changed, scroll);
                IntColumn(1, &tas->rom_id, selected, BasicColumnType::READONLY, &changed, scroll);
                ImGui::BeginDisabled(comp->Locked());
                TextColumn(2, &tas->name, selected, BasicColumnType::EDITABLE, &changed, scroll);
                if (ButtonColumn(3, "delete", scroll)) {
                    comp->SetIDToDelete(tas->id);
                }
                ImGui::EndDisabled();
                if (changed) {
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
            ImGui::SameLine();
            if (ImGui::Button("import fm2")) {
                std::string path;
                if (nfdext::FileOpenDialog(&path)) {
                    std::ifstream ifs(path);
                    std::vector<nes::ControllerState> inputs;
                    nes::ReadFM2File(ifs, &inputs, nullptr);
                    if (inputs.empty()) {
                        std::cerr << "empty fm2? or error?" << std::endl;
                    } else {
                        m_SelectedTASID = m_Database->InsertNewTAS(
                                m_PendingROMID, path, inputs);
                        Refresh();
                    }
                }
            }
        }
    }
    ImGui::End();

    if (m_SelectedTASID != m_IDForComponent) {
        m_IDForComponent = m_SelectedTASID;
        SetSubComponentTAS();
    }

    OnSubComponentFrames();
}

void NESDBTASComponent::SetSubComponentTAS()
{
    if (m_IDForComponent == -1) {
        m_NESTasComponent->ClearTAS();
    } else {
        for (auto & tas : m_NESTasInfo) {
            if (tas.id == m_IDForComponent) {
                if (SetSubComponentTAS(tas)) {
                    return;
                }
                break;
            }
        }

        // didn't find it...
        std::cout << "Error setting tas?" << std::endl;
        m_NESTasComponent->ClearTAS();
    }
}

bool NESDBTASComponent::SetSubComponentTAS(const nes::db::nes_tas& tas)
{
    auto rom = m_Database->GetRomCached(tas.rom_id);
    if (!rom) {
        return false;
    }

    std::vector<nes::ControllerState> inputs;
    if (!m_Database->SelectTAS(tas.id, nullptr, &inputs)) {
        return false;
    }

    m_NESTasComponent->SetTAS(this, rom->data(), rom->size(),
            tas.start_string, inputs);
    return true;
}

////////////////////////////////////////////////////////////////////////////////
