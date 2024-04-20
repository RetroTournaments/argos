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

#ifndef ARGOS_GAME_GAMEDBUI_HEADER
#define ARGOS_GAME_GAMEDBUI_HEADER

#include "game/gamedb.h"
#include "rgmui/rgmui.h"
#include "ext/sqliteext/sqliteextui.h"

namespace argos::game::ui
{

class GameDBComponent : public rgmui::IApplicationComponent
{
public:
    GameDBComponent(GameDatabase* db);
    ~GameDBComponent();
};

////////////////////////////////////////////////////////////////////////////////
template <typename T>
class ISmallKVComponent : public rgmui::IApplicationComponent
{
public:
    ISmallKVComponent(GameDatabase* db, const char* table)
        : m_Database(db)
        , m_Table(table)
        , m_First(true)
        , m_Locked(true)
    {
    }
    virtual ~ISmallKVComponent() {};

    void OnFrame() override final {
        if (m_First) {
            Refresh();
            m_First = false;
        }

        if (ImGui::Begin(m_Table.c_str())) {
            if (ImGui::Button("Refresh")) Refresh();
            ImGui::SameLine();
            ImGui::Checkbox("Locked", &m_Locked);
            if (ImGui::BeginTable("##table", 3)) {
                ImGui::TableSetupColumn("key            ", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("value          ", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("     ", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableHeadersRow();

                int scroll = 0;
                std::string to_delete = "";
                for (auto & [key, value] : m_KV) {
                    bool selected = false;
                    ImGui::PushID(key.c_str());
                    sqliteext::ui::TextColumn(0, &key, &selected, sqliteext::ui::BasicColumnType::READONLY, nullptr, &scroll);
                    bool changed = false;
                    sqliteext::ui::BasicColumnType type = (m_Locked) ? sqliteext::ui::BasicColumnType::READONLY : sqliteext::ui::BasicColumnType::EDITABLE;
                    if (selected) {
                        m_Locked = false;
                    }
                    selected = false;
                    DoColumn(key, &value, &selected, type, &changed, &scroll);
                    if (selected) {
                        m_Locked = false;
                    }
                    if (sqliteext::ui::ButtonColumn(1, "delete", &scroll)) {
                        to_delete = key;
                    }
                    ImGui::PopID();
                }

                if (!to_delete.empty()) {
                    Delete(to_delete);
                    Refresh();
                }

                ImGui::EndTable();
            }
            ImGui::Separator();
            if (ImGui::CollapsingHeader("insert new key/value")) {
                if (DoInsertControls()) {
                    Refresh();
                }
            }
        }
        ImGui::End();
    }
    virtual T GetValue(sqlite3_stmt* stmt, int column) = 0;
    virtual void DoColumn(const std::string& key, T* value, bool* selected, sqliteext::ui::BasicColumnType type, bool* changed, int* scroll) = 0;
    void Delete(std::string key) {
        sqlite3_stmt* stmt;
        sqliteext::PrepareOrThrow(m_Database->m_Database, "DELETE FROM " + m_Table + " WHERE key = ?;", &stmt);
        sqliteext::BindStrOrThrow(stmt, 1, key);
        sqliteext::StepAndFinalizeOrThrow(stmt);
    }
    void Refresh() {
        m_KV.clear();
        sqlite3_stmt* stmt;
        sqliteext::PrepareOrThrow(m_Database->m_Database, "SELECT key, value FROM " + m_Table, &stmt);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            m_KV.emplace_back();
            m_KV.back().first = sqliteext::column_str(stmt, 0);
            m_KV.back().second = GetValue(stmt, 1);
        }
        sqlite3_finalize(stmt);
    }
    virtual bool DoInsertControls() = 0;

private:
    GameDatabase* m_Database;
    std::string m_Table;
    std::vector<std::pair<std::string, T>> m_KV;
    bool m_First;
    bool m_Locked;
};

class KVIntComponent : public ISmallKVComponent<int>
{
public:
    KVIntComponent(GameDatabase* db);
    virtual ~KVIntComponent();

    virtual int GetValue(sqlite3_stmt* stmt, int column) override final;
    virtual void DoColumn(const std::string& key, int* value, bool* selected, 
            sqliteext::ui::BasicColumnType type, bool* changed, int* scroll) override final;
    virtual bool DoInsertControls() override final;

private:
    GameDatabase* m_Database;
    std::string m_PendingKey;
    int m_PendingValue;
};

class KVRealComponent : public ISmallKVComponent<double>
{
public:
    KVRealComponent(GameDatabase* db);
    virtual ~KVRealComponent();

    virtual double GetValue(sqlite3_stmt* stmt, int column) override final;
    virtual void DoColumn(const std::string& key, double* value, bool* selected, 
            sqliteext::ui::BasicColumnType type, bool* changed, int* scroll) override final;
    virtual bool DoInsertControls() override final;

private:
    GameDatabase* m_Database;
    std::string m_PendingKey;
    double m_PendingValue;
};

class KVTextComponent : public ISmallKVComponent<std::string>
{
public:
    KVTextComponent(GameDatabase* db);
    virtual ~KVTextComponent();

    virtual std::string GetValue(sqlite3_stmt* stmt, int column) override final;
    virtual void DoColumn(const std::string& key, std::string* value, bool* selected, 
            sqliteext::ui::BasicColumnType type, bool* changed, int* scroll) override final;
    virtual bool DoInsertControls() override final;

private:
    GameDatabase* m_Database;
    std::string m_PendingKey;
    std::string m_PendingValue;
};

class KVBlobComponent : public rgmui::IApplicationComponent
{
public:
    KVBlobComponent(GameDatabase* db);
    virtual ~KVBlobComponent();

    void OnFrame() override final;

private:
    void Refresh();
    void Delete(const std::string& key);

private:
    GameDatabase* m_Database;
    struct BlobInfo {
        std::string Key;
        size_t Size;
        int Type;
    };
    std::vector<BlobInfo> m_BlobInfo;
};

}

#endif
