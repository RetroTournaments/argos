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

#ifndef ARGOS_SMB_SMBDB_HEADER
#define ARGOS_SMB_SMBDB_HEADER

#include "smb/smb.h"
#include "nes/nesdb.h"

namespace argos::smb
{

namespace db
{

struct sound_effect
{
    SoundEffect effect;
    std::vector<uint8_t> wav_data;
};

struct music_track
{
    MusicTrack track;
    std::vector<uint8_t> wav_data;
};

}

class SMBDatabase : public nes::NESDatabase
{
public:
    SMBDatabase(const std::string& path);
    ~SMBDatabase();

    bool GetSoundEffectWav(SoundEffect effect, std::vector<uint8_t>* data);
    bool GetMusicTrackWav(MusicTrack track, std::vector<uint8_t>* data);

    static const char* SoundEffectSchema();
    static const char* MusicTrackSchema();
};


//
bool InsertSoundEffect(SMBDatabase* database, SoundEffect effect, const std::string& wavpath);
bool InsertMusicTrack(SMBDatabase* database, MusicTrack track, const std::string& wavpath);

}

#endif
