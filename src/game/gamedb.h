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
// This gives the base setup for each game's database.
//
// The database provides a centralized location for all auxiliary data in one
// single structured file. Consider: 'https://www.sqlite.org/appfileformat.html'

#ifndef ARGOS_GAME_GAMEDB_HEADER
#define ARGOS_GAME_GAMEDB_HEADER

#include "ext/sqliteext/sqliteext.h"
#include "ext/opencvext/opencvext.h"

namespace argos::game
{

class GameDatabase : public sqliteext::SQLiteExtDB
{
public:
    GameDatabase(const std::string& path);
    ~GameDatabase();

    // Basic persistent key value storage for the simple data types 
    // (throws on unknown key), stored in kv_int
    int GetInt(const char* key);
    int GetInt(const char* key, int default_value);
    void SetInt(const char* key, int value);

    // stored in kv_real
    double GetReal(const char* key);
    double GetReal(const char* key, double default_value);
    void SetReal(const char* key, double value);

    // stored in kv_text
    std::string GetText(const char* key);
    std::string GetText(const char* key, std::string default_value);
    void SetText(const char* key, const std::string& str);

    // Full blobs (good for small ish raw files), stored in kv_blob
    void GetBlob(const char* key, std::vector<uint8_t>* result);
    void SetBlob(const char* key, const uint8_t* ptr, size_t size);
    // THIS POINTER ONLY VALID UNTIL A TYPE CONVERSION (see https://www.sqlite.org/c3ref/column_blob.html)
    const void* GetBlob(const char* key);

    // useful to have an image now and then huh! (these are compressed as pngs in the database)!, stored in kv_blob
    cv::Mat GetImage(const char* key);
    void SetImage(const char* key, cv::Mat img);

    // Then of course you can specify your own tables as well. Refer to sqliteext.h
    // int SQLiteExtDB::ExecOrThrow(const std::string& query,
    //        std::function<bool(int argc, char** data, char** columns)> cback = nullptr);

    // Schemas for the simple key value stuff
    static const char* KVIntSchema();
    static const char* KVDoubleSchema();
    static const char* KVStringSchema();
    static const char* KVBlobSchema();
};

}

#endif
