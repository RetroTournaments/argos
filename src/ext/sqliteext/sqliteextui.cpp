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

#include "ext/sqliteext/sqliteextui.h"

using namespace argos::sqliteext;
using namespace argos::sqliteext::ui;

void argos::sqliteext::ui::DoSchemaDisplay(const char* label, const char* schema)
{
    std::string title = fmt::format("{}::schema", label);
    if (ImGui::CollapsingHeader(title.c_str())) {
        std::string dat(schema);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImVec2 sz = ImGui::CalcTextSize(schema);
        sz.x = 0;
        sz.y += ImGui::GetStyle().FramePadding.y * 2.0f + 2.0f;
        ImGui::InputTextMultiline("##schema", dat.data(), dat.size(), sz, ImGuiInputTextFlags_ReadOnly);
    }
}

void argos::sqliteext::ui::CheckScrollExt(int* scroll)
{
    if (!ImGui::IsItemHovered()) {
        return;
    }

    rgmui::SliderExtIsHovered();
    auto& io = ImGui::GetIO();
    if (io.MouseWheel != 0) {
        *scroll = -io.MouseWheel;
    }
}

void argos::sqliteext::ui::IntColumn(int column, int* v, bool* selected, BasicColumnType type, bool* changed, int* scroll)
{
    ImGui::PushID(column);
    ImGui::TableNextColumn();
    if (!v) {
        ImGui::AlignTextToFramePadding();
        ImGui::Selectable("null", selected);
        CheckScrollExt(scroll);
    } else if (type == BasicColumnType::EDITABLE) {
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputInt("##value", v)) {
            *changed = true;
        }
        if (ImGui::IsItemActivated()) {
            *selected = true;
        }
    } else if (type == BasicColumnType::READONLY) {
        ImGui::AlignTextToFramePadding();
        ImGui::Selectable(fmt::format("{}", *v).c_str(), selected);
        CheckScrollExt(scroll);
    } else {
        assert(false);
    }
    ImGui::PopID();
}

void argos::sqliteext::ui::DoubleColumn(int column, double* v, bool* selected, BasicColumnType type, bool* changed, int* scroll)
{
    ImGui::PushID(column);
    ImGui::TableNextColumn();
    if (!v) {
        ImGui::AlignTextToFramePadding();
        ImGui::Selectable("null", selected);
        CheckScrollExt(scroll);
    } else if (type == BasicColumnType::EDITABLE) {
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputDouble("##value", v)) {
            *changed = true;
        }
        if (ImGui::IsItemActivated()) {
            *selected = true;
        }
    } else if (type == BasicColumnType::READONLY) {
        ImGui::AlignTextToFramePadding();
        ImGui::Selectable(fmt::format("{}", *v).c_str(), selected);
        CheckScrollExt(scroll);
    } else {
        assert(false);
    }
    ImGui::PopID();
}

void argos::sqliteext::ui::TextColumn(int column, std::string* v, bool* selected, BasicColumnType type, bool* changed, int* scroll)
{
    ImGui::PushID(column);
    ImGui::TableNextColumn();
    if (!v) {
        ImGui::AlignTextToFramePadding();
        ImGui::Selectable("null", selected);
        CheckScrollExt(scroll);
    } else if (type == BasicColumnType::EDITABLE) {
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (rgmui::InputText("##value", v)) {
            *changed = true;
        }
        if (ImGui::IsItemActivated()) {
            *selected = true;
        }
    } else if (type == BasicColumnType::READONLY) {
        ImGui::AlignTextToFramePadding();
        ImGui::Selectable(v->c_str(), selected);
        CheckScrollExt(scroll);
    } else {
        assert(false);
    }
    ImGui::PopID();
}

bool argos::sqliteext::ui::ButtonColumn(int column, const char* label, int* scroll)
{
    ImGui::PushID(column);
    ImGui::TableNextColumn();
    bool ret = false;
    ImGui::AlignTextToFramePadding();
    if (ImGui::Button(label)) {
        ret = true;
    }
    CheckScrollExt(scroll);
    ImGui::PopID();
    return ret;
}

////////////////////////////////////////////////////////////////////////////////

DBSchemaComponent::DBSchemaComponent(sqliteext::SQLiteExtDB* db, std::string title)
    : m_Database(db)
    , m_Title(title)
{
    if (m_Title.empty()) m_Title = "DBSchema";

    if (!db) {
        m_Message = "null database";
        return;
    }

    try {
        db->ExecOrThrow("SELECT name, sql FROM sqlite_master WHERE type='table' ORDER BY name;", 
                [&](int argc, char** data, char**){
                    assert(argc == 2);
                    m_TableSchemas.emplace_back(std::string(data[0]), std::string(data[1]));
                    return true;
                });
        if (m_TableSchemas.empty()) {
            m_Message = "no tables";
        }
    } catch (const std::runtime_error& except) {
        m_Message = except.what();
    }
}

DBSchemaComponent::~DBSchemaComponent()
{
}

void DBSchemaComponent::OnFrame()
{
    if (ImGui::Begin(m_Title.c_str())) {
        if (!m_Message.empty()) {
            ImGui::TextUnformatted(m_Message.c_str());
        } else {
            for (auto & [name, sql] : m_TableSchemas) {
                DoSchemaDisplay(name.c_str(), sql.c_str());
            }
        }
    }
    ImGui::End();
}

