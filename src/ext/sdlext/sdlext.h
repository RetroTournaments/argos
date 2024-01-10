////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2024 Matthew Deutsch
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

#ifndef ARGOS_EXT_SDLEXT_HEADER
#define ARGOS_EXT_SDLEXT_HEADER

#include <vector>
#include <cstdint>

#include "SDL.h" 
#include "SDL3_mixer/SDL_mixer.h"

namespace argos::sdlext
{

class SDLExtMixInit
{
public:
    SDLExtMixInit();
    ~SDLExtMixInit();
};

class SDLExtMixChunk
{
public:
    SDLExtMixChunk(const std::vector<uint8_t>& wav_data);
    ~SDLExtMixChunk();

    SDLExtMixChunk(const SDLExtMixChunk&) = delete;

    Mix_Chunk* Chunk;
};

class SDLExtMixMusic
{
public:
    SDLExtMixMusic(const std::vector<uint8_t>& wav_data);
    ~SDLExtMixMusic();

    SDLExtMixMusic(const SDLExtMixMusic&) = delete;

    Mix_Music* Music;
private:
    std::vector<uint8_t> m_WavData;
};

}

#endif
