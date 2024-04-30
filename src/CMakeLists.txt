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
project(src)

include_directories(.)
include_directories(../include)

################################################################################
# General purpose libraries
add_subdirectory(ext)   # extensions to third party things
add_subdirectory(util)  # annoying utilities required for c++
add_subdirectory(rgmui) # the ui (extending imgui)
#add_subdirectory(video) # working with video files and streams

################################################################################
add_subdirectory(game)
add_subdirectory(nes)

################################################################################
add_subdirectory(smb)

################################################################################
# And the main library / exe that uses all that stuff
add_subdirectory(argos)

################################################################################
add_subdirectory(nesceptor)
