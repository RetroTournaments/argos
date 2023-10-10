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

#include "fmt/fmt.h"

#include "util/arg.h"
#include "util/file.h"

#include "argos/main.h"

using namespace argos;
using namespace argos::util;
using namespace argos::main;

template <typename... T>
void Error(fmt::format_string<T...> fmt, T&&... args) {
    std::cerr << "error: " << fmt::vformat(fmt, fmt::make_format_args(args...));
    std::cerr << "\n";
}

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
// The 'help' command is important because who can remember anything these days?
REGISTER_COMMAND(help, "print the documentation for the given command[s]",
R"(
EXAMPLES:
    argos help --all
    argos help config

USAGE: 
    argos help [--all | <command>...]

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
            Error("unrecognized command. '{}'", arg);
        }
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// The 'config' command is to help with setting all those annoying little
// options and details.
class ConfigCommand
{
public:
    ConfigCommand(const RuntimeConfig* config);
    ~ConfigCommand();

    int EditConfig();
    int SetDefaults();
    int RestoreLast();

private:
    const RuntimeConfig* m_RuntimeConfig;
};
REGISTER_COMMAND(config, "edit / manage the global configuration file",
R"(
EXAMPLES
    argos config

USAGE:
    argos config [--set-defaults | --restore-last]

DESCRIPTION:
    The 'config' command allows the operator to edit / manage the main global
    configuration parameters for argos.

    If no options are given then the current config is backed up, and an editor is
    opened to modify the config.

OPTIONS:
    --set-defaults
        Save the current config, and create the defaults config.

    --restore-last
        Swap the current config with the previous saved config
)")
{
    if (argc > 1) {
        Error("at most one argument expected to 'config'");
        return 1;
    }

    ConfigCommand cmd(config);

    if (argc == 0) {
        return cmd.EditConfig();
    } else {
        std::string arg(argv[0]);
        if (arg == "--set-defaults") {
            return cmd.SetDefaults();
        } else if (arg == "--restore-last") {
            return cmd.RestoreLast();
        } else {
            Error("unrecognized argument. '{}'", arg);
            return 1;
        }
    }
    return 1;
}

ConfigCommand::ConfigCommand(const RuntimeConfig* config)
    : m_RuntimeConfig(config)
{
}

ConfigCommand::~ConfigCommand()
{
}

int ConfigCommand::EditConfig()
{
    std::string configPath = RuntimeConfig::RuntimeConfigPath(m_RuntimeConfig);
    if (!util::FileExists(configPath)) {
        int ret = SetDefaults();
        if (ret) return ret;
    }

    std::vector<uint8_t> originalContents;
    ReadFileToVector(configPath, &originalContents);

    system(fmt::format("editor {}", configPath).c_str());

    std::vector<uint8_t> newContents;
    ReadFileToVector(configPath, &newContents);

    if (newContents != originalContents) {
        WriteVectorToFile(configPath + "~", originalContents);
        std::cout << "no change " << configPath << std::endl;
    } else {
        std::cout << "edited " << configPath << std::endl;
    }
    return 0;
}

int ConfigCommand::SetDefaults()
{
    std::string configPath = RuntimeConfig::RuntimeConfigPath(m_RuntimeConfig);

    if (util::FileExists(configPath)) {
        fs::rename(fs::path(configPath), fs::path(configPath + "~"));
    } else {
        fs::path configDirectory(configPath);
        configDirectory.remove_filename();
        fs::create_directories(configDirectory);
    }

    RuntimeConfig config = RuntimeConfig::Defaults();
    config.ArgosDirectory = m_RuntimeConfig->ArgosDirectory;

    std::ofstream ofs(configPath);
    if (!ofs.good()) {
        Error("unable to write to config '{}'", configPath);
        return 1;
    }

    ofs << std::setw(2) << nlohmann::json(config);
    std::cout << "defaults written to " << configPath << std::endl;
    return 0;
}

int ConfigCommand::RestoreLast()
{
    std::string configPath = RuntimeConfig::RuntimeConfigPath(m_RuntimeConfig);

    if (util::FileExists(configPath + "~")) {
        // no error checking because yolo
        fs::rename(fs::path(configPath), fs::path(configPath + "~~"));
        fs::rename(fs::path(configPath + "~"), fs::path(configPath));
        fs::rename(fs::path(configPath + "~~"), fs::path(configPath + "~"));
        std::cout << "restored " << configPath << std::endl;
    } else {
        Error("backup '{}~' does not exist", configPath);
        return 1;
    }
    return 0;
}


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
int main(int argc, char** argv)
{
    util::ArgNext(&argc, &argv); // skip program argument
    if (argc == 0) {
        Error("arguments required");
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
                Error("path required after --argos-dir");
                return 1;
            }
        } else {
            for (auto & cmd : GetRegisteredCommands()) {
                if (cmd.name == arg) {
                    cmd.func(&config, argc, argv);
                    return 0;
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
