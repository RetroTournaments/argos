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

#ifndef ARGOS_UTIL_FILE_HEADER
#define ARGOS_UTIL_FILE_HEADER

#include <functional>
#include <vector>
#include <string>

#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <experimental/filesystem>


namespace argos::util {
namespace fs = std::experimental::filesystem;

// recall fs::remove(fs::path p);

void ForFileInDirectory(const std::string& directory,
        std::function<bool(fs::path p)> cback);
// Include "." so: ".png"
void ForFileOfExtensionInDirectory(const std::string& directory,
        const std::string& extension,
        std::function<bool(fs::path p)> cback);

bool FileExists(const std::string& path);

int ReadFileToVector(const std::string& path, std::vector<uint8_t>* contents);
void WriteVectorToFile(const std::string& path, const std::vector<uint8_t>& contents);

std::string ReadFileToString(const std::string& path);
void WriteStringToFile(const std::string& path, const std::string& str);

}

#endif
