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

#include "static/staticdb.h"
#include "static/main.h"
#include "util/file.h"

using namespace sta;
using namespace sta::util;
using namespace sta::main;

////////////////////////////////////////////////////////////////////////////////
// The 'db' command is to
REGISTER_COMMAND(db, "edit / manage application data stored in static.db",
R"(
EXAMPLES
    static db
    static db --reset

USAGE:
    static db [--reset ]

DESCRIPTION:
    The 'db' command allows the operator to edit / manage the application data
    stored in static.db

    If no options are given then sqlite3 is opened to manage the database.

OPTIONS:
    --reset
        Wipe the database.

    --path
        Print the path to database.
)")
{
    EnsureStaticDirectoryWriteable(*config);

    std::string staticdbpath = config->StaticPathTo("static.db");

    if (argc == 0) {
        StaticDB db(staticdbpath);
        return db.SystemLaunchSQLite3WithExamples();
    } else {
        std::string arg(argv[0]);
        if (arg == "--reset") {
            fs::remove(staticdbpath);
            StaticDB db(staticdbpath);
            return 0;
        } else if (arg == "path" || arg == "--path") {
            std::cout << staticdbpath << std::endl;
            return 0;
        } else {
            Error("unrecognized argument. '{}'", arg);
            return 1;
        }
    }

    return 1;
}
