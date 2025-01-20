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
using namespace sta::util;
using namespace sta::main;

////////////////////////////////////////////////////////////////////////////////
// The 'help' command is important because who can remember anything these days?
REGISTER_COMMAND(help, "print the documentation for the given command[s]",
R"(
EXAMPLES:
    static help --all
    static help config

USAGE:
    static help [--all | <command>...]

DESCRIPTION:
    The 'help' command prints the help / usage information for the chosen
    command. Also prints examples of how the command can be used and configured
    for different common applications.
)")
{
    if (argc == 0) {
        PrintProgramUsage(std::cout);
        return 0;
    }

    auto PrintUsage = [](const Command& command, bool separate){
        if (separate) {
            for (int i = 0; i < 80; i++) {
                std::cout << "-";
            }
            std::cout << "\nstatic help " << command.name << "\n\n";
        }

        std::cout << command.usage;
        if (separate) {
            std::cout << "\n";
        }
    };

    std::string arg;
    if (util::ArgPeekString(&argc, &argv, &arg)) {
        if (arg == "--all") {
            std::cout << "static help\n\n";
            PrintProgramUsage(std::cout);

            for (auto & cmd : GetRegisteredCommands()) {
                PrintUsage(cmd, true);
            }
            return 0;
        }
    }

    bool separate = argc > 1;
    while (util::ArgReadString(&argc, &argv, &arg)) {
        // O(n^2) because meh
        bool found = false;
        for (auto & cmd : GetRegisteredCommands()) {
            if (cmd.name == arg) {
                PrintUsage(cmd, separate);
                found = true;
                break;
            }
        }

        if (!found) {
            Error("unrecognized command. '{}'", arg);
        }
    }

    return 0;
}
