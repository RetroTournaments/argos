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

#include <iostream>
#include <stdexcept>

#include "ext/sdlext/sdlext.h"

using namespace argos::sdlext;

SDLExtMixInit::SDLExtMixInit()
{
    if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
        if (SDL_Init(SDL_INIT_AUDIO) < 0) {
            throw std::runtime_error(SDL_GetError());
        }
    }
    if (Mix_OpenAudio(SDL_AUDIO_DEVICE_DEFAULT_OUTPUT, nullptr) < 0) {
        throw std::runtime_error(Mix_GetError());
    }
}

SDLExtMixInit::~SDLExtMixInit()
{
    //Mix_CloseAudio();
    //Mix_Quit();
}

SDLExtMixChunk::SDLExtMixChunk(const std::vector<uint8_t>& wav_data)
{
    SDL_IOStream* rwops = SDL_IOFromConstMem(wav_data.data(), wav_data.size());
    if (!rwops) throw std::runtime_error(SDL_GetError());
    Chunk = Mix_LoadWAV_IO(rwops, SDL_TRUE);
    if (!Chunk) throw std::runtime_error(Mix_GetError());
}

SDLExtMixChunk::~SDLExtMixChunk()
{
    Mix_FreeChunk(Chunk);
}

SDLExtMixMusic::SDLExtMixMusic(const std::vector<uint8_t>& wav_data)
    : m_WavData(wav_data)
{
    SDL_IOStream* rwops = SDL_IOFromConstMem(m_WavData.data(), m_WavData.size());
    if (!rwops) throw std::runtime_error(SDL_GetError());
    Music = Mix_LoadMUS_IO(rwops, SDL_TRUE);
    if (!Music) throw std::runtime_error(Mix_GetError());
}

SDLExtMixMusic::~SDLExtMixMusic()
{
    // Because of SDL_TRUE in Mix_LoadMUS_IO I think this is unnecessary?
    // There is issues here
    //Mix_FreeMusic(Music);
}

