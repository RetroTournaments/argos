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

#ifndef ARGOS_ARGOS_MAIN_HEADER
#define ARGOS_ARGOS_MAIN_HEADER

#include <iostream>
#include <vector>

#include "fmt/fmt.h"

#include "argos/argos.h"
#include "rgmui/rgmui.h"

namespace argos::main
{

void PrintProgramUsage(std::ostream& os);
void PrintProgramVersion(std::ostream& os);

template <typename... T>
void Error(fmt::format_string<T...> fmt, T&&... args) {
    std::cerr << "error: " << fmt::vformat(fmt, fmt::make_format_args(args...));
    std::cerr << "\n";
}

////////////////////////////////////////////////////////////////////////////////

//int RunIApplication(const char* identifier, rgmui::IApplication* app, bool load_previous,
//        ArgosDB* db = nullptr,


////////////////////////////////////////////////////////////////////////////////

#define COMMAND_ARGS argos::RuntimeConfig* config, int argc, char** argv
typedef int (*CommandFunc)(COMMAND_ARGS);

struct Command
{
    std::string name;
    std::string usage;
    std::string oneline;
    CommandFunc func;
};
std::vector<Command>& GetRegisteredCommands();
int RegisterCommand(const char* name, const char* oneline, const char* usage, CommandFunc func);

#define COMMAND_FUNC_NAME_(command_name) command_func_ ## command_name
#define COMMAND_INT_NAME_(command_name) g_command_int_ ## command_name

#define REGISTER_COMMAND(command_name, oneline, usage) \
int COMMAND_FUNC_NAME_(command_name)(COMMAND_ARGS); \
static int COMMAND_INT_NAME_(command_name) = RegisterCommand(#command_name, oneline, usage, COMMAND_FUNC_NAME_(command_name)); \
int COMMAND_FUNC_NAME_(command_name)(COMMAND_ARGS)

}

#endif
