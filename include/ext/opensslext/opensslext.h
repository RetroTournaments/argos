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

#ifndef ARGOS_EXT_OPENSSLEXT_HEADER
#define ARGOS_EXT_OPENSSLEXT_HEADER

#include <array>
#include <cstdint>

#include "openssl/md5.h"

namespace argos::opensslext
{

typedef std::array<uint8_t, MD5_DIGEST_LENGTH> MD5Sum;
MD5Sum ComputeMD5Sum(const uint8_t* data, size_t num_bytes);

}

#endif
