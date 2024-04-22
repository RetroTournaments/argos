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

#include "argos/main.h"
#include "util/arg.h"
#include "util/serial.h"

#include "nes/nesceptor.h"

using namespace argos;
using namespace argos::util;
using namespace argos::nesceptor;
using namespace argos::main;

////////////////////////////////////////////////////////////////////////////////
// The 'nesceptor' command is for the nes mod
REGISTER_COMMAND(nesceptor, "NESceptor mod",
R"(
EXAMPLES:
    argos nesceptor /dev/ttyUSB2

USAGE:
    argos nesceptor [<args>...] ttypath

DESCRIPTION:
    The 'nesceptor' command is to interface with the nes mod

OPTIONS:
)")
{
    std::string arg;
    if (!ArgReadString(&argc, &argv, &arg)) {
        std::cerr << "error: must provide ttypath argument like '/dev/ttyUSB2'\n";
        return 1;
    }

    try {
        util::SimpleSerialPort port(arg, NESCEPTOR_BAUD);

        std::deque<std::pair<nesceptor::MessageParseStatus, uint8_t>> bytebuffer;
        nesceptor::MessageParseInfo parseinfo;

        int READ_BUFFER_SIZE = 256;
        size_t MAX_BUFFER_SIZE = 1000;
        std::vector<uint8_t> readBuffer(READ_BUFFER_SIZE);
        std::cout << "hi" << std::endl;
        for (;;) {
            int read = port.Read(readBuffer.data(), readBuffer.size());
            for (int j = 0; j < read; j++) {
                uint8_t byte = readBuffer[j];
                auto status = nesceptor::ProgressMessageParse(&parseinfo, byte);
                if (status == nesceptor::MessageParseStatus::SUCCESS) {
                    //OnMessage(parseinfo);
                    nesceptor::DebugPrintMessage(parseinfo, std::cout);
                }

                bytebuffer.emplace_back(status, byte);
                while (bytebuffer.size() > MAX_BUFFER_SIZE) {
                    bytebuffer.pop_front();
                }
            }
        }


    } catch (const std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}
