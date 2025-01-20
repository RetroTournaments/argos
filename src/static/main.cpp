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

#include <fstream>
#include <signal.h>

#include "util/arg.h"
#include "util/file.h"
#include "rgmui/rgmuimain.h"
#include "ext/sdlext/sdlext.h"

#include "static/main.h"

using namespace sta;
using namespace sta::util;
using namespace sta::main;


////////////////////////////////////////////////////////////////////////////////
const char* STATIC_USAGE = R"(
EXAMPLES:

USAGE:
    static [--help] [--version] [--static-dir <path>] <command> [<args>...]

DESCRIPTION:
    static is the main executable for the entire simultaneous time attack
    paradigm. Generally many static instances will work together to capture,
    interpret, and combine.

OPTIONS:
    --help
        Print this helpful help message and then exit.

    --version
        Print the static version number and then exit

    --static-dir <path>
        Override the default static directory path (/home/_/.static/) with your
        specification.

    <command> [<args>...]
        Run the command with the associated arguments. Each command is
        documented separately with examples and descriptions of their arguments,
        access that documentation via 'static help'.
)";

////////////////////////////////////////////////////////////////////////////////
std::vector<Command>& sta::main::GetRegisteredCommands()
{
    static std::vector<Command> s_Commands;
    return s_Commands;
}

int sta::main::RegisterCommand(const char* name, const char* oneline, const char* usage, CommandFunc func)
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
void sta::main::PrintProgramUsage(std::ostream& os)
{
    os << (STATIC_USAGE + 1);

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

void sta::main::PrintProgramVersion(std::ostream& os)
{
    os << "static version " << STATIC_MAJOR_VERSION << "." << STATIC_MINOR_VERSION << "." << STATIC_PATCH_VERSION << "\n";
}

////////////////////////////////////////////////////////////////////////////////
int sta::main::RunIApplication(const sta::RuntimeConfig* config, const char* name, rgmui::IApplication* app, StaticDB* db)
{
    EnsureStaticDirectoryWriteable(*config);

    db::AppCache cache = db::AppCache::Defaults(name);

    std::shared_ptr<StaticDB> adb;
    if (!db) {
        adb = std::make_shared<StaticDB>(config->StaticPathTo("static.db"));
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

bool sta::main::g_SIGINT = false;
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
    sta::RuntimeConfig config;
    sta::InitDefaultRuntimeConfig(&config);

    std::string arg;
    while(util::ArgReadString(&argc, &argv, &arg)) {
        if (arg == "--help") {
            PrintProgramUsage(std::cout);
            return 0;
        } else if (arg == "--version") {
            PrintProgramVersion(std::cout);
            return 0;
        } else if (arg == "--static-dir") {
            if (!util::ArgReadString(&argc, &argv, &config.StaticDirectory)) {
                Error("path required after --static-dir");
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
