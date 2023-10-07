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

#include "util/arg.h"
#include "argos/main.h"

using namespace argos;
using namespace argos::main;

////////////////////////////////////////////////////////////////////////////////
const char* ARGOS_USAGE = R"(
EXAMPLES:

USAGE:
    argos [--help] [--version] [--argos-dir <path>] <command> [<args>...]

DESCRIPTION:
    argos is the main executable for the entire simultaneous time attack paradigm.
    Generally many argos instances will work together to capture, interpret, and combine.

OPTIONS:
    --help
        Print this helpful help message and then exit.
    
    --version
        Print the argos version number and then exit

    --argos-dir <path>
        Override the default argos directory path (/home/_/.argos/) with your specification.

    <command> [<args>...]
        Run the command with the associated arguments.
        Each command is documented separately with examples and descriptions of their arguments, access that documentation via 'argos help'.

)";


////////////////////////////////////////////////////////////////////////////////
// The 'help' command is important because who can remember anything these days?
REGISTER_COMMAND(help, R"(
EXAMPLES:
    argos help --all
    argos help config

USAGE: 
    argos help [--all | <command>...]

DESCRIPTION:
    The 'help' command prints the help / usage information for the chosen command.
    Also prints examples of how the command can be used and configured for different common applications.
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
            std::cout << "\nargos help " << command.name << "\n\n";
        }

        std::cout << command.usage;
        if (separate) {
            std::cout << "\n";
        }
    };

    std::string arg;
    if (util::ArgPeekString(&argc, &argv, &arg)) {
        if (arg == "--all") {
            std::cout << "argos help\n\n";
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
            std::cerr << "error: unrecognized command. '" << arg << "'\n";
        }
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
std::vector<Command>& argos::main::GetRegisteredCommands()
{
    static std::vector<Command> s_Commands;
    return s_Commands;
}

int argos::main::RegisterCommand(const char* name, const char* usage, CommandFunc func)
{
    Command cmd;
    cmd.name = std::string(name);
    while (usage[0] == '\n') usage++;
    cmd.usage = std::string(usage);
    cmd.func = func;

    auto& cmds = GetRegisteredCommands();
    cmds.push_back(std::move(cmd));
    return static_cast<int>(cmds.size());
}

////////////////////////////////////////////////////////////////////////////////
void argos::main::PrintProgramUsage(std::ostream& os)
{
    os << (ARGOS_USAGE + 1);
}

void argos::main::PrintProgramVersion(std::ostream& os)
{
    os << "argos version " << ARGOS_MAJOR_VERSION << "." << ARGOS_MINOR_VERSION << "." << ARGOS_PATCH_VERSION << "\n";
}

////////////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv)
{
    util::ArgNext(&argc, &argv); // skip program argument
    if (argc == 0) {
        std::cerr << "error: arguments required.";
        PrintProgramUsage(std::cerr);
        return 1;
    }

    argos::RuntimeConfig config;
    argos::InitDefaultRuntimeConfig(&config);

    std::string arg;
    while(util::ArgReadString(&argc, &argv, &arg)) {
        if (arg == "--help") {
            PrintProgramUsage(std::cout);
            return 0;
        } else if (arg == "--version") {
            PrintProgramVersion(std::cout);
            return 0;
        } else if (arg == "--argos-dir") {
            if (!util::ArgReadString(&argc, &argv, &config.ArgosDirectory)) {
                std::cerr << "error: path required after --argos-dir\n";
                return 1;
            }
        } else {
            for (auto & cmd : GetRegisteredCommands()) {
                if (cmd.name == arg) {
                    cmd.func(&config, argc, argv);
                    return 0;
                }
            }

            std::cerr << "error: unrecognized command. '" << arg << "'\n";
            return 1;
        }
    }

    std::cerr << "error: command is required.\n";
    PrintProgramUsage(std::cerr);
    return 1;
}
