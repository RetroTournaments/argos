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


#include <array>

#include "fmt/fmt.h"

#include "util/string.h"

using namespace sta::util;

bool sta::util::StringEndsWith(const std::string& str, const std::string& ending)
{
    if (str.length() >= ending.length()) {
        return str.compare(str.length() - ending.length(), ending.length(), ending) == 0;
    }
    return false;
}

bool sta::util::StringStartsWith(const std::string& str, const std::string& start)
{
    if (str.length() >= start.length()) {
        return str.compare(0, start.length(), start) == 0;
    }
    return false;
}

std::string sta::util::BytesFmt(size_t bytes)
{
    if (bytes < 1000) {
        return fmt::format("{} B", bytes);
    }
    size_t exp = 0;
    size_t div = 1;
    for (size_t n = bytes; n >= 1000; n /= 1000) {
        div *= 1000;
        exp++;
    }

    double v = static_cast<double>(bytes) / static_cast<double>(div);
    std::array<char, 7> prefixes = {' ', 'k', 'M', 'G', 'T', 'P', 'E'};
    return fmt::format("{:.1f} {}B", v, prefixes.at(exp));
}
