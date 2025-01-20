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

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>

#include <sstream>
#include <iostream>

#include "util/serial.h"

using namespace sta::util;

SimpleSerialPort::SimpleSerialPort()
    : m_SerialPort(-1)
{
}

SimpleSerialPort::SimpleSerialPort(const std::string& path, int baud)
    : m_SerialPort(-1)
{
    OpenOrThrow(path, baud);
}

SimpleSerialPort::~SimpleSerialPort()
{
    if (m_SerialPort != -1) {
        Close();
    }
}

static void ThrowLinuxCallFailed(const std::string& method)
{
    std::ostringstream os;
    os << "'" << method << "' failed. errno: [" << errno << "] '" << strerror(errno) << "'";
    throw std::runtime_error(os.str());
}


void SimpleSerialPort::SetTerminalAttributes(int serialPort, int baud)
{
    struct termios tty;
    if (tcgetattr(serialPort, &tty) != 0) {
        ThrowLinuxCallFailed("tcgetattr");
    }

    tty.c_cflag &= ~PARENB; // clear parity bit
    tty.c_cflag &= ~CSTOPB; // only one stop bit
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8; // 8 bits per byte
    tty.c_cflag &= ~CRTSCTS; // disable rts flow control
    tty.c_cflag |= CREAD | CLOCAL; // allow read and ignore ctrl lines

    tty.c_lflag &= ~ICANON; // Do not treat newlines / backspaces differently
    tty.c_lflag &= ~ECHO;
    tty.c_lflag &= ~ECHOE;
    tty.c_lflag &= ~ECHONL;
    tty.c_lflag &= ~ISIG;

    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

    tty.c_oflag &= ~OPOST;
    tty.c_oflag &= ~ONLCR;

    tty.c_cc[VTIME] = 0;
    tty.c_cc[VMIN] = 0;

    cfsetspeed(&tty, baud);

    if (tcsetattr(serialPort, TCSANOW, &tty) != 0) {
        ThrowLinuxCallFailed("tcsetattr");
    }
}

void SimpleSerialPort::OpenOrThrow(const std::string& path, int baud)
{
    m_SerialPort = open(path.c_str(), O_RDWR);
    if (m_SerialPort < 0) {
        ThrowLinuxCallFailed("open");
    }
    SimpleSerialPort::SetTerminalAttributes(m_SerialPort, baud);
}

void SimpleSerialPort::Close()
{
    close(m_SerialPort);
}

size_t SimpleSerialPort::Read(uint8_t* buffer, size_t size)
{
    int n = read(m_SerialPort, buffer, static_cast<int>(size));
    return static_cast<size_t>(n);
}

