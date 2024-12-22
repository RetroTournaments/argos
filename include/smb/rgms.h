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
//
// This file contains lightly ported, but always awful, code from the precursor
// repo. 'rgms'
//
// The idea is that eventually I will port this correctly but with the upcoming
// competitions clean up has to take a back seat to getting stuff to work
// regardless of the quality.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef ARGOS_RGMS_HEADER
#define ARGOS_RGMS_HEADER

#include <cstdint>
#include "nes/nes.h"

namespace argos::internesceptor {

enum class MessageParseState
{
    WAITING_FOR_TYPE_BYTE,
    EXPECTING_TYPE_BYTE,
    EXPECTING_SIZE_BYTE,
    EXPECTING_DATA,
};

struct MessageParseInfo
{
    MessageParseState state;
    uint8_t index;

    uint8_t type;
    uint8_t size;
    uint8_t data[4];

    static MessageParseInfo InitialState();
};
void DebugPrintMessage(const MessageParseInfo& message, std::ostream& os);

enum class MessageParseStatus
{
    UNKNOWN_ERROR,
    WARNING_BYTE_IGNORED_WAITING,
    ERROR_INVALID_TYPE_NO_HIGH_BIT,
    ERROR_INVALID_SIZE_HIGH_BIT_SET,
    ERROR_INVALID_SIZE_TOO_LARGE,
    ERROR_INVALID_DATA_HIGH_BIT_SET,
    AGAIN, // All good, keep going!
    SUCCESS, // The message is complete
};

bool IsMessageParseError(MessageParseStatus status);
MessageParseStatus ProgressMessageParse(MessageParseInfo* message, uint8_t nextByte);

enum ControllerInfoBits : uint8_t
{
    CONTROLLER_INFO_BUTTON_PRESSED = 0b00000001,
    CONTROLLER_INFO_READ_WRITE =     0b00000010,
};

// Should be shared with nes_observe.luc somehow
// Everything is in a bit of a state
enum class MessageType : uint8_t
{
    // 8b_0000000
    RST_LOW         = 0x01, // Used as a 'connected / idle' message for when the console is off

    M2_COUNT        = 0x02, // Used to help with timing, the number of m2 cycles since power on
                            // data is the m2_count / 256 essentially.
    UNUSED_1        = 0x03,

    CONTROLLER_INFO = 0x04, // For reads/writes to 0x4016 (and maybe 0x4017 eventually)
                            // data as:     0000 00rd
                            //                     ||> (d0 || d1)
                            //                     |>   r/w

    RAM_WRITE       = 0x05, // size = 3, data[0] = value, data[1] = lower 8 bits, data[2] = higher 3 bits

    UNUSED_2        = 0x06,

    // All size 1, just with the data as expected
    PPUCTRL_WRITE   = 0x07, // Address 0x2000, bits data[0] = VPHB SINN (
                            //      NMI enable (V), PPU master/slave (P), sprite height (H)
                            //      background tile select (B), sprite tile select (S),
                            //      increment mode (I), nametable select(NN)
    PPUMASK_WRITE   = 0x08, // Address 0x2001, write
                            // BGRs bMmG, color emphasis (BGR), sprite enable (s), background enable (b),
                            // sprite left column enable (M), background left column enable (m), greyscale (G)
    PPUSTATUS_READ  = 0x09, // Address 0x2002, read  VS0 (vblank, sprite 0 hit, sprite overflow (O):
                            // resets write pair for 0x2005, 0x2006
    OAMADDR_WRITE   = 0x0a, // Address 0x2003, write
    OAMDATA_WRITE   = 0x0b, // Address 0x2004, write
    OAMDATA_READ    = 0x0c, // Address 0x2004, read
    PPUSCROLL_WRITE = 0x0d, // Address 0x2005, write
    PPU_ADDR_WRITE  = 0x0e, // Address 0x2006, write
    PPU_DATA_WRITE  = 0x0f, // Address 0x2007, write
    PPU_DATA_READ   = 0x10, // Address 0x2007, read
    OAM_DMA_WRITE   = 0x11, // Address 0x4014, write

};

struct RamWrite
{
    uint16_t address;
    uint8_t value;
};
void ExtractRamWrite(const MessageParseInfo& message, RamWrite* write);
struct RAMMessageState
{
    nes::Ram Ram;

    static RAMMessageState InitialState();
};
void ProcessMessage(const MessageParseInfo& message, RAMMessageState* ram);

struct PPUMessageState
{
    uint8_t PPUMask;
    uint8_t PPUController;
    uint16_t PPUAddress;
    uint8_t XScroll;
    uint8_t YScroll;
    int PPUAddrLatch;
    int PPUScrollLatch;

    nes::Oam PPUOam;
    nes::FramePalette PPUFramePalette;
    // when/if we come to other mappers all of this is going to fall apart
    nes::NameTable PPUNameTables[2];

    static PPUMessageState InitialState();
};
void ProcessMessage(const MessageParseInfo& message, PPUMessageState* ppu);

struct ControllerMessageState
{
    nes::ControllerState State;
    // TODO other player? other types of controllers?
    int Latch;

    static ControllerMessageState InitialState();
};
void ProcessMessage(const MessageParseInfo& message, ControllerMessageState* controller);

struct NESMessageState
{
    bool ConsolePoweredOn;
    uint64_t M2Count;
    RAMMessageState RamState;
    PPUMessageState PPUState;
    ControllerMessageState ControllerState;

    static NESMessageState InitialState();
};
void ProcessMessage(const MessageParseInfo& message, NESMessageState* nes);


}


#endif
