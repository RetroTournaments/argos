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

#include <sstream>

#include "util/arg.h"

using namespace sta::util;

bool sta::util::ArgPeekString(int* argc, char*** argv, std::string* str)
{
    if (*argc <= 0) return false;
    *str = (*argv)[0];
    return true;
}

void sta::util::ArgNext(int* argc, char*** argv)
{
    (*argc)--;
    (*argv)++;
}

bool sta::util::ArgReadString(int* argc, char*** argv, std::string* str)
{
    if (!ArgPeekString(argc, argv, str)) return false;
    ArgNext(argc, argv);
    return true;
}

bool sta::util::ArgReadInt(int* argc, char*** argv, int* v)
{
    int64_t v64;
    if (ArgReadInt64(argc, argv, &v64)) {
        *v = static_cast<int>(v64);
        return true;
    }
    return false;
}

bool sta::util::ArgReadInt64(int* argc, char*** argv, int64_t* v)
{
    std::string arg;
    if (!ArgReadString(argc, argv, &arg)) return false;
    std::istringstream is(arg);
    int64_t q;
    is >> q;
    if (is.fail()) {
        return false;
    }
    *v = q;
    return true;
}

bool sta::util::ArgReadDouble(int* argc, char*** argv, double* v)
{
    std::string arg;
    if (!ArgReadString(argc, argv, &arg)) return false;

    std::istringstream is(arg);
    double q;
    is >> q;
    if (is.fail()) {
        return false;
    }
    *v = q;
    return true;
}

