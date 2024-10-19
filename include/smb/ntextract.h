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

#ifndef ARGOS_SMB_NTEXTRACT_HEADER
#define ARGOS_SMB_NTEXTRACT_HEADER

#include "smb/smb.h"
#include "smb/smbdb.h"
#include "rgmui/rgmui.h"

namespace argos::smbui
{

// A very early task was to extract all of the nametables from all of the
// levels. This encapsulates that process. At a high level:
//
//  1) Identify the relevant memory addresses, in this case reading the
//     excellent disassembly from doppelganger et al.
//
//     AREA_DATA_LOW           = 0x00e7,
//     AREA_DATA_HIGH          = 0x00e8,
//     SCREENEDGE_PAGELOC      = 0x071a,
//     SCREENEDGE_X_POS        = 0x071c,
//
//  2) Construct a series of TASes that go through all of the areas
//  3) On the appropriate frame extract the nametable and store it in the
//     database
//
// A few failed attempts before this would extract images (pngs) of the
// backgrounds and stitch things together semi-manually and all that. But then
// how to handle nametable changes? Like coins being collected or blocks
// breaking?

class SMBNTExtractApplication : public rgmui::IApplication
{
public:
    SMBNTExtractApplication(smb::SMBDatabase* db);
    ~SMBNTExtractApplication();
};

class NTExtractTASComponent : public rgmui::IApplicationComponent
{
public:
    NTExtractTASComponent(smb::SMBDatabase* db);
    ~NTExtractTASComponent();

    void OnFrame() override;

private:
    void SetInputsID(int id);

private:
    smb::SMBDatabase* m_Database;
    std::vector<smb::db::nt_extract_inputs> m_NTExtractInputs;
    std::vector<smb::db::nt_extract_record> m_NTExtractRecords;
    int m_NTExtractID;
};


}

#endif
