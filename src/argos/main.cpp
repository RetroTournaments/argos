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
#include <signal.h>

#include "util/arg.h"
#include "util/file.h"
#include "rgmui/rgmuimain.h"
#include "ext/sdlext/sdlext.h"

#include "argos/main.h"

using namespace argos;
using namespace argos::util;
using namespace argos::main;


////////////////////////////////////////////////////////////////////////////////
const char* ARGOS_USAGE = R"(
EXAMPLES:

USAGE:
    argos [--help] [--version] [--argos-dir <path>] <command> [<args>...]

DESCRIPTION:
    argos is the main executable for the entire simultaneous time attack
    paradigm. Generally many argos instances will work together to capture,
    interpret, and combine.

OPTIONS:
    --help
        Print this helpful help message and then exit.

    --version
        Print the argos version number and then exit

    --argos-dir <path>
        Override the default argos directory path (/home/_/.argos/) with your
        specification.

    <command> [<args>...]
        Run the command with the associated arguments. Each command is
        documented separately with examples and descriptions of their arguments,
        access that documentation via 'argos help'.
)";

////////////////////////////////////////////////////////////////////////////////
std::vector<Command>& argos::main::GetRegisteredCommands()
{
    static std::vector<Command> s_Commands;
    return s_Commands;
}

int argos::main::RegisterCommand(const char* name, const char* oneline, const char* usage, CommandFunc func)
{
    Command cmd;
    cmd.name = std::string(name);
    while (usage[0] == '\n') usage++;
    cmd.usage = std::string(usage);
    cmd.oneline = oneline;
    cmd.func = func;

    auto& cmds = GetRegisteredCommands();
    cmds.push_back(std::move(cmd));
    return static_cast<int>(cmds.size());
}

////////////////////////////////////////////////////////////////////////////////
void argos::main::PrintProgramUsage(std::ostream& os)
{
    os << (ARGOS_USAGE + 1);

    std::vector<std::pair<std::string, std::string>> commandNames;
    for (auto & cmd : GetRegisteredCommands()) {
        commandNames.push_back(std::make_pair(cmd.name, cmd.oneline));
    }
    if (commandNames.empty()) return;

    std::sort(commandNames.begin(), commandNames.end());
    os << "\nCOMMANDS:\n";
    for (auto & cmd : commandNames) {
        os << "    " << std::setw(12) << std::left << cmd.first << ": " << cmd.second << "\n";
    }
}

void argos::main::PrintProgramVersion(std::ostream& os)
{
    os << "argos version " << ARGOS_MAJOR_VERSION << "." << ARGOS_MINOR_VERSION << "." << ARGOS_PATCH_VERSION << "\n";
}

////////////////////////////////////////////////////////////////////////////////
int argos::main::RunIApplication(const argos::RuntimeConfig* config, const char* name, rgmui::IApplication* app, ArgosDB* db)
{
    EnsureArgosDirectoryWriteable(*config);

    db::AppCache cache = db::AppCache::Defaults(name);

    std::shared_ptr<ArgosDB> adb;
    if (!db) {
        adb = std::make_shared<ArgosDB>(config->ArgosPathTo("argos.db"));
        db = adb.get();
    }

    db->LoadAppCache(&cache);

    rgmui::Window window(cache.Name, cache.GetWinRect(), cache.Display, nullptr, &cache.IniData);
    rgmui::WindowAppMainLoop(&window, app, std::chrono::microseconds(15000));

    cache.SetWinRect(window.GetScreenRect());
    cache.Display = window.GetDisplay();
    window.SaveIniToString(&cache.IniData);
    db->SaveAppCache(cache);

    return 0;
}

bool argos::main::g_SIGINT = false;
static void sig_handler(int signum)
{
    static bool sig_handled = false;
    if (g_SIGINT || sig_handled) {
        std::exit(1);
    }
    g_SIGINT = true;
    sig_handled = true;
}

////////////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv)
{
    signal(SIGINT, sig_handler);
    util::ArgNext(&argc, &argv); // skip program argument
    if (argc == 0) {
        Error("arguments required");
        PrintProgramUsage(std::cerr);
        return 1;
    }

    sdlext::SDLExtMixInit mix; // TODO.. order is important so it shouldn't be ad hoc
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
                Error("path required after --argos-dir");
                return 1;
            }
        } else {
            for (auto & cmd : GetRegisteredCommands()) {
                if (cmd.name == arg) {
                    return cmd.func(&config, argc, argv);
                }
            }

            Error("unrecognized command. '{}'", arg);
            return 1;
        }
    }

    Error("command is required.");
    PrintProgramUsage(std::cerr);
    return 1;
}
