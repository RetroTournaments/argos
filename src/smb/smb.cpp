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

#include "smb/smb.h"

using namespace argos;
using namespace argos::smb;

const std::vector<SoundEffect>& argos::smb::AudibleSoundEffects()
{
    static std::vector<SoundEffect> s_Effects = {
        SoundEffect::SMALL_JUMP,
        SoundEffect::FLAGPOLE,
        SoundEffect::FIREBALL,
        SoundEffect::PIPEDOWN_INJURY,
        SoundEffect::ENEMY_SMACK,
        SoundEffect::ENEMY_STOMP_SWIM,
        SoundEffect::BUMP,
        SoundEffect::BIG_JUMP,
        SoundEffect::BOWSER_FALL,
        SoundEffect::EXTRA_LIFE,
        SoundEffect::GRAB_POWER_UP,
        SoundEffect::TIMERTICK,
        SoundEffect::BLAST,
        SoundEffect::GROW_VINE,
        SoundEffect::GROW_POWER_UP,
        SoundEffect::GRAB_COIN,
        SoundEffect::BOWSER_FLAME,
        SoundEffect::BRICK_SHATTER,
    };
    return s_Effects;
}

std::string argos::smb::ToString(SoundEffect effect)
{
    switch (effect) {
        case(SoundEffect::SILENCE): return "SoundEffect::SILENCE";
        case(SoundEffect::SMALL_JUMP): return "SoundEffect::SMALL_JUMP";
        case(SoundEffect::FLAGPOLE): return "SoundEffect::FLAGPOLE";
        case(SoundEffect::FIREBALL): return "SoundEffect::FIREBALL";
        case(SoundEffect::PIPEDOWN_INJURY): return "SoundEffect::PIPEDOWN_INJURY";
        case(SoundEffect::ENEMY_SMACK): return "SoundEffect::ENEMY_SMACK";
        case(SoundEffect::ENEMY_STOMP_SWIM): return "SoundEffect::ENEMY_STOMP_SWIM";
        case(SoundEffect::BUMP): return "SoundEffect::BUMP";
        case(SoundEffect::BIG_JUMP): return "SoundEffect::BIG_JUMP";
        case(SoundEffect::BOWSER_FALL): return "SoundEffect::BOWSER_FALL";
        case(SoundEffect::EXTRA_LIFE): return "SoundEffect::EXTRA_LIFE";
        case(SoundEffect::GRAB_POWER_UP): return "SoundEffect::GRAB_POWER_UP";
        case(SoundEffect::TIMERTICK): return "SoundEffect::TIMERTICK";
        case(SoundEffect::BLAST): return "SoundEffect::BLAST";
        case(SoundEffect::GROW_VINE): return "SoundEffect::GROW_VINE";
        case(SoundEffect::GROW_POWER_UP): return "SoundEffect::GROW_POWER_UP";
        case(SoundEffect::GRAB_COIN): return "SoundEffect::GRAB_COIN";
        case(SoundEffect::BOWSER_FLAME): return "SoundEffect::BOWSER_FLAME";
        case(SoundEffect::BRICK_SHATTER): return "SoundEffect::BRICK_SHATTER";
        default: break;
    };

    return "SoundEffect::UNKNOWN";
}


const std::vector<MusicTrack>& argos::smb::AllMusicTracks()
{
    static std::vector<MusicTrack> s_Tracks = {
        MusicTrack::STAR_POWER, 
        MusicTrack::PIPE_INTRO,
        MusicTrack::CLOUD,
        MusicTrack::CASTLE,
        MusicTrack::UNDERGROUND,
        MusicTrack::WATER,
        MusicTrack::GROUND,
        MusicTrack::TIME_RUNNING_OUT,
        MusicTrack::END_OF_LEVEL,
        MusicTrack::ALT_GAME_OVER,
        MusicTrack::END_OF_CASTLE, 
        MusicTrack::VICTORY,
        MusicTrack::GAME_OVER,
        MusicTrack::DEATH_MUSIC,
    };

    return s_Tracks;
}

std::string argos::smb::ToString(MusicTrack track)
{
    switch(track) {
        case(MusicTrack::SILENCE): return "MusicTrack::SILENCE";
        case(MusicTrack::STAR_POWER): return "MusicTrack::STAR_POWER";
        case(MusicTrack::PIPE_INTRO): return "MusicTrack::PIPE_INTRO";
        case(MusicTrack::CLOUD): return "MusicTrack::CLOUD";
        case(MusicTrack::CASTLE): return "MusicTrack::CASTLE";
        case(MusicTrack::UNDERGROUND): return "MusicTrack::UNDERGROUND";
        case(MusicTrack::WATER): return "MusicTrack::WATER";
        case(MusicTrack::GROUND): return "MusicTrack::GROUND";
        case(MusicTrack::TIME_RUNNING_OUT): return "MusicTrack::TIME_RUNNING_OUT";
        case(MusicTrack::END_OF_LEVEL): return "MusicTrack::END_OF_LEVEL";
        case(MusicTrack::ALT_GAME_OVER): return "MusicTrack::ALT_GAME_OVER";
        case(MusicTrack::END_OF_CASTLE): return "MusicTrack::END_OF_CASTLE";
        case(MusicTrack::VICTORY): return "MusicTrack::VICTORY";
        case(MusicTrack::GAME_OVER): return "MusicTrack::GAME_OVER";
        case(MusicTrack::DEATH_MUSIC): return "MusicTrack::DEATH_MUSIC";
        default: break;
    };

    return "MusicTrack::UNKNOWN";
}

const MinimapPalette& argos::smb::DefaultMinimapPalette()
{
    static MinimapPalette p = {
        0x00, 0x3d, 0x2d, 0x1d
    };
    return p;
}

const MinimapPaletteBGR& argos::smb::DefaultMinimapPaletteBGR()
{
    static MinimapPaletteBGR b = {
        0xff, 0xff, 0xff,
        0xa9, 0xa9, 0xa9,
        0x3c, 0x3c, 0x3c,
        0x00, 0x00, 0x00
    };
    return b;
}
