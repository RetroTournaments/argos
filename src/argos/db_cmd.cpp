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

#include "argos/argosdb.h"
#include "argos/main.h"
#include "util/file.h"

using namespace argos;
using namespace argos::util;
using namespace argos::main;

////////////////////////////////////////////////////////////////////////////////
// The 'db' command is to 
REGISTER_COMMAND(db, "edit / manage application data stored in argos.db",
R"(
EXAMPLES
    argos db
    argos db --reset

USAGE:
    argos db [--reset ]

DESCRIPTION:
    The 'db' command allows the operator to edit / manage the application data
    stored in argos.db

    If no options are given then sqlite3 is opened to manage the database.

OPTIONS:
    --reset
        Wipe the database.

    --path
        Print the path to database.
)")
{
    if (argc == 0) {
        ArgosDB db(RuntimeConfig::ArgosDatabasePath(config));
        return db.SystemLaunchSQLite3WithExamples();
    } else {
        std::string arg(argv[0]);
        if (arg == "--reset") {
            fs::remove(RuntimeConfig::ArgosDatabasePath(config));
            ArgosDB db(RuntimeConfig::ArgosDatabasePath(config));
            return 0;
        } else if (arg == "path" || arg == "--path") {
            std::cout << RuntimeConfig::ArgosDatabasePath(config) << std::endl;
            return 0;
        } else {
            Error("unrecognized argument. '{}'", arg);
            return 1;
        }
    }

    return 1;
}
