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

#ifndef EXT_SQLITEEXTUI_HEADER
#define EXT_SQLITEEXTUI_HEADER

#include "rgmui/rgmui.h"
#include "ext/sqliteext/sqliteext.h"

namespace sqliteext::ui
{

void DoSchemaDisplay(const char* label, const char* schema);

enum BasicColumnType
{
    EDITABLE,
    READONLY
};
void CheckScrollExt(int* scroll);
void IntColumn(int column, int* v, bool* selected, BasicColumnType type, bool* changed, int* scroll);
void DoubleColumn(int column, double* v, bool* selected, BasicColumnType type, bool* changed, int* scroll);
void TextColumn(int column, std::string* v, bool* selected, BasicColumnType type, bool* changed, int* scroll);
bool ButtonColumn(int column, const char* label, int* scroll);

template <typename T>
bool DoDBViewTable(const char* label, std::vector<T>* items, int* selected_id, int columns,
        std::function<void(void*)> setupcolumns,
        std::function<bool(T*, bool*, int*, void*)> dorow,
        void* user_data)
{
    bool changed = false;
    int scroll = 0;

    int selected_index = -1;
    std::vector<int> ids;
    if (ImGui::BeginTable(label, columns)) {
        setupcolumns(user_data);
        ImGui::TableHeadersRow();

        for (int i = 0; i < static_cast<int>(items->size()); i++) {
            ImGui::PushID(i);

            int this_id = items->at(i).id;

            bool selected = false;
            if (selected_id) {
                selected = this_id == *selected_id;
            }

            changed = dorow(&items->at(i), &selected, &scroll, user_data) || changed;

            if (selected_id && selected) {
                *selected_id = this_id;
            }
            ImGui::PopID();

            if (selected_id) {
                if (*selected_id == this_id) {
                    selected_index = i;
                }
                ids.push_back(this_id);
            }
        }

        ImGui::EndTable();
    }

    if (scroll && selected_index != -1 && selected_id) {
        int q = selected_index + scroll;
        if (q >= 0 && q < ids.size()) {
            *selected_id = ids[q];
        }
    }

    return changed;
}

////////////////////////////////////////////////////////////////////////////////

class DBSchemaComponent : public sta::rgmui::IApplicationComponent
{
public:
    DBSchemaComponent(sqliteext::SQLiteExtDB* db, std::string title = "");
    ~DBSchemaComponent();

    void OnFrame();

private:
    sqliteext::SQLiteExtDB* m_Database;
    std::string m_Title;

    std::string m_Message;
    std::vector<std::pair<std::string, std::string>> m_TableSchemas;
};

}

#endif

