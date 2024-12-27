################################################################################
##
## Copyright (C) 2023 Matthew Deutsch
##
## Argos is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
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
cmake_minimum_required(VERSION 3.15)
project(util)

add_library(utillib
    arg.cpp
    bitmanip.cpp
    clock.cpp
    file.cpp
    plotting.cpp
    string.cpp
    serial.cpp
    nixutil.cpp
)
target_include_directories(utillib PUBLIC
    ${spdlog_INCLUDE_DIRS}
)
target_link_libraries(utillib
    stdc++fs
    spdlog::spdlog
)

