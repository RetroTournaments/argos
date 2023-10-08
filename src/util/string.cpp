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

#include "util/string.h"

using namespace argos::util;

bool argos::util::StringEndsWith(const std::string& str, const std::string& ending)
{
    if (str.length() >= ending.length()) {
        return str.compare(str.length() - ending.length(), ending.length(), ending) == 0;
    } 
    return false;
}

bool argos::util::StringStartsWith(const std::string& str, const std::string& start)
{
    if (str.length() >= start.length()) {
        return str.compare(0, start.length(), start) == 0;
    }
    return false;
}

