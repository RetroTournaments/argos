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

#include "static/main.h"
#include "util/arg.h"

using namespace sta;
using namespace sta::main;

REGISTER_COMMAND(list, "List out relevant information of some set",
R"(
EXAMPLES:
    static list serial
    static list v4l2
    static list pactl

USAGE:
    static list <set>

DESCRIPTION:
    List something relevant to static line by line.

SETS:
    v4l2     : Input devices for video capture, uses `v4l2-ctl --list-devices`
    pactl    : The pulse audio sources, uses `pactl list short sources`
    serial   : The serial sources, uses `journalctl`
    commands : The commands that static supports
)")
{
    std::string set;
    if (!util::ArgReadString(&argc, &argv, &set)) {
        Error("identify the set (v4l2, pactl, serial, ...)");
        return 1;
    }

    if (set == "v4l2") {
        return system("v4l2-ctl --list-devices");
    } else if (set == "pactl") {
        return system("pactl list short sources");
    } else if (set == "serial") {
        return system("journalctl --dmesg -o short-monotonic --no-hostname --no-pager | grep tty");
    } else if (set == "command" || set == "commands") {
        for (auto & cmd : sta::main::GetRegisteredCommands()) {
            std::cout << cmd.name << std::endl;
        }
    } else {
        std::cout << "unknown set: " << set << std::endl;
        return 1;
    }
    return 0;
}

