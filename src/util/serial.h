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

#ifndef ARGOS_UTIL_SERIAL_HEADER
#define ARGOS_UTIL_SERIAL_HEADER

#include <string>
#include <cstdint>
#include <vector>
#include <chrono>

namespace argos::util
{

// This interface is simple, but therefore not as customizable as you may need.
// In that case use the linux stuff directly huh. (or make a better one)
class SimpleSerialPort
{
public:
    SimpleSerialPort(const std::string& path, int baud); // Like '/dev/ttyACM0'
    SimpleSerialPort();
    ~SimpleSerialPort();

    void OpenOrThrow(const std::string& path, int baud);
    void Close();

    // Returns number of bytes read
    size_t Read(uint8_t* buffer, size_t size);

    static void SetTerminalAttributes(int serialPort, int baud);

private:
    int m_SerialPort;
};


}

#endif

