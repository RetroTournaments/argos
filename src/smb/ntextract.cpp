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

#include "smb/ntextract.h"

using namespace argos;
using namespace argos::smbui;
using namespace argos::smb;

SMBNTExtractApplication::SMBNTExtractApplication(smb::SMBDatabase* db)
{
    RegisterComponent(std::make_shared<NTExtractTASComponent>(db));
}

SMBNTExtractApplication::~SMBNTExtractApplication()
{
}


NTExtractTASComponent::NTExtractTASComponent(smb::SMBDatabase* db)
    : m_Database(db)
    , m_NTExtractID(-1)
{
    m_Database->GetAllNTExtractInputs(&m_NTExtractInputs);
}

NTExtractTASComponent::~NTExtractTASComponent()
{
}

void NTExtractTASComponent::OnFrame()
{
    if (ImGui::Begin("nt_extract_inputs")) {
        if (ImGui::BeginCombo("inputs", std::to_string(m_NTExtractID).c_str())) {
            for (auto & v : m_NTExtractInputs) {
                if (ImGui::Selectable(v.name.c_str(), m_NTExtractID == v.id)) {
                    if (m_NTExtractID != v.id) {
                        SetInputsID(v.id);
                    }
                }
            }

            ImGui::EndCombo();
        }
    }
    ImGui::End();
}

void NTExtractTASComponent::SetInputsID(int id) {
    m_NTExtractID = id;
    m_Database->GetAllNTExtractRecords(id, &m_NTExtractRecords);

}

