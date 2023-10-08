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

#include <fstream>

#include "fmt/fmt.h"

#include "util/string.h"
#include "util/file.h"

using namespace argos::util;

void argos::util::ForFileInDirectory(const std::string& directory,
        std::function<bool(fs::path p)> cback)
{
    for (const auto & entry : fs::directory_iterator(directory)) {
        if (!cback(entry.path())) {
            return;
        }
    }
}

void argos::util::ForFileOfExtensionInDirectory(const std::string& directory,
        const std::string& extension, std::function<bool(fs::path p)> cback)
{
    for (const auto & entry : fs::directory_iterator(directory)) {
        if (StringEndsWith(entry.path().string(), extension)) {
            if (!cback(entry.path())) {
                return;
            }
        }
    }
}

bool argos::util::FileExists(const std::string& path)
{
    return fs::exists(path);
}

int argos::util::ReadFileToVector(const std::string& path, std::vector<uint8_t>* contents)
{
    std::ifstream ifs(path, std::ios::in | std::ios::binary);
    if (!ifs.good()) {
        throw std::invalid_argument("ifstream not good");
    }
    *contents = std::move(std::vector<uint8_t>(
            std::istreambuf_iterator<char>(ifs), 
            std::istreambuf_iterator<char>()));
    return static_cast<int>(contents->size());
}

void argos::util::WriteVectorToFile(const std::string& path, const std::vector<uint8_t>& contents)
{
    std::ofstream ofs(path, std::ios::out | std::ios::binary);
    if (!ofs.good()) {
        throw std::invalid_argument("ofstream not good");
    }
    ofs.write(reinterpret_cast<const char*>(contents.data()), contents.size());
}

std::string argos::util::ReadFileToString(const std::string& path)
{
    std::ifstream ifs(path);
    if (!ifs.good()) {
        throw std::invalid_argument(fmt::format("ifstream not good '{}'", path));
    }
    return std::string(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
}

void argos::util::WriteStringToFile(const std::string& path, const std::string& str)
{
    std::ofstream ofs(path);
    if (!ofs.good()) {
        throw std::invalid_argument("ofstream not good");
    }
    ofs << str;
}
