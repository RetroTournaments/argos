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
project(3rd)

################################################################################
# imgui is just a very nice paradigm for developer friendly user interfaces
set(imgui_INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/"
    PARENT_SCOPE
)
add_library(3rdimgui STATIC
    "imgui/imgui.cpp"
    "imgui/imgui_draw.cpp"
    "imgui/imgui_demo.cpp"
    "imgui/imgui_widgets.cpp"
    "imgui/imgui_tables.cpp"
    "imgui/backends/imgui_impl_sdl3.cpp"
    "imgui/backends/imgui_impl_sdlrenderer3.cpp"
    "imgui/backends/imgui_impl_opengl3.cpp"
)
set_target_properties(3rdimgui PROPERTIES COMPILE_FLAGS "-fPIC")
target_include_directories(3rdimgui PUBLIC
    "imgui/"
)
target_link_libraries(3rdimgui SDL3::SDL3 SDL3_mixer::SDL3_mixer)
target_compile_options(3rdimgui PUBLIC -Wno-unknown-warning-option)

################################################################################
# spdlog is mainly included for 'fmt' but there are some useful bits in spdlog too
add_subdirectory(spdlog)
set(spdlog_INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/spdlog/include/"
    "${CMAKE_CURRENT_SOURCE_DIR}/spdlog/include/spdlog/"
    PARENT_SCOPE
)

################################################################################
# cppzmq wraps the zmq messaging paradigm nicely for cpp
set(cppzmq_INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/cppzmq/"
    PARENT_SCOPE
)

################################################################################
# nlohmann/json is a header only json library (embedded instead of submoduled
# because of size)
set(json_INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/json/single_include/"
    PARENT_SCOPE
)


################################################################################
# nestopia is an accurate and performant NES emulator.
add_subdirectory(nestopia)
set(nestopia_INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/nestopia/source/core/"
    "${CMAKE_CURRENT_SOURCE_DIR}/nestopia/source/core/api"
    PARENT_SCOPE
)

#################################################################################
# nativefiledialog is a nice file dialog library
set(nativefiledialog_INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/nativefiledialog/src/include/"
    PARENT_SCOPE
)
set(3rdnativefiledialoglib 
    "${CMAKE_CURRENT_SOURCE_DIR}/nativefiledialog/build/lib/Release/x64/libnfd.a"
    PARENT_SCOPE)
set(3rdnativefiledialoglib 
    "${CMAKE_CURRENT_SOURCE_DIR}/nativefiledialog/build/lib/Release/x64/libnfd.a")
add_custom_command(OUTPUT ${3rdnativefiledialoglib}
                   COMMAND make CFLAGS=-fPIC
                   WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/nativefiledialog/build/gmake_linux"
                   VERBATIM)
add_custom_target(3rdlibnfd DEPENDS ${3rdnativefiledialoglib})

