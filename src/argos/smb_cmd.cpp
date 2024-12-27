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
#include "util/file.h"

#include "ext/opensslext/opensslext.h"
#include "ext/nfdext/nfdext.h"

#include "smb/smbdb.h"
#include "smb/smbdbui.h"

using namespace argos;
using namespace argos::util;
using namespace argos::main;

static bool GetSMBRom(int argc, char** argv, const argos::RuntimeConfig* config, std::vector<uint8_t>* rom)
{
    std::string rom_path;
    ArgReadString(&argc, &argv, &rom_path);

    rom->resize(smb::BASE_ROM_SIZE);

    if (rom_path == "") {
        rom_path = config->SourcePathTo("data/smb/smb.nes");
        if (FileExists(rom_path)) {
            argos::util::ReadFileToVector(rom_path, rom);
        } else {
            rom_path = config->SourcePathTo("data/smb/blob.bin");
            if (FileExists(rom_path)) {
                if (FileSize(rom_path) == 31482) {
                    std::string cmd = "gpg --pinentry-mode loopback --decrypt " + rom_path;
                    auto pipe = popen(cmd.c_str(), "r");
                    if (!pipe) {
                        Error("failed opening pipe '{}'", cmd);
                        return false;
                    }
                    size_t red = fread(reinterpret_cast<void*>(rom->data()), 1, rom->size(), pipe);
                    if (red != smb::BASE_ROM_SIZE) {
                        Error("Did not read expected size from pipe");
                        return false;
                    }
                } else {
                    std::cerr << "Maybe you need to run 'git lfs pull origin main' in "
                              << config->SourceDirectory << std::endl;
                }
            } else if (argos::nfdext::FileOpenDialog(&rom_path)) {
                argos::util::ReadFileToVector(rom_path, rom);
            } else {
                Error("Must provide the base smb rom");
                return false;
            }
        }
    } else {
        argos::util::ReadFileToVector(rom_path, rom);
    }

    if (rom->size() != smb::BASE_ROM_SIZE) {
        Error("Provided SMB Rom '{}' is not the correct size '{}'",
                rom_path, smb::BASE_ROM_SIZE);
        return false;
    }

    auto sum = argos::opensslext::ComputeMD5Sum(rom->data(), rom->size());
    bool md5correct = true;
    for (size_t i = 0; i < MD5_DIGEST_LENGTH; i++) {
        if (sum[i] != argos::smb::BASE_ROM_MD5[i]) {
            md5correct = false;
        }
    }
    if (!md5correct) {
        Error("Did not get expected rom, md5sum incorrect");
        std::cerr << "got      : ";
        for (size_t i = 0; i < MD5_DIGEST_LENGTH; i++) {
            std::cerr << fmt::format("{:02x}", sum[i]);
        }
        std::cerr << "\n";
        std::cerr << "expected : ";
        for (size_t i = 0; i < MD5_DIGEST_LENGTH; i++) {
            std::cerr << fmt::format("{:02x}", argos::smb::BASE_ROM_MD5[i]);
        }
        std::cerr << "\n";
        return false;
    }

    return true;
}

// argos smb db init
int DoSMBDBInit(const argos::RuntimeConfig* config, smb::SMBDatabase* orig, int argc, char** argv)
{
    if (argc > 1) {
        Error("expected at most one argument (the path to the smb rom)");
        return 1;
    }

    std::vector<uint8_t> smb_rom;
    if (!GetSMBRom(argc, argv, config, &smb_rom)) {
        Error("Failed to init smb rom");
        return 1;
    }

    std::string TEMP_SMB_DB_PATH = "smbtemp.db";
    {
        if (fs::exists(TEMP_SMB_DB_PATH)) {
            fs::remove(TEMP_SMB_DB_PATH);
        }
        smb::SMBDatabase smbdb(TEMP_SMB_DB_PATH);
        std::string data_path = config->SourcePathTo("data/smb/");
        if (!smb::InitializeSMBDatabase(&smbdb, data_path, smb_rom)) {
            Error("Failed to initialize SMBDatabase");
            return 1;
        }
    }
    orig->Close();
    fs::remove(orig->m_DatabasePath);
    fs::rename(TEMP_SMB_DB_PATH, orig->m_DatabasePath);
    orig->Open();
    return 0;
}

bool SMBDBInit(const argos::RuntimeConfig* config, smb::SMBDatabase* smbdb) {
    if (!smbdb->IsInit()) {
        if (DoSMBDBInit(config, smbdb, 0, nullptr)) {
            Error("SMB Database is not initialized. Run 'argos smb db init'");
            return false;
        }
    }
    return true;
}

// 'argos smb db'
int DoSMBDB(const argos::RuntimeConfig* config, smb::SMBDatabase* smbdb, int argc, char** argv)
{
    EnsureArgosDirectoryWriteable(*config);

    std::string arg;
    if (!ArgReadString(&argc, &argv, &arg)) {
        return smbdb->SystemLaunchSQLite3WithExamples();
    }

    if (arg == "edit") {
        return smbdb->SystemLaunchSQLite3WithExamples();
    } else if (arg == "ui") {
        if (!SMBDBInit(config, smbdb)) {
            return 1;
        }
        smbui::SMBDatabaseApplication app(smbdb);
        return RunIApplication(config, "argos smb db", &app);
    } else if (arg ==  "path") {
        std::cout << config->ArgosPathTo("smb.db") << std::endl;;
        return 0;
    } else if (arg ==  "init") {
        return DoSMBDBInit(config, smbdb, argc, argv);
    } else {
        Error("unrecognized argument. '{}', expected 'edit', 'ui', 'path', or 'init'", arg);
        return 1;
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// The 'smb' command is for the 1985 nes super mario bros
REGISTER_COMMAND(smb, "Nintendo Entertainment System, Super Mario Bros., 1985",
R"(
EXAMPLES:
    argos smb db init
    argos smb db ui

USAGE:
    argos smb <action> [<args>...]

DESCRIPTION:
    The 'smb' command is for the Nintendo Entertainment System game 'Super
    Mario Bros.' This was the first game supported.

OPTIONS:
    db init [rom path]
        Initialize the database using the smb rom

    db edit
        Edit the smb database in sqlite.

    db ui
        Edit/view the smb database with the imgui user interface.

    db path
        Print the path to the smb database.

)")
{
    std::string action;
    if (!ArgReadString(&argc, &argv, &action)) {
        Error("at least one argument expected to 'smb'");
        return 1;
    }

    smb::SMBDatabase smbdb(config->ArgosPathTo("smb.db"));

    if (action == "db") {
        return DoSMBDB(config, &smbdb, argc, argv);
    } else {
        Error("unrecognized action. '{}', expected 'db'", action);
        return 1;
    }

    return 1;
}

