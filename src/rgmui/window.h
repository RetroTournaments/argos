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

#ifndef ARGOS_RGMUI_WINDOW_HEADER
#define ARGOS_RGMUI_WINDOW_HEADER

#include <string>

#include "SDL.h" 
#include "GL/gl.h"

#include "util/rect.h"

namespace argos::rgmui
{

class Window
{
public:
    Window(const std::string& name, const util::Rect2I& extents, int display = 0,
            std::string* iniPath = nullptr, std::string* iniData = nullptr);
    Window(const std::string& name, int width = 1920, int height = 1080, 
            int winx = SDL_WINDOWPOS_CENTERED_DISPLAY(0), 
            int winy = SDL_WINDOWPOS_CENTERED_DISPLAY(0), 
            int display = 0, std::string* iniPath = nullptr, std::string* iniData = nullptr);
    ~Window();

    int ScreenWidth() const;
    int ScreenHeight() const;

    util::Rect2I GetScreenRect() const;

    int GetDisplay() const;

    void SaveIniToString(std::string* str);

    bool OnSDLEvent(const SDL_Event& e);

    void NewFrame();
    void EndFrame();

private:
    SDL_Window* m_Window;
    SDL_GLContext m_Context;
    std::string m_IniPath;
};

}

#endif
