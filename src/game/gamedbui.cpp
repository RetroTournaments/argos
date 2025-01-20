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

#include "game/gamedbui.h"
#include "util/string.h"

using namespace sta::game::ui;

GameDBComponent::GameDBComponent(GameDatabase* db)
{
    RegisterSubComponent(std::make_shared<sqliteext::ui::DBSchemaComponent>(db, "db_schema"));
    RegisterSubComponent(std::make_shared<KVIntComponent>(db));
    RegisterSubComponent(std::make_shared<KVRealComponent>(db));
    RegisterSubComponent(std::make_shared<KVTextComponent>(db));
    RegisterSubComponent(std::make_shared<KVBlobComponent>(db));
}

GameDBComponent::~GameDBComponent()
{
}

////////////////////////////////////////////////////////////////////////////////

KVIntComponent::KVIntComponent(GameDatabase* db)
    : ISmallKVComponent<int>(db, "kv_int")
    , m_Database(db)
    , m_PendingValue(0)
{
}

KVIntComponent::~KVIntComponent()
{
}

int KVIntComponent::GetValue(sqlite3_stmt* stmt, int column)
{
    return sqlite3_column_int(stmt, column);
}

void KVIntComponent::DoColumn(const std::string& key, int* value, bool* selected,
        sqliteext::ui::BasicColumnType type, bool* changed, int* scroll)
{
    sqliteext::ui::IntColumn(0, value, selected, type, changed, scroll);
    if (*changed) {
        m_Database->SetInt(key.c_str(), *value);
    }
}

bool KVIntComponent::DoInsertControls()
{
    bool insert = false;
    insert = rgmui::InputText("key", &m_PendingKey, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::InputInt("value", &m_PendingValue);
    ImGui::BeginDisabled(m_PendingKey.empty());
    if (ImGui::Button("insert") || insert) {
        m_Database->SetInt(m_PendingKey.c_str(), m_PendingValue);
        return true;
    }
    ImGui::EndDisabled();
    return false;
}

KVRealComponent::KVRealComponent(GameDatabase* db)
    : ISmallKVComponent<double>(db, "kv_real")
    , m_Database(db)
    , m_PendingValue(0)
{
}

KVRealComponent::~KVRealComponent()
{
}

double KVRealComponent::GetValue(sqlite3_stmt* stmt, int column)
{
    return sqlite3_column_double(stmt, column);
}

void KVRealComponent::DoColumn(const std::string& key, double* value, bool* selected,
        sqliteext::ui::BasicColumnType type, bool* changed, int* scroll)
{
    sqliteext::ui::DoubleColumn(0, value, selected, type, changed, scroll);
    if (*changed) {
        m_Database->SetReal(key.c_str(), *value);
    }
}

bool KVRealComponent::DoInsertControls()
{
    bool insert = false;
    insert = rgmui::InputText("key", &m_PendingKey, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::InputDouble("value", &m_PendingValue);
    ImGui::BeginDisabled(m_PendingKey.empty());
    if (ImGui::Button("insert") || insert) {
        m_Database->SetReal(m_PendingKey.c_str(), m_PendingValue);
        return true;
    }
    ImGui::EndDisabled();
    return false;
}

////////////////////////////////////////////////////////////////////////////////

KVTextComponent::KVTextComponent(GameDatabase* db)
    : ISmallKVComponent<std::string>(db, "kv_text")
    , m_Database(db)
{
}

KVTextComponent::~KVTextComponent()
{
}

std::string KVTextComponent::GetValue(sqlite3_stmt* stmt, int column)
{
    return sqliteext::column_str(stmt, column);
}

void KVTextComponent::DoColumn(const std::string& key, std::string* value, bool* selected,
        sqliteext::ui::BasicColumnType type, bool* changed, int* scroll)
{
    sqliteext::ui::TextColumn(0, value, selected, type, changed, scroll);
    if (*changed) {
        m_Database->SetText(key.c_str(), *value);
    }
}

bool KVTextComponent::DoInsertControls()
{
    bool insert = false;
    insert = rgmui::InputText("key", &m_PendingKey, ImGuiInputTextFlags_EnterReturnsTrue);
    insert = rgmui::InputText("value", &m_PendingValue, ImGuiInputTextFlags_EnterReturnsTrue) || insert;
    ImGui::BeginDisabled(m_PendingKey.empty());
    if (ImGui::Button("insert") || insert) {
        m_Database->SetText(m_PendingKey.c_str(), m_PendingValue);
        return true;
    }
    ImGui::EndDisabled();
    return false;
}

////////////////////////////////////////////////////////////////////////////////

KVBlobComponent::KVBlobComponent(GameDatabase* db)
    : m_Database(db)
{
}

KVBlobComponent::~KVBlobComponent()
{
}

void KVBlobComponent::Refresh()
{
    m_BlobInfo.clear();
    sqlite3_stmt* stmt;

    sqliteext::PrepareOrThrow(m_Database->m_Database, "SELECT key, LENGTH(value), type FROM kv_blob ORDER BY key", &stmt);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        m_BlobInfo.emplace_back();
        m_BlobInfo.back().Key = sqliteext::column_str(stmt, 0);
        m_BlobInfo.back().Size = sqlite3_column_int(stmt, 1);
        m_BlobInfo.back().Type = sqlite3_column_int(stmt, 2);
    }
    sqlite3_finalize(stmt);
}

void KVBlobComponent::Delete(const std::string& key)
{
    sqlite3_stmt* stmt;
    sqliteext::PrepareOrThrow(m_Database->m_Database, "DELETE FROM kv_blob WHERE key = ?;", &stmt);
    sqliteext::BindStrOrThrow(stmt, 1, key);
    sqliteext::StepAndFinalizeOrThrow(stmt);
}

void KVBlobComponent::OnFrame()
{
    if (ImGui::Begin("kv_blob")) {
        if (ImGui::Button("Refresh")) Refresh();

        if (ImGui::BeginTable("##blobs", 4)) {
            ImGui::TableSetupColumn("key            ", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("size    ", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("type    ", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("     ", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableHeadersRow();

            std::string to_delete = "";
            for (auto & info : m_BlobInfo) {
                bool selected = false;
                int scroll = 0;
                sqliteext::ui::TextColumn(0, &info.Key, &selected, sqliteext::ui::BasicColumnType::READONLY, nullptr, &scroll);
                std::string v = util::BytesFmt(info.Size);
                sqliteext::ui::TextColumn(1, &v, &selected, sqliteext::ui::BasicColumnType::READONLY, nullptr, &scroll);

                sqliteext::ui::IntColumn(2, &info.Type, &selected, sqliteext::ui::BasicColumnType::READONLY, nullptr, &scroll);
                if (sqliteext::ui::ButtonColumn(3, "delete", &scroll)) {
                    to_delete = info.Key;
                }
            }
            if (!to_delete.empty()) {
                Delete(to_delete);
                Refresh();
            }

            ImGui::EndTable();
        }
    }
    ImGui::End();
}

