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

#include <array>

#include "util/bitmanip.h"

using namespace sta::util;

uint8_t sta::util::Reverse(uint8_t b)
{
    // https://stackoverflow.com/a/2602885
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

uint8_t sta::util::BitCount(uint8_t b)
{
    static std::array<uint8_t, 16> LOOKUP = {
        0x00,  // 0000
        0x01,  // 0001
        0x01,  // 0010
        0x02,  // 0011
        0x01,  // 0100
        0x02,  // 0101
        0x02,  // 0110
        0x03,  // 0111
        0x01,  // 1000
        0x02,  // 1001
        0x02,  // 1010
        0x03,  // 1011
        0x02,  // 1100
        0x03,  // 1101
        0x03,  // 1110
        0x04,  // 1111
    };
    return LOOKUP[b >> 4] + LOOKUP[b & 0x0f];
}
