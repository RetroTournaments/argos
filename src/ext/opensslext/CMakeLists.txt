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
cmake_minimum_required(VERSION 3.15)
project(opensslext)

add_library(opensslextlib STATIC
    opensslext.cpp
)
target_include_directories(opensslextlib PUBLIC
    ${OPENSSL_INCLUDE_DIRS}
)
target_link_libraries(opensslextlib PUBLIC
    ${OPENSSL_LIBRARIES}
)
target_compile_options(opensslextlib PRIVATE
    "-Wno-deprecated-declarations")
