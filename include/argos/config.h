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
//
// The config file should contain mostly static information that is controlled
// by the user. Paths, Global defaults, that sort of thing.
//
// Purposefully stored in a human readable / editable json file.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef ARGOS_ARGOS_CONFIG_HEADER
#define ARGOS_ARGOS_CONFIG_HEADER

#include <string>

#include "nlohmann/json.hpp"

namespace argos
{

struct RuntimeConfig
{
    std::string ArgosDirectory;
    std::string SourceDirectory;

    static RuntimeConfig Defaults();

    static std::string RuntimeConfigPath(const RuntimeConfig* config);
    static std::string ArgosDatabasePath(const RuntimeConfig* config);
    static std::string SMBDatabasePath(const RuntimeConfig* config);
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(RuntimeConfig,
    ArgosDirectory,
    SourceDirectory
);

void InitDefaultRuntimeConfig(RuntimeConfig* config);
void EnsureArgosDirectoryWriteable(const RuntimeConfig& config);

}

#endif
