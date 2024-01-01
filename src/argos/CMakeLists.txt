################################################################################
##
## Copyright (C) 2023 Matthew Deutsch
##
## Argos is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 3 of the License, or
## (at your option) any later version.
##
## Argos is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with Argos; if not, write to the Free Software
## Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
##
################################################################################
cmake_minimum_required(VERSION 3.6)
project(argos)

add_library(argoslib
    config.cpp
    argosdb.cpp
)
target_link_libraries(argoslib
    pthread GL dl

    utillib
    jsonextlib
    sqliteextlib
)
target_include_directories(argoslib PUBLIC
    ${cppzmq_INCLUDE_DIRS}
    ${ZeroMQ_INCLUDE_DIR}
    ${spdlog_INCLUDE_DIRS}
)
target_compile_definitions(argoslib PRIVATE
    CMAKE_SOURCE_DIR="${CMAKE_SOURCE_DIR}/"
)

################################################################################
add_executable(argos
    main.cpp
    config_cmd.cpp
    db_cmd.cpp
    help_cmd.cpp
    smb_cmd.cpp
)
target_link_libraries(argos
    argoslib
    rgmuilib
    smblib
    smbuilib
)
target_compile_options(argos PUBLIC -Wno-string-plus-int)

