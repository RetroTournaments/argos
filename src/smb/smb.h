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

#ifndef ARGOS_SMB_SMB_HEADER
#define ARGOS_SMB_SMB_HEADER

#include "nes/nes.h"

namespace argos::smb
{

// Some of the most relevant RAM addresses in SMB
enum RamAddress : uint16_t
{
    GAME_ENGINE_SUBROUTINE  = 0x000e,
    AREA_DATA               = 0x00e7,
    AREA_DATA_LOW           = 0x00e7,
    AREA_DATA_HIGH          = 0x00e8,
    SPRITE_DATA             = 0x0200,
    SCREENEDGE_PAGELOC      = 0x071a,
    SCREENEDGE_X_POS        = 0x071c,
    HORIZONTAL_SCROLL       = 0x073f,
    INTERVAL_TIMER_CONTROL  = 0x077f,

    WORLD_NUMBER            = 0x075f,
    LEVEL_NUMBER            = 0x075c,

    OPER_MODE               = 0x0770,
    OPER_MODE_TASK          = 0x0772,
    PLAYER_X_SPEED          = 0x0057,

    BLOCK_BUFFER_1          = 0x0500,
    BLOCK_BUFFER_84_DISC    = 0x062a, // Extending threecreepios suggestion
    BLOCK_BUFFER_2          = 0x05d0,

    // https://github.com/Kolpa/FF1Bot/blob/master/emulator/luaScripts/SMB-HitBoxes.lua
    ENEMY_FLAG              = 0x000f,
    MARIO_BOUNDING_BOX      = 0x04ac, // ulx, uly, drx, dry
    ENEMY_BOUNDING_BOX      = 0x04b0,
};


// see smb.asm:     ;sound effects constants
enum class SoundEffect : uint32_t
{
    SILENCE             = 0b00000000000000000000000000000000,

    SMALL_JUMP          = 0b00000000000000000000000010000000,
    FLAGPOLE            = 0b00000000000000000000000001000000,
    FIREBALL            = 0b00000000000000000000000000100000,
    PIPEDOWN_INJURY     = 0b00000000000000000000000000010000,
    ENEMY_SMACK         = 0b00000000000000000000000000001000,
    ENEMY_STOMP_SWIM    = 0b00000000000000000000000000000100,
    BUMP                = 0b00000000000000000000000000000010,
    BIG_JUMP            = 0b00000000000000000000000000000001,

    BOWSER_FALL         = 0b00000000000000001000000000000000,
    EXTRA_LIFE          = 0b00000000000000000100000000000000,
    GRAB_POWER_UP       = 0b00000000000000000010000000000000,
    TIMERTICK           = 0b00000000000000000001000000000000, // 4 frames in between
    BLAST               = 0b00000000000000000000100000000000, // fireworks and bullet bills
    GROW_VINE           = 0b00000000000000000000010000000000,
    GROW_POWER_UP       = 0b00000000000000000000001000000000,
    GRAB_COIN           = 0b00000000000000000000000100000000,

    BOWSER_FLAME        = 0b00000000100000000000000000000000,
    BRICK_SHATTER       = 0b00000000010000000000000000000000,
};
const std::vector<SoundEffect>& AudibleSoundEffects();
std::string ToString(SoundEffect effect);
    
// music
enum class MusicTrack : uint32_t
{
    SILENCE             = 0b00000000000000000000000000000000,

    // most of these tracks repeat
    STAR_POWER          = 0b00000000000000000000000001000000, 
    PIPE_INTRO          = 0b00000000000000000000000000100000, // does not repeat
    CLOUD               = 0b00000000000000000000000000010000,
    CASTLE              = 0b00000000000000000000000000001000,
    UNDERGROUND         = 0b00000000000000000000000000000100,
    WATER               = 0b00000000000000000000000000000010,
    GROUND              = 0b00000000000000000000000000000001,

    // most of these tracks do not repeat
    TIME_RUNNING_OUT    = 0b00000000000000000100000000000000,
    END_OF_LEVEL        = 0b00000000000000000010000000000000, // repeats
    ALT_GAME_OVER       = 0b00000000000000000001000000000000,
    END_OF_CASTLE       = 0b00000000000000000000100000000000, 
    VICTORY             = 0b00000000000000000000010000000000,
    GAME_OVER           = 0b00000000000000000000001000000000,
    DEATH_MUSIC         = 0b00000000000000000000000100000000,
};
const std::vector<MusicTrack>& AllMusicTracks();
std::string ToString(MusicTrack track);

// Game end state
//  OPER_MODE == 2, OPER_MODE_TASK == 0x00, PLAYER_X_SPEED == 0x18

}

#endif
