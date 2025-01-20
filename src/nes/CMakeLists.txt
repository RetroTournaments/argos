################################################################################
##
## Copyright (C) 2023 Matthew Deutsch
##
## Static is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 3 of the License, or
## (at your option) any later version.
##
## Static is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with Static; if not, write to the Free Software
## Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
##
################################################################################
cmake_minimum_required(VERSION 3.15)
project(nes)

add_library(neslib
    nes.cpp 
    nestopiaimpl.cpp
    nesdb.cpp
    ppux.cpp
)
target_include_directories(neslib PUBLIC
    ${nestopia_INCLUDE_DIRS}
)
target_link_libraries(neslib
    utillib
    gamelib
    3rdnestopia
)

add_library(nesuilib
    nesui.cpp
    nesdbui.cpp
)
target_link_libraries(nesuilib
    rgmuilib
    neslib
    gameuilib
    nfdextlib
)

add_library(nesceptorlib
    nesceptor.cpp
)
target_link_libraries(nesceptorlib
    neslib
    jsonextlib
)
