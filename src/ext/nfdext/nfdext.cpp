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

#include "nfd.h"

#include "rgmui/rgmui.h"
#include "ext/nfdext/nfdext.h"

bool nfdext::FileOpenDialog(std::string* path)
{
    nfdchar_t* outPath = nullptr;
    nfdresult_t result = NFD_OpenDialog(NULL, NULL, &outPath);
    if (result == NFD_OKAY) {
        if (path) {
            *path = std::string(outPath);
        }
        free(outPath);
        return true;
    }
    return false;
}

bool nfdext::FileOpenButton(const char* label, std::string* path)
{
    if (ImGui::Button(label)) {
        return FileOpenDialog(path);
    }
    return false;
}
