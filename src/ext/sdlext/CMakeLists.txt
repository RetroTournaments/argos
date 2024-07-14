################################################################################
##
## Copyright (C) 2024 Matthew Deutsch
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
cmake_minimum_required(VERSION 3.6)
project(sdlext)

add_library(sdlextlib STATIC
    sdlext.cpp
)
target_link_libraries(sdlextlib PUBLIC
    SDL2
    SDL2_mixer
)
target_include_directories(sdlextlib PUBLIC
    ${SDL_mixer_INCLUDE_DIRS}
    ${SDL_INCLUDE_DIRS}
)

add_library(sdlextuilib
    sdlextui.cpp
)
target_link_libraries(sdlextuilib
    sdlextlib
    rgmuilib
)

