
#include "rgm/ext/sqliteext/sqliteextui.h"

using namespace rgms::sqliteext;
using namespace rgms::sqliteext::ui;

void rgms::sqliteext::ui::DoSchemaDisplay(const char* label, const char* schema)
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

void rgms::sqliteext::ui::CheckScrollExt(int* scroll)
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

void rgms::sqliteext::ui::IntColumn(int column, int* v, bool* selected, BasicColumnType type, bool* changed, int* scroll)
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

void rgms::sqliteext::ui::DoubleColumn(int column, double* v, bool* selected, BasicColumnType type, bool* changed, int* scroll)
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

void rgms::sqliteext::ui::TextColumn(int column, std::string* v, bool* selected, BasicColumnType type, bool* changed, int* scroll)
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

bool rgms::sqliteext::ui::ButtonColumn(int column, const char* label, int* scroll)
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
    if (db) {
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
    } else {
        m_Message = "null database";
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

