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

#undef NDEBUG
#include <cassert>
#include <iomanip>

#include "smb/rgms.h"

using namespace argos::internesceptor;

bool argos::internesceptor::IsMessageParseError(MessageParseStatus status)
{
    if (status == MessageParseStatus::WARNING_BYTE_IGNORED_WAITING ||
        status == MessageParseStatus::AGAIN ||
        status == MessageParseStatus::SUCCESS) {
        return false;
    }
    return true;
}

void argos::internesceptor::DebugPrintMessage(const MessageParseInfo& message, std::ostream& os)
{
    auto OutByte = [&](uint8_t byte) {
        os << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(byte);
    };
    os << "t[0x";
    OutByte(message.type);
    os << "] ";
    os << "s(";
    OutByte(message.size);
    int size = message.size;
    if (message.size > 4) {
        os << "TOO BIG!?";
        size = 4;
    }

    os << ")  ";
    for (uint8_t i = 0; i < 4; i++) {
        if (i == 2) {
            os << " ";
        }
        if (i >= size) {
            os << "..";
        } else {
            OutByte(message.data[i]);
        }
    }
    os << "  ";

    for (uint8_t i = 0; i < 4; i++) {
        if (i >= size) {
            os << ".";
        } else {
            if (isprint(static_cast<int>(message.data[i]))) {
                os << static_cast<char>(message.data[i]);
            } else {
                os << ".";
            }
        }
    }
}

MessageParseInfo MessageParseInfo::InitialState()
{
    MessageParseInfo message;
    message.state = MessageParseState::WAITING_FOR_TYPE_BYTE;
    return message;
}

MessageParseStatus argos::internesceptor::ProgressMessageParse(MessageParseInfo* message, uint8_t nextByte)
{
    switch (message->state) {
        case MessageParseState::WAITING_FOR_TYPE_BYTE: {
            if (!(nextByte & 0b10000000)) {
                return MessageParseStatus::WARNING_BYTE_IGNORED_WAITING;
            }
        }
        case MessageParseState::EXPECTING_TYPE_BYTE: {
            if (!(nextByte & 0b10000000)) {
                message->state = MessageParseState::WAITING_FOR_TYPE_BYTE;
                return MessageParseStatus::ERROR_INVALID_TYPE_NO_HIGH_BIT;
            }

            message->type = nextByte & 0b01111111;
            message->state = MessageParseState::EXPECTING_SIZE_BYTE;
            break;
        }
        case MessageParseState::EXPECTING_SIZE_BYTE: {
            if ((nextByte & 0b10000000)) {
                message->state = MessageParseState::WAITING_FOR_TYPE_BYTE;
                return MessageParseStatus::ERROR_INVALID_SIZE_HIGH_BIT_SET;
            }

            message->size = (nextByte & 0b01110000) >> 4;
            message->index = 0;

            message->data[0] = (nextByte & 0b00001000) << 4;
            message->data[1] = (nextByte & 0b00000100) << 5;
            message->data[2] = (nextByte & 0b00000010) << 6;
            message->data[3] = (nextByte & 0b00000001) << 7;

            if (message->size) {
                message->state = MessageParseState::EXPECTING_DATA;
            } else {
                message->state = MessageParseState::EXPECTING_TYPE_BYTE;
                return MessageParseStatus::SUCCESS;
            }
            break;
        }
        case MessageParseState::EXPECTING_DATA: {
            if ((nextByte & 0b10000000)) {
                message->state = MessageParseState::WAITING_FOR_TYPE_BYTE;
                return MessageParseStatus::ERROR_INVALID_DATA_HIGH_BIT_SET;
            }

            message->data[message->index] |= nextByte;
            message->index++;
            if (message->index == message->size) {
                message->state = MessageParseState::EXPECTING_TYPE_BYTE;
                return MessageParseStatus::SUCCESS;
            }
            break;
        }
        default: {
            return MessageParseStatus::UNKNOWN_ERROR;
        }
    }
    return MessageParseStatus::AGAIN;
}

static void TestProgressMessageParse1()
{
    MessageParseInfo message = MessageParseInfo::InitialState();

    assert(ProgressMessageParse(&message, 0b00101010) == MessageParseStatus::WARNING_BYTE_IGNORED_WAITING);
    assert(ProgressMessageParse(&message, 0b10001010) == MessageParseStatus::AGAIN);
    assert(ProgressMessageParse(&message, 0b10000110) == MessageParseStatus::ERROR_INVALID_SIZE_HIGH_BIT_SET);
}

static void TestProgressMessageParse2()
{
    MessageParseInfo message = MessageParseInfo::InitialState();
    assert(ProgressMessageParse(&message, 0b10001010) == MessageParseStatus::AGAIN);
    assert(ProgressMessageParse(&message, 0b00000000) == MessageParseStatus::SUCCESS);

    assert(message.type == 0b00001010);
    assert(message.size == 0b00000000);

    assert(ProgressMessageParse(&message, 0b10100001) == MessageParseStatus::AGAIN);
    assert(ProgressMessageParse(&message, 0b00011000) == MessageParseStatus::AGAIN);
    assert(ProgressMessageParse(&message, 0b01111111) == MessageParseStatus::SUCCESS);

    assert(message.type    == 0b00100001);
    assert(message.size    == 0b00000001);
    assert(message.data[0] == 0b11111111);
}

static void TestProgressMessageParse3()
{
    MessageParseInfo message = MessageParseInfo::InitialState();
    assert(ProgressMessageParse(&message, 0b10100001) == MessageParseStatus::AGAIN);
    assert(ProgressMessageParse(&message, 0b00011000) == MessageParseStatus::AGAIN);
    assert(ProgressMessageParse(&message, 0b10001111) == MessageParseStatus::ERROR_INVALID_DATA_HIGH_BIT_SET);
}

static void TestProgressMessageParse4()
{
    MessageParseInfo message = MessageParseInfo::InitialState();
    assert(ProgressMessageParse(&message, 0b10101001) == MessageParseStatus::AGAIN);
    assert(ProgressMessageParse(&message, 0b00111010) == MessageParseStatus::AGAIN);
    assert(ProgressMessageParse(&message, 0b00001111) == MessageParseStatus::AGAIN);
    assert(ProgressMessageParse(&message, 0b00101111) == MessageParseStatus::AGAIN);
    assert(ProgressMessageParse(&message, 0b00001001) == MessageParseStatus::SUCCESS);

    assert(message.type    == 0b00101001);
    assert(message.size    == 0b00000011);
    assert(message.data[0] == 0b10001111);
    assert(message.data[1] == 0b00101111);
    assert(message.data[2] == 0b10001001);
}

static void TestProgressMessageParse()
{
    TestProgressMessageParse1();
    TestProgressMessageParse2();
    TestProgressMessageParse3();
    TestProgressMessageParse4();
}

void argos::internesceptor::ExtractRamWrite(const MessageParseInfo& message, RamWrite* write)
{
    if (!write) return;

    write->value = message.data[0];
    write->address = static_cast<uint16_t>(message.data[1]) | (static_cast<uint16_t>(message.data[2] & 0b111) << 8);
}

RAMMessageState RAMMessageState::InitialState()
{
    RAMMessageState r;
    std::fill(r.Ram.begin(), r.Ram.end(), 0x00);
    return r;
}

void argos::internesceptor::ProcessMessage(const MessageParseInfo& message, RAMMessageState* ram)
{
    auto t = static_cast<MessageType>(message.type);
    if (t != MessageType::RAM_WRITE) {
        return;
    }

    RamWrite rw;
    ExtractRamWrite(message, &rw);

    ram->Ram.at(rw.address) = rw.value;
}


PPUMessageState PPUMessageState::InitialState()
{
    PPUMessageState s;
    s.PPUMask = 0;
    s.PPUController = 0;
    s.PPUAddress = 0;
    s.PPUAddrLatch = 0;
    s.PPUScrollLatch = 0;

    std::fill(s.PPUOam.begin(), s.PPUOam.end(), 255);
    std::fill(s.PPUFramePalette.begin(), s.PPUFramePalette.end(), 0x00);
    std::fill(s.PPUNameTables[0].begin(), s.PPUNameTables[0].end(), 0x00);
    std::fill(s.PPUNameTables[1].begin(), s.PPUNameTables[1].end(), 0x00);

    return s;
}

void argos::internesceptor::ProcessMessage(const MessageParseInfo& message, PPUMessageState* ppu)
{
    auto t = static_cast<MessageType>(message.type);
    switch (t) {
        case MessageType::PPUCTRL_WRITE: {
            ppu->PPUController = message.data[0];
            break;
        }
        case MessageType::PPUMASK_WRITE: {
            ppu->PPUMask = message.data[0];
            break;
        }
        case MessageType::PPUSTATUS_READ: {
            ppu->PPUAddrLatch = 0;
            ppu->PPUScrollLatch = 0;
            break;
        }
        case MessageType::OAMADDR_WRITE: {
            break;
        }
        case MessageType::OAMDATA_WRITE: {
            break;
        }
        case MessageType::OAMDATA_READ: {
            break;
        }
        case MessageType::PPUSCROLL_WRITE: {
            //DebugPrintMessage(message, std::cout);
            //std::cout << std::endl;
            if (ppu->PPUScrollLatch == 0) {
                ppu->XScroll = message.data[0];
                ppu->PPUScrollLatch = 1;
            } else {
                ppu->YScroll = message.data[0];
                ppu->PPUScrollLatch = 0;
            }
            break;
        }
        case MessageType::PPU_ADDR_WRITE: {
            if (ppu->PPUAddrLatch == 0) {
                ppu->PPUAddrLatch = 1;
                ppu->PPUAddress = (message.data[0] << 8) | (ppu->PPUAddress & 0x00ff);
            } else {
                ppu->PPUAddress = (ppu->PPUAddress & 0xff00) | (message.data[0]);
                ppu->PPUAddrLatch = 0;
            }
            break;
        }
        case MessageType::PPU_DATA_WRITE: {
            if (ppu->PPUAddress >= 0x3F00 && ppu->PPUAddress <= 0x3FFF) {
                uint16_t a = (ppu->PPUAddress - 0x3F00) % argos::nes::FRAMEPALETTE_SIZE;
                ppu->PPUFramePalette[a] = message.data[0];
                if ((a % 4) == 0) {
                    if (a >= 0x0f) {
                        ppu->PPUFramePalette[a - 16] = message.data[0];
                    } else {
                        ppu->PPUFramePalette[a + 16] = message.data[0];
                    }
                }
            }
            if (ppu->PPUAddress >= 0x2000 && ppu->PPUAddress <= 0x2FFF) {
                uint16_t a = ppu->PPUAddress;
                if (a > 0x2800) {
                    a -= 0x800;
                }
                a -= 0x2000;

                if (a >= 0x400) {
                    ppu->PPUNameTables[1][a - 0x400] = message.data[0];
                } else {
                    ppu->PPUNameTables[0][a] = message.data[0];
                }
            }

            if (ppu->PPUController & 0b00000100) {
                ppu->PPUAddress += 32;
            } else {
                ppu->PPUAddress += 1;
            }
            break;
        }
        case MessageType::PPU_DATA_READ: {
            break;
        }
        case MessageType::OAM_DMA_WRITE: {
            break;
        }
        default: {
            break;
        }
    }

    //if (t != MessageType::PPU_REG_WRITE) {
    //    return false;
    //}

    //if (message.data[1] == 0x00) {
    //    ppu->PPUController = message.data[0];
    //}
    //if (message.data[1] == 0x06) {
    //    if (ppu->PPULatch == 0) {
    //        ppu->PPULatch = 1;
    //        ppu->PPUAddress = (message.data[0] << 8) | (ppu->PPUAddress & 0x00ff);
    //    } else {
    //        ppu->PPUAddress = (ppu->PPUAddress & 0xff00) | (message.data[0]);
    //        ppu->PPULatch = 0;
    //    }
    //} else {
    //    ppu->PPULatch = 0; // assume that it gets reset..
    //}

    //if (message.data[1] == 0x07) {
    //    if (ppu->PPUAddress >= 0x3F00 && ppu->PPUAddress <= 0x3FFF) {
    //        uint16_t a = (ppu->PPUAddress - 0x3F00) % argos::nes::FRAMEPALETTE_SIZE;
    //        ppu->PPUFramePalette[a] = message.data[0];
    //        if ((a % 4) == 0) {
    //            if (a >= 0x0f) {
    //                ppu->PPUFramePalette[a - 16] = message.data[0];
    //            } else {
    //                ppu->PPUFramePalette[a + 16] = message.data[0];
    //            }
    //        }
    //    }
    //    if (ppu->PPUAddress >= 0x2000 && ppu->PPUAddress <= 0x2FFF) {
    //        uint16_t a = ppu->PPUAddress;
    //        if (a > 0x2800) {
    //            a -= 0x800;
    //        }
    //        a -= 0x2000;

    //        if (a >= 0x400) {
    //            ppu->PPUNameTables[1][a - 0x400] = message.data[0];
    //        } else {
    //            ppu->PPUNameTables[0][a] = message.data[0];
    //        }
    //    }

    //    if (ppu->PPUController & 0b00000100) {
    //        ppu->PPUAddress += 32;
    //    } else {
    //        ppu->PPUAddress += 1;
    //    }
    //}
}


ControllerMessageState ControllerMessageState::InitialState()
{
    ControllerMessageState cont;
    cont.State = 0x00;
    cont.Latch = 0;
    return cont;
}

void argos::internesceptor::ProcessMessage(const MessageParseInfo& message, ControllerMessageState* controller)
{
    auto t = static_cast<MessageType>(message.type);
    if (t != MessageType::CONTROLLER_INFO) {
        return;
    }

    uint8_t data = message.data[0];

    if (data & CONTROLLER_INFO_READ_WRITE) {
        if (controller->Latch < 8) {
            uint8_t bitSel = 1 << controller->Latch;
            controller->State = controller->State & ~bitSel;
            if (data & CONTROLLER_INFO_BUTTON_PRESSED) {
                controller->State |= bitSel;
            }
            controller->Latch++;
        }
    } else {
        controller->Latch = 0;
    }
}

NESMessageState NESMessageState::InitialState()
{
    NESMessageState nm;
    nm.ConsolePoweredOn = false;
    nm.RamState = RAMMessageState::InitialState();
    nm.PPUState = PPUMessageState::InitialState();
    nm.ControllerState = ControllerMessageState::InitialState();
    return nm;
}

void argos::internesceptor::ProcessMessage(const MessageParseInfo& message, NESMessageState* nes)
{
    auto t = static_cast<MessageType>(message.type);
    if (t == MessageType::RST_LOW) {
        *nes = NESMessageState::InitialState();
        nes->M2Count = 0x00000000;
    } else {
        nes->ConsolePoweredOn = true;
        ProcessMessage(message, &nes->RamState);
        ProcessMessage(message, &nes->PPUState);
        ProcessMessage(message, &nes->ControllerState);

        if (t == MessageType::M2_COUNT) {
            nes->M2Count = static_cast<uint64_t>(message.data[0]) <<  8 |
                           static_cast<uint64_t>(message.data[1]) << 16 |
                           static_cast<uint64_t>(message.data[2]) << 24 |
                           static_cast<uint64_t>(message.data[3]) << 32;
        }
    }
}
