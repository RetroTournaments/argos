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

#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"

#include "rgmui/window.h"
#include "rgmui/rgmui.h"

using namespace sta::rgmui;
using namespace sta::util;

Window::Window(const std::string& name, const util::Rect2I& extents, int display,
            std::string* iniPath, std::string* iniData, Uint32 flags)
    : Window(name, extents.Width, extents.Height, extents.X, extents.Y, display,
            iniPath, iniData, flags)
{
}

Window::Window(const std::string& name, int width, int height, int winx, int winy, int display,
        std::string* iniPath, std::string* inidata, Uint32 flags)
    : m_Window(nullptr)
    , m_Context(nullptr)
{
    if (iniPath && inidata) {
        throw std::runtime_error("invalid parameters for ini handling");
    }
    if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            throw std::runtime_error(SDL_GetError());
        }
    }
    int x = SDL_WINDOWPOS_UNDEFINED;
    int y = SDL_WINDOWPOS_UNDEFINED;

    if (flags == 0) {
        flags = SDL_WINDOW_RESIZABLE;
    }
    flags |= SDL_WINDOW_OPENGL;

    if (display && winx == -1) {
        int nd = SDL_GetNumVideoDisplays();
        if (display <= nd) {
            SDL_Rect r;
            SDL_GetDisplayBounds(display, &r);

            x = r.x;
            y = r.y;

            //if (width > 0) {
            //    x = r.x + (r.w - width) / 2;
            //    y = r.y + (r.h - height) / 2;
            //}
        }
    }

    if (winx != -1) {
        x = winx;
        y = winy;
    }

    m_Window = SDL_CreateWindow(name.c_str(),
            x, y,
            width, height, flags);
    //m_Window = SDL_CreateWindowWithProperties(
    //    name.c_str(),
    //    x, y,
    //    width,
    //    height,
    //    SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    //);
    if (m_Window == nullptr) {
        throw std::runtime_error(SDL_GetError());
    }

    if (width <= 0 || height <= 0) {
        //SDL_SetWindowFullscreen(m_Window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        SDL_SetWindowFullscreen(m_Window, SDL_TRUE);
    }

    m_Context = SDL_GL_CreateContext(m_Window);
    if (m_Context == nullptr) {
        throw std::runtime_error(SDL_GetError());
    }
    if (SDL_GL_MakeCurrent(m_Window, m_Context) != 0) {
        throw std::runtime_error(SDL_GetError());
    }

    //  -1 for adaptive
    //   0 for immediate
    //   1 for synchronized
    int swpInterval = 0;
    SDL_GL_SetSwapInterval(swpInterval);

    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    if (!ImGui_ImplSDL2_InitForOpenGL(m_Window, m_Context)) {
        throw std::runtime_error("Failure initializing sdl for opengl, maybe update your graphics driver?");
    }
    if (!ImGui_ImplOpenGL3_Init("#version 330")) {
        throw std::runtime_error("Failure initializing opengl, maybe update your graphics driver?");
    }

    // TODO use io.IniFilename = NULL,
    // LoadIniSettingsFromMemory()
    // SaveIniSettingsToMemory()
    // and io.WantSaveIniSettings
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    if (iniPath) {
        m_IniPath = *iniPath;
        io.IniFilename = m_IniPath.c_str();
    }
    if (inidata) {
        io.IniFilename = nullptr;
        ImGui::LoadIniSettingsFromMemory(inidata->c_str());
    }
    //io.ConfigWindowsMoveFromTitleBarOnly = true;
}

Window::~Window()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    if (m_Context) SDL_GL_DeleteContext(m_Context);
    if (m_Window) SDL_DestroyWindow(m_Window);

    SDL_Quit();
}

bool Window::OnSDLEvent(const SDL_Event& e)
{
    ImGui_ImplSDL2_ProcessEvent(&e);
    if (e.type == SDL_QUIT) {
        return false;
    }
    return true;
}

int Window::ScreenWidth() const
{
    int w;
    SDL_GetWindowSize(m_Window, &w, NULL);
    return w;
}

int Window::ScreenHeight() const
{
    int h;
    SDL_GetWindowSize(m_Window, NULL, &h);
    return h;
}

Rect2I Window::GetScreenRect() const
{
    Rect2I rect;


    SDL_GetWindowSize(m_Window, &rect.Width, &rect.Height);
    SDL_GetWindowPosition(m_Window, &rect.X, &rect.Y);

    int topsize;
    SDL_GetWindowBordersSize(m_Window, &topsize, nullptr, nullptr, nullptr);
    rect.Y -= topsize;

    return rect;
}

int Window::GetDisplay() const
{
    // TODO
    return 0;
    //return SDL_GetDisplayForWindow(m_Window);
}

void Window::NewFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    //ImGui_ImplSDL3_NewFrame(m_Window);
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    rgmui::NewFrame();

    glViewport(0, 0, ScreenWidth(), ScreenHeight());
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void Window::EndFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(m_Window);
}

void Window::SaveIniToString(std::string* str)
{
    if (str) {
        *str = std::string(ImGui::SaveIniSettingsToMemory());
        ImGuiIO& io = ImGui::GetIO();
        io.WantSaveIniSettings = false;
    }
}

