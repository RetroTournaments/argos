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

#include "argos/main.h"
#include "util/arg.h"

using namespace argos;
using namespace argos::util;
using namespace argos::main;

////////////////////////////////////////////////////////////////////////////////
// The 'rgms' command is for bad original code that needs to be moved eventually
REGISTER_COMMAND(rgms, "RGMS",
R"(
EXAMPLES:

USAGE:

DESCRIPTION:

OPTIONS:

)")
{
    std::string action;
    if (!ArgReadString(&argc, &argv, &action)) {
        Error("at least one argument expected to 'rgms'");
        return 1;
    }

    return 0;
}
