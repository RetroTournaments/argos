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

#include "util/nixutil.h"
#include "util/file.h"

#include "static/config.h"

using namespace sta;
using namespace sta::util;

std::string sta::RuntimeConfig::RuntimeConfigPath() const
{
    return fs::path(StaticDirectory) / fs::path("config.json");
}

std::string sta::RuntimeConfig::StaticPathTo(const std::string& path) const
{
    return fs::path(StaticDirectory) / fs::path(path);
}

std::string sta::RuntimeConfig::SourcePathTo(const std::string& path) const
{
    return fs::path(SourceDirectory) / fs::path(path);
}

RuntimeConfig RuntimeConfig::Defaults()
{
    RuntimeConfig cfg;
    InitDefaultRuntimeConfig(&cfg);
    return cfg;
}

////////////////////////////////////////////////////////////////////////////////

void sta::InitDefaultRuntimeConfig(RuntimeConfig* config)
{
    config->StaticDirectory = fs::path(sta::util::GetHomeDirectory()) / fs::path(".static/");
#ifdef CMAKE_SOURCE_DIR
    config->SourceDirectory = CMAKE_SOURCE_DIR;
#else
    config->SourceDirectory = "";
#endif
}

void sta::EnsureStaticDirectoryWriteable(const RuntimeConfig& config)
{
    std::filesystem::file_status st = std::filesystem::status(config.StaticDirectory);
    if (!std::filesystem::exists(st)) {
        std::filesystem::create_directories(config.StaticDirectory);
    }
}

