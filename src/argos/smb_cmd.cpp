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

#include "smb/smbdb.h"
#include "smb/smbdbui.h"

using namespace argos;
using namespace argos::util;
using namespace argos::main;

// 'argos smb db'
int DoSMBDB(const argos::RuntimeConfig* config, int argc, char** argv)
{
    smb::SMBDatabase smbdb(RuntimeConfig::SMBDatabasePath(config));

    std::string arg;
    if (!ArgReadString(&argc, &argv, &arg)) {
        return smbdb.SystemLaunchSQLite3WithExamples();
    }

    if (arg == "edit") {
        return smbdb.SystemLaunchSQLite3WithExamples();
    } else if (arg == "ui") {
        smbui::SMBDatabaseApplication app(&smbdb);
        return RunIApplication(config, "argos smb db", &app);
    } else {
        Error("unrecognized argument. '{}'", arg);
        return 1;
    }
    return 0;
}



////////////////////////////////////////////////////////////////////////////////
// The 'smb' command is for the 1985 nes super mario bros
REGISTER_COMMAND(smb, "Nintendo Entertainment System, Super Mario Bros., 1985",
R"(
EXAMPLES
    argos smb db

    <todo?> argos smb host --port 5555
    <todo?> argos smb join --nestopia --connect "udp://127.0.0.1:5555"
    <todo?> argos smb 

USAGE:
    argos smb <action> [<args>...]

DESCRIPTION:
    The 'smb' command is for the Nintendo Entertainment System game 'Super
    Mario Bros.' This was the first game supported.

OPTIONS:
)")
{
    std::string action;
    if (!ArgReadString(&argc, &argv, &action)) {
        Error("at least one argument expected to 'smb'");
        return 1;
    }

    if (action == "db") {
        return DoSMBDB(config, argc, argv);
    } else {
        Error("unrecognized action. '{}'", action);
        return 1;
    }

    return 1;
}

