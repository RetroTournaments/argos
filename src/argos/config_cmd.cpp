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

#include "argos/main.h"
#include "util/arg.h"
#include "util/file.h"

using namespace argos;
using namespace argos::util;
using namespace argos::main;

class ConfigCommand
{
public:
    ConfigCommand(const RuntimeConfig* config);
    ~ConfigCommand();

    int EditConfig();
    int SetDefaults();
    int RestoreLast();
    int Path();

private:
    const RuntimeConfig* m_RuntimeConfig;
};

////////////////////////////////////////////////////////////////////////////////
// The 'config' command is to help with setting all those annoying little
// options and details.
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
        Swap the current config with the previous saved config.

    --path
        Print the config file path and exit
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
        } else if (arg == "path" || arg == "--path") {
            return cmd.Path();
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

    int ret = system(fmt::format("editor {}", configPath).c_str());
    if (ret) {
        std::cerr << "editor returned non-zero status?" << std::endl;
        return ret;
    }

    std::vector<uint8_t> newContents;
    ReadFileToVector(configPath, &newContents);

    if (newContents != originalContents) {
        WriteVectorToFile(configPath + "~", originalContents);
        std::cout << "edited " << configPath << std::endl;
    } else {
        std::cout << "no change " << configPath << std::endl;
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

int ConfigCommand::Path()
{
    std::cout << RuntimeConfig::RuntimeConfigPath(m_RuntimeConfig) << std::endl;
    return 0;
}
