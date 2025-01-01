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
#include <sstream>
#include <unordered_set>
#include <fstream>
#include <bitset>
#include <random>

#include "zmq.hpp"
#include "zmq_addon.hpp"
#include "spdlog/spdlog.h"

#include "smb/rgms.h"
#include "nes/nesui.h"
#include "util/file.h"
#include "util/string.h"
#include "util/lerp.h"

using namespace argos::internesceptor;
using namespace argos::rgms;
using namespace argos;

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

SMBMessageProcessor::SMBMessageProcessor(smb::SMBNametableCachePtr nametables)
    : m_BackgroundNametables(nametables)
    , m_PrevAPX(-1)
    , m_LastOutM2(0)
{
    m_SoundQueues.fill(0x00);
    Reset();
}

SMBMessageProcessor::~SMBMessageProcessor()
{
}

void SMBMessageProcessor::Reset()
{
    m_SoundQueues.fill(0x00);
    m_NESState = internesceptor::NESMessageState::InitialState();
    SetOutputFromNESMessageState(m_NESState, &m_Output, m_BackgroundNametables, m_SoundQueues);
}

bool SMBMessageProcessor::OnMessage(const internesceptor::MessageParseInfo& message, int64_t elapsed)
{
    internesceptor::ProcessMessage(message, &m_NESState);
    auto t = static_cast<internesceptor::MessageType>(message.type);

    if (t == internesceptor::MessageType::RAM_WRITE) {
        internesceptor::RamWrite rw;
        internesceptor::ExtractRamWrite(message, &rw);

        if (rw.address >= smb::RamAddress::PAUSE_SOUND_QUEUE && rw.address <= smb::RamAddress::SQUARE1_SOUND_QUEUE) {
            if (rw.value) {
                // only play first sound in each queue in this fashion...
                // potentially not ideal!?
                int a = rw.address - smb::RamAddress::PAUSE_SOUND_QUEUE;
                if (m_SoundQueues.at(a) == 0x00) {
                    m_SoundQueues.at(a) = rw.value;
                }
            }
        }
    }


    // TODO this is (kinda) arbitrary lol
    if ((t == internesceptor::MessageType::CONTROLLER_INFO && m_NESState.ControllerState.Latch == 0) ||
        (t == internesceptor::MessageType::RST_LOW)) {
        m_Output.Elapsed = elapsed;
        SetOutputFromNESMessageState(m_NESState, &m_Output, m_BackgroundNametables, m_SoundQueues);
        m_SoundQueues.fill(0x00);
        if (m_Output.ConsolePoweredOn && m_Output.M2Count == m_LastOutM2) {
            return false;
        }
        m_LastOutM2 = m_Output.M2Count;

        m_Output.Frame.PrevAPX = m_PrevAPX;
        m_PrevAPX = m_Output.Frame.APX;
        return true;
    }
    return false;
}

SMBMessageProcessorOutputPtr SMBMessageProcessor::GetLatestProcessorOutput() const
{
    return std::make_shared<SMBMessageProcessorOutput>(m_Output);
}

void argos::rgms::ClearSMBMessageProcessorOutput(SMBMessageProcessorOutput* output)
{
    output->ConsolePoweredOn = false;
    output->M2Count = 0x0000;
    output->Frame.AID = static_cast<argos::smb::AreaID>(0xffff);
    output->Frame.PrevAPX = -1;
    output->Frame.APX = -1;
    output->Frame.OAMX.clear();
    output->Frame.NTDiffs.clear();
    output->Frame.Time = -1;
    output->Frame.World = 0;
    output->Frame.Level = 0;
    output->Frame.GameEngineSubroutine = 0xff;
    output->Frame.IntervalTimerControl = 0xff;
    output->Frame.OperMode = 0xff;
    output->Frame.TitleScreen.ScoreTiles.fill(0x00);
    output->Frame.TitleScreen.CoinTiles.fill(0x00);
    output->Frame.TitleScreen.LifeTiles.fill(0x00);
    output->Frame.TitleScreen.WorldTile = 0x00;
    output->Frame.TitleScreen.LevelTile = 0x00;
    output->Frame.PauseSoundQueue = 0x00;
    output->Frame.AreaMusicQueue = 0x00;
    output->Frame.EventMusicQueue = 0x00;
    output->Frame.NoiseSoundQueue = 0x00;
    output->Frame.Square2SoundQueue = 0x00;
    output->Frame.Square1SoundQueue = 0x00;
    output->Controller = 0x00;
    std::fill(output->FramePalette.begin(), output->FramePalette.end(), 0x00);
}

int argos::rgms::AreaPointerXFromData(uint8_t screenedge_pageloc, uint8_t screenedge_x_pos, argos::smb::AreaID aid, uint8_t block_buffer_84_disc)
{
    int apx = 256 * static_cast<int>(screenedge_pageloc) + static_cast<int>(screenedge_x_pos);
    if (apx < 512 && aid == argos::smb::AreaID::CASTLE_AREA_6 && block_buffer_84_disc != 0x00) { // #BIG YIKES
        apx += 1024;
    }
    return apx;
}

void SMBMessageProcessor::SetOutputFromNESMessageState(const internesceptor::NESMessageState& nes,
        SMBMessageProcessorOutput* output, smb::SMBNametableCachePtr backgroundNametables, const std::array<uint8_t, 6>& soundQueues)
{
    ClearSMBMessageProcessorOutput(output);

    output->ConsolePoweredOn = nes.ConsolePoweredOn;
    if (!output->ConsolePoweredOn) {
        return;
    }

    output->M2Count = nes.M2Count;
    output->Controller = nes.ControllerState.State;
    output->FramePalette = nes.PPUState.PPUFramePalette;

    argos::smb::AreaID aid = smb::AreaIDFromRAM(
            nes.RamState.Ram[smb::RamAddress::AREA_DATA_LOW],
            nes.RamState.Ram[smb::RamAddress::AREA_DATA_HIGH]);
    int apx = rgms::AreaPointerXFromData(
            nes.RamState.Ram[smb::RamAddress::SCREENEDGE_PAGELOC],
            nes.RamState.Ram[smb::RamAddress::SCREENEDGE_X_POS],
            aid,
            nes.RamState.Ram[smb::RamAddress::BLOCK_BUFFER_84_DISC]);

    output->Frame.AID = aid;
    output->Frame.PrevAPX = output->Frame.APX;
    output->Frame.APX = apx;
    output->Frame.IntervalTimerControl = nes.RamState.Ram[smb::RamAddress::INTERVAL_TIMER_CONTROL];
    output->Frame.GameEngineSubroutine = nes.RamState.Ram[smb::RamAddress::GAME_ENGINE_SUBROUTINE];
    output->Frame.OperMode = nes.RamState.Ram[smb::RamAddress::OPER_MODE];

    output->Frame.Time = static_cast<int>(nes.PPUState.PPUNameTables[0][0x007a]) * 100 +
                         static_cast<int>(nes.PPUState.PPUNameTables[0][0x007b]) * 10 +
                         static_cast<int>(nes.PPUState.PPUNameTables[0][0x007c]) * 1;

    output->Frame.World = nes.RamState.Ram[smb::RamAddress::WORLD_NUMBER] + 0x01;
    output->Frame.Level = nes.RamState.Ram[smb::RamAddress::LEVEL_NUMBER] + 0x01;

    auto PPUNT0At = [&](int x, int y){
        return nes.PPUState.PPUNameTables[0][x + y * nes::NAMETABLE_WIDTH_BYTES];
    };

    for (int i = 0; i < output->Frame.TitleScreen.ScoreTiles.size(); i++) {
        output->Frame.TitleScreen.ScoreTiles[i] = PPUNT0At(TITLESCREEN_SCORE_X + i, TITLESCREEN_SCORE_Y);
    }
    output->Frame.TitleScreen.CoinTiles[0] = PPUNT0At(TITLESCREEN_COIN_X + 0, TITLESCREEN_COIN_Y);
    output->Frame.TitleScreen.CoinTiles[1] = PPUNT0At(TITLESCREEN_COIN_X + 1, TITLESCREEN_COIN_Y);

    output->Frame.TitleScreen.WorldTile = PPUNT0At(TITLESCREEN_WORLD_X, TITLESCREEN_WORLD_Y);
    output->Frame.TitleScreen.LevelTile = PPUNT0At(TITLESCREEN_LEVEL_X, TITLESCREEN_LEVEL_Y);

    output->Frame.TitleScreen.LifeTiles[0] = PPUNT0At(TITLESCREEN_LIFE_X + 0, TITLESCREEN_LIFE_Y);
    output->Frame.TitleScreen.LifeTiles[1] = PPUNT0At(TITLESCREEN_LIFE_X + 1, TITLESCREEN_LIFE_Y);


    int lpage = apx / 256;
    int rpage = lpage + 1;
    if ((apx % 512) >= 256) {
        std::swap(lpage, rpage);
    }

    output->Frame.NTDiffs.clear();
    output->Frame.TopRows.clear();
    if (backgroundNametables) {
        std::array<const argos::smb::db::nametable_page*, 2> nts = {nullptr, nullptr};
        nts[0] = backgroundNametables->MaybeGetNametable(aid, lpage);
        nts[1] = backgroundNametables->MaybeGetNametable(aid, rpage);

        for (int i = 0; i < 2; i++) {
            if (i == 0) {
                for (int j = 0; j < 32 * 4; j++) {
                    int y = j / nes::NAMETABLE_WIDTH_BYTES;
                    int x = j % nes::NAMETABLE_WIDTH_BYTES;
                    if (x <= 1 || x >= 30 || y <= 1) {
                        output->Frame.TopRows.push_back(36); // yolo
                    } else {
                        output->Frame.TopRows.push_back(nes.PPUState.PPUNameTables[i][j]);
                    }
                }
                for (int j = 0; j < 32; j++) {
                    output->Frame.TopRows.push_back(nes.PPUState.PPUNameTables[i][j + nes::NAMETABLE_ATTRIBUTE_OFFSET]);
                }
            }

            if (nts[i]) {
                std::unordered_set<int> diffAttrs;
                for (int j = 32*4; j < nes::NAMETABLE_ATTRIBUTE_OFFSET; j++) {
                    if (nes.PPUState.PPUNameTables[i][j] != nts[i]->nametable[j]) {
                        int y = j / nes::NAMETABLE_WIDTH_BYTES;
                        int x = j % nes::NAMETABLE_WIDTH_BYTES;

                        int tapx = (x * 8) + static_cast<int>(nts[i]->page) * 256;
                        if ((tapx > (apx - 8)) && (tapx < (apx + 256))) {
                            smb::SMBNametableDiff diff;
                            diff.NametablePage = nts[i]->page;
                            diff.Offset = j;
                            diff.Value = nes.PPUState.PPUNameTables[i][j];

                            output->Frame.NTDiffs.push_back(diff);

                            int cy = y / 4;
                            int cx = x / 4;

                            int attrIndex = nes::NAMETABLE_ATTRIBUTE_OFFSET + cy * (nes::NAMETABLE_WIDTH_BYTES / 4) + cx;
                            if (nes.PPUState.PPUNameTables[i][attrIndex] != nts[i]->nametable[attrIndex]) {
                                diffAttrs.insert(attrIndex);
                            }

                            //output->NameTables2[i][j] = nes.PPUState.PPUNameTables[i][j];

                        }
                    }

                    for (auto & attrIndex : diffAttrs) {
                        smb::SMBNametableDiff diff;
                        diff.NametablePage = nts[i]->page;
                        diff.Offset = attrIndex;
                        diff.Value = nes.PPUState.PPUNameTables[i][attrIndex];
                        output->Frame.NTDiffs.push_back(diff);
                    }
                }
            }
        }

    }

    //output->NameTables[0] = nes.PPUState.PPUNameTables[0];
    //output->NameTables[1] = nes.PPUState.PPUNameTables[1];

    const uint8_t* fpal = output->FramePalette.data();
    for (int i = 0; i < nes::NUM_OAM_ENTRIES; i++) {
        if (i == 0) { // Skip sprite zero, it's always the bottom of the coin
            continue;
        }
        uint8_t y = nes.RamState.Ram.at(smb::RamAddress::SPRITE_DATA + (i * 4) + 0);
        uint8_t tile_index = nes.RamState.Ram.at(smb::RamAddress::SPRITE_DATA + (i * 4) + 1);
        uint8_t attributes = nes.RamState.Ram.at(smb::RamAddress::SPRITE_DATA + (i * 4) + 2);
        uint8_t x = nes.RamState.Ram.at(smb::RamAddress::SPRITE_DATA + (i * 4) + 3);

        if (y > 240) { // 'off screen'
            continue;
        }

        nes::OAMxEntry oamx;
        oamx.X = static_cast<int>(x);
        oamx.Y = static_cast<int>(y);
        oamx.TileIndex = tile_index;
        oamx.Attributes = attributes;
        oamx.PatternTableIndex = 0;

        oamx.TilePalette[0] = fpal[16];
        uint8_t p = attributes & nes::OAM_PALETTE;
        for (int j = 1; j < 4; j++) {
            oamx.TilePalette[j] = fpal[16 + p * 4 + j];
        }

        output->Frame.OAMX.push_back(oamx);
    }

    output->Frame.PauseSoundQueue = soundQueues[0];
    output->Frame.AreaMusicQueue = soundQueues[1];
    output->Frame.EventMusicQueue = soundQueues[2];
    output->Frame.NoiseSoundQueue = soundQueues[3];
    output->Frame.Square2SoundQueue = soundQueues[4];
    output->Frame.Square1SoundQueue = soundQueues[5];
}

////////////////////////////////////////////////////////////////////////////////

SMBSerialProcessor::SMBSerialProcessor(smb::SMBNametableCachePtr nametables, int maxFramesStored)
    : m_MessageProcessor(nametables)
    , m_MaxFramesStored(std::max(0, maxFramesStored))
    , m_Message(internesceptor::MessageParseInfo::InitialState())
    , m_ErrorCount(0)
    , m_MessageCount(0)
{
}

SMBSerialProcessor::~SMBSerialProcessor()
{
}

void SMBSerialProcessor::Reset()
{
    m_MessageProcessor.Reset();
    m_Message = internesceptor::MessageParseInfo::InitialState();
    m_OutputDeck.clear();
    m_ErrorCount = 0;
    m_MessageCount = 0;
}

int SMBSerialProcessor::OnBytes(const uint8_t* buffer, size_t size, bool* obtainedNewOutput, int64_t* elapsed)
{
    if (obtainedNewOutput) *obtainedNewOutput = false;
    int messageCount = 0;
    for (size_t i = 0; i < size; i++) {
        auto status = internesceptor::ProgressMessageParse(
                &m_Message, buffer[i]);
        //std::cout << fmt::format("0x{:02x}", buffer[i]) << " " << std::bitset<8>(buffer[i]) << " " << status << std::endl;
        if (internesceptor::IsMessageParseError(status)) {
            m_Message = internesceptor::MessageParseInfo::InitialState();
            m_ErrorCount++;
        } else if (status == internesceptor::MessageParseStatus::SUCCESS) {
            int64_t el = 0;
            if (elapsed) {
                el = *elapsed;
            }
            bool newOutput = OnMessage(m_Message, el);
            if (newOutput && m_MaxFramesStored > 0) {
                m_OutputDeck.push_back(m_MessageProcessor.GetLatestProcessorOutput());
                while (m_OutputDeck.size() > m_MaxFramesStored) {
                    m_OutputDeck.pop_front();
                }
            }

            if (obtainedNewOutput && newOutput) {
                *obtainedNewOutput = true;
            }

            messageCount++;
        }
    }
    return messageCount;
}

bool SMBSerialProcessor::OnMessage(const internesceptor::MessageParseInfo& message, int64_t elapsed)
{
    m_MessageCount++;
    return m_MessageProcessor.OnMessage(message, elapsed);
}

int SMBSerialProcessor::GetErrorCount() const
{
    return m_ErrorCount;
}

int SMBSerialProcessor::GetMessageCount() const
{
    return m_MessageCount;
}

SMBMessageProcessorOutputPtr SMBSerialProcessor::GetLatestProcessorOutput() const
{
    return m_MessageProcessor.GetLatestProcessorOutput();
}

SMBMessageProcessorOutputPtr SMBSerialProcessor::GetNextProcessorOutput() const
{
    if (m_OutputDeck.empty()) {
        return nullptr;
    }
    auto p = m_OutputDeck.front();
    m_OutputDeck.pop_front();
    return p;
}



////////////////////////////////////////////////////////////////////////////////

SMBSerialProcessorThreadParameters SMBSerialProcessorThreadParameters::Defaults()
{
    SMBSerialProcessorThreadParameters params;

    params.Baud = SMB_SERIAL_BAUD;
    params.BufferSize = 1024;
    params.MaxFramesStored = 128;

    return params;
}

SMBSerialProcessorThread::SMBSerialProcessorThread(const std::string& path,
        smb::SMBNametableCachePtr nametables,
        SMBSerialProcessorThreadParameters params)
    : t_SerialProcessor(nametables, params.MaxFramesStored)
    , t_SerialPort(path, params.Baud)
    , t_Buffer(params.BufferSize)
    , m_ShouldStop(false)
    , m_ErrorCount(0)
    , m_MessageCount(0)
    , m_ByteCount(0)
    , m_ApproxBytesPerSecond(0.0)
    , m_ApproxMessagesPerSecond(0.0)
    , m_IsRecording(false)
{
    std::ostringstream os;
    os << path << " @ " << params.Baud << "baud";
    m_InformationString = os.str();
    m_WatchingThread = std::thread(&SMBSerialProcessorThread::SerialThread, this);
}

SMBSerialProcessorThread::~SMBSerialProcessorThread()
{
    m_ShouldStop = true;
    StopRecording();
    m_WatchingThread.join();
}

void SMBSerialProcessorThread::GetInfo(SMBSerialProcessorThreadInfo* info)
{
    info->InformationString = m_InformationString;
    info->ByteCount = m_ByteCount;
    info->ApproxBytesPerSecond = m_ApproxBytesPerSecond;
    info->ApproxMessagesPerSecond = m_ApproxMessagesPerSecond;
    info->ErrorCount = m_ErrorCount;
    info->MessageCount = m_MessageCount;
}

void SMBSerialProcessorThread::SerialThread()
{
    while (!m_ShouldStop) {
        //t_SerialPort.SetLatency(m_Latency);
        size_t read = t_SerialPort.Read(t_Buffer.data(), t_Buffer.size());
        if (read) {
            int64_t elapsed = util::ElapsedMillisFrom(t_RecStart);
            bool obtainedNewOutput = false;
            int messageCount = t_SerialProcessor.OnBytes(t_Buffer.data(), read, &obtainedNewOutput, &elapsed);
            t_MessageRateEstimator.Tick(static_cast<double>(messageCount));
            t_ByteRateEstimator.Tick(static_cast<double>(read));

            m_ByteCount += static_cast<int>(read);
            m_ErrorCount = t_SerialProcessor.GetErrorCount();
            m_MessageCount = t_SerialProcessor.GetMessageCount();
            m_ApproxBytesPerSecond = t_ByteRateEstimator.TicksPerSecond();
            m_ApproxMessagesPerSecond = t_MessageRateEstimator.TicksPerSecond();

            if (obtainedNewOutput) {
                std::lock_guard<std::mutex> lock(m_OutputMutex);
                m_OutputLatest = t_SerialProcessor.GetLatestProcessorOutput();
                while (auto p = t_SerialProcessor.GetNextProcessorOutput()) {
                    m_OutputNext.push_back(p);
                }
            }

            if (m_IsRecording) {
                int64_t elapsed = util::ElapsedMillisFrom(t_RecStart);

                t_OutputStream->write(reinterpret_cast<const char*>(&elapsed), sizeof(elapsed));
                t_OutputStream->write(reinterpret_cast<const char*>(&read), sizeof(read));
                t_OutputStream->write(reinterpret_cast<const char*>(t_Buffer.data()), read);
            }
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    }
}

SMBMessageProcessorOutputPtr SMBSerialProcessorThread::GetLatestProcessorOutput()
{
    std::lock_guard<std::mutex> lock(m_OutputMutex);
    return m_OutputLatest;
}

SMBMessageProcessorOutputPtr SMBSerialProcessorThread::GetNextProcessorOutput()
{
    std::lock_guard<std::mutex> lock(m_OutputMutex);
    if (m_OutputNext.empty()) {
        return nullptr;
    }
    auto p = m_OutputNext.front();
    m_OutputNext.pop_front();
    return p;
}

bool SMBSerialProcessorThread::IsRecording(std::string* recordingPath) const
{
    if (recordingPath) {
        *recordingPath = m_RecordingPath;
    }
    return m_IsRecording;
}

void SMBSerialProcessorThread::StartRecording(const std::string& recordingPath)
{
    if (m_IsRecording) throw std::runtime_error("already recording");

    m_RecordingPath = recordingPath;
    t_RecStart = util::Now();
    t_OutputStream = std::make_unique<std::ofstream>(recordingPath, std::ios::binary);
    m_IsRecording = true;
}

void SMBSerialProcessorThread::StopRecording()
{
    m_IsRecording = false;
}


//////////////////////////////////////////////////////////////////////////////

ISMBSerialSource::ISMBSerialSource() {
}

ISMBSerialSource::~ISMBSerialSource() {
}

////////////////////////////////////////////////////////////////////////////////

SMBSerialRecording::SMBSerialRecording(const std::string& path,
        smb::SMBNametableCachePtr nametables)
    : m_Path(path)
    , m_DataIndex(0)
    , m_IsPaused(false)
    , m_Start(util::Now())
    , m_AddToSeek(0)
    , m_StartMillis(-1)
    , m_Nametables(nametables)
    , m_SerialProcessor(nametables, 128) // todo, more frames?
    , m_LastSeek(0)
{
    util::ReadFileToVector(path, &m_Data);
}

SMBSerialRecording::~SMBSerialRecording()
{
}

std::string SMBSerialRecording::GetPath() const
{
    return m_Path;
}

size_t SMBSerialRecording::GetNumBytes() const
{
    return m_Data.size();
}

int64_t SMBSerialRecording::GetCurrentElapsedMillis() const
{
    return m_LastSeek;
}

int64_t SMBSerialRecording::GetTotalElapsedMillis() const
{
    size_t data_index = 0;
    int64_t elapsed = 0;
    size_t read = 0;
    const uint8_t* bytes;
    while ((data_index + sizeof(elapsed) + sizeof(read) + 1) < m_Data.size()) {
        PreStep(m_Data.data(), data_index, &elapsed, &read, &bytes);
        data_index += sizeof(elapsed) + sizeof(read) + read;
    }
    return elapsed;
}

void SMBSerialRecording::Reset()
{
    m_SerialProcessor.Reset();
    m_Start = util::Now();
    m_DataIndex = 0;
    m_AddToSeek = 0;
    m_LastSeek = 0;
}

void SMBSerialRecording::ResetToStartAndPause()
{
    SetPaused(true);
    Reset();

    for (;;) {
        if (m_DataIndex >= m_Data.size()) {
            return;
        }

        DoStep(m_Data.data(), &m_DataIndex, &m_SerialProcessor);
        bool fnd = false;
        while (auto out = m_SerialProcessor.GetNextProcessorOutput()) {
            if (out->ConsolePoweredOn &&
                out->Frame.AID == argos::smb::AreaID::GROUND_AREA_6 && out->Frame.APX < 15 &&
                (out->Frame.Time <= 400 && out->Frame.Time >= 399)) {

                size_t read;
                const uint8_t* data;
                PreStep(m_Data.data(), m_DataIndex, &m_AddToSeek, &read, &data);
                fnd = true;
                break;
            }
        }
        if (fnd) {
            m_StartMillis = m_AddToSeek;
            break;
        }

    }
}

bool SMBSerialRecording::GetPaused()
{
    return m_IsPaused;
}

void SMBSerialRecording::StartAt(int64_t millis)
{
    m_AddToSeek = millis;
    SeekTo(millis);
}

void SMBSerialRecording::SeekFromStartTo(int64_t millis)
{
    if (m_StartMillis < 0) throw std::runtime_error("not ok time to use this garbage");

    SeekTo(m_StartMillis + millis);
}

void SMBSerialRecording::SetPaused(bool pause)
{
    if (pause == m_IsPaused) return;

    if (pause) {
        m_IsPaused = true;
        m_AddToSeek += util::ElapsedMillisFrom(m_Start);
    } else {
        m_IsPaused = false;
        m_Start = util::Now();
    }
}

bool SMBSerialRecording::Done() const
{
    return m_DataIndex >= m_Data.size();
}

void SMBSerialRecording::Seek()
{
    if (!m_IsPaused) {
        int64_t to = util::ElapsedMillisFrom(m_Start) + m_AddToSeek;
        SeekTo(to);
    }
}

void SMBSerialRecording::PreStep(const uint8_t* dataIn, size_t dataIndex,
        int64_t* elapsed, size_t* read, const uint8_t** byteData)
{
    const char* data = reinterpret_cast<const char*>(dataIn) + dataIndex;

    *elapsed = *reinterpret_cast<const int64_t*>(data);
    *read = *reinterpret_cast<const size_t*>(data + sizeof(*elapsed));
    *byteData = reinterpret_cast<const uint8_t*>(data + sizeof(*elapsed) + sizeof(*read));
}

void SMBSerialRecording::DoStep(const uint8_t* data, size_t* dataIndex, SMBSerialProcessor* proc)
{
    int64_t elapsed;
    size_t read;
    const uint8_t* byteData;

    PreStep(data, *dataIndex, &elapsed, &read, &byteData);
    proc->OnBytes(byteData, read, nullptr, &elapsed);
    *dataIndex += sizeof(elapsed) + sizeof(read) + read;
}

void SMBSerialRecording::SeekTo(int64_t millis) {
    m_LastSeek = millis;
    if (m_Data.empty()) return;

    if (m_DataIndex >= m_Data.size()) {
        return;
    }

    int64_t elapsed;
    size_t read;
    const uint8_t* byteData;
    while (true) {
        if (m_DataIndex >= m_Data.size()) {
            return;
        }
        PreStep(m_Data.data(), m_DataIndex, &elapsed, &read, &byteData);
        if (elapsed < millis) {
            DoStep(m_Data.data(), &m_DataIndex, &m_SerialProcessor);
        } else {
            break;
        }
    }
}

SMBMessageProcessorOutputPtr SMBSerialRecording::GetLatestProcessorOutput()
{
    Seek();
    return m_SerialProcessor.GetLatestProcessorOutput();
}

SMBMessageProcessorOutputPtr SMBSerialRecording::GetNextProcessorOutput()
{
    Seek();
    return m_SerialProcessor.GetNextProcessorOutput();
}

void SMBSerialRecording::GetAllOutputs(std::vector<SMBMessageProcessorOutputPtr>* outputs)
{
    if (!outputs) return;

    outputs->clear();
    SMBSerialProcessor proc(m_Nametables, 2);
    size_t index = 0;

    bool waitingForStart = true;

    uint64_t startM2 = 0;

    while(index < m_Data.size()) {
        DoStep(m_Data.data(), &index, &proc);
        while (auto out = proc.GetNextProcessorOutput()) {
            out->UserM2 = 0;
            if (waitingForStart &&
                out->ConsolePoweredOn &&
                out->Frame.AID == argos::smb::AreaID::GROUND_AREA_6 && out->Frame.APX < 15 &&
                (out->Frame.Time <= 400 && out->Frame.Time >= 399)) {

                startM2 = out->M2Count;
                waitingForStart = false;
            }
            if (!waitingForStart) {
                if (out->ConsolePoweredOn) {
                    out->UserM2 = out->M2Count - startM2;
                }
            }
            outputs->push_back(out);
        }
    }

    //for (size_t i = 0; i < m_Data.size(); i++) {
    //    proc.OnBytes(&m_Data[i], 1, nullptr);
    //    while (auto out = proc.GetNextProcessorOutput()) {
    //        outputs->push_back(out);
    //    }
    //}
}

void SMBSerialProcessorThread::SetLatency(int millis)
{
    m_Latency = std::max(millis, 0);
}


template <typename T>
size_t out_t(uint8_t* to, const T& v)
{
    auto* p = reinterpret_cast<T*>(to);
    *p = v;
    return sizeof(T);
}

template<typename T>
size_t in_t(const uint8_t* in, T* v)
{
    const auto* p = reinterpret_cast<const T*>(in);
    *v = *p;
    return sizeof(T);
}

inline constexpr size_t HEADER_SIZE = 4 + sizeof(int64_t) + sizeof(uint64_t) * 2 + sizeof(argos::nes::ControllerState);
inline constexpr size_t MID_SIZE = sizeof(argos::nes::FramePalette) +
    sizeof(argos::smb::AreaID) +
    sizeof(int) * 2 +
    sizeof(uint8_t) * 3 +
    sizeof(size_t) * 3 +
    sizeof(uint8_t) * 2 +
    sizeof(uint8_t) * 13 +
    sizeof(int) +
    sizeof(uint8_t) * 6;

inline constexpr size_t OAMX_SIZE = sizeof(int) * 3 +
    sizeof(uint8_t) * 6;
inline constexpr size_t NTDIFF_SIZE = sizeof(int) * 2 + sizeof(uint8_t);



void argos::rgms::OutputToBytes(SMBMessageProcessorOutputPtr ptr, std::vector<uint8_t>* buffer)
{
    buffer->resize(HEADER_SIZE);

    (*buffer)[0] = 0x69;
    (*buffer)[1] = 0x04;
    (*buffer)[2] = 0x20;
    (*buffer)[3] = (ptr->ConsolePoweredOn) ? 0x01 : 0x00;

    size_t v = 4;
    v += out_t<int64_t>(&(*buffer)[v], ptr->Elapsed);
    v += out_t<uint64_t>(&(*buffer)[v], ptr->M2Count);
    v += out_t<uint64_t>(&(*buffer)[v], ptr->UserM2);
    v += out_t<argos::nes::ControllerState>(&(*buffer)[v], ptr->Controller);

    if (ptr->ConsolePoweredOn) {
        buffer->resize(buffer->size() + MID_SIZE);

        for (int i = 0; i < argos::nes::FRAMEPALETTE_SIZE; i++) {
            v += out_t<uint8_t>(&(*buffer)[v], ptr->FramePalette[i]);
        }

        v += out_t<argos::smb::AreaID>(&(*buffer)[v], ptr->Frame.AID);
        v += out_t<int>(&(*buffer)[v], ptr->Frame.PrevAPX);
        v += out_t<int>(&(*buffer)[v], ptr->Frame.APX);
        v += out_t<uint8_t>(&(*buffer)[v], ptr->Frame.GameEngineSubroutine);
        v += out_t<uint8_t>(&(*buffer)[v], ptr->Frame.OperMode);
        v += out_t<uint8_t>(&(*buffer)[v], ptr->Frame.IntervalTimerControl);
        v += out_t<size_t>(&(*buffer)[v], ptr->Frame.OAMX.size());
        v += out_t<size_t>(&(*buffer)[v], ptr->Frame.NTDiffs.size());
        v += out_t<size_t>(&(*buffer)[v], ptr->Frame.TopRows.size());
        v += out_t<uint8_t>(&(*buffer)[v], ptr->Frame.World);
        v += out_t<uint8_t>(&(*buffer)[v], ptr->Frame.Level);
        for (int i = 0; i < 7; i++) {
            v += out_t<uint8_t>(&(*buffer)[v], ptr->Frame.TitleScreen.ScoreTiles[i]);
        }
        v += out_t<uint8_t>(&(*buffer)[v], ptr->Frame.TitleScreen.CoinTiles[0]);
        v += out_t<uint8_t>(&(*buffer)[v], ptr->Frame.TitleScreen.CoinTiles[1]);
        v += out_t<uint8_t>(&(*buffer)[v], ptr->Frame.TitleScreen.WorldTile);
        v += out_t<uint8_t>(&(*buffer)[v], ptr->Frame.TitleScreen.LevelTile);
        v += out_t<uint8_t>(&(*buffer)[v], ptr->Frame.TitleScreen.LifeTiles[0]);
        v += out_t<uint8_t>(&(*buffer)[v], ptr->Frame.TitleScreen.LifeTiles[1]);
        v += out_t<int>(&(*buffer)[v], ptr->Frame.Time);
        v += out_t<uint8_t>(&(*buffer)[v], ptr->Frame.PauseSoundQueue);
        v += out_t<uint8_t>(&(*buffer)[v], ptr->Frame.AreaMusicQueue);
        v += out_t<uint8_t>(&(*buffer)[v], ptr->Frame.EventMusicQueue);
        v += out_t<uint8_t>(&(*buffer)[v], ptr->Frame.NoiseSoundQueue);
        v += out_t<uint8_t>(&(*buffer)[v], ptr->Frame.Square2SoundQueue);
        v += out_t<uint8_t>(&(*buffer)[v], ptr->Frame.Square1SoundQueue);

        size_t oamxsize = ptr->Frame.OAMX.size();
        size_t ntdiffssize = ptr->Frame.NTDiffs.size();
        size_t toprowssize = ptr->Frame.TopRows.size();

        size_t trailer_size = oamxsize * OAMX_SIZE + ntdiffssize * NTDIFF_SIZE + toprowssize;
        buffer->resize(buffer->size() + trailer_size);

        for (size_t i = 0; i < oamxsize; i++) {
            v += out_t<int>(&(*buffer)[v], ptr->Frame.OAMX[i].X);
            v += out_t<int>(&(*buffer)[v], ptr->Frame.OAMX[i].Y);
            v += out_t<uint8_t>(&(*buffer)[v], ptr->Frame.OAMX[i].TileIndex);
            v += out_t<uint8_t>(&(*buffer)[v], ptr->Frame.OAMX[i].Attributes);
            v += out_t<int>(&(*buffer)[v], ptr->Frame.OAMX[i].PatternTableIndex);
            v += out_t<uint8_t>(&(*buffer)[v], ptr->Frame.OAMX[i].TilePalette[0]);
            v += out_t<uint8_t>(&(*buffer)[v], ptr->Frame.OAMX[i].TilePalette[1]);
            v += out_t<uint8_t>(&(*buffer)[v], ptr->Frame.OAMX[i].TilePalette[2]);
            v += out_t<uint8_t>(&(*buffer)[v], ptr->Frame.OAMX[i].TilePalette[3]);
        }
        for (size_t i = 0; i < ntdiffssize; i++) {
            v += out_t<int>(&(*buffer)[v], ptr->Frame.NTDiffs[i].NametablePage);
            v += out_t<int>(&(*buffer)[v], ptr->Frame.NTDiffs[i].Offset);
            v += out_t<uint8_t>(&(*buffer)[v], ptr->Frame.NTDiffs[i].Value);
        }
        for (size_t i = 0; i < toprowssize; i++) {
            v += out_t<uint8_t>(&(*buffer)[v], ptr->Frame.TopRows[i]);
        }
    }
}

SMBMessageProcessorOutputPtr argos::rgms::BytesToOutput(const uint8_t* bytes, size_t size)
{
    if (size < HEADER_SIZE) {
        std::cout << "bytes to output too small?" << std::endl;
        return nullptr;
    }
    if (bytes[0] != 0x69 || bytes[1] != 0x04 || bytes[2] != 0x20) {
        std::cout << "bytes to output no magic?" << std::endl;
        return nullptr;
    }

    SMBMessageProcessorOutputPtr p = std::make_shared<SMBMessageProcessorOutput>();
    ClearSMBMessageProcessorOutput(p.get());
    p->ConsolePoweredOn = bytes[3] == 0x01;

    size_t v = 4;
    v += in_t<int64_t>(&bytes[v], &p->Elapsed);
    v += in_t<uint64_t>(&bytes[v], &p->M2Count);
    v += in_t<uint64_t>(&bytes[v], &p->UserM2);
    v += in_t<argos::nes::ControllerState>(&bytes[v], &p->Controller);

    if (p->ConsolePoweredOn) {
        if (size < (HEADER_SIZE + MID_SIZE)) {
            std::cout << "bytes to output too small?" << std::endl;
            return nullptr;
        }

        for (int i = 0; i < argos::nes::FRAMEPALETTE_SIZE; i++) {
            v += in_t<uint8_t>(&bytes[v], &p->FramePalette[i]);
        }

        v += in_t<argos::smb::AreaID>(&bytes[v], &p->Frame.AID);
        v += in_t<int>(&bytes[v], &p->Frame.PrevAPX);
        v += in_t<int>(&bytes[v], &p->Frame.APX);
        v += in_t<uint8_t>(&bytes[v], &p->Frame.GameEngineSubroutine);
        v += in_t<uint8_t>(&bytes[v], &p->Frame.OperMode);
        v += in_t<uint8_t>(&bytes[v], &p->Frame.IntervalTimerControl);
        size_t oamxsize;
        size_t ntdiffssize;
        size_t toprowssize;
        v += in_t<size_t>(&bytes[v], &oamxsize);
        v += in_t<size_t>(&bytes[v], &ntdiffssize);
        v += in_t<size_t>(&bytes[v], &toprowssize);
        v += in_t<uint8_t>(&bytes[v], &p->Frame.World);
        v += in_t<uint8_t>(&bytes[v], &p->Frame.Level);
        for (int i = 0; i < 7; i++) {
            v += in_t<uint8_t>(&bytes[v], &p->Frame.TitleScreen.ScoreTiles[i]);
        }
        v += in_t<uint8_t>(&bytes[v], &p->Frame.TitleScreen.CoinTiles[0]);
        v += in_t<uint8_t>(&bytes[v], &p->Frame.TitleScreen.CoinTiles[1]);
        v += in_t<uint8_t>(&bytes[v], &p->Frame.TitleScreen.WorldTile);
        v += in_t<uint8_t>(&bytes[v], &p->Frame.TitleScreen.LevelTile);
        v += in_t<uint8_t>(&bytes[v], &p->Frame.TitleScreen.LifeTiles[0]);
        v += in_t<uint8_t>(&bytes[v], &p->Frame.TitleScreen.LifeTiles[1]);
        v += in_t<int>(&bytes[v], &p->Frame.Time);
        v += in_t<uint8_t>(&bytes[v], &p->Frame.PauseSoundQueue);
        v += in_t<uint8_t>(&bytes[v], &p->Frame.AreaMusicQueue);
        v += in_t<uint8_t>(&bytes[v], &p->Frame.EventMusicQueue);
        v += in_t<uint8_t>(&bytes[v], &p->Frame.NoiseSoundQueue);
        v += in_t<uint8_t>(&bytes[v], &p->Frame.Square2SoundQueue);
        v += in_t<uint8_t>(&bytes[v], &p->Frame.Square1SoundQueue);

        p->Frame.OAMX.resize(oamxsize);
        p->Frame.NTDiffs.resize(ntdiffssize);
        p->Frame.TopRows.resize(toprowssize);

        size_t trailer_size = oamxsize * OAMX_SIZE + ntdiffssize * NTDIFF_SIZE + toprowssize;
        if (size < (HEADER_SIZE + MID_SIZE + trailer_size)) {
            std::cout << "bytes to output trailer wrong size?" << std::endl;
            return nullptr;
        }

        for (size_t i = 0; i < oamxsize; i++) {
            v += in_t<int>(&bytes[v], &p->Frame.OAMX[i].X);
            v += in_t<int>(&bytes[v], &p->Frame.OAMX[i].Y);
            v += in_t<uint8_t>(&bytes[v], &p->Frame.OAMX[i].TileIndex);
            v += in_t<uint8_t>(&bytes[v], &p->Frame.OAMX[i].Attributes);
            v += in_t<int>(&bytes[v], &p->Frame.OAMX[i].PatternTableIndex);
            v += in_t<uint8_t>(&bytes[v], &p->Frame.OAMX[i].TilePalette[0]);
            v += in_t<uint8_t>(&bytes[v], &p->Frame.OAMX[i].TilePalette[1]);
            v += in_t<uint8_t>(&bytes[v], &p->Frame.OAMX[i].TilePalette[2]);
            v += in_t<uint8_t>(&bytes[v], &p->Frame.OAMX[i].TilePalette[3]);
        }
        for (size_t i = 0; i < ntdiffssize; i++) {
            v += in_t<int>(&bytes[v], &p->Frame.NTDiffs[i].NametablePage);
            v += in_t<int>(&bytes[v], &p->Frame.NTDiffs[i].Offset);
            v += in_t<uint8_t>(&bytes[v], &p->Frame.NTDiffs[i].Value);
        }
        for (size_t i = 0; i < toprowssize; i++) {
            v += in_t<uint8_t>(&bytes[v], &p->Frame.TopRows[i]);
        }
    }

    return p;
}

bool argos::rgms::OutputPtrsEqual(SMBMessageProcessorOutputPtr a, SMBMessageProcessorOutputPtr b)
{
    if (!a && !b) return true;
    if (!a || !b) return false;


    if (a->Elapsed != b->Elapsed) return false;
    if (a->ConsolePoweredOn != b->ConsolePoweredOn) return false;
    if (a->M2Count != b->M2Count) return false;
    if (a->UserM2 != b->UserM2) return false;
    if (a->Controller != b->Controller) return false;

    if (a->ConsolePoweredOn) {
        if (a->FramePalette != b->FramePalette) return false;
        if (a->Frame.AID != b->Frame.AID) return false;
        if (a->Frame.PrevAPX != b->Frame.PrevAPX) return false;
        if (a->Frame.APX != b->Frame.APX) return false;
        if (a->Frame.GameEngineSubroutine != b->Frame.GameEngineSubroutine) return false;
        if (a->Frame.OperMode != b->Frame.OperMode) return false;
        if (a->Frame.IntervalTimerControl != b->Frame.IntervalTimerControl) return false;
        if (a->Frame.World != b->Frame.World) return false;
        if (a->Frame.Level != b->Frame.Level) return false;
        if (a->Frame.TitleScreen.ScoreTiles != b->Frame.TitleScreen.ScoreTiles) return false;
        if (a->Frame.TitleScreen.CoinTiles != b->Frame.TitleScreen.CoinTiles) return false;
        if (a->Frame.TitleScreen.WorldTile != b->Frame.TitleScreen.WorldTile) return false;
        if (a->Frame.TitleScreen.LevelTile != b->Frame.TitleScreen.LevelTile) return false;
        if (a->Frame.TitleScreen.LifeTiles != b->Frame.TitleScreen.LifeTiles) return false;
        if (a->Frame.Time != b->Frame.Time) return false;
        if (a->Frame.PauseSoundQueue != b->Frame.PauseSoundQueue) return false;
        if (a->Frame.AreaMusicQueue != b->Frame.AreaMusicQueue) return false;
        if (a->Frame.EventMusicQueue != b->Frame.EventMusicQueue) return false;
        if (a->Frame.Square2SoundQueue != b->Frame.Square2SoundQueue) return false;
        if (a->Frame.Square1SoundQueue != b->Frame.Square1SoundQueue) return false;

        if (a->Frame.OAMX.size() != b->Frame.OAMX.size()) return false;
        for (size_t i = 0; i < a->Frame.OAMX.size(); i++) {
            if (a->Frame.OAMX[i].X != b->Frame.OAMX[i].X) return false;
            if (a->Frame.OAMX[i].Y != b->Frame.OAMX[i].Y) return false;
            if (a->Frame.OAMX[i].TileIndex != b->Frame.OAMX[i].TileIndex) return false;
            if (a->Frame.OAMX[i].Attributes != b->Frame.OAMX[i].Attributes) return false;
            if (a->Frame.OAMX[i].PatternTableIndex != b->Frame.OAMX[i].PatternTableIndex) return false;
            if (a->Frame.OAMX[i].TilePalette[0] != b->Frame.OAMX[i].TilePalette[0]) return false;
            if (a->Frame.OAMX[i].TilePalette[1] != b->Frame.OAMX[i].TilePalette[1]) return false;
            if (a->Frame.OAMX[i].TilePalette[2] != b->Frame.OAMX[i].TilePalette[2]) return false;
            if (a->Frame.OAMX[i].TilePalette[3] != b->Frame.OAMX[i].TilePalette[3]) return false;
        }
        if (a->Frame.NTDiffs.size() != b->Frame.NTDiffs.size()) return false;
        for (size_t i = 0; i < a->Frame.NTDiffs.size(); i++) {
            if (a->Frame.NTDiffs[i].NametablePage != b->Frame.NTDiffs[i].NametablePage) return false;
            if (a->Frame.NTDiffs[i].Offset != b->Frame.NTDiffs[i].Offset) return false;
            if (a->Frame.NTDiffs[i].Value != b->Frame.NTDiffs[i].Value) return false;
        }
        if (a->Frame.TopRows != b->Frame.TopRows) return false;
    }

    return true;
}

class SMBZMQContext
{
public:
    static SMBZMQContext* get_context();

    size_t connect(const std::string& bind, const std::string& p2);

    SMBMessageProcessorOutputPtr GetLatest(size_t tag);
    SMBMessageProcessorOutputPtr GetNext(size_t tag);

private:
    void pump();

    SMBZMQContext();
    ~SMBZMQContext();

    std::unique_ptr<zmq::context_t> m_context_t;
    std::unique_ptr<zmq::socket_t> m_socket_t;

    std::unordered_map<std::string, size_t> m_p2_to_tag;
    std::vector<std::deque<SMBMessageProcessorOutputPtr>> m_decks;
    std::vector<SMBMessageProcessorOutputPtr> m_lasts;
};

SMBZMQContext* SMBZMQContext::get_context() {
    static SMBZMQContext s_context;
    return &s_context;
}

SMBZMQContext::SMBZMQContext()
{
    m_context_t = std::make_unique<zmq::context_t>(4);
    m_socket_t = std::make_unique<zmq::socket_t>(*m_context_t, zmq::socket_type::sub);
    m_socket_t->set(zmq::sockopt::subscribe, "smb");
}

SMBZMQContext::~SMBZMQContext()
{
}

size_t SMBZMQContext::connect(const std::string& bind, const std::string& p2)
{
    size_t tag = m_decks.size();

    m_socket_t->connect(bind);
    m_decks.emplace_back();
    m_lasts.push_back(nullptr);
    m_p2_to_tag[p2] = tag;
    return tag;
}

void SMBZMQContext::pump()
{
    while (true) {
        std::vector<zmq::message_t> recv_msgs;
        zmq::recv_result_t result = zmq::recv_multipart(*m_socket_t,
                std::back_inserter(recv_msgs), zmq::recv_flags::dontwait);
        if (!result) {
            break;
        }

        auto it = m_p2_to_tag.find(recv_msgs[1].to_string());
        if (it != m_p2_to_tag.end()) {
            size_t tag = it->second;
            auto p = BytesToOutput(reinterpret_cast<const uint8_t*>(recv_msgs[2].data()), recv_msgs[2].size());
            m_lasts[tag] = p;
            m_decks[tag].push_back(p);
        }

        //std::cout << recv_msgs[0].to_string() << " " << recv_msgs[1].to_string() << " " << recv_msgs[2].size() << std::endl;
    }
}

SMBMessageProcessorOutputPtr SMBZMQContext::GetLatest(size_t tag)
{
    pump();
    return m_lasts.at(tag);
}

SMBMessageProcessorOutputPtr SMBZMQContext::GetNext(size_t tag)
{
    pump();
    auto& deck = m_decks.at(tag);
    if (!deck.empty()) {
        auto p = m_decks.at(tag).front();
        m_decks.at(tag).pop_front();
        return p;
    }
    return nullptr;
}


////////////////////////////////////////////////////////////////////////////////

SMBZMQRef::SMBZMQRef(const std::string& path, smb::SMBNametableCachePtr nametables)
{
    std::size_t pos = path.rfind(':');
    if (pos == std::string::npos) {
        throw std::invalid_argument("invalid path: " + path);
    }

    m_tag = SMBZMQContext::get_context()->connect(path.substr(0, pos), path.substr(pos + 1));
}

SMBZMQRef::~SMBZMQRef()
{
}

SMBMessageProcessorOutputPtr SMBZMQRef::GetLatestProcessorOutput()
{
    return SMBZMQContext::get_context()->GetLatest(m_tag);
}
SMBMessageProcessorOutputPtr SMBZMQRef::GetNextProcessorOutput()
{
    return SMBZMQContext::get_context()->GetNext(m_tag);
}

////////////////////////////////////////////////////////////////////////////////

//static void FromTxt(argos::RuntimeConfig* info, const char* nm, std::string* txt)
//{
//    std::string path = info->ArgosDirectory + "data/txt/" + std::string(nm) + ".txt";
//    *txt = rgms::util::ReadFileToString(path);
//}
//
//static void ToTxt(argos::RuntimeConfig* info, const char* nm, const std::string& txt)
//{
//    std::string path = info->ArgosDirectory + "data/txt/" + std::string(nm) + ".txt";
//    rgms::util::WriteStringToFile(path, txt);
//}

static void DrawTowerStateFrame(nes::PPUx* ppux, int x, int y, int w, int h, const nes::Palette& palette)
{
    uint8_t a = 0x36; // light tan
    uint8_t b = 0x17; // dark brown
    uint8_t c = 0x0f; // black

    uint8_t p = 0x17;
    for (int ty = y; ty < y + h; ty++) {
        for (int tx = x; tx < x + w; tx++) {
            if ((tx <= x + 1) || ty == y) {
                p = a;
            } else if (tx >= (x + w - 2) || ty == (y + h - 1)) {
                p = c;
            } else {
                p = b;
            }
            ppux->RenderPaletteData(tx, ty, 1, 1, &p, palette.data(), nes::PPUx::RPD_PLACE_PIXELS_DIRECT,
                    nes::EffectInfo::Defaults());
        }
    }

    //uint8_t d = 0x00;
    //ppux->RenderHardcodedSprite(x, y,
    //        {{a, a, a, a, a, a, a, a, a, a, a, a},
    //         {a, a, d, d, d, d, d, d, d, d, d, d},
    //         {a, a, d, d, d, d, d, d, d, d, d, d},
    //         {a, a, d, d, d, d, a, a, a, a, d, d},
    //         {a, a, d, d, d, d, a, a, a, a, c, c},
    //         {a, a, d, d, d, d, d, d, c, c, c, c},
    //         {a, a, d, d, d, d, d, d, d, d, d, d},
    //         {a, a, d, d, d, d, d, d, d, d, d, d}},
    //         palette.data(), nes::EffectInfo::Defaults());
    //ppux->RenderHardcodedSprite(x + w - 12, y,
    //        {{a, a, a, a, a, a, a, a, a, a, c, c},
    //         {d, d, d, d, d, d, d, d, d, d, c, c},
    //         {d, d, d, d, d, d, d, d, d, d, c, c},
    //         {d, a, a, a, a, d, d, d, d, d, c, c},
    //         {d, a, a, a, a, c, c, d, d, d, c, c},
    //         {d, d, d, c, c, c, c, d, d, d, c, c},
    //         {d, d, d, d, d, d, d, d, d, d, c, c},
    //         {d, d, d, d, d, d, d, d, d, d, c, c}},
    //         palette.data(), nes::EffectInfo::Defaults());
    //ppux->RenderHardcodedSprite(x + w - 8, y + h - 8,
    //        {{d, d, d, d, d, d, d, d, c},
    //         {d, d, d, d, d, d, d, d, c},
    //         {d, d, d, d, d, d, d, d, c},
    //         {d, a, a, a, a, d, d, d, c},
    //         {d, a, a, a, a, c, c, d, c},
    //         {d, d, c, c, c, c, c, d, c},
    //         {d, d, d, d, d, d, d, d, c},
    //         {c, c, c, c, c, c, c, c, c}},
    //         palette.data(), nes::EffectInfo::Defaults());
    //ppux->RenderHardcodedSprite(x, y + h - 8,
    //        {{a, a, d, d, d, d, d, d, d},
    //         {a, a, d, d, d, d, d, d, d},
    //         {a, a, d, d, d, d, d, d, d},
    //         {a, a, d, a, a, a, a, d, d},
    //         {a, a, d, a, a, a, a, c, c},
    //         {a, a, d, d, c, c, c, c, c},
    //         {a, a, d, d, d, d, d, d, d},
    //         {c, c, c, c, c, c, c, c, c}},
    //         palette.data(), nes::EffectInfo::Defaults());
}

ISMBCompSimpleWindowComponent::ISMBCompSimpleWindowComponent(std::string windowName)
    : m_WindowName(windowName)
    , m_WasClosed(false)
{
}

ISMBCompSimpleWindowComponent::~ISMBCompSimpleWindowComponent()
{
}

void ISMBCompSimpleWindowComponent::OnFrame()
{
    m_WasClosed = false;
    bool open = true;
    if (ImGui::Begin(m_WindowName.c_str(), &open)) {
        DoControls();
    }
    ImGui::End();
    if (!open) {
        m_WasClosed = true;
    }
}

bool ISMBCompSimpleWindowComponent::WindowWasClosedLastFrame()
{
    bool ret = m_WasClosed;
    m_WasClosed = false;
    return ret;
}

////////////////////////////////////////////////////////////////////////////////

ISMBCompSimpleWindowContainerComponent::ISMBCompSimpleWindowContainerComponent(std::string menuName, bool startsOpen)
    : m_MenuName(menuName)
    , m_IsOpen(startsOpen)
{
}

ISMBCompSimpleWindowContainerComponent::~ISMBCompSimpleWindowContainerComponent()
{
}

void ISMBCompSimpleWindowContainerComponent::OnFrame()
{
    bool close = false;;
    for (auto & window : m_Windows) {
        if (window->WindowWasClosedLastFrame()) {
            close = true;
        }
    }

    if (close) m_IsOpen = false;

    if (!m_IsOpen) return;
    OnSubComponentFrames();
}

void ISMBCompSimpleWindowContainerComponent::DoMenuItem()
{
    bool v = m_IsOpen;
    if (ImGui::MenuItem(m_MenuName.c_str(), NULL, v)) {
        m_IsOpen = !m_IsOpen;
    }
}

void ISMBCompSimpleWindowContainerComponent::RegisterWindow(std::shared_ptr<ISMBCompSimpleWindowComponent> window)
{
    m_Windows.push_back(window);
    RegisterSubComponent(window.get());
}

////////////////////////////////////////////////////////////////////////////////

ISMBCompSingleWindowComponent::ISMBCompSingleWindowComponent(std::string menuName, std::string windowName, bool startsOpen)
    : m_WindowName(windowName)
    , m_MenuName(menuName)
    , m_IsOpen(startsOpen)
{
}

ISMBCompSingleWindowComponent::~ISMBCompSingleWindowComponent()
{
}

void ISMBCompSingleWindowComponent::OnFrame()
{
    OnFrameAlways();
    if (!m_IsOpen) return;

    if (ImGui::Begin(m_WindowName.c_str(), &m_IsOpen)) {
        DoControls();
    }
    ImGui::End();
}

void ISMBCompSingleWindowComponent::DoMenuItem()
{
    bool v = m_IsOpen;
    if (ImGui::MenuItem(m_MenuName.c_str(), NULL, v)) {
        m_IsOpen = !m_IsOpen;
    }
}

void ISMBCompSingleWindowComponent::OnFrameAlways()
{
}

////////////////////////////////////////////////////////////////////////////////


void argos::rgms::InitializeSMBCompPlayerInputs(SMBCompPlayerInputs* inputs)
{
    inputs->Video.Path = "/dev/video0";
    inputs->Video.Crop = util::Rect2I(42, 0, 644, 480);

    inputs->Audio.Path = "alsa_input.pci-0000_05_00.0.stereo-fallback";
    inputs->Audio.Format = "PA_SIMPLE_S16LE";
    inputs->Audio.Channels = 2;
    inputs->Audio.Rate = 44100;

    inputs->Serial.Path = "/dev/ttyUSB1";
    inputs->Serial.Baud = 40000000;
}

nes::RenderInfo argos::rgms::DefaultSMBCompRenderInfo(const SMBComp& comp)
{
    nes::RenderInfo render;
    render.OffX = 0;
    render.OffY = 0;
    render.Scale = 1;
    render.PatternTables.push_back(comp.StaticData.ROM.CHR0);
    render.PatternTables.push_back(comp.StaticData.ROM.CHR1);
    render.PaletteBGR = comp.Config.Visuals.Palette.data();
    return render;
}

SMBMessageProcessorOutputPtr argos::rgms::GetLatestPlayerOutput(SMBComp& comp, const SMBCompPlayer& player)
{
    SMBCompFeed* feed = GetPlayerFeed(player, &comp.Feeds);
    if (feed && feed->Source) {
        if (!feed->CachedOutput) {
            feed->CachedOutput = feed->Source->GetLatestProcessorOutput();
        }
        return feed->CachedOutput;
    }
    return nullptr;
}

////////////////////////////////////////////////////////////////////////////////

void argos::rgms::InitializePlayerColors(PlayerColors* colors, bool mario)
{
    if (mario) {
        colors->RepresentativeColor = 0x16;
        colors->MarioColors[0] = 0x16;
        colors->MarioColors[1] = 0x27;
        colors->MarioColors[2] = 0x18;
        colors->FireMarioColors[0] = 0x37;
        colors->FireMarioColors[1] = 0x27;
        colors->FireMarioColors[2] = 0x16;
    } else {
        colors->RepresentativeColor = 0x19;
        colors->MarioColors[0] = 0x30;
        colors->MarioColors[1] = 0x27;
        colors->MarioColors[2] = 0x19;
        colors->FireMarioColors[0] = 0x37;
        colors->FireMarioColors[1] = 0x27;
        colors->FireMarioColors[2] = 0x19;
    }
    colors->OutlineColor = 0x0f;
}

void argos::rgms::InitializeSMBCompPlayer(SMBCompPlayer* player)
{
    player->UniquePlayerID = 0;
    player->Names.ShortName = "flibidy";
    player->Names.FullName = "flibidydibidy";
    InitializePlayerColors(&player->Colors, true);
    InitializeSMBCompPlayerInputs(&player->Inputs);
    player->ControllerType = argos::rgms::ControllerType::BRICK;
}

void argos::rgms::InitializeSMBCompPlayers(SMBCompPlayers* players)
{
    players->Players.clear();
    players->InvalidPlayerIDs.clear();
}

void argos::rgms::InitializeSMBCompVisuals(SMBCompVisuals* visuals)
{
    visuals->Palette = nes::DefaultPaletteBGR();
    visuals->Scale = 5;
    visuals->OtherAlpha = 0.6;
    visuals->OutlineRadius = 4;
    visuals->OutlineType = true;
    visuals->UsePlayerColors = true;
    visuals->PlayerNameAlpha = 0.3;
}

void argos::rgms::InitializeSMBCompTournament(SMBCompTournament* tournament)
{
    tournament->DisplayName = "any%";
    tournament->ScoreName = "any%";
    tournament->FileName = "anyp";

    tournament->Category = "any_percent";

    tournament->Seats.clear();
    tournament->Players.clear();

    tournament->PointIncrements = {6, 5, 4, 3, 2, 1};
    tournament->DNFInc = 0;
    tournament->DNSInc = -1;

    tournament->Schedule.clear();

    tournament->TowerName = "any%";
    tournament->CurrentRound = 0;
}

void argos::rgms::InitializeSMBCompConfiguration(SMBCompConfiguration* config)
{
    InitializeSMBCompTournament(&config->Tournament);
    InitializeSMBCompVisuals(&config->Visuals);
    InitializeSMBCompPlayers(&config->Players);
}

void argos::rgms::InitializeSMBCompCombinedViewInfo(SMBCompCombinedViewInfo* view)
{
    view->Active = true;
    view->FollowSmart = true;
    view->SmartInfo.Cnt = 60;
    view->PlayerID = 0;
    view->Type = ViewType::NO_PLAYER;
    view->AID = smb::AreaID::GROUND_AREA_6;
    view->APX = 0;
    view->Width = 1024;
    view->NamesVisible = true;
}

void argos::rgms::InitializeSMBCompPoints(SMBCompPoints* points)
{
    points->LastPoints.clear();
    points->Points.clear();
    points->Visible = false;
    points->Countdown = 0;
}

void argos::rgms::InitializeSMBComp(const argos::RuntimeConfig* info, SMBComp* comp)
{
    InitializeSMBCompStaticData(info, &comp->StaticData);
    InitializeSMBCompConfiguration(&comp->Config);
    InitializeSMBCompPoints(&comp->Points);
    InitializeSMBCompMinimap(&comp->Minimap);
    InitializeSMBCompTimingTower(&comp->Tower);
    InitializeSMBCompCombinedViewInfo(&comp->CombinedView);
    InitializeSMBCompCombinedViewInfo(&comp->CombinedView2);

    comp->FrameNumber = 0;
    comp->DoingRecordingOfRecordings = false;
    comp->BeginCountdown = false;
    comp->SetTxtViewTo = "";
    comp->SetToOverlay = false;
}

////////////////////////////////////////////////////////////////////////////////
//
//void argos::rgms::InitializeSMBCompControllerData(SMBCompControllerData* controllers)
//{
//    controllers->ControllerColors = nesui::ControllerColors::Defaults();
//    controllers->ControllerGeometry = nesui::ControllerGeometry::Defaults();
//    controllers->DogboneGeometry = nesui::DogboneGeometry::Defaults();
//    controllers->DogboneScale = 1.35;
//    controllers->ControllerScale = 1.45f;
//}
//
void argos::rgms::InitializeSMBCompROMData(argos::smb::SMBDatabase* db,
        SMBCompROMData* rom)
{
    rom->rom = db->GetBaseRom();
    rom->ROM = rom->rom->data();
    rom->PRG = rom->rom->data() + 0x10;
    rom->CHR0 = rom->rom->data() + 0x8010;
    rom->CHR1 = rom->rom->data() + 0x9010;
}

void argos::rgms::InitializeSMBCompSounds(smb::SMBDatabase* db, SMBCompSounds* sounds)
{
    argos::sdlext::SDLExtMixInit init;

    for (auto effect : smb::AudibleSoundEffects()) {
        std::vector<uint8_t> data;
        if (db->GetSoundEffectWav(effect, &data)) {
            sounds->SoundEffects[effect] = std::make_shared<argos::sdlext::SDLExtMixChunk>(data);
        } else {
            std::cout << smb::ToString(effect) << std::endl;
        }
    }

    for (auto & track : smb::AudibleMusicTracks()) {
        std::vector<uint8_t> data;
        if (db->GetMusicTrackWav(track, &data)) {
            sounds->Musics[track] = std::make_shared<argos::sdlext::SDLExtMixMusic>(data);
        } else {
            std::cout << smb::ToString(track) << std::endl;
        }
    }
}

void argos::rgms::InitializeSMBCompStaticData(const argos::RuntimeConfig* info, SMBCompStaticData* staticData)
{
    smb::SMBDatabase db(info->ArgosPathTo("smb.db"));
    if (!db.IsInit()) {
        throw std::runtime_error("SMB Database is not initialized. Run 'argos smb db init'");
    }

    staticData->Nametables = db.GetNametableCache();
    InitializeSMBRaceCategories(&db, &staticData->Categories);

    //InitializeSMBCompControllerData(&staticData->Controllers);
    InitializeSMBCompROMData(&db, &staticData->ROM);
    if (!db.GetPatternTableByName("font", &staticData->Font)) {
        throw std::runtime_error("no font?");
    }

    InitializeSMBCompSounds(&db, &staticData->Sounds);
}


////////////////////////////////////////////////////////////////////////////////
static std::mt19937 GetGen()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    return gen;
}

uint32_t argos::rgms::GetNewUniquePlayerID(const SMBCompPlayers& players)
{
    static std::mt19937 gen = GetGen();
    std::uniform_int_distribution<uint32_t> dist(10, std::numeric_limits<uint32_t>::max());

    int MAX_TRIES = 500;
    for(int i = 0; i < MAX_TRIES; i++) {
        uint32_t id = dist(gen);
        for (auto & player : players.Players) {
            if (player.UniquePlayerID == id) continue;
        }
        for (auto & invalidId : players.InvalidPlayerIDs) {
            if (invalidId == id) continue;
        }
        return id;
    }
    // I honestly don't think this is possible. But hey, if it's the future and
    // you're tracking this down then obviously something went RIGHT!
    throw std::runtime_error("Unable to find a valid UniquePlayerID.");
    return 0;
}

void argos::rgms::AddNewPlayer(SMBCompPlayers* players, const SMBCompPlayer& player)
{
    uint32_t id = GetNewUniquePlayerID(*players);
    players->Players.push_back(player);
    players->Players.back().UniquePlayerID = id;
}

void argos::rgms::RemovePlayer(SMBCompPlayers* players, uint32_t uniqueID)
{
    std::erase_if(players->Players, [=](const SMBCompPlayer& player){
        return player.UniquePlayerID == uniqueID;
    });
    players->InvalidPlayerIDs.push_back(uniqueID);
}

const SMBCompPlayer* argos::rgms::FindPlayer(const SMBCompPlayers& players, uint32_t uniqueID)
{
    for (auto & player : players.Players) {
        if (player.UniquePlayerID == uniqueID) {
            return &player;
        }
    }
    return nullptr;
}


////////////////////////////////////////////////////////////////////////////////

SMBCompConfigurationPlayersComponent::SMBCompConfigurationPlayersComponent(argos::RuntimeConfig* info, SMBCompPlayers* players, const SMBCompVisuals* visuals, const SMBCompStaticData* data)
    : ISMBCompSimpleWindowComponent("Config Players")
    , m_Info(info)
    , m_Players(players)
    , m_Visuals(visuals)
    , m_StaticData(data)
    , m_FirstInputs(true)
{
    InitializeSMBCompPlayer(&m_PendingPlayer);
}

SMBCompConfigurationPlayersComponent::~SMBCompConfigurationPlayersComponent()
{
}

bool SMBCompConfigurationPlayersComponent::PlayerNamesEditingControls(SMBCompPlayerNames* names)
{
    bool changed = false;
    changed = rgmui::InputText("name", &names->ShortName) || changed;
    changed = rgmui::InputText("long name", &names->FullName) || changed;
    return changed;
}

//void SMBCompConfigurationPlayersComponent::PlayerEditingLabel(const char* label, const SMBCompPlayer* player)
//{
//    ImGui::TextUnformatted(label);
//    ImGui::SameLine();
//    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(IM_COL32(100, 100, 100, 255)));
//    rgmui::TextFmt("{:08X}", player->UniquePlayerID);
//    ImGui::PopStyleColor();
//}

bool SMBCompConfigurationPlayersComponent::PlayerColorsEditingPopup(PlayerColors* colors, const SMBCompVisuals* visuals, const SMBCompStaticData* staticData)
{
    nes::EffectInfo effects = nes::EffectInfo::Defaults();
    effects.Opacity = 1.0f;

    nes::RenderInfo render;
    render.OffX = 0;
    render.OffY = 0;
    render.Scale = visuals->Scale;
    render.PatternTables.push_back(staticData->ROM.CHR0);
    render.PaletteBGR = visuals->Palette.data();

    nes::OAMxEntry oamx;
    oamx.X = 0;
    oamx.Y = 0;
    oamx.TileIndex = 0x3a;
    oamx.Attributes = 0x00;
    oamx.PatternTableIndex = 0;
    for (int i = 0; i < 3; i++) {
        oamx.TilePalette[i + 1] = colors->MarioColors[i];
    }

    nes::PPUx ppux(10 * 8 * render.Scale, 6 * 8 * render.Scale, nes::PPUxPriorityStatus::ENABLED);
    ppux.FillBackground(0x22, render.PaletteBGR);

    ppux.BeginOutline();

    nes::NextOAMx(&oamx,  1, 3, 0x3a, 0x00); ppux.RenderOAMxEntry(oamx, render, effects);
    nes::NextOAMx(&oamx,  1, 0, 0x37, 0x00); ppux.RenderOAMxEntry(oamx, render, effects);
    nes::NextOAMx(&oamx, -1, 1, 0x4f, 0x00); ppux.RenderOAMxEntry(oamx, render, effects);
    nes::NextOAMx(&oamx,  1, 0, 0x4f, 0x40); ppux.RenderOAMxEntry(oamx, render, effects);

    nes::NextOAMx(&oamx,  2, -3, 0x00, 0x00); ppux.RenderOAMxEntry(oamx, render, effects);
    nes::NextOAMx(&oamx,  1,  0, 0x01, 0x00); ppux.RenderOAMxEntry(oamx, render, effects);
    nes::NextOAMx(&oamx, -1,  1, 0x4c, 0x00); ppux.RenderOAMxEntry(oamx, render, effects);
    nes::NextOAMx(&oamx,  1,  0, 0x4d, 0x00); ppux.RenderOAMxEntry(oamx, render, effects);
    nes::NextOAMx(&oamx, -1,  1, 0x4a, 0x00); ppux.RenderOAMxEntry(oamx, render, effects);
    nes::NextOAMx(&oamx,  1,  0, 0x4a, 0x40); ppux.RenderOAMxEntry(oamx, render, effects);
    nes::NextOAMx(&oamx, -1,  1, 0x4b, 0x00); ppux.RenderOAMxEntry(oamx, render, effects);
    nes::NextOAMx(&oamx,  1,  0, 0x4b, 0x40); ppux.RenderOAMxEntry(oamx, render, effects);

    for (int i = 0; i < 3; i++) {
        oamx.TilePalette[i + 1] = colors->FireMarioColors[i];
    }

    nes::NextOAMx(&oamx,  2, -3, 0x00, 0x00); ppux.RenderOAMxEntry(oamx, render, effects);
    nes::NextOAMx(&oamx,  1,  0, 0x01, 0x00); ppux.RenderOAMxEntry(oamx, render, effects);
    nes::NextOAMx(&oamx, -1,  1, 0x02, 0x00); ppux.RenderOAMxEntry(oamx, render, effects);
    nes::NextOAMx(&oamx,  1,  0, 0x03, 0x00); ppux.RenderOAMxEntry(oamx, render, effects);
    nes::NextOAMx(&oamx, -1,  1, 0x04, 0x00); ppux.RenderOAMxEntry(oamx, render, effects);
    nes::NextOAMx(&oamx,  1,  0, 0x05, 0x00); ppux.RenderOAMxEntry(oamx, render, effects);
    nes::NextOAMx(&oamx, -1,  1, 0x06, 0x00); ppux.RenderOAMxEntry(oamx, render, effects);
    nes::NextOAMx(&oamx,  1,  0, 0x07, 0x00); ppux.RenderOAMxEntry(oamx, render, effects);

    if (visuals->OutlineType) {
        ppux.StrokeOutlineX(visuals->OutlineRadius, colors->OutlineColor, render.PaletteBGR);
    } else {
        ppux.StrokeOutlineO(visuals->OutlineRadius, colors->OutlineColor, render.PaletteBGR);
    }

    cv::Mat m(ppux.GetHeight(), ppux.GetWidth(), CV_8UC3, ppux.GetBGROut());
    rgmui::Mat("m", m);

    bool changed = false;

    auto PV = [&](const char* label, uint8_t* paletteIndex){
        auto v = fmt::format("{}_pal", label);
        changed = rgmui::InputPaletteIndex(v.c_str(), paletteIndex, visuals->Palette.data(),
                nes::PALETTE_ENTRIES) || changed;
        int q = static_cast<int>(*paletteIndex);
        ImGui::SameLine();
        bool p = rgmui::SliderIntExt(label, &q, 0, nes::PALETTE_ENTRIES - 1);
        changed |= p;
        if (p) {
            *paletteIndex = static_cast<int>(q);
        }
    };

    PV("main", &colors->RepresentativeColor);
    PV("outline", &colors->OutlineColor);
    PV("mario 0", &colors->MarioColors[0]);
    PV("mario 1", &colors->MarioColors[1]);
    PV("mario 2", &colors->MarioColors[2]);
    PV("fire mario 0", &colors->FireMarioColors[0]);
    PV("fire mario 1", &colors->FireMarioColors[1]);
    PV("fire mario 2", &colors->FireMarioColors[2]);

    return changed;
}

static void SetPlayerCropFromAOI(SMBCompPlayer* player, const AreaOfInterest& aoi)
{

    player->Inputs.Video.Crop.X = static_cast<int>(std::round(aoi.Crop.X));
    player->Inputs.Video.Crop.Y = static_cast<int>(std::round(aoi.Crop.Y));
    player->Inputs.Video.Crop.Width = static_cast<int>(std::round(aoi.Crop.Width));
    player->Inputs.Video.Crop.Height = static_cast<int>(std::round(aoi.Crop.Height));
}

bool SMBCompConfigurationPlayersComponent::PlayerInputsVideoEditingPopup(SMBCompPlayer* player, GetAOIcback cback)
{
    //bool changed = false;

    //ImGui::PushItemWidth(100);
    //changed = ImGui::InputInt("crop x", &player->Inputs.Video.Crop.X) || changed;
    //ImGui::SameLine();
    //changed = ImGui::InputInt("crop y", &player->Inputs.Video.Crop.Y) || changed;
    //changed = ImGui::InputInt("width ", &player->Inputs.Video.Crop.Width) || changed;
    //ImGui::SameLine();
    //changed = ImGui::InputInt("height", &player->Inputs.Video.Crop.Height) || changed;
    //ImGui::PopItemWidth();

    //if (cback) {
    //    ImGui::Separator();
    //    std::vector<NamedAreaOfInterest> naois;
    //    cback(&naois, false);
    //    rgmui::TextFmt("Select from one of the named areas of interest");
    //    rgmui::TextFmt("(.aoi) in argos::RuntimeConfig->ArgosDirectory");
    //    if (ImGui::BeginCombo("named areas of interest", "")) {
    //        for (auto & naoi : naois) {
    //            if (ImGui::Selectable(naoi.Name.c_str(), false)) {
    //                SetPlayerCropFromAOI(player, naoi.AOI);
    //                changed = true;
    //            }
    //        }

    //        ImGui::EndCombo();
    //    }
    //    if (ImGui::Button("refresh")) {
    //        cback(&naois, true);
    //    }
    //}

    //ImGui::Separator();
    //{
    //    rgmui::TextFmt("For now define the area of interest using this:");
    //    rgms::AreaOfInterest aoi;
    //    auto& crop = player->Inputs.Video.Crop;
    //    InitAOICrop(&aoi, crop.X, crop.Y, crop.Width, crop.Height, 256, 240);
    //    nlohmann::json j(aoi);
    //    std::ostringstream os;
    //    os << j;
    //    rgmui::CopyableText(fmt::format("rgms define aoi --live {} --aoi '{}'", player->Inputs.Video.Path,
    //                os.str()));
    //}

    //if (ImGui::Button("paste from clip")) {
    //    std::string txt(ImGui::GetClipboardText());
    //    nlohmann::json j = nlohmann::json::parse(txt);
    //    rgms::AreaOfInterest aoi = j;
    //    SetPlayerCropFromAOI(player, aoi);
    //    changed = true;
    //}
    //return changed;
    return false;
}


bool SMBCompConfigurationPlayersComponent::PlayerInputsEditingControls(SMBCompPlayer* player,
        GetAOIcback cback)
{
    bool changed = false;

    changed = rgmui::InputText("video path", &player->Inputs.Video.Path) || changed;
    changed = rgmui::InputText("audio path", &player->Inputs.Audio.Path) || changed;
    changed = rgmui::InputText("serial path", &player->Inputs.Serial.Path) || changed;

    return changed;
}

void SMBCompConfigurationPlayersComponent::GetNamedAreasOfInterest(std::vector<NamedAreaOfInterest>* aois, bool refresh)
{
    if (m_FirstInputs || refresh) {
        //m_NamedAreasOfInterest.clear();

        //ScanDirectoryForAOIs(m_Info->ArgosDirectory, &m_NamedAreasOfInterest);
        //std::erase_if(m_NamedAreasOfInterest, [](const NamedAreaOfInterest& aoi){
        //    return aoi.AOI.Type != AreaOfInterestType::CROP;
        //});
        m_FirstInputs = false;
    }

    *aois = m_NamedAreasOfInterest;
}


bool SMBCompConfigurationPlayersComponent::PlayerEditingControls(const char* label, SMBCompPlayer* player,
        const SMBCompVisuals* visuals, GetAOIcback cback, const SMBCompStaticData* staticData)
{
    ImGui::PushID(player->UniquePlayerID);

    bool changed = false;
    changed = PlayerNamesEditingControls(&player->Names) || changed;
    changed = PlayerInputsEditingControls(player, cback) || changed;
    //changed = rgmui::Combo3("controller type", &player->ControllerType,
    //        std::vector<nesui::ControllerType>{nesui::ControllerType::BRICK, nesui::ControllerType::DOGBONE},
    //        {"brick", "dogbone"});

    auto v = fmt::format("{}_color_popup", player->UniquePlayerID);
    if (ImGui::Button("edit colors")) {
        ImGui::OpenPopup(v.c_str());
    }
    ImGui::SameLine();
    auto q = fmt::format("{}_edit_video_popup", player->UniquePlayerID);
    if (ImGui::Button("edit video")) {
        ImGui::OpenPopup(q.c_str());
    }
    ImGui::SameLine();
    auto l = fmt::format("{}_edit_inputs_popup", player->UniquePlayerID);
    if (ImGui::Button("edit input")) {
        ImGui::OpenPopup(l.c_str());
    }

    if (ImGui::BeginPopup(v.c_str())) {
        changed = PlayerColorsEditingPopup(&player->Colors, visuals, staticData) || changed;
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup(q.c_str())) {
        changed = PlayerInputsVideoEditingPopup(player, cback) || changed;
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup(l.c_str())) {
        changed = ImGui::InputInt("baud", &player->Inputs.Serial.Baud);
        ImGui::Separator();
        changed = rgmui::InputText("format", &player->Inputs.Audio.Format);
        changed = ImGui::InputInt("channels", &player->Inputs.Audio.Channels);
        changed = ImGui::InputInt("rate", &player->Inputs.Audio.Rate);
        ImGui::EndPopup();
    }

    ImGui::PopID();
    return changed;
}


void SMBCompConfigurationPlayersComponent::DoControls()
{
    auto TGetAOIcback = [this](std::vector<NamedAreaOfInterest>* aois, bool refresh){
        GetNamedAreasOfInterest(aois, refresh);
    };

    uint32_t idToRemove = 0;
    for (auto & player : m_Players->Players) {
        auto v = fmt::format("{:08X}", player.UniquePlayerID);
        ImGui::PushID(v.c_str());
        if (ImGui::CollapsingHeader(v.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            PlayerEditingControls(player.Names.ShortName.c_str(), &player, m_Visuals, TGetAOIcback, m_StaticData);
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(63, 9, 4, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(130, 46, 34, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(132, 35, 92, 255));
            if (ImGui::Button("remove player from competition")) {
                idToRemove = player.UniquePlayerID;
            }
            ImGui::PopStyleColor(3);
        }
        ImGui::PopID();
    }

    if (idToRemove != 0) {
        RemovePlayer(m_Players, idToRemove);
    }

    if (ImGui::CollapsingHeader("Pending player", ImGuiTreeNodeFlags_DefaultOpen)) {
        PlayerEditingControls("Pending Player", &m_PendingPlayer, m_Visuals, TGetAOIcback, m_StaticData);
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(49, 99, 0, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(129, 186, 40, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(14, 105, 46, 255));
        if (ImGui::Button("add pending player to competition")) {
            AddNewPlayer(m_Players, m_PendingPlayer);
        }
        ImGui::PopStyleColor(3);
    }
}



//////////////////////////////////////////////////////////////////////////////

SMBCompConfigurationVisualsComponent::SMBCompConfigurationVisualsComponent(SMBCompVisuals* visuals)
    : ISMBCompSimpleWindowComponent("Config Visuals")
    , m_Visuals(visuals)
{
}

SMBCompConfigurationVisualsComponent::~SMBCompConfigurationVisualsComponent()
{
}

void SMBCompConfigurationVisualsComponent::DoControls()
{
    nesui::NESPaletteComponentOptions options;
    options.BGROrder = true;
    options.AllowEdits = true;

    nesui::NESPaletteComponent::Controls(&m_Visuals->Palette, options);
    //rgmui::SliderIntExt("scale", &m_Visuals->Scale, 1, 6);
    //rgmui::SliderFloatExt("outline radius", &m_Visuals->OutlineRadius, 0.0f, 10.0f);
    //ImGui::Checkbox("outline type", &m_Visuals->OutlineType);
    ImGui::Checkbox("use player colors", &m_Visuals->UsePlayerColors);
    rgmui::SliderFloatExt("other alpha", &m_Visuals->OtherAlpha, 0.0f, 1.0f);
    rgmui::SliderFloatExt("name alpha", &m_Visuals->PlayerNameAlpha, 0.0f, 1.0f);
}

//////////////////////////////////////////////////////////////////////////////

int argos::rgms::ScanDirectoryForSMBCompConfigurations(const std::string& directory, std::vector<NamedSMBCompConfiguration>* configs)
{
    int cnt = 0;
    util::ForFileOfExtensionInDirectory(directory, RGMS_SMB_CONFIG_EXTENSION, [&](util::fs::path p){
        if (configs) {
            configs->emplace_back();
            NamedSMBCompConfiguration& config = configs->back();;

            config.Name = p.stem().stem().string();

            std::ifstream ifs(p);
            nlohmann::json j;
            ifs >> j;
            config.Config = j;
        }
        cnt++;
        return true;
    });
    return cnt;
}

//////////////////////////////////////////////////////////////////////////////

SMBCompConfigurationSaveLoadComponent::SMBCompConfigurationSaveLoadComponent(argos::RuntimeConfig* info,
        SMBCompConfiguration* config)
    : ISMBCompSimpleWindowComponent("Config SaveLoad")
    , m_Info(info)
    , m_Config(config)
    , m_FirstLoad(true)
{
}

SMBCompConfigurationSaveLoadComponent::~SMBCompConfigurationSaveLoadComponent()
{
}

void SMBCompConfigurationSaveLoadComponent::DoControls()
{
    if (ImGui::BeginCombo("Config Name", m_LastName.c_str())) {
        if (m_FirstLoad) {
            rgms::ScanDirectoryForSMBCompConfigurations(m_Info->ArgosDirectory, &m_KnownConfigs);
            m_FirstLoad = false;
        }
        for (auto & nconfig : m_KnownConfigs) {
            if (ImGui::Selectable(nconfig.Name.c_str(), nconfig.Name == m_LastName)) {
                *m_Config = nconfig.Config;
                m_LastName = nconfig.Name;
            }
        }
        ImGui::EndCombo();
    }

    if (ImGui::Button("scan root directory")) {
        m_KnownConfigs.clear();
        rgms::ScanDirectoryForSMBCompConfigurations(m_Info->ArgosDirectory, &m_KnownConfigs);
    }
    ImGui::Separator();

    bool s = rgmui::InputText("save as", &m_PendingName, ImGuiInputTextFlags_EnterReturnsTrue);
    std::string path = fmt::format("{}{}{}", m_Info->ArgosDirectory, m_PendingName, RGMS_SMB_CONFIG_EXTENSION);
    bool pendingValid = !m_PendingName.empty(); // TODO

    if (!pendingValid) {
        path = "ERROR: path must not be empty";
        ImGui::BeginDisabled();
        s = false;
    }
    if (ImGui::Button("save") || s) {
        std::ofstream of(path);
        nlohmann::json j(*m_Config);
        of << std::setw(2) << j << std::endl;
        m_LastName = m_PendingName;
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(path.c_str());
    if (!pendingValid) {
        ImGui::EndDisabled();
    }
}

void SMBCompConfigurationSaveLoadComponent::LoadNamedConfig(const std::string& name)
{
    if (m_FirstLoad) {
        rgms::ScanDirectoryForSMBCompConfigurations(m_Info->ArgosDirectory, &m_KnownConfigs);
        m_PendingName = name;
        m_FirstLoad = false;
    }
    bool found = false;
    for (auto & nconfig : m_KnownConfigs) {
        if (nconfig.Name == name) {
            found = true;

            *m_Config = nconfig.Config;
            m_LastName = name;
            break;
        }
    }
    if (!found) {
        m_PendingName = name;
    }
}

//////////////////////////////////////////////////////////////////////////////

SMBCompConfigurationTournamentComponent::SMBCompConfigurationTournamentComponent(SMBComp* comp, const SMBCompStaticData* staticData)
    : ISMBCompSimpleWindowComponent("Config Tournament")
    , m_Comp(comp)
    , m_StaticData(staticData)
{
}

SMBCompConfigurationTournamentComponent::~SMBCompConfigurationTournamentComponent()
{
}

void SMBCompConfigurationTournamentComponent::DoControls()
{
    auto* tournament = &m_Comp->Config.Tournament;
    rgmui::InputText("display name", &tournament->DisplayName);
    rgmui::InputText("tower name", &tournament->TowerName);
    rgmui::InputText("score name", &tournament->ScoreName);
    rgmui::InputText("file name", &tournament->FileName);

    if (rgmui::Combo4("category", &tournament->Category,
                m_StaticData->Categories.CategoryNames,
                m_StaticData->Categories.CategoryNames)) {
        ResetSMBCompTimingTower(&m_Comp->Tower);
    }


    ////if (ImGui::CollapsingHeader("increments")) {
    ////    rgmui::InputVectorInt("increments", &race->Increments);
    ////}
}

//////////////////////////////////////////////////////////////////////////////

SMBCompConfigurationComponent::SMBCompConfigurationComponent(argos::RuntimeConfig* info, SMBComp* comp,
        const SMBCompStaticData* staticData)
    : ISMBCompSimpleWindowContainerComponent("Configuration", false)
{
    SMBCompConfiguration* config = &comp->Config;
    m_SaveLoadComponent = std::make_shared<SMBCompConfigurationSaveLoadComponent>(info, config);
    RegisterWindow(m_SaveLoadComponent);
    RegisterWindow(std::make_shared<SMBCompConfigurationTournamentComponent>(comp, staticData));
    RegisterWindow(std::make_shared<SMBCompConfigurationVisualsComponent>(&config->Visuals));
    RegisterWindow(std::make_shared<SMBCompConfigurationPlayersComponent>(info, &config->Players, &config->Visuals, staticData));
}

SMBCompConfigurationComponent::~SMBCompConfigurationComponent()
{
}

void SMBCompConfigurationComponent::LoadNamedConfig(const std::string& name)
{
    m_SaveLoadComponent->LoadNamedConfig(name);
}

////////////////////////////////////////////////////////////////////////////////

SMBCompPlayerWindow::SMBCompPlayerWindow(
        SMBComp* comp,
        uint32_t id,
        const SMBCompPlayers* players,
        const SMBCompVisuals* visuals,
        const SMBCompStaticData* staticData,
        SMBCompFeeds* feeds,
        argos::RuntimeConfig* info,
        std::vector<std::string>* priorRecordings
        )
    : ISMBCompSimpleWindowComponent(fmt::format("{:08X}", id))
    , m_Competition(comp)
    , m_PlayerID(id)
    , m_Players(players)
    , m_Visuals(visuals)
    , m_StaticData(staticData)
    , m_Feeds(feeds)
    , m_Info(info)
    , m_PriorRecordings(priorRecordings)
{
}

SMBCompPlayerWindow::~SMBCompPlayerWindow()
{
}

const SMBCompPlayer* SMBCompPlayerWindow::GetPlayer()
{
    for (auto & thisPlayer : m_Players->Players) {
        if (thisPlayer.UniquePlayerID == m_PlayerID) {
            return &thisPlayer;
        }
    }
    return nullptr;
}

void SMBCompPlayerWindow::DoControls()
{
    const SMBCompPlayer* player = GetPlayer();
    if (!player) {
        ImGui::TextUnformatted("no player of this ID found");
        return;
    }

    DoControls(player);
}

//void SMBCompPlayerWindow::DoVideoControls(const SMBCompPlayer* player, SMBCompFeeds* feeds)
//{
//    SMBCompFeed* feed = GetPlayerFeed(*player, feeds);
//    if (!feed->LiveVideoThread) {
//        if (ImGui::Button(fmt::format("open video: {}", player->Inputs.Video.Path).c_str())) {
//            feed->ErrorMessage = "";
//            InitializeFeedLiveVideoThread(*player, feed);
//        }
//    } else {
//        if (feed->LiveVideoThread->HasError()) {
//            rgmui::TextFmt("error: {}", feed->LiveVideoThread->GetError());
//        } else {
//            auto v = feed->LiveVideoThread->InputInformation();
//            ImGui::TextWrapped("info: %s", v.c_str());
//        }
//
//        auto f = feed->LiveVideoThread->GetLatestFrame();
//        if (f) {
//            cv::Mat img = cv::Mat(f->Height, f->Width, CV_8UC3, f->Buffer);
//
//            rgms::AreaOfInterest aoi;
//            auto& crop = player->Inputs.Video.Crop;
//            InitAOICrop(&aoi, crop.X, crop.Y, crop.Width, crop.Height, 256, 240);
//            cv::Mat t = rgms::ExtractAOI(img, aoi);
//            cv::resize(t, t, {}, 2, 2, cv::INTER_NEAREST);
//
//            rgmui::Mat("img", t);
//        }
//
//        if (ImGui::Button("close")) {
//            feed->LiveVideoThread.reset();
//        }
//    }
//}
//
//static void DoPlayerController(nesui::ControllerType type, nes::ControllerState state, const SMBCompControllerData& data)
//{
//    nesui::NESControllerComponentOptions options;
//    options.Colors = data.ControllerColors;
//    options.Geometry = data.ControllerGeometry;
//    options.DogGeometry = data.DogboneGeometry;
//
//    options.IsDogbone = type == nesui::ControllerType::DOGBONE;
//    if (options.IsDogbone) {
//        options.Scale = data.DogboneScale;
//    } else {
//        options.Scale = data.ControllerScale;
//    }
//    options.ButtonPad = 0.0f;
//    options.AllowEdits = false;
//
//    nesui::NESControllerComponent::Controls(&state, &options);
//}

static void DoPlayerFramePalette(const nes::Palette& palette, const nes::FramePalette& framePalette)
{
    nesui::FramePaletteComponentOptions options;
    options.BGROrder = true;
    options.AllowEdits = false;
    options.NesPalette = palette;
    options.NesPaletteP = nullptr;

    nes::FramePalette fpal = framePalette;
    nesui::FramePaletteComponent::Controls(&fpal, options, nullptr);
}

static void DoPlayerOAMX(const nes::Palette& palette, uint8_t bg, const std::vector<nes::OAMxEntry> OAMX, const SMBCompStaticData* staticData)
{
    nes::PPUx ppux(256, 240, nes::PPUxPriorityStatus::ENABLED);
    ppux.FillBackground(bg, palette.data());

    nes::EffectInfo effects = nes::EffectInfo::Defaults();
    effects.Opacity = 1.0f;

    nes::RenderInfo render;
    render.OffX = 0;
    render.OffY = 0;
    render.Scale = 1;
    render.PatternTables.push_back(staticData->ROM.CHR0);
    render.PaletteBGR = palette.data();

    for (auto & oamx : OAMX) {
        ppux.RenderOAMxEntry(oamx, render, effects);
    }

    cv::Mat m(ppux.GetHeight(), ppux.GetWidth(), CV_8UC3, ppux.GetBGROut());
    rgmui::MatAnnotator anno("m", m);
}

static void DoPlayerAPX(const nes::Palette& palette, const nes::FramePalette& fpal,
        smb::AreaID aid, int apx, const std::vector<smb::SMBNametableDiff>& diffs, const SMBCompStaticData* staticData)
{
    nes::PPUx ppux(256, 240, nes::PPUxPriorityStatus::ENABLED);
    //std::cout << static_cast<int>(ap) << " " << apx << " ";

    staticData->Nametables->RenderTo(aid, apx, 256, &ppux, 0, palette,
            staticData->ROM.CHR1, nullptr, fpal.data(), &diffs);

    //int page = apx / 256;
    //auto* bgnt = staticData->Nametables.FindNametable(ap, page);
    //if (bgnt) {
    //    //std::cout << "fnd" << std::endl;
    //    nes::EffectInfo effects = nes::EffectInfo::Defaults();
    //    effects.Opacity = 1.0f;

    //    nes::RenderInfo render;
    //    render.OffX = 0;
    //    render.OffY = 0;
    //    render.Scale = 1;
    //    render.PatternTables.push_back(staticData->ROM.CHR0);
    //    render.PatternTables.push_back(staticData->ROM.CHR1);
    //    render.PaletteBGR = palette.data();

    //    nes::Nametablex ntx;
    //    ntx.X = 0;
    //    ntx.Y = 0;
    //    ntx.NametableP = &bgnt->Nametable;
    //    ntx.FramePalette = fpal;
    //    ntx.PatternTableIndex = 1;

    //    ppux.RenderNametableX(ntx, render, effects);
    //} else {
    //    //std::cout << "NOT FOUND?" << std::endl;
    //}

    cv::Mat m(ppux.GetHeight(), ppux.GetWidth(), CV_8UC3, ppux.GetBGROut());
    rgmui::Mat("m2", m);
}

void SMBCompPlayerWindow::DoSerialPlayerOutput(const SMBCompPlayer* player, const
        SMBMessageProcessorOutput* out)
{
    ImGui::TextUnformatted("console: ");
    ImGui::SameLine();
    if (out->ConsolePoweredOn) {
        rgmui::GreenText("ON");
    } else {
        rgmui::RedText("OFF");
    }
    rgmui::TextFmt("m2: 0x{:016x} uptime: {}", out->M2Count,
            util::SimpleMillisFormat(static_cast<int64_t>(static_cast<double>(out->M2Count) * nes::NTSC_MS_PER_M2),
                util::SimpleTimeFormatFlags::MSCS));

    //DoPlayerController(player->ControllerType, out->Controller, m_StaticData->Controllers);
    DoPlayerFramePalette(m_Visuals->Palette, out->FramePalette);

    rgmui::TextFmt("game engine subroutine: 0x{:02x}", out->Frame.GameEngineSubroutine);
    rgmui::TextFmt("oper mode: 0x{:02x}", out->Frame.OperMode);
    rgmui::TextFmt("area id: {} - apx {:4d}", ToString(out->Frame.AID), out->Frame.APX);
    rgmui::TextFmt("interval timer control: 0x{:02x}", out->Frame.IntervalTimerControl);
    if (out->Frame.IntervalTimerControl == 0) {
        ImGui::SameLine();
        rgmui::TextFmt("zero");
    }
    if (out->Frame.Time >= 0) {
        rgmui::TextFmt("time: {:03d}", out->Frame.Time);
    } else {
        rgmui::TextFmt("time:");
    }
    rgmui::TextFmt("{}-{}", out->Frame.World, out->Frame.Level);
    rgmui::TextFmt("|oamx|: {:03d}  |diff|: {:04d}", out->Frame.OAMX.size(), out->Frame.NTDiffs.size());

    DoPlayerOAMX(m_Visuals->Palette, out->FramePalette[0], out->Frame.OAMX, m_StaticData);
    ImGui::SameLine();
    DoPlayerAPX(m_Visuals->Palette, out->FramePalette, out->Frame.AID, out->Frame.APX, out->Frame.NTDiffs, m_StaticData);


    //{
    //    nes::PPUx ppux(512, 240, nes::PPUxPriorityStatus::ENABLED);

    //    ppux.RenderNametable(0, 0, nes::NAMETABLE_WIDTH_BYTES, nes::NAMETABLE_HEIGHT_BYTES,
    //            out->NameTables[0].data(),
    //            out->NameTables[0].data() + nes::NAMETABLE_ATTRIBUTE_OFFSET,
    //            m_StaticData->ROM.CHR1,
    //            out->FramePalette.data(),
    //            m_Visuals->Palette.data(),
    //            1, nes::EffectInfo::Defaults());

    //    ppux.RenderNametable(256, 0, nes::NAMETABLE_WIDTH_BYTES, nes::NAMETABLE_HEIGHT_BYTES,
    //            out->NameTables[1].data(),
    //            out->NameTables[1].data() + nes::NAMETABLE_ATTRIBUTE_OFFSET,
    //            m_StaticData->ROM.CHR1,
    //            out->FramePalette.data(),
    //            m_Visuals->Palette.data(),
    //            1, nes::EffectInfo::Defaults());


    //    cv::Mat m(ppux.GetHeight(), ppux.GetWidth(), CV_8UC3, ppux.GetBGROut());
    //    rgmui::Mat("m99", m);
    //}
    //{
    //    nes::PPUx ppux(512, 240, nes::PPUxPriorityStatus::ENABLED);

    //    ppux.RenderNametable(0, 0, nes::NAMETABLE_WIDTH_BYTES, nes::NAMETABLE_HEIGHT_BYTES,
    //            out->NameTables2[0].data(),
    //            out->NameTables2[0].data() + nes::NAMETABLE_ATTRIBUTE_OFFSET,
    //            m_StaticData->ROM.CHR1,
    //            out->FramePalette.data(),
    //            m_Visuals->Palette.data(),
    //            1, nes::EffectInfo::Defaults());

    //    ppux.RenderNametable(256, 0, nes::NAMETABLE_WIDTH_BYTES, nes::NAMETABLE_HEIGHT_BYTES,
    //            out->NameTables2[1].data(),
    //            out->NameTables2[1].data() + nes::NAMETABLE_ATTRIBUTE_OFFSET,
    //            m_StaticData->ROM.CHR1,
    //            out->FramePalette.data(),
    //            m_Visuals->Palette.data(),
    //            1, nes::EffectInfo::Defaults());


    //    cv::Mat m(ppux.GetHeight(), ppux.GetWidth(), CV_8UC3, ppux.GetBGROut());
    //    rgmui::Mat("m69", m);
    //}
}

void SMBCompPlayerWindow::DoSerialControls(const SMBCompPlayer* player, SMBCompFeeds* feeds)
{
    SMBCompFeed* feed = GetPlayerFeed(*player, feeds);
    if (!feed->MySMBSerialProcessorThread) {
        if (ImGui::Button(fmt::format("open serial: {}", player->Inputs.Serial.Path).c_str())) {
            feed->ErrorMessage = "";
            InitializeFeedSerialThread(*player, *m_StaticData, feed);
        }
    } else {
        SMBSerialProcessorThreadInfo info;
        feed->MySMBSerialProcessorThread->GetInfo(&info);

        auto& thread = feed->MySMBSerialProcessorThread;

        std::string recordingPath;
        if (thread->IsRecording(&recordingPath)) {
            rgmui::RedText(fmt::format("RECORDING: {}", recordingPath).c_str());
            ImGui::SameLine();
            if (ImGui::Button("stop")) {
                thread->StopRecording();
            }
        } else {
            recordingPath = fmt::format("{}rec/{}_{}_{}.rec", m_Info->ArgosDirectory, util::GetTimestampNow(),
                    m_Competition->Config.Tournament.FileName,
                    player->Names.ShortName);
            if (ImGui::Button("record")) {
                thread->StartRecording(recordingPath);
            }
        }

        ImGui::Separator();

        rgmui::TextFmt("{}", player->Inputs.Serial.Path);
        rgmui::TextFmt("{:12d} bytes    {:10.1f} bps  {} errors", info.ByteCount, info.ApproxBytesPerSecond, info.ErrorCount);
        rgmui::TextFmt("{:12d} messages {:10.1f} mps", info.MessageCount, info.ApproxMessagesPerSecond);

        ImGui::Separator();
        auto out = feed->MySMBSerialProcessorThread->GetLatestProcessorOutput();
        if (out) {
            DoSerialPlayerOutput(player, out.get());
        }

        if (ImGui::Button("close")) {
            feed->MySMBSerialProcessorThread.reset();
            feed->Source = nullptr;
        }
    }
}

void SMBCompPlayerWindow::DoControls(const SMBCompPlayer* player)
{
    ImGui::TextUnformatted(player->Names.ShortName.c_str());

    auto f = ImGuiTreeNodeFlags_DefaultOpen;
    SMBCompFeed* feed = GetPlayerFeed(*player, m_Feeds);
    if (feed->ErrorMessage != "") {
        rgmui::RedText("Error:");
        ImGui::SameLine();
        ImGui::TextUnformatted(feed->ErrorMessage.c_str());
        if (ImGui::Button("clear error")) {
            feed->ErrorMessage = "";
        }
    }

    //if (ImGui::CollapsingHeader(fmt::format("{:08X} - Video", player->UniquePlayerID).c_str(), f)) {
    //    rgmui::PushPopID h("video");
    //    DoVideoControls(player, m_Feeds);
    //}
    if (ImGui::CollapsingHeader(fmt::format("{:08X} - Serial", player->UniquePlayerID).c_str(), f)) {
        rgmui::PushPopID h("serial");
        DoSerialControls(player, m_Feeds);
    }
    if (ImGui::CollapsingHeader(fmt::format("{:08X} - Recording", player->UniquePlayerID).c_str(), f)) {
        rgmui::PushPopID h("recording");
        DoRecordingControls(player, m_Feeds);
    }
}

void SMBCompPlayerWindow::UpdateRecordings2(argos::RuntimeConfig* info, std::vector<std::string>* priorRecordings)
{
    priorRecordings->clear();
    util::ForFileOfExtensionInDirectory(fmt::format("{}rec/", info->ArgosDirectory), "rec", [&](util::fs::path p){
        priorRecordings->push_back(p.string());
        return true;
    });
    if (!priorRecordings->empty()) {
        std::sort(priorRecordings->begin(), priorRecordings->end());
    }
}

void SMBCompPlayerWindow::UpdateRecordings()
{
    UpdateRecordings2(m_Info, m_PriorRecordings);
}

void SMBCompPlayerWindow::DoRecordingControls(const SMBCompPlayer* player, SMBCompFeeds* feeds)
{
    SMBCompFeed* feed = GetPlayerFeed(*player, feeds);
    if (!feed->MySMBSerialRecording) {
        if (ImGui::BeginCombo("recording", m_PendingRecording.c_str())) {
            if (m_PriorRecordings->size() == 1 && m_PriorRecordings->at(0) == "first time") {
                UpdateRecordings();
            }

            for (auto & rec : *m_PriorRecordings) {
                if (ImGui::Selectable(rec.c_str(), rec == m_PendingRecording)) {
                    m_PendingRecording = rec;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("refresh")) {
            UpdateRecordings();
        }

        if (ImGui::Button("open")) {
            InitializeFeedRecording(feed, *m_StaticData, m_PendingRecording);
        }
        ImGui::SameLine();
        if (ImGui::Button("open to start")) {
            InitializeFeedRecording(feed, *m_StaticData, m_PendingRecording);
            if (feed->MySMBSerialRecording) {
                feed->MySMBSerialRecording->ResetToStartAndPause();
            }
        }

    } else {
        auto* recording = feed->MySMBSerialRecording.get();
        rgmui::TextFmt("{}: {}", recording->GetPath(), util::BytesFmt(recording->GetNumBytes()));
        if (ImGui::Button("Reset")) {
            recording->Reset();
        }
        ImGui::SameLine();
        if (ImGui::Button("ResetToStart")) {
            recording->ResetToStartAndPause();
        }

        if (recording->GetPaused()) {
            if (ImGui::Button("Resume")) {
                recording->SetPaused(false);
            }
        } else {
            if (ImGui::Button("Pause")) {
                recording->SetPaused(true);
            }
        }

        ImGui::Separator();
        auto out = feed->Source->GetLatestProcessorOutput();
        if (out) {
            DoSerialPlayerOutput(player, out.get());
        }


        if (ImGui::Button("close")) {
            feed->MySMBSerialRecording.reset();
            feed->Source = nullptr;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

SMBCompPlayerWindowsComponent::SMBCompPlayerWindowsComponent(argos::RuntimeConfig* info, SMBComp* comp)
    : m_Info(info)
    , m_Competition(comp)
    , m_IsOpen(false)
{
    m_PriorRecordings = {"first time"};
}

SMBCompPlayerWindowsComponent::~SMBCompPlayerWindowsComponent()
{
}

void SMBCompPlayerWindowsComponent::OnFrame()
{
    if (!m_IsOpen) return;

    std::unordered_set<uint32_t> existing;
    for (auto & player : m_Competition->Config.Players.Players) {
        uint32_t id = player.UniquePlayerID;
        existing.insert(id);
        if (m_Windows.find(id) == m_Windows.end()) {
            m_Windows.emplace(id, std::make_shared<SMBCompPlayerWindow>(m_Competition, id, &m_Competition->Config.Players,
                        &m_Competition->Config.Visuals, &m_Competition->StaticData, &m_Competition->Feeds, m_Info,
                        &m_PriorRecordings));
        }
    }

    bool wasClosed = false;
    std::unordered_set<uint32_t> toerase;
    for (auto & [key, window] : m_Windows) {
        if (existing.find(key) == existing.end()) {
            toerase.insert(key);
        } else if (m_IsOpen) {
            ImGui::PushID(key);
            window->OnFrame();
            ImGui::PopID();
            wasClosed = window->WindowWasClosedLastFrame() || wasClosed;
        }
    }

    for (auto & k : toerase) {
        m_Windows.erase(k);
    }

    if (wasClosed) {
        m_IsOpen = false;
    }
}

void SMBCompPlayerWindowsComponent::DoMenuItem()
{
    bool v = m_IsOpen;
    if (ImGui::MenuItem("Player Windows", NULL, v)) {
        m_IsOpen = !m_IsOpen;
    }
}


//////////////////////////////////////////////////////////////////////////////

void argos::rgms::StepCombinedView(SMBComp* comp, SMBCompCombinedViewInfo* view)
{
    if (view->FollowSmart) {
        if (!comp->Locations.ScreenLocations.empty()) {
            std::sort(comp->Locations.ScreenLocations.begin(), comp->Locations.ScreenLocations.end(), [&](
                        const auto& l, const auto& r){
                if (l.SectionIndex == r.SectionIndex) {
                    return l.CategoryX < r.CategoryX;
                }
                return l.SectionIndex < r.SectionIndex;
            });

            bool useback = false;
            SMBCompPlayerLocations::PlayerScreenLocation backuploc;
            std::erase_if(comp->Locations.ScreenLocations, [&](
                        const auto& v){
                auto it = comp->Tower.Timings.find(v.PlayerID);

                SMBCompPlayerTimings* timings = nullptr;
                if (it != comp->Tower.Timings.end()) {
                    timings = &it->second;
                    if (timings->State == TimingState::WAITING_FOR_1_1) {
                        const SMBCompPlayer* tplayer = FindPlayer(comp->Config.Players, v.PlayerID);
                        if (tplayer) {
                            auto tout = GetLatestPlayerOutput(*comp, *tplayer);
                            if (tout) {
                                if (tout->Frame.AID == smb::AreaID::GROUND_AREA_6 &&
                                        tout->Frame.APX >= 4848) {
                                    useback = true;
                                    backuploc = v;
                                    return true;
                                }

                            } else {
                                return true;
                            }
                        }
                    }
                }
                return false;
            });
            if (comp->Locations.ScreenLocations.empty() && useback) {
                comp->Locations.ScreenLocations.push_back(backuploc);
            }

            if (!comp->Locations.ScreenLocations.empty()) {
                int currentSec = -1;
                for (auto & loc : comp->Locations.ScreenLocations) {
                    if (loc.PlayerID == view->PlayerID) {
                        currentSec = loc.SectionIndex;
                    }
                }

                auto& loc = comp->Locations.ScreenLocations.back();
                if (loc.PlayerID != view->PlayerID) {
                    view->SmartInfo.Cnt--;
                    if (loc.SectionIndex > currentSec) {
                        view->SmartInfo.Cnt -= 5;
                    }
                    if (view->SmartInfo.Cnt < 0) {
                        view->SmartInfo.Cnt = 0;
                    }
                    if (view->PlayerID == 0 || view->SmartInfo.Cnt == 0)
                    {
                        view->SmartInfo.Cnt = 40;
                        view->PlayerID = loc.PlayerID;
                    }
                } else {
                    view->SmartInfo.Cnt = 40;
                }
            }
        }
    }

    static std::unordered_map<uint8_t, int> AREA_POINTER_ENDS = {
        {0x01, 3072},
        {0x20, 2608},
        {0x21, 3664},
        {0x22, 3808},
        {0x23, 3664},
        {0x24, 3408},
        {0x25, 3376},
        {0x26, 2624},
        {0x27, 3792},
        {0x28, 3408},
        {0x2a, 3392},
        {0x2c, 2544},
        {0x2d, 2864},
        {0x2e, 3216},
        {0x2f, 1024},
        {0x30, 6224},
        {0x31, 3408},
        {0x32, 3664},
        {0x33, 3072},
        {0x35, 3552},
        {0x41, 3136},
        {0xc0, 3056},
        {0x02, 1136},
        {0x60, 2560},
        {0x61, 3072},
        {0x62, 2560},
        {0x63, 2560},
        {0x64, 3584},
    };

    const SMBCompPlayer* player = FindPlayer(comp->Config.Players, view->PlayerID);
    if (!player) {
        view->Type = ViewType::NO_PLAYER;
        return;
    }

    auto out = GetLatestPlayerOutput(*comp, *player);
    if (!out) {
        view->Type = ViewType::NO_OUTPUT;
        return;
    }

    if (!out->ConsolePoweredOn) {
        view->Type = ViewType::CONSOLE_OFF;
        return;
    }

    if (out->Frame.GameEngineSubroutine == 0x00) {
        //view->UseLastDiffs = true;
        //if (out->Frame.AP == 0x25 && out->Frame.APX < 16) {
        //    view->UseLastDiffs = false;
        //}
        view->Type = ViewType::TITLESCREEN_MODE;
        return;
    }
    if (comp->Config.Tournament.Category == "warpless" &&
        out->Frame.AID == smb::AreaID::GROUND_AREA_6 &&
        out->Frame.APX >= 2816 &&
        out->Frame.GameEngineSubroutine == 0x05 && out->Frame.OperMode == 0x01) {
        // TODO YEEESH
        //view->UseLastDiffs = true;
        view->Type = ViewType::TITLESCREEN_MODE;
        return;
    }
    if (out->Frame.GameEngineSubroutine == 0x06 && out->Frame.OperMode == 0x03) {
        //view->UseLastDiffs = true;
        view->Type = ViewType::GAMEOVER_MODE;
        return;
    }
    int WIDTH = 480;

    //view->UseLastDiffs = false;
    view->Type = ViewType::PLAYING;

    view->AID = out->Frame.AID;
    if (out->Frame.AID == smb::AreaID::UNDERGROUND_AREA_3) {
        view->Width = 272;
        view->APX = out->Frame.APX;
    } else if (out->Frame.AID == smb::AreaID::GROUND_AREA_10) {
        view->Width = 256;
        view->APX = out->Frame.APX; // 0
    } else {
        view->Width = WIDTH;

        view->APX = out->Frame.APX - (WIDTH-256)/2 - 64;
        view->APX = std::max(view->APX, 0);

        int end = 0;
        if (smb::AreaIDEnd(out->Frame.AID, &end)) {
            view->APX = std::min(view->APX, end - WIDTH);
        }
    }

    view->FramePalette = out->FramePalette;
}

SMBCompCombinedViewComponent::SMBCompCombinedViewComponent(argos::RuntimeConfig* info, SMBComp* comp)
    : ISMBCompSingleWindowComponent("Combined view", "combined view", true)
    , m_Info(info)
    , m_Competition(comp)
{
    m_Competition->CombinedView2.Active = false;
    m_Competition->CombinedView2.FollowSmart = false;
}

SMBCompCombinedViewComponent::~SMBCompCombinedViewComponent()
{
}

bool SMBCompCombinedViewComponent::MakeImageIndividual(SMBComp* comp, nes::PPUx* ppux, const SMBCompPlayer* player,
        bool applyVisuals, bool fullView,
        int* screenLeft, int* screenRight,
        int* doingOwnOAMx, SMBMessageProcessorOutputPtr* output,
        SMBCompCombinedViewInfo* view)
{
    if (output) *output = nullptr;
    nes::RenderInfo render = DefaultSMBCompRenderInfo(*comp);

    ppux->FillBackground(nes::PALETTE_ENTRY_BLACK, render.PaletteBGR);
    int WIDTH = ppux->GetWidth();

    auto CenterWhiteString = [&](const std::string& str){
        std::array<uint8_t, 4> tpal = {0x00, nes::PALETTE_ENTRY_WHITE, 0x20, 0x20};
        int scal = 2;
        ppux->ResetPriority();
        ppux->BeginOutline();
        ppux->RenderString(WIDTH / 2 - str.size() * 8, 112, str,
                comp->StaticData.Font.data(), tpal.data(), render.PaletteBGR, 2,
                nes::EffectInfo::Defaults());
        ppux->StrokeOutlineO(2.0f, nes::PALETTE_ENTRY_BLACK, render.PaletteBGR);
    };

    int left = std::max(ppux->GetWidth() - view->Width, 0) / 2;


    if (!player) {
        CenterWhiteString("no player");
        return false;
    }

    SMBCompFeed* feed = GetPlayerFeed(*player, &comp->Feeds);
    if (!feed || !feed->Source) {
        CenterWhiteString("no output");
        return false;
    }

    auto out = GetLatestPlayerOutput(*comp, *player);
    if (!out) {
        CenterWhiteString("no output");
        return false;
    }

    if (!out->ConsolePoweredOn) {
        CenterWhiteString("console off");
        return false;
    }

    if (output) *output = out;

    if (out->ConsolePoweredOn && out->Frame.GameEngineSubroutine != 0x00 &&
        out->Frame.AID == view->AID &&
        out->Frame.APX >= view->APX - view->Width &&
        out->Frame.APX <= (view->APX + view->Width)) {

        // TODO put left to zero and include previous diffs
        int lx = left + out->Frame.APX - view->APX;
        int rx = lx + 256;
        if (rx <= 0) return false;
        if (lx > WIDTH) return false;

        if (rx > WIDTH) {
            rx = WIDTH;
        }

        if (screenLeft) {
            *screenLeft = lx;
            if (*screenLeft < 0) {
                *screenLeft = 0;
            }
        }
        if (screenRight) *screenRight = rx;

        if (fullView) {
            comp->StaticData.Nametables->RenderTo(view->AID, view->APX, ppux->GetWidth(),
                    ppux,
                    left,
                    comp->Config.Visuals.Palette,
                    comp->StaticData.ROM.CHR1,
                    nullptr,
                    view->FramePalette.data(),
                    &out->Frame.NTDiffs);
        } else {
            comp->StaticData.Nametables->RenderTo(view->AID, out->Frame.APX, 256,
                    ppux,
                    lx,
                    comp->Config.Visuals.Palette,
                    comp->StaticData.ROM.CHR1,
                    nullptr,
                    view->FramePalette.data(),
                    &out->Frame.NTDiffs);
        }

        if (doingOwnOAMx) {
            *doingOwnOAMx = out->Frame.APX - view->APX + left;
        } else {
            if (applyVisuals) {
//                ppux->BeginOutline();
            }

            nes::EffectInfo effects = nes::EffectInfo::Defaults();
            for (auto oamx : out->Frame.OAMX) {
                oamx.X += out->Frame.APX - view->APX + left;
                ppux->RenderOAMxEntry(oamx, render, effects);
            }

            if (applyVisuals) {
//                ppux->StrokeOutlineO(1.0f, player->Colors.RepresentativeColor, render.PaletteBGR);
            }
        }

        return true;
    } else if (fullView) {
        comp->StaticData.Nametables->RenderTo(view->AID, view->APX, ppux->GetWidth(),
                ppux,
                left,
                comp->Config.Visuals.Palette,
                comp->StaticData.ROM.CHR1,
                nullptr,
                view->FramePalette.data(),
                nullptr);
        return true;
    }
    return false;
}


void ApplyPlayerColors(nes::OAMxEntry* oamx, const SMBCompPlayer* player)
{
    if (oamx->TilePalette[1] == 0x16 &&
        oamx->TilePalette[2] == 0x27 &&
        oamx->TilePalette[3] == 0x18) {

        oamx->TilePalette[1] = player->Colors.MarioColors[0];
        oamx->TilePalette[2] = player->Colors.MarioColors[1];
        oamx->TilePalette[3] = player->Colors.MarioColors[2];

    } else if (oamx->TilePalette[1] == 0x37 &&
               oamx->TilePalette[2] == 0x27 &&
               oamx->TilePalette[3] == 0x16) {

        oamx->TilePalette[1] = player->Colors.FireMarioColors[0];
        oamx->TilePalette[2] = player->Colors.FireMarioColors[1];
        oamx->TilePalette[3] = player->Colors.FireMarioColors[2];
    }
}

void SMBCompCombinedViewComponent::MakeImage(SMBComp* comp, nes::PPUx* ppux, SMBCompCombinedViewInfo* view)
{
    nes::RenderInfo render = DefaultSMBCompRenderInfo(*comp);

    int w = ppux->GetWidth();
    int h = ppux->GetHeight();

    const SMBCompPlayer* mainPlayer = FindPlayer(comp->Config.Players, view->PlayerID);
    int mainOAMXOffset = 0;
    SMBMessageProcessorOutputPtr mainOutput;

    struct PlayerNameInfo
    {
        int x;
        int y;
        std::string name;
    };
    std::vector<PlayerNameInfo> playerNames;

    if (MakeImageIndividual(comp, ppux, mainPlayer, false, true, nullptr, nullptr, &mainOAMXOffset, &mainOutput, view)) {
        cv::Mat denom = cv::Mat::zeros(h, w, CV_32FC3);
        denom += cv::Scalar(1.0f, 1.0f, 1.0f);
        cv::Mat numer;
        cv::Mat m(h, w, CV_8UC3, ppux->GetBGROut());
        m.convertTo(numer, CV_32FC3, 1/255.0f);




        struct OtherPlayerInfo {
            int Offset;
            SMBMessageProcessorOutputPtr Out;
            cv::Mat Oam;
            cv::Mat Mask;
        };
        std::vector<uint32_t> otherPlayerIds;
        std::unordered_map<uint32_t, OtherPlayerInfo> otherPlayers;
        for (auto & player : comp->Config.Players.Players) {
            if (player.UniquePlayerID != mainPlayer->UniquePlayerID) {
                nes::PPUx ppux2(w, h, nes::PPUxPriorityStatus::ENABLED);
                int tl, tr;
                OtherPlayerInfo info;

                if (MakeImageIndividual(comp, &ppux2, &player, true, false, &tl, &tr, &info.Offset, &info.Out, view)) {
                    cv::Rect r(tl, 0, tr - tl, 240);
                    if (r.width > 0 && tl < w) {
                        denom(r) += cv::Scalar(1.0f, 1.0f, 1.0f);
                        cv::Mat m2(h, w, CV_8UC3, ppux2.GetBGROut());
                        cv::Mat n2;
                        m2.convertTo(n2, CV_32FC3, 1/255.0f);

                        numer(r) += n2(r);


                        uint8_t* bgr = ppux2.GetBGROut();
                        for (int i = 0; i < w * h * 3; i++) {
                            bgr[i] = 0x69;
                        }

                        ppux2.BeginOutline();
                        nes::EffectInfo effects = nes::EffectInfo::Defaults();
                        bool mariofound = false;
                        int mariox = -1;
                        int marioy = -1;
                        for (auto oamx : info.Out->Frame.OAMX) {
                            oamx.X += info.Offset;
                            if (smb::IsMarioTile(oamx.TileIndex)) {
                                if (comp->Config.Visuals.UsePlayerColors) {
                                    ApplyPlayerColors(&oamx, &player);
                                }
                                if (!mariofound) {
                                    mariofound = true;
                                    mariox = oamx.X;
                                    marioy = oamx.Y;
                                }
                                if (oamx.X < mariox) {
                                    mariox = oamx.X;
                                }
                                if (oamx.Y < marioy) {
                                    marioy = oamx.Y;
                                }
                            }
                            ppux2.RenderOAMxEntry(oamx, render, effects);
                        }
                        ppux2.StrokeOutlineO(1.0f, player.Colors.RepresentativeColor, render.PaletteBGR);
                        if (mariofound && info.Out->Frame.GameEngineSubroutine != 0x00) {
                            PlayerNameInfo nameinfo;
                            nameinfo.x = mariox;
                            nameinfo.y = marioy;
                            nameinfo.name = player.Names.ShortName;
                            playerNames.push_back(nameinfo);
                        }

                        m2.copyTo(info.Oam);
                        info.Mask = cv::Mat::zeros(h, w, CV_8UC3);
                        uint8_t* msk = info.Mask.data;

                        for (int i = 0; i < w * h * 3; i+=3) {
                            if (bgr[i + 0] != 0x69 ||
                                bgr[i + 1] != 0x69 ||
                                bgr[i + 2] != 0x69) {
                                msk[i + 0] = 0xff;
                                msk[i + 1] = 0xff;
                                msk[i + 2] = 0xff;
                            }
                        }

                        otherPlayers[player.UniquePlayerID] = info;
                        otherPlayerIds.push_back(player.UniquePlayerID);
                    }
                }
            }
        }

        cv::Mat img = numer / denom; // So BEAUTIFUL look at the coins, my god.
        img.convertTo(m, CV_8UC3, 255.0f);

        cv::Mat bg = m.clone();

        std::sort(otherPlayerIds.begin(), otherPlayerIds.end());
        std::mt19937 gen(static_cast<uint16_t>(mainOutput->Frame.AID)); // seed on ap so it changes between section kinda..
        std::shuffle(otherPlayerIds.begin(), otherPlayerIds.end(), gen);

        cv::Mat otherOam = m.clone();
        for (auto & id : otherPlayerIds) {
            auto& info = otherPlayers.at(id);

            uint8_t* d = info.Oam.data;
            uint8_t* msk = info.Mask.data;
            uint8_t* o = otherOam.data;
            for (int i = 0; i < w * h * 3; i++) {
                if (msk[i] == 0xff) {
                    o[i] = d[i];
                }
            }
        }

        float alpha = comp->Config.Visuals.OtherAlpha;
        cv::addWeighted(m, 1.0f - alpha, otherOam, alpha, 0.0, m);


        if (mainOutput->Frame.GameEngineSubroutine != 0x00) {
            ppux->BeginOutline();
            nes::EffectInfo effects = nes::EffectInfo::Defaults();
            bool mariofound = false;
            int mariox = -1;
            int marioy = -1;

            for (auto oamx : mainOutput->Frame.OAMX) {
                oamx.X += mainOAMXOffset;
                if (smb::IsMarioTile(oamx.TileIndex)) {
                    if (comp->Config.Visuals.UsePlayerColors) {
                        ApplyPlayerColors(&oamx, mainPlayer);
                    }
                    if (!mariofound) {
                        mariofound = true;
                        mariox = oamx.X;
                        marioy = oamx.Y;
                    }
                    if (oamx.X < mariox) {
                        mariox = oamx.X;
                    }
                    if (oamx.Y < marioy) {
                        marioy = oamx.Y;
                    }
                }
                ppux->RenderOAMxEntry(oamx, render, effects);
            }
            if (mariofound && mainOutput->Frame.GameEngineSubroutine != 0x00) {
                PlayerNameInfo nameinfo;
                nameinfo.x = mariox;
                nameinfo.y = marioy;
                nameinfo.name = mainPlayer->Names.ShortName;
                playerNames.push_back(nameinfo);
            }
            ppux->StrokeOutlineO(1.0f, mainPlayer->Colors.RepresentativeColor, render.PaletteBGR);
        }

        // todo playernames render them all? Maybe too busy. :(

        //ppux->SetPriorityStatus(nes::PPUxPriorityStatus::DISABLED);
        std::array<uint8_t, 4> tpal = {0x00, nes::PALETTE_ENTRY_WHITE, 0x20, 0x20};
        for (auto & name : playerNames) {
            int offset = 0;
            if (name.name.size() > 2) {
                offset += ((name.name.size() - 2) * 8) / 2;
            } else if (name.name.size() == 1) {
                offset = -4;
            }
            nes::EffectInfo effects = nes::EffectInfo::Defaults();
            effects.Opacity = 1.0f;
            ppux->RenderString(name.x - offset, name.y - 10, name.name,
                    comp->StaticData.Font.data(), tpal.data(), render.PaletteBGR, 1,
                    effects);
        }
    }
}


void SMBCompCombinedViewComponent::DoControls()
{
    auto* view1 = &m_Competition->CombinedView;
    ImGui::PushID(1);
    DoViewControls(view1);
    ImGui::PopID();
    if (ImGui::CollapsingHeader("picture in picture")) {
        ImGui::PushID(2);
        auto* view2 = &m_Competition->CombinedView2;
        DoViewControls(view2);
        ImGui::PopID();
    }
}

void SMBCompCombinedViewComponent::DoViewControls(SMBCompCombinedViewInfo* view)
{
    ImGui::Checkbox("active", &view->Active);
    ImGui::Checkbox("follow smart", &view->FollowSmart);
    ImGui::Checkbox("names visible", &view->NamesVisible);
    const SMBCompPlayer* followPlayer = FindPlayer(m_Competition->Config.Players, view->PlayerID);
    std::string v = (followPlayer) ? followPlayer->Names.ShortName : "";
    const std::vector<SMBCompPlayer>& players = m_Competition->Config.Players.Players;
    for (auto & player : players) {
        if (ImGui::Selectable(player.Names.ShortName.c_str(), player.UniquePlayerID == view->PlayerID)) {
            view->PlayerID = player.UniquePlayerID;
            view->FollowSmart = false;
        }
    }

    int WIDTH = 480;
    if (view->Img.cols != WIDTH || view->Img.rows != 240) {
        view->Img = cv::Mat::zeros(240, WIDTH, CV_8UC3);
    }

    if (view->Active) {
        nes::PPUx ppux(WIDTH, 240, view->Img.data, nes::PPUxPriorityStatus::ENABLED);
        MakeImage(m_Competition, &ppux, view);

        cv::Mat m(ppux.GetHeight(), ppux.GetWidth(), CV_8UC3, ppux.GetBGROut());
        rgmui::Mat("m3", m);
    }
}

////////////////////////////////////////////////////////////////////////////////

SMBCompIndividualViewsComponent::SMBCompIndividualViewsComponent(argos::RuntimeConfig* info, SMBComp* comp)
    : ISMBCompSingleWindowComponent("Individual views", "individual views", false)
    , m_Info(info)
    , m_Competition(comp)
    , m_Columns(4)
{
}

SMBCompIndividualViewsComponent::~SMBCompIndividualViewsComponent()
{
}

void SMBCompIndividualViewsComponent::DoControls()
{
    int WIDTH = 480;
    rgmui::SliderIntExt("col", &m_Columns, 1, 6);

    if (ImGui::BeginTable("views", m_Columns)) {
        const std::vector<SMBCompPlayer>& players = m_Competition->Config.Players.Players;
        for (auto & player : players) {
            nes::PPUx ppux(WIDTH, 240, nes::PPUxPriorityStatus::ENABLED);

            SMBCompCombinedViewComponent::MakeImageIndividual(m_Competition, &ppux, &player, true,
                    false, nullptr, nullptr, nullptr, nullptr, &m_Competition->CombinedView);

            ImGui::TableNextColumn();
            cv::Mat m(ppux.GetHeight(), ppux.GetWidth(), CV_8UC3, ppux.GetBGROut());
            rgmui::Mat(fmt::format("indview{}", player.UniquePlayerID).c_str(), m);
        }
        ImGui::EndTable();
    }
}

////////////////////////////////////////////////////////////////////////////////

void argos::rgms::InitializeLerper(Lerper* lerper)
{
    lerper->Target = 0.0f;
    lerper->Position = 0.0f;

    lerper->LastVelocity[0] = 0.0f;
    lerper->LastVelocity[1] = 0.0f;
    lerper->LastVelocity[2] = 0.0f;
    lerper->LastVelocity[3] = 0.0f;

    lerper->Acceleration = 5.0f;
    lerper->DampenAmount = 0.2f;
    lerper->MaxVelocity = 10.0f;
}

void argos::rgms::StepLerper(Lerper* lerper)
{
    util::mclock::time_point now = util::Now();

    float v = (lerper->Target - lerper->Position) * lerper->DampenAmount;
    if (v == 0.0f) {
        return;
    }

    float vo = v;
    float lv = lerper->LastVelocity[0];
    if ((v * lv) < 0 || (std::abs(v) > std::abs(lv))) {
        if ((v - lv) > lerper->Acceleration) {
            v = lv + lerper->Acceleration;
        } else if ((lv - v) > lerper->Acceleration) {
            v = lv - lerper->Acceleration;
        }
    }

    if (std::abs(v) > lerper->MaxVelocity) {
        v = (v > 0) ? lerper->MaxVelocity : -lerper->MaxVelocity;
    }

    lerper->LastVelocity[0] = lerper->LastVelocity[1];
    lerper->LastVelocity[1] = lerper->LastVelocity[2];
    lerper->LastVelocity[2] = lerper->LastVelocity[3];
    lerper->LastVelocity[3] = v;

    float v2 = 0.0f;
    for (int i = 0; i < 4; i++) {
        v2 += lerper->LastVelocity[i];
    }

    float truev = v2 / 4.0f;


    lerper->LastVelocity[0] = truev;

    lerper->Position += truev;
}

////////////////////////////////////////////////////////////////////////////////

void argos::rgms::InitializeSMBCompMinimap(SMBCompMinimap* minimap)
{
    minimap->LeftX = 0;
    minimap->Width = 1920;
    minimap->FollowMethod = MinimapFollowMethod::FOLLOW_FARTHEST;
    minimap->PlayerID = 0;

    minimap->TargetMarioX = -1;

    InitializeLerper(&minimap->CameraLerp);
}

////////////////////////////////////////////////////////////////////////////////

SMBCompMinimapViewComponent::SMBCompMinimapViewComponent(argos::RuntimeConfig* info, SMBComp* comp)
    : ISMBCompSingleWindowComponent("Minimap", "minimap", true)
    , m_Info(info)
    , m_Competition(comp)
{
}

SMBCompMinimapViewComponent::~SMBCompMinimapViewComponent()
{
}

void SMBCompMinimapViewComponent::DoMinimapEditControls()
{
    SMBCompMinimap* minimap = &m_Competition->Minimap;
    const auto& route = m_Competition->StaticData.Categories.Routes.at(
        m_Competition->Config.Tournament.Category);

    if (rgmui::SliderIntExt("x", &minimap->LeftX, 0, route->TotalWidth() - minimap->Width)) {
        minimap->FollowMethod = MinimapFollowMethod::FOLLOW_NONE;
    }

    const std::vector<SMBCompPlayer>& players = m_Competition->Config.Players.Players;

    std::string v;
    if (minimap->FollowMethod == MinimapFollowMethod::FOLLOW_NONE) {
        v = "no follow";
    } else if (minimap->FollowMethod == MinimapFollowMethod::FOLLOW_PLAYER) {
        const SMBCompPlayer* followPlayer = FindPlayer(m_Competition->Config.Players, minimap->PlayerID);
        v = (followPlayer) ? followPlayer->Names.ShortName : "";
    } else {
        v = "follow farthest";
    }

    if (ImGui::BeginCombo("follow", v.c_str())) {
        if (ImGui::Selectable("no follow", v == "no follow")) {
            minimap->FollowMethod = MinimapFollowMethod::FOLLOW_NONE;
        }
        if (ImGui::Selectable("follow farthest", v == "follow farthest")) {
            minimap->FollowMethod = MinimapFollowMethod::FOLLOW_FARTHEST;
        }
        ImGui::Separator();
        for (auto & player : players) {
            if (ImGui::Selectable(player.Names.ShortName.c_str(), player.UniquePlayerID == minimap->PlayerID)) {
                minimap->FollowMethod = MinimapFollowMethod::FOLLOW_PLAYER;
                minimap->PlayerID = player.UniquePlayerID;
            }
        }

        ImGui::EndCombo();
    }

    rgmui::TextFmt("target mario x: {}", minimap->TargetMarioX);
}

static void RenderLeftTriangle(nes::PPUx* ppux, uint8_t color, const uint8_t* bgr, int x, int y)
{
    nes::EffectInfo teffects = nes::EffectInfo::Defaults();
    uint8_t t = color;
    ppux->RenderHardcodedSprite(x, y,
            {{0, 0, 0, 0, 0, 0, 0, t},
             {0, 0, 0, 0, 0, 0, t, t},
             {0, 0, 0, 0, 0, t, t, t},
             {0, 0, 0, 0, t, t, t, t},
             {0, 0, 0, t, t, t, t, t},
             {0, 0, t, t, t, t, t, t},
             {0, t, t, t, t, t, t, t},
             {t, t, t, t, t, t, t, t},
             {0, t, t, t, t, t, t, t},
             {0, 0, t, t, t, t, t, t},
             {0, 0, 0, t, t, t, t, t},
             {0, 0, 0, 0, t, t, t, t},
             {0, 0, 0, 0, 0, t, t, t},
             {0, 0, 0, 0, 0, 0, t, t},
             {0, 0, 0, 0, 0, 0, 0, t}},
             bgr, teffects);
}
static void RenderRightTriangle(nes::PPUx* ppux, uint8_t color, const uint8_t* bgr, int x, int y)
{
    nes::EffectInfo teffects = nes::EffectInfo::Defaults();
    uint8_t t = color;
    ppux->RenderHardcodedSprite(x, y,
            {{t, 0, 0, 0, 0, 0, 0, 0},
             {t, t, 0, 0, 0, 0, 0, 0},
             {t, t, t, 0, 0, 0, 0, 0},
             {t, t, t, t, 0, 0, 0, 0},
             {t, t, t, t, t, 0, 0, 0},
             {t, t, t, t, t, t, 0, 0},
             {t, t, t, t, t, t, t, 0},
             {t, t, t, t, t, t, t, t},
             {t, t, t, t, t, t, t, 0},
             {t, t, t, t, t, t, 0, 0},
             {t, t, t, t, t, 0, 0, 0},
             {t, t, t, t, 0, 0, 0, 0},
             {t, t, t, 0, 0, 0, 0, 0},
             {t, t, 0, 0, 0, 0, 0, 0},
             {t, 0, 0, 0, 0, 0, 0, 0}},
             bgr, teffects);
}

void SMBCompMinimapViewComponent::DoControls()
{
    // TODO this unfortunately does the 'step minimap' at the moment

    DoMinimapEditControls();
    SMBCompMinimap* minimap = &m_Competition->Minimap;

    if (minimap->Img.cols != minimap->Width || minimap->Img.rows != 240) {
        minimap->Img = cv::Mat::zeros(240, minimap->Width, CV_8UC3);
    }

    nes::PPUx ppux(minimap->Width, 240, minimap->Img.data, nes::PPUxPriorityStatus::ENABLED);
    //ppux.SetSpritePriorityGlitch(false);
    ppux.FillBackground(nes::PALETTE_ENTRY_WHITE, m_Competition->Config.Visuals.Palette.data());

    const auto& route = m_Competition->StaticData.Categories.Routes.at(
        m_Competition->Config.Tournament.Category);
    std::vector<smb::WorldSection> visibleSections;
    route->RenderMinimapTo(&ppux, minimap->LeftX,
            smb::DefaultMinimapPalette(),
            m_Competition->StaticData.Nametables.get(), &visibleSections);

    nes::RenderInfo render = DefaultSMBCompRenderInfo(*m_Competition);

    // Add World Level Text
    {
        std::array<uint8_t, 4> tpal = {0x00, nes::PALETTE_ENTRY_WHITE, 0x20, 0x20};
        std::string lastLevel = "";
        for (size_t i = 0; i < visibleSections.size(); i++) {
            auto& vsec = visibleSections[i];

            std::string thisLevel = fmt::format("{}-{}",
                    vsec.World, vsec.Level);
            if (thisLevel == lastLevel) continue;


            // Adjust text position to look nice
            int tx = vsec.XLoc;
            if (i == 0 && tx < 16) {
                int lastLevel = 0;
                for (auto & sec : route->Sections()) {
                    if (sec == vsec) break;
                    lastLevel = sec.Level;
                }
                if (lastLevel == vsec.Level) {
                    tx = 4;
                }
            }

            if (tx < 4) {
                tx = 4;
            }
            int tw = 3 * 16;
            if (i != visibleSections.size() - 1) {
                if (visibleSections[i + 1].Level != vsec.Level) {
                    int endx = visibleSections[i + 1].XLoc - 16;
                    if ((tx + tw) > endx) {
                        tx = endx - tw;
                    }
                }
            }

            ppux.BeginOutline();
            ppux.RenderString(tx, 4, thisLevel,
                    m_Competition->StaticData.Font.data(), tpal.data(), render.PaletteBGR, 2,
                    nes::EffectInfo::Defaults());
            ppux.StrokeOutlineO(2.0f, nes::PALETTE_ENTRY_BLACK, render.PaletteBGR);

            lastLevel = thisLevel;
        }
    }


    // Now the players
    const std::vector<SMBCompPlayer>& players = m_Competition->Config.Players.Players;

    std::vector<int> targetMarioXs;

    auto* screenLocations = &m_Competition->Locations.ScreenLocations;
    screenLocations->clear();

    for (auto & player : players) {
        std::array<uint8_t, 4> tpal = {0x00, player.Colors.RepresentativeColor,
            player.Colors.RepresentativeColor,
            player.Colors.RepresentativeColor};

        screenLocations->emplace_back();
        auto& loc = screenLocations->back();
        loc.PlayerID = player.UniquePlayerID;
        loc.OnScreen = false;
        loc.SectionIndex = 0;
        loc.CategoryX = 0;

        if (auto out = GetLatestPlayerOutput(*m_Competition, player)) {
            if (!out) continue;
            if (out->Frame.GameEngineSubroutine == 0x00) {
                continue;
            }

            int categoryX = 0;
            int sectionIndex = 0;
            if (!route->InCategory(out->Frame.AID, out->Frame.APX, out->Frame.World, out->Frame.Level, &categoryX, &sectionIndex)) continue;
            if (m_Competition->Config.Tournament.Category == "warpless" &&
                    sectionIndex == 5 && out->Frame.GameEngineSubroutine == 0x05 &&
                    out->Frame.OperMode == 0x01) {
                continue;
            }
            loc.OnScreen = true;
            loc.SectionIndex = sectionIndex;
            loc.CategoryX = categoryX;

            int mariox = 0, marioy = 0;
            if (!MarioInOutput(out, &mariox, &marioy)) {
                targetMarioXs.push_back(categoryX + 112);
                if (player.UniquePlayerID == minimap->PlayerID) {
                    minimap->TargetMarioX = categoryX + 112;
                }
                continue;
            }

            int ppuX = categoryX - minimap->LeftX + mariox;

            const auto& sec = route->at(sectionIndex);
            nes::EffectInfo effects = nes::EffectInfo::Defaults();
            for (auto & vsec : visibleSections) {
                //if (vsec == sec) {
                //    effects.CropWithin = true;
                //    effects.Crop.X = vsec.XLoc;
                //    effects.Crop.Y = 0;
                //    effects.Crop.Width = vsec.Right - vsec.Left;
                //    effects.Crop.Height = 240;
                //    break;
                //}
            }


            ppux.BeginOutline();
            // Render mario
            for (auto oamx : out->Frame.OAMX) {
                if (smb::IsMarioTile(oamx.TileIndex)) {
                    oamx.X += (ppuX - mariox);

                    oamx.TilePalette[0] = 0x00;
                    oamx.TilePalette[1] = player.Colors.RepresentativeColor;
                    oamx.TilePalette[2] = player.Colors.RepresentativeColor;
                    oamx.TilePalette[3] = player.Colors.RepresentativeColor;

                    ppux.RenderOAMxEntry(oamx, render, effects);
                }
            }

            // Render triangles
            if (ppuX < 0) {
                int trix = -8 - ppuX;
                if (trix > 4) {
                    trix = 4;
                }
                RenderLeftTriangle(&ppux, player.Colors.RepresentativeColor, render.PaletteBGR, trix, marioy);
            }
            if (ppuX > (minimap->Width - 16)) {
                int trix = minimap->Width - (ppuX - (minimap->Width - 16));
                if (trix < (minimap->Width - 12)) {
                    trix = minimap->Width - 12;
                }
                RenderRightTriangle(&ppux, player.Colors.RepresentativeColor, render.PaletteBGR, trix, marioy);
            }

            { // Render player name
                int len = player.Names.ShortName.size();
                int textx = ppuX + (len - 1) * -8;
                if (textx < 4) {
                    textx = 4;
                }
                int textw = len * 16;
                if ((textx + textw) > (minimap->Width - 4)) {
                    textx = minimap->Width - 4 - textw;
                }
                effects.CropWithin = false;
                ppux.RenderString(textx, marioy - 20, player.Names.ShortName,
                    m_Competition->StaticData.Font.data(), tpal.data(),
                    render.PaletteBGR, 2, effects);
            }

            uint8_t outline = nes::PALETTE_ENTRY_BLACK;
            if (player.UniquePlayerID == m_Competition->CombinedView.PlayerID) {
                outline = 0x2c;
            }

            //ppux.StrokeOutlineO(1.0f, outline, render.PaletteBGR);

            // update target mariox

            targetMarioXs.push_back(categoryX + mariox);
            if (minimap->FollowMethod == MinimapFollowMethod::FOLLOW_PLAYER && player.UniquePlayerID == minimap->PlayerID) {
                minimap->PlayerID = player.UniquePlayerID;
                minimap->TargetMarioX = categoryX + mariox;
            }
        }
    }

    cv::Mat m(ppux.GetHeight(), ppux.GetWidth(), CV_8UC3, ppux.GetBGROut());
    rgmui::Mat("minimap", m);

    if (minimap->FollowMethod != MinimapFollowMethod::FOLLOW_NONE) {
        if (minimap->FollowMethod == MinimapFollowMethod::FOLLOW_FARTHEST) {
            if (!targetMarioXs.empty()) {
                minimap->TargetMarioX = *std::max_element(targetMarioXs.begin(), targetMarioXs.end());
            }
        }
        auto* lerper = &minimap->CameraLerp;
        int marioOnScreenPos = minimap->TargetMarioX - minimap->LeftX;
        if (marioOnScreenPos < 128) {
            lerper->Target = minimap->TargetMarioX - 128;
        }
        if (marioOnScreenPos > minimap->Width - 384) {
            lerper->Target = minimap->TargetMarioX - (minimap->Width - 384);
        }

        // TODO Adjust target to smooth out level transitions?

        if (lerper->Target < 0) {
            lerper->Target = 0;
        }
        if (lerper->Target > (route->TotalWidth() - minimap->Width)) {
            lerper->Target = route->TotalWidth() - minimap->Width;
        }

        StepLerper(lerper);

        if (std::abs(lerper->Target - lerper->Position) > 1000) {
            lerper->Position = lerper->Target;
        }

        minimap->LeftX = std::round(lerper->Position);

        //if (ImGui::Button("init lerper")) {
        //    InitializeLerper(lerper);
        //}
        //rgmui::SliderFloatExt("position", &lerper->Position, -3.0f, 3.0f);
        //rgmui::SliderFloatExt("target", &lerper->Target, -3.0f, 3.0f);

        //rgmui::SliderFloatExt("last velocity0", &lerper->LastVelocity[0], -10.0f, 10.0f);
        //rgmui::SliderFloatExt("last velocity1", &lerper->LastVelocity[1], -10.0f, 10.0f);
        //rgmui::SliderFloatExt("last velocity2", &lerper->LastVelocity[2], -10.0f, 10.0f);
        //rgmui::SliderFloatExt("last velocity3", &lerper->LastVelocity[3], -10.0f, 10.0f);

        //rgmui::SliderFloatExt("acceleration", &lerper->Acceleration, 0.0f, 20.0f);
        //rgmui::SliderFloatExt("dampen", &lerper->DampenAmount, 0.0f, 0.5f);
        //rgmui::SliderFloatExt("max velocity", &lerper->MaxVelocity, 0.0f, 20.0f);
    }
}

////////////////////////////////////////////////////////////////////////////////

void argos::rgms::InitializeSMBCompPlayerTimings(SMBCompPlayerTimings* timings)
{
    timings->State = TimingState::WAITING_FOR_1_1;
    timings->SplitM2s.clear();
    timings->SplitPageM2s.clear();
}

void argos::rgms::ResetSMBCompTimingTower(rgms::SMBCompTimingTower* tower)
{
    tower->Timings.clear();
}
void argos::rgms::InitializeSMBCompTimingTower(rgms::SMBCompTimingTower* tower)
{
    ResetSMBCompTimingTower(tower);
    tower->FromLeader = true;
}

void argos::rgms::TimingsSectionPageIndex(const SMBCompPlayerTimings& timings, int* section, int* page)
{
    assert(timings.SplitM2s.size() == timings.SplitPageM2s.size());
    *section = timings.SplitM2s.size() - 1;
    *page = timings.SplitPageM2s.back().size() - 1;
}

void argos::rgms::SplitTimings(SMBCompPlayerTimings* timings, int section, int page, uint64_t m2)
{
    if (!timings || (section == 0 && page == 0)) {
        return;
    }

    timings->SplitM2s.resize(section + 1, m2);
    timings->SplitPageM2s.resize(section + 1);
    timings->SplitPageM2s.back().resize(page + 1, m2);
    timings->SplitPageM2s.back().back() = m2;
}


void argos::rgms::StepSMBCompPlayerTimings(SMBCompPlayerTimings* timings, SMBMessageProcessorOutputPtr out,
        const smb::Route* route)
{
    switch (timings->State) {
        case (TimingState::WAITING_FOR_1_1): {
            if (out->Frame.AID == smb::AreaID::GROUND_AREA_6 && out->Frame.APX < 15 &&
                (out->Frame.Time <= 400 && out->Frame.Time >= 399)) {

                timings->State = TimingState::RUNNING;
                timings->SplitM2s = {out->M2Count};
                timings->SplitPageM2s = {{out->M2Count}};
            }
            break;
        }
        case (TimingState::RUNNING): {
            if (!out->ConsolePoweredOn) {
                InitializeSMBCompPlayerTimings(timings);
                return;
            }
            int currentSection = 0;

            if (out->Frame.GameEngineSubroutine != 0x00 &&
                route->InCategory(out->Frame.AID, out->Frame.APX, out->Frame.World, out->Frame.Level, nullptr, &currentSection)) {

                int fromCatStart = out->Frame.APX - route->Sections()[currentSection].Left;
                int currentPage = fromCatStart / 256;

                int lastSection, lastPage;
                TimingsSectionPageIndex(*timings, &lastSection, &lastPage);

                if (currentSection != lastSection || currentPage != lastPage) {
                    if (lastSection == 2 && lastPage == 2 && currentSection == 5 && currentPage == 1) {
                        // skip this TODO
                    } else {
                        SplitTimings(timings, currentSection, currentPage, out->M2Count);
                    }
                }
            }

            if (out->Frame.World == 8 && out->Frame.Level == 4 &&
                    out->Frame.APX > 4096 && out->Frame.OperMode == 0x02) {

                timings->SplitM2s.resize(route->size() + 1, 0x00);
                timings->SplitPageM2s.resize(route->size() + 1);

                timings->SplitM2s.back() = out->M2Count;
                timings->SplitPageM2s.back().push_back(out->M2Count);
                timings->State = TimingState::WAITING_FOR_1_1;
            }
            break;
        }
        default: break;
    }
}

static void ComputeNewPositionIndices(const std::vector<int>& originalIndices, const std::vector<int>& newPositions,
        std::vector<int>* newIndices)
{
    int n = static_cast<int>(originalIndices.size());

    bool tryAll = false;
    {
        std::vector<int> cnt(n, 0);
        for (int i = 0; i < n; i++) {
            cnt[newPositions[i]]++;
            if (cnt[newPositions[i]] > 1) {
                tryAll = true;
                break;
            }
        }
    }

    *newIndices = newPositions;
    if (!tryAll) {
        return;
    }



    // We want to 'move' each original index as little as possible
    // But because it's normally so small I think I'll just do the dumb
    // factorial / n^2 thing unless I need to come back to this.

    // 8! is really tiny for a computer....
    if (n > 9) {
        throw std::runtime_error("No. You are not allowed to use a dumb n! algorithm with n greater than 9. I won't allow it");
    }

    std::vector<int> sortedPositions = newPositions;
    std::sort(sortedPositions.begin(), sortedPositions.end());

    std::vector<std::vector<int>> ok(n);
    for (int i = 0; i < n; i++) {
        ok[i].resize(n, 0);
    }

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (newPositions[j] == sortedPositions[i]) {
                ok[j][i] = 1;
            }
        }
    }

    std::vector<int> perm(n);
    std::iota(perm.begin(), perm.end(), 0);

    int bestov = -1;
    int x = 0;
    int z = 0;
    do {
        int ov = 0;
        bool tok = true;
        for (int j = 0; j < n; j++) {
            ov += (perm[j] - originalIndices[j]) * (perm[j] - originalIndices[j]);
            if (ok[j][perm[j]] == 0) {
                tok = false;
                break;
            }
        }
        if (tok) {
            if (bestov == -1 || ov < bestov) {
                bestov = ov;
                *newIndices = perm;
            }
            x++;
        }
        z++;
    } while (std::next_permutation(perm.begin(), perm.end()));

//    std::vector<std::vector<int>> groups(n);
//    for (int i = 0; i < n; i++) {
//        groups[newPositions[i]].push_back(i);
//    }
//
//   newIndices->resize(n);
//
//   for (int k = 0; k < n; k++)
//
//   int i = 0;
//   for (auto q : groups) {
//       for (auto v : q) {
//           (*newIndices)[v] = i;
//           i++;
//       }
//   }
//   int ov = 0;
//
//   for (int j = 0; j < n; j++) {
//       ov += ((*newIndices)[j] - originalIndices[j]) * ((*newIndices)[j] - originalIndices[j]);
//       std::cout << (*newIndices)[j] << std::endl;
//   }
//   std::cout << "obj: " << ov << std::endl;


}

static void ReconcileTargetAndDrawState(const TimingTowerState& target, TimingTowerState* draw,
        TimingTowerStateReconciliation* recon)
{
    draw->Title = target.Title;
    draw->Subtitle = target.Subtitle;

    auto SetDirect = [&]() {
        draw->Entries = target.Entries;
        recon->Entries.resize(target.Entries.size());
        recon->MovingTimer = 0;
        int entIndex = 0;
        for (auto & rentry : recon->Entries) {
            rentry.PositionIndex = target.Entries[entIndex].Y / TIMING_TOWER_Y_SPACING;
            entIndex++;
        }
    };

    if (draw->Entries.size() != target.Entries.size() ||
            recon->Entries.size() != target.Entries.size()) {
        SetDirect();
        return;
    }

    auto& targetEntries = target.Entries;
    auto* drawEntries = &(draw->Entries);

    bool positionsChanged = false;
    for (size_t entryIndex = 0; entryIndex < targetEntries.size(); entryIndex++) {
        auto tEntry = targetEntries[entryIndex];
        auto* dEntry = &((*drawEntries)[entryIndex]);

        if (tEntry.Color != dEntry->Color || tEntry.Name != dEntry->Name) {
            SetDirect();
            return;
        }

        int startPosition = dEntry->Position;
        dEntry->Position = tEntry.Position;
        dEntry->IntervalMS = std::max(tEntry.IntervalMS, 1l);
        dEntry->IsFinalTime = tEntry.IsFinalTime;
        dEntry->InSection = tEntry.InSection;
        dEntry->IsHighlight = tEntry.IsHighlight;

        if (dEntry->Position != startPosition) {
            positionsChanged = true;
            //std::cout << "position change: entryIndex " << entryIndex << " " << startPosition << " -> " << dEntry->Position << std::endl;

        }
    }

    const int TIMER_TIME = 16;

    if (positionsChanged) {
        recon->MovingTimer = TIMER_TIME;

        std::vector<int> originalIndices;
        std::vector<int> newPositions;

        {
            size_t i = 0;
            for (auto & rentry : recon->Entries) {
                originalIndices.push_back(rentry.PositionIndex);
                rentry.StartY = (*drawEntries)[i].Y;
                i++;
            }
        }
        for (auto & dentry : (*drawEntries)) {
            newPositions.push_back(dentry.Position - 1);
        }

        std::vector<int> newIndices;
        ComputeNewPositionIndices(originalIndices, newPositions, &newIndices);

        for (size_t i = 0; i < targetEntries.size(); i++) {
            recon->Entries[i].PositionIndex = newIndices[i];
            //(*drawEntries)[i].Y = newIndices[i] * TIMING_TOWER_Y_SPACING;
        }
    }

    if (recon->MovingTimer) {
        for (size_t i = 0; i < targetEntries.size(); i++) {
            double y = util::Lerp(
                    static_cast<double>(recon->MovingTimer), static_cast<double>(TIMER_TIME), static_cast<double>(1),
                    static_cast<double>(recon->Entries[i].StartY),
                    static_cast<double>(recon->Entries[i].PositionIndex * TIMING_TOWER_Y_SPACING));
            (*drawEntries)[i].Y = std::round(y);
        }
        recon->MovingTimer--;
    }

    // matchbox 20 moment.
    std::vector<
        std::pair<
            std::vector<std::pair<size_t, int>>,
            std::vector<int64_t>
        >> groups;
    groups.resize(targetEntries.size() + 1);
    for (size_t i = 0; i < targetEntries.size(); i++) {
        int pos = (*drawEntries)[i].Position;
        if (pos > 1 && !(*drawEntries)[i].IsFinalTime) {
            groups[pos].first.emplace_back(i, recon->Entries[i].PositionIndex);
            groups[pos].second.push_back((*drawEntries)[i].IntervalMS);
        }
    }
    for (auto & [idxpos, ms] : groups) {
        if (idxpos.size() > 1) {
            std::sort(idxpos.begin(), idxpos.end(), [&](const std::pair<size_t, int>& l, const std::pair<size_t, int>& r){
                return l.second < r.second;
            });
            std::sort(ms.begin(), ms.end(), [&](int64_t l, int64_t r){
                return l > r;
            });

            for (size_t i = 0; i < idxpos.size(); i++) {
                (*drawEntries)[idxpos[i].first].IntervalMS = ms[i];
            }
        }
    }

    for (size_t i = 0; i < targetEntries.size(); i++) {
        if (recon->Entries[i].PositionIndex == 0 && !(*drawEntries)[i].IsFinalTime) {
            (*drawEntries)[i].IntervalMS = 0;
        }
    }

}

void argos::rgms::StepTimingTower(SMBComp* comp, SMBCompTimingTower* tower, SMBCompPlayerLocations* locations,
        SMBCompReplayComponent* replay, SMBCompSoundComponent* sound)
{
    const std::vector<SMBCompPlayer>& players = comp->Config.Players.Players;
    for (auto & player : players) {
        auto it = tower->Timings.find(player.UniquePlayerID);

        SMBCompPlayerTimings* timings = nullptr;
        if (it == tower->Timings.end()) {
            SMBCompPlayerTimings t;
            InitializeSMBCompPlayerTimings(&t);
            tower->Timings[player.UniquePlayerID] = t;
            timings = &tower->Timings[player.UniquePlayerID];
        } else {
            timings = &it->second;
        }

        //const smb::SMBRaceCategoryInfo* catInfo = comp->StaticData.Categories.FindCategory(comp->Config.Tournament.Category);
        const auto& route = comp->StaticData.Categories.Routes.at(comp->Config.Tournament.Category);
        SMBCompFeed* feed = GetPlayerFeed(player, &comp->Feeds);
        if (feed && feed->Source) {
            while (auto out = feed->Source->GetNextProcessorOutput()) {
                StepSMBCompPlayerTimings(timings, out, route.get());

                replay->NoteOutput(player, out);
                sound->NoteOutput(player, out);
            }
        }
    }

    auto* state = &tower->TargetState;
    state->Entries.clear();

    int highestSectionIndex = -1;

    struct TimingInfo {
        int PlayerIndex;
        int SectionIndex;
        int PageIndex;
        int64_t LastSplitTimeMS;
    };
    std::vector<TimingInfo> timingInfo;

    int playerIndex = 0;
    for (auto & player : players) {

        auto& timings = tower->Timings.at(player.UniquePlayerID);
        auto& splits = timings.SplitM2s;

        TimingTowerEntry tte;
        tte.Position = -1;
        tte.Color = player.Colors.RepresentativeColor;
        tte.Name = player.Names.ShortName;
        tte.IntervalMS = -1;
        tte.InSection = false;
        tte.IsHighlight = false;
        tte.Y = 0;

        state->Entries.push_back(tte);


        int sectionIndex = static_cast<int>(splits.size() - 1);
        if (sectionIndex > highestSectionIndex) {
            highestSectionIndex = sectionIndex;
        }

        TimingInfo info;
        info.PlayerIndex = playerIndex;
        info.SectionIndex = sectionIndex;
        if (sectionIndex < 0) {
            info.PageIndex = -1;
        } else {
            if (sectionIndex < timings.SplitPageM2s.size()) {
                info.PageIndex = static_cast<int>(timings.SplitPageM2s.at(sectionIndex).size()) - 1;
            } else {
                info.PageIndex = -1;
            }
        }
        info.LastSplitTimeMS = -1;
        if (!splits.empty()) {
            int64_t elapsedM2s = timings.SplitPageM2s.back().back() - splits.front();
            info.LastSplitTimeMS = static_cast<int64_t>(std::round(static_cast<double>(elapsedM2s) * nes::NTSC_MS_PER_M2));
        }
        timingInfo.push_back(info);

        playerIndex++;
    }

    //const smb::SMBRaceCategoryInfo* catInfo = comp->StaticData.Categories.FindCategory(comp->Config.Tournament.Category);
    const auto& route = comp->StaticData.Categories.Routes.at( comp->Config.Tournament.Category);
    state->Subtitle = comp->Config.Tournament.TowerName;
    state->Title = "1-1";
    if (highestSectionIndex >= 0) {
        int tsec = highestSectionIndex;
        if (tsec >= route->size()) {
            tsec = route->size() - 1;
        }
        state->Title = fmt::format("{}-{}", route->at(tsec).World,
                route->at(tsec).Level);
    }

    std::sort(timingInfo.begin(), timingInfo.end(), [](const TimingInfo& l, const TimingInfo& r){
        if (l.SectionIndex == r.SectionIndex) {
            if (l.PageIndex == r.PageIndex) {
                return l.LastSplitTimeMS < r.LastSplitTimeMS;
            }
            return l.PageIndex > r.PageIndex;
        }
        return l.SectionIndex > r.SectionIndex;
    });

    int positionIndex = 0;
    int actualPosition = 1;
    int positionInc = 0;

    bool lastInSec = true;
    for (auto & ti : timingInfo) {
        auto& entry = state->Entries[ti.PlayerIndex];
        entry.InSection = ti.SectionIndex == highestSectionIndex;
        bool changedSecToFalse = false;
        if (lastInSec && !entry.InSection) {
            changedSecToFalse = true;
        }
        lastInSec = entry.InSection;
        entry.Y = positionIndex * TIMING_TOWER_Y_SPACING;
        entry.IntervalMS = ti.LastSplitTimeMS;
        entry.IsFinalTime = false;
        if (ti.SectionIndex >= 0 && ti.SectionIndex >= route->size()) {
            auto& timingsThis = tower->Timings.at(players[timingInfo[positionIndex].PlayerIndex].UniquePlayerID);
            entry.IntervalMS =
                std::round(static_cast<double>(timingsThis.SplitM2s.back() - timingsThis.SplitM2s.front()) * nes::NTSC_MS_PER_M2);
            entry.IsFinalTime = true;
        } else if (ti.SectionIndex >= 0 && ti.PageIndex >= 0) {
            if (positionIndex == 0) {
                entry.IntervalMS = 0;
            } else {
                auto& timingsThis = tower->Timings.at(players[timingInfo[positionIndex + 0].PlayerIndex].UniquePlayerID);
                auto& timingsPrev = tower->Timings.at(players[timingInfo[positionIndex - 1].PlayerIndex].UniquePlayerID);

                try {

                    //int64_t extraTimeM2s = 0;
                    //if (timingsPrev.SplitPageM2s.at(ti.SectionIndex).size() > (ti.PageIndex + 1)) {
                    //    extraTimeM2s = timingsPrev.SplitPageM2s.back().back() -
                    //        timingsPrev.SplitPageM2s.at(ti.SectionIndex).at(ti.PageIndex + 1);
                    //} else if (timingsPrev.SplitPageM2s.size() > (ti.SectionIndex + 1)) {
                    //    extraTimeM2s = timingsPrev.SplitPageM2s.back().back() -
                    //        timingsPrev.SplitPageM2s.at(ti.SectionIndex + 1).at(0);
                    //}


                    int64_t intervalM2s = (timingsThis.SplitPageM2s.at(ti.SectionIndex).at(ti.PageIndex) - timingsThis.SplitPageM2s.front().front()) -
                                          (timingsPrev.SplitPageM2s.at(ti.SectionIndex).at(ti.PageIndex) - timingsPrev.SplitPageM2s.front().front());


                    entry.IntervalMS = std::round(static_cast<double>(intervalM2s) * nes::NTSC_MS_PER_M2 / 100.0) * 100.0;
                    if (entry.IntervalMS < 0) {
                        entry.IntervalMS = 0;
                    }
                } catch (std::out_of_range) {
                    entry.IntervalMS = 0;
                }
            }
        } else {
            entry.IntervalMS = -1;
        }
        if (entry.IntervalMS == 0 && positionIndex != 0) {
            entry.IntervalMS = 1;
        }

        if (entry.IntervalMS >= 100 || changedSecToFalse || (entry.IsFinalTime && positionIndex != 0)) {
            actualPosition += positionInc;
            positionInc = 1;
        } else {
            positionInc++;
        }
        entry.Position = actualPosition;

        positionIndex++;
    }


    ReconcileTargetAndDrawState(tower->TargetState, &tower->DrawState, &tower->Reconcilation);
    locations->PlayerIdsByPosition.resize(tower->TargetState.Entries.size());
    for (size_t i = 0; i < tower->TargetState.Entries.size(); i++) {
        locations->PlayerIdsByPosition[tower->Reconcilation.Entries[i].PositionIndex] = players[i].UniquePlayerID;
    }
}


////////////////////////////////////////////////////////////////////////////////

SMBCompTimingTowerViewComponent::SMBCompTimingTowerViewComponent(argos::RuntimeConfig* info, SMBComp* comp)
    : ISMBCompSingleWindowComponent("Timing Tower", "timing tower", true)
    , m_Info(info)
    , m_Competition(comp)
{
}

SMBCompTimingTowerViewComponent::~SMBCompTimingTowerViewComponent()
{
}

void SMBCompTimingTowerViewComponent::DoControls()
{
    //const smb::SMBRaceCategoryInfo* catInfo = m_Competition->StaticData.Categories.FindCategory(m_Competition->Config.Tournament.Category);
    const auto& route = m_Competition->StaticData.Categories.Routes.at(
        m_Competition->Config.Tournament.Category);

    ImGui::Checkbox("from leader", &m_Competition->Tower.FromLeader);

    if (ImGui::CollapsingHeader("tables")) {
        if (ImGui::BeginTable("players", 3)) {
            ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("state", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("time", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableHeadersRow();

            //for (auto [id, v] : m_Competition->Tower.Timings) {
            int rowId = 0;
            for (auto & player : m_Competition->Config.Players.Players) {
                if (m_Competition->Tower.Timings.find(player.UniquePlayerID) == m_Competition->Tower.Timings.end()) continue;

                const SMBCompPlayerTimings* timings = &m_Competition->Tower.Timings.at(player.UniquePlayerID);
                //auto* player = FindPlayer(m_Competition->Config.Players, id);

                ImGui::PushID(rowId++);

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(player.Names.ShortName.c_str());

                ImGui::TableNextColumn();
                if (timings->State == TimingState::WAITING_FOR_1_1) {
                    ImGui::TextUnformatted("wait 1-1");
                } else if (timings->State == TimingState::RUNNING) {
                    ImGui::TextUnformatted(" running");
                }

                ImGui::TableNextColumn();
                if (timings->SplitM2s.size() > 0) {
                    int64_t elapsedM2s = timings->SplitM2s.back() - timings->SplitM2s.front();
                    auto out = GetLatestPlayerOutput(*m_Competition, player);
                    if (out) {
                        if (timings->SplitM2s.size() != route->size() + 1 && out) {
                            elapsedM2s = out->M2Count - timings->SplitM2s.front();
                        }
                        std::string time = util::SimpleMillisFormat(
                                static_cast<int64_t>(static_cast<double>(elapsedM2s) * nes::NTSC_MS_PER_M2),
                            util::SimpleTimeFormatFlags::MSCS);
                        rgmui::TextFmt("{}", time);
                    } else {
                        rgmui::TextFmt("--:--.--");
                    }
                } else {
                    rgmui::TextFmt("--:--.--");
                }

                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        ImGui::Separator();

        int numPlayers = m_Competition->Config.Players.Players.size();
        if (ImGui::BeginTable("splits", numPlayers + 1)) {
            ImGui::TableSetupColumn("section index", ImGuiTableColumnFlags_WidthFixed);
            for (auto & player : m_Competition->Config.Players.Players) {
                ImGui::TableSetupColumn(player.Names.ShortName.c_str(), ImGuiTableColumnFlags_WidthFixed);
            }
            ImGui::TableHeadersRow();

            for (int sectionIndex = 0; sectionIndex < route->size() + 1; sectionIndex++) {
                ImGui::TableNextColumn();
                rgmui::TextFmt("{}", sectionIndex);

                for (auto & player : m_Competition->Config.Players.Players) {
                    if (m_Competition->Tower.Timings.find(player.UniquePlayerID) == m_Competition->Tower.Timings.end()) continue;
                    SMBCompPlayerTimings* timings = &m_Competition->Tower.Timings.at(player.UniquePlayerID);

                    ImGui::TableNextColumn();
                    if (timings->SplitM2s.size() > sectionIndex) {
                        int64_t elapsedM2s = timings->SplitM2s[sectionIndex] - timings->SplitM2s[0];
                        std::string time = util::SimpleMillisFormat(
                                static_cast<int64_t>(static_cast<double>(elapsedM2s) * nes::NTSC_MS_PER_M2),
                            util::SimpleTimeFormatFlags::MSCS);
                        rgmui::TextFmt("{}", time);
                    } else {
                        rgmui::TextFmt("..");
                    }
                }
            }

            ImGui::EndTable();
        }
    }

    int w, h;
    DrawTowerStateSize(m_Competition->Tower.DrawState, &w, &h);
    nes::PPUx ppux(w, h, nes::PPUxPriorityStatus::ENABLED);

    DrawTowerState(&ppux, 0, 0, m_Competition->Config.Visuals.Palette, m_Competition->StaticData.Font,
            m_Competition->Tower.DrawState, m_Competition->Tower.FromLeader);

    cv::Mat m(ppux.GetHeight(), ppux.GetWidth(), CV_8UC3, ppux.GetBGROut());
    cv::resize(m, m_Competition->Tower.Img, {}, 2.0, 4.0, cv::INTER_NEAREST);
    rgmui::MatAnnotator anno("tt", m_Competition->Tower.Img);

}

////////////////////////////////////////////////////////////////////////////////

SMBCompApp::SMBCompApp(argos::RuntimeConfig* info)
    : m_Info(info)
    , m_CompConfigurationComponent(info, &m_Competition, &m_Competition.StaticData)
    , m_CompPlayerWindowsComponent(info, &m_Competition)
    , m_CompCombinedViewComponent(info, &m_Competition)
//    , m_CompIndividualViewsComponent(info, &m_Competition)
//    , m_CompPointsComponent(info, &m_Competition)
    , m_CompMinimapViewComponent(info, &m_Competition)
//    , m_CompRecordingsHelperComponent(info, &m_Competition)
    , m_CompTimingTowerViewComponent(info, &m_Competition)
    , m_CompCompetitionComponent(info, &m_Competition)
//    , m_CompGhostViewComponent(info, &m_Competition)
//    , m_CompPointTransitionComponent(info, &m_Competition)
//    , m_CompRecreateComponent(info, &m_Competition)
//    , m_CompCreditsComponent(info, &m_Competition)
    , m_CompSoundComponent(info, &m_Competition)
    , m_CompReplayComponent(info, &m_Competition)
//    , m_CompFixedOverlay(info, &m_Competition)
//    , m_CompTxtDisplay(info, &m_Competition)
    , m_CompTournamentComponent(info, &m_Competition)
    , m_SharedMemory(nullptr)
    , m_AuxVisibleInPrimary(true)
    , m_AuxVisibleScale(0.24)
    , m_AuxDisplayType(1)
    , m_CountingDown(false)
    , m_ShowTimer(true)
{
    InitializeSMBComp(info, &m_Competition);

    RegisterComponent(&m_CompConfigurationComponent);
    RegisterComponent(&m_CompPlayerWindowsComponent);
    RegisterComponent(&m_CompCombinedViewComponent);
//    RegisterComponent(&m_CompIndividualViewsComponent);
//    RegisterComponent(&m_CompPointsComponent);
    RegisterComponent(&m_CompMinimapViewComponent);
//    RegisterComponent(&m_CompRecordingsHelperComponent);
    RegisterComponent(&m_CompTimingTowerViewComponent);
    RegisterComponent(&m_CompCompetitionComponent);
//    RegisterComponent(&m_CompGhostViewComponent);
//    RegisterComponent(&m_CompPointTransitionComponent);
//    RegisterComponent(&m_CompRecreateComponent);
//    RegisterComponent(&m_CompCreditsComponent);
    RegisterComponent(&m_CompSoundComponent);
    RegisterComponent(&m_CompReplayComponent);
//    RegisterComponent(&m_CompFixedOverlay);
//    RegisterComponent(&m_CompTxtDisplay);
    RegisterComponent(&m_CompTournamentComponent);

    m_AuxDisplay = cv::Mat::zeros(1080, 1920, CV_8UC3);
}

SMBCompApp::~SMBCompApp()
{
    if (m_SharedMemory) {
        uint8_t* b = reinterpret_cast<uint8_t*>(m_SharedMemory);
        b[SHARED_MEM_QUIT] = 0x01;
    }
}

void SMBCompApp::DoAuxDisplay()
{
    if (m_Competition.CombinedView.Img.cols != 0) {
        cv::Mat m2;
        cv::resize(m_Competition.CombinedView.Img, m2, {}, 4, 4, cv::INTER_NEAREST);
        m2(cv::Rect(0, 60, 1920, 840)).copyTo(m_AuxDisplay(cv::Rect(0, 0, 1920, 840)));
    }
    if (m_Competition.Minimap.Img.cols != 0) {
        m_Competition.Minimap.Img.copyTo(m_AuxDisplay(cv::Rect(0, 840, 1920, 240)));
    }
    if (m_Competition.Tower.Img.cols != 0) {
        auto& t = m_Competition.Tower.Img;

        cv::Rect r(40, 40, t.cols, t.rows);

        cv::addWeighted(m_AuxDisplay(r), 0.2, t, 0.8, 0.0, m_AuxDisplay(r));
    }

    if (m_Competition.Points.Visible) {
        m_AuxDisplay += cv::Scalar(30, 30, 30);
        cv::GaussianBlur(m_AuxDisplay, m_AuxDisplay, cv::Size(9, 9), 0, 0);

        int w, h;
        DrawPointsSize(&m_Competition, &w, &h);
        nes::PPUx ppux(w, h, nes::PPUxPriorityStatus::ENABLED);
        DrawPoints(&ppux, 0, 0, m_Competition.Config.Visuals.Palette,
                m_Competition.StaticData.Font, &m_Competition);

        cv::Mat m(ppux.GetHeight(), ppux.GetWidth(), CV_8UC3, ppux.GetBGROut());
        cv::resize(m, m, {}, 4.0, 8.0, cv::INTER_NEAREST);

        int x = (1920 - m.cols) / 2;
        int y = (1080 - m.rows) / 2;

        m.copyTo(m_AuxDisplay(cv::Rect(x, y, m.cols, m.rows)));
    }

    if (m_CountingDown) {
        m_AuxDisplay += cv::Scalar(30, 30, 30);
        cv::GaussianBlur(m_AuxDisplay, m_AuxDisplay, cv::Size(9, 9), 0, 0);

        util::mclock::time_point now = util::Now();
        int64_t el = util::ElapsedMillisFrom(m_CountdownStart);
        int64_t durat = 1000;

        int w = 64, h = 28;
        nes::PPUx ppux(w, h, nes::PPUxPriorityStatus::ENABLED);
        DrawTowerStateFrame(&ppux, 0, 0, w, h, m_Competition.Config.Visuals.Palette);

        nes::RenderInfo render = DefaultSMBCompRenderInfo(m_Competition);

        std::array<uint8_t, 4> tpal = {0x00, nes::PALETTE_ENTRY_WHITE, 0x20, 0x20};
        ppux.BeginOutline();
        std::string txt = "go!";
        if (el < durat) {
            txt = " 3 ";
        } else if (el < durat * 2) {
            txt = " 2 ";
        } else if (el < durat * 3) {
            txt = " 1 ";
        } else if (el > durat * 5) {
            m_CountingDown = false;
        }
        int tx = 8;
        if (txt == "go!") {
            tx += 2;
        }
        ppux.RenderStringX(tx, 6, txt,
            m_Competition.StaticData.Font.data(), tpal.data(), render.PaletteBGR, 2, 2,
            nes::EffectInfo::Defaults());
        ppux.StrokeOutlineX(2.0f, nes::PALETTE_ENTRY_BLACK, render.PaletteBGR);

        cv::Mat m(ppux.GetHeight(), ppux.GetWidth(), CV_8UC3, ppux.GetBGROut());
        cv::resize(m, m, {}, 6.0, 12.0, cv::INTER_NEAREST);

        int x = (1920 - m.cols) / 2;
        int y = (1080 - m.rows) / 2;

        m.copyTo(m_AuxDisplay(cv::Rect(x, y, m.cols, m.rows)));
    }


    if (m_ShowTimer) {
        int64_t elmil = -1;
        std::string t;

        std::vector<SMBCompPlayer>& players = m_Competition.Config.Players.Players;
        for (auto & player : players) {
            std::string timetxt;
            int64_t elapsedts = 0;
            const SMBCompPlayerTimings* timings = &m_Competition.Tower.Timings.at(player.UniquePlayerID);
            bool end = TimingsToText(&m_Competition, timings, player, &timetxt, &elapsedts);
            int64_t timems = static_cast<int64_t>(static_cast<double>(elapsedts) * nes::NTSC_MS_PER_M2);
            if (timems > elmil) {
                t = fmt::format("{:>6s}", util::SimpleMillisFormat(timems, util::SimpleTimeFormatFlags::MINS));
                elmil = timems;
            }
        }

        int w = 6 * 16 + 12, h = 28;
        nes::PPUx ppux(w, h, nes::PPUxPriorityStatus::ENABLED);
        DrawTowerStateFrame(&ppux, 0, 0, w, h, m_Competition.Config.Visuals.Palette);

        nes::RenderInfo render = DefaultSMBCompRenderInfo(m_Competition);

        std::array<uint8_t, 4> tpal = {0x00, nes::PALETTE_ENTRY_WHITE, 0x20, 0x20};
        ppux.BeginOutline();
        ppux.RenderStringX(6, 8, t,
            m_Competition.StaticData.Font.data(), tpal.data(), render.PaletteBGR, 2, 2,
            nes::EffectInfo::Defaults());
        ppux.StrokeOutlineX(2.0f, nes::PALETTE_ENTRY_BLACK, render.PaletteBGR);

        cv::Mat m(ppux.GetHeight(), ppux.GetWidth(), CV_8UC3, ppux.GetBGROut());
        cv::resize(m, m, {}, 2.0, 3.0, cv::INTER_NEAREST);

        int x = (1920 - m.cols) - 16;
        int y = (1080 - m.rows - 240 - 16);

        m.copyTo(m_AuxDisplay(cv::Rect(x, y, m.cols, m.rows)));
    }

    if (m_Competition.DoingRecordingOfRecordings) {
        std::string out = fmt::format("{}rec/{}_{}", m_Info->ArgosDirectory, m_Competition.Config.Tournament.FileName, m_Competition.Config.Tournament.CurrentRound);
        std::string dir = out + "/";
        cv::imwrite(fmt::format("{}{:07d}.png", dir, m_Competition.FrameNumber), m_AuxDisplay);
    }


}

bool SMBCompApp::OnFrame()
{
    bool ret = true;
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Exit", "Ctrl+W")) {
                ret = false;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Components")) {
            m_CompConfigurationComponent.DoMenuItem();
            m_CompPlayerWindowsComponent.DoMenuItem();
            m_CompCombinedViewComponent.DoMenuItem();
//            m_CompIndividualViewsComponent.DoMenuItem();
//            m_CompPointsComponent.DoMenuItem();
            m_CompMinimapViewComponent.DoMenuItem();
            m_CompTimingTowerViewComponent.DoMenuItem();
//            m_CompRecordingsHelperComponent.DoMenuItem();
//            m_CompGhostViewComponent.DoMenuItem();
//            m_CompPointTransitionComponent.DoMenuItem();
//            m_CompRecreateComponent.DoMenuItem();
//            m_CompCreditsComponent.DoMenuItem();
            m_CompSoundComponent.DoMenuItem();
            m_CompReplayComponent.DoMenuItem();
//            m_CompFixedOverlay.DoMenuItem();
//            m_CompTxtDisplay.DoMenuItem();
            m_CompTournamentComponent.DoMenuItem();
            bool v = m_AuxVisibleInPrimary;
            if (ImGui::MenuItem("Aux In Primary", NULL, v)) {
                m_AuxVisibleInPrimary = !m_AuxVisibleInPrimary;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    for (auto & player : m_Competition.Config.Players.Players) {
        auto feed = GetPlayerFeed(player, &m_Competition.Feeds);
        if (feed) {
            feed->CachedOutput = nullptr;
        }
    }

    if (m_AuxVisibleInPrimary) {
        if (ImGui::Begin("aux display")) {
            rgmui::Combo4("aux display type", &m_AuxDisplayType,
                    std::vector<int>{0, 1, 2},
                    {"overlay", "main", "txt"});
            ImGui::Checkbox("show timer", &m_ShowTimer);
            if (ImGui::Button("reset timings")) {
                ResetSMBCompTimingTower(&m_Competition.Tower);
                m_Competition.CombinedView.AID = smb::AreaID::GROUND_AREA_6;
                m_Competition.CombinedView.APX = 0;
                m_Competition.CombinedView.FramePalette = {
                    0x22, 0x29, 0x1a, 0x0f,
                    0x0f, 0x36, 0x17, 0x0f,
                    0x0f, 0x30, 0x21, 0x0f,
                    0x0f, 0x07, 0x17, 0x0f,

                    0x22, 0x16, 0x27, 0x18,
                    0x0f, 0x1a, 0x30, 0x27,
                    0x0f, 0x16, 0x30, 0x27,
                    0x0f, 0x0f, 0x0f, 0x0f,
                };
            }

            //cv::Mat m = opencvext::ResizePrefNearest(m_AuxDisplay, m_AuxVisibleScale);
            cv::Mat m(m_AuxDisplay.rows / 4, m_AuxDisplay.cols / 4, m_AuxDisplay.type());
            for (int y = 0; y < m.rows; y++) {
                for (int x = 0; x < m.cols; x++) {
                    m.at<cv::Vec3b>(y, x) = m_AuxDisplay.at<cv::Vec3b>(y * 4, x * 4);
                }
            }
            rgmui::MatAnnotator anno("ax", m);
        }
        ImGui::End();
    }

    StepTimingTower(&m_Competition, &m_Competition.Tower, &m_Competition.Locations, &m_CompReplayComponent, &m_CompSoundComponent);
    // TODO: Step Minimap
    StepCombinedView(&m_Competition, &m_Competition.CombinedView);
    StepCombinedView(&m_Competition, &m_Competition.CombinedView2);
    StepSMBCompPoints(&m_Competition, &m_Competition.Points);

    if (m_Competition.BeginCountdown && !m_CountingDown) {
        m_Competition.BeginCountdown = false;

        m_CountingDown = true;
        m_AuxDisplayType = 1;
        m_CountdownStart = util::Now();
    }

    if (!m_Competition.SetTxtViewTo.empty()) {
        m_AuxDisplayType = 2;

//        m_CompTxtDisplay.SetStem(m_Competition.SetTxtViewTo);
        m_Competition.SetTxtViewTo = "";
    }
    if (m_Competition.SetToOverlay) {
        m_AuxDisplayType = 0;
        m_Competition.SetToOverlay = false;
    }



    if (ImGui::Begin("fps")) {
        rgmui::TextFmt("{:.2f}", ImGui::GetIO().Framerate);
    }
    ImGui::End();

    if (m_SharedMemory || m_Competition.DoingRecordingOfRecordings || m_AuxVisibleInPrimary) {
        if (m_AuxDisplayType == 0) { // overlay
//            m_CompFixedOverlay.DoDisplay(&m_AuxDisplay);
        } else if (m_AuxDisplayType == 1) {
            DoAuxDisplay();
        } else if (m_AuxDisplayType == 2) {
//            m_CompTxtDisplay.DoDisplay(m_AuxDisplay);
        } else if (m_AuxDisplayType == 3) {
        }

        if (!m_CompReplayComponent.DoReplay(m_AuxDisplay))
        {
            if (m_Competition.CombinedView2.Active) {
                auto& img = m_Competition.CombinedView2.Img;
                if (img.cols && img.rows) {
                    nes::PPUx ppux2(m_AuxDisplay.cols, m_AuxDisplay.rows, m_AuxDisplay.data,
                            nes::PPUxPriorityStatus::ENABLED);
                    auto& nesPalette = nes::DefaultPaletteBGR();
                    int y = 1080 - 480 - 32;
                    ppux2.DrawBorderedBox(16, y - 16, 480 + 32, 240 + 32, {0x36, 0x36, 0x36,  0x36, 0x17, 0x0f,  0x0f, 0x0f, 0x0f}, nesPalette.data(), 2);

                    img.copyTo(
                            m_AuxDisplay(cv::Rect(32, y, img.cols, img.rows)));
                }
            }
        }

        if (m_SharedMemory) {
            memcpy(m_SharedMemory, m_AuxDisplay.data, SHARED_MEM_MAT);
        }
    }

    return ret;
}

void SMBCompApp::LoadNamedConfig(const std::string& name)
{
    m_CompTournamentComponent.LoadTournament(name);
    //m_CompConfigurationComponent.LoadNamedConfig(name);
}

void SMBCompApp::SetSharedMemory(void* sharedMem)
{
    m_SharedMemory = sharedMem;
}

////////////////////////////////////////////////////////////////////////////////

SMBCompFeed* argos::rgms::GetPlayerFeed(const SMBCompPlayer& player, SMBCompFeeds* feeds)
{
    assert(feeds);
    auto it = feeds->Feeds.find(player.UniquePlayerID);
    if (it == feeds->Feeds.end()) {
        feeds->Feeds[player.UniquePlayerID] = std::make_unique<SMBCompFeed>();
        feeds->Feeds[player.UniquePlayerID]->UniquePlayerID = player.UniquePlayerID;
        feeds->Feeds[player.UniquePlayerID]->Source = nullptr;
        return feeds->Feeds[player.UniquePlayerID].get();
    }
    return it->second.get();
}

void argos::rgms::InitializeFeedSerialThread(const SMBCompPlayer& player, const SMBCompStaticData& data, SMBCompFeed* feed)
{
    try {
        auto params = rgms::SMBSerialProcessorThreadParameters::Defaults();
        params.Baud = player.Inputs.Serial.Baud;
        feed->MySMBSerialRecording.reset(nullptr);


        if (!player.Inputs.Serial.Path.empty() && player.Inputs.Serial.Path[0] == 't') {
            feed->MySMBZMQRef = std::make_unique<rgms::SMBZMQRef>(player.Inputs.Serial.Path, data.Nametables);
            feed->Source = feed->MySMBZMQRef.get();
        } else {
            feed->MySMBSerialProcessorThread = std::make_unique<rgms::SMBSerialProcessorThread>(
                    player.Inputs.Serial.Path,
                    data.Nametables,
                    params);
            feed->Source = feed->MySMBSerialProcessorThread.get();
        }
    } catch (std::runtime_error& err) {
        feed->MySMBSerialProcessorThread = nullptr;
        feed->MySMBZMQRef = nullptr;
        feed->Source = nullptr;
        feed->ErrorMessage = err.what();
        std::cout << feed->ErrorMessage << std::endl;
    }
}

//void argos::rgms::InitializeFeedLiveVideoThread(const SMBCompPlayer& player, SMBCompFeed* feed)
//{
//    try {
//        feed->LiveVideoThread = std::make_unique<rgms::video::LiveVideoThread>(
//            std::move(std::make_unique<video::V4L2VideoSource>(player.Inputs.Video.Path)),
//            20, true); // todo config?
//    } catch (std::runtime_error& err) {
//        feed->LiveVideoThread = nullptr;
//        feed->ErrorMessage = err.what();
//    }
//}
//

void argos::rgms::InitializeFeedRecording(SMBCompFeed* feed, const SMBCompStaticData& data, const std::string& path)
{
    if (!path.empty() && util::FileExists(path)) {
        feed->ErrorMessage = "";
        feed->MySMBSerialProcessorThread.reset(nullptr);
        feed->MySMBZMQRef.reset(nullptr);
        feed->MySMBSerialRecording = std::make_unique<SMBSerialRecording>(
                path, data.Nametables);
        feed->Source = feed->MySMBSerialRecording.get();
    } else {
        feed->ErrorMessage = "unable to open (empty or file not exist)";
    }
}

////////////////////////////////////////////////////////////////////////////////

bool argos::rgms::MarioInOutput(SMBMessageProcessorOutputPtr out, int* mariox, int* marioy)
{
    if (!out) return false;

    int tx = 0;
    int ty = 0;
    bool first = true;
    for (auto & oamx : out->Frame.OAMX) {
        if (smb::IsMarioTile(oamx.TileIndex)) {
            if (first || oamx.X < tx) tx = oamx.X;
            if (first || oamx.Y < ty) ty = oamx.Y;
            first = false;
        }
    }
    if (mariox) *mariox = tx;
    if (marioy) *marioy = ty;
    return !first;
}

////////////////////////////////////////////////////////////////////////////////

void argos::rgms::DrawTowerStateSize(const TimingTowerState& state, int* w, int* h)
{
    if (w) *w = 27 + 10*8 + 6*8 - 4 + 16 + 8;
    if (h) *h = state.Entries.size() * 12 + 24 - 1;
}


static void ComputeFromLeaderTimes(const std::vector<TimingTowerEntry>& entries, std::vector<int64_t>* fromLeaderTimes)
{
    struct FromLeaderInfo {
        size_t EntryIndex;
        int Position;
        bool IsFinalTime;
        int64_t IntervalMS;
        int64_t FromTime;
    };
    size_t n = entries.size();
    if (n == 0 || !fromLeaderTimes) return;

    fromLeaderTimes->resize(n, 0);
    std::vector<FromLeaderInfo> info(n);

    for (size_t entryIndex = 0; entryIndex < n; entryIndex++) {
        info[entryIndex].EntryIndex = entryIndex;
        info[entryIndex].Position = entries[entryIndex].Position;
        info[entryIndex].IsFinalTime = entries[entryIndex].IsFinalTime;
        info[entryIndex].IntervalMS = entries[entryIndex].IntervalMS;
    }

    std::sort(info.begin(), info.end(), [&](const FromLeaderInfo& l, const FromLeaderInfo& r){
        if (l.Position == r.Position) {
            return l.IntervalMS > r.IntervalMS;
        }
        return l.Position < r.Position;
    });

    (*fromLeaderTimes)[info[0].EntryIndex] = info[0].IntervalMS;

    int64_t from = 0;
    for (size_t infoIndex = 1; infoIndex < n; infoIndex++) {
        //std::cout << "info: from: " << infoIndex << "  " << info[infoIndex].IntervalMS << "  from: " << from << std::endl;
        if (info[infoIndex].IntervalMS < 0) {
            (*fromLeaderTimes)[info[infoIndex].EntryIndex] = info[infoIndex].IntervalMS;
        } else if (info[infoIndex].IsFinalTime) {
            from += info[infoIndex].IntervalMS - info[infoIndex - 1].IntervalMS;
            (*fromLeaderTimes)[info[infoIndex].EntryIndex] = info[infoIndex].IntervalMS;
        } else {
            from += info[infoIndex].IntervalMS;
            (*fromLeaderTimes)[info[infoIndex].EntryIndex] = from;
        }
    }

    //std::unordered_map<int, int64_t> position_to_time;
    //for (auto & inf : info) {
    //    if (info.Position > 1) {
    //        position_to_time[info.Position] = std::max(
    //    }

    //}

}

void argos::rgms::DrawTowerState(nes::PPUx* ppux, int x, int y,
        const nes::Palette& palette,
        const nes::PatternTable& font,
        const TimingTowerState& state,
        bool fromLeader)
{
    int w = 0, h = 0;
    DrawTowerStateSize(state, &w, &h);

    DrawTowerStateFrame(ppux, x, y, w, h, palette);


    std::array<uint8_t, 4> tpal = {0x00, nes::PALETTE_ENTRY_WHITE, 0x20, 0x20};
    std::array<uint8_t, 4> qpal = {0x00, 0x3d, 0x20, 0x20};
    std::array<uint8_t, 4> fpal = {0x00, 0x2a, 0x20, 0x20};
    std::array<uint8_t, 4> hpal = {0x00, 0x38, 0x20, 0x20};

    ppux->BeginOutline();
    ppux->RenderString(x + 6, y + 4, state.Title, font.data(), tpal.data(), palette.data(), 2,
            nes::EffectInfo::Defaults());
    ppux->RenderString(x + w - 8 * state.Subtitle.size()-4, y + 4, state.Subtitle, font.data(), tpal.data(), palette.data(), 1,
            nes::EffectInfo::Defaults());

    ppux->StrokeOutlineO(1.0f, nes::PALETTE_ENTRY_BLACK, palette.data());

    std::vector<int64_t> fromLeaderTimes;
    if (fromLeader) {
        ComputeFromLeaderTimes(state.Entries, &fromLeaderTimes);
    }

    int ty = y + 24;
    int qx = x - 2;
    size_t entryIndex = 0;
    for (auto entry : state.Entries) {
        const uint8_t* textpal = tpal.data();
        if (entry.IsFinalTime) {
            textpal = fpal.data();
        } else if (!entry.InSection) {
            textpal = qpal.data();
        } else if (entry.IsHighlight) {
            textpal = hpal.data();
        }


        ppux->BeginOutline();
        int qy = ty + entry.Y;
        if (entry.Position >= 1) {
            ppux->RenderString(qx + 8, qy, fmt::format("{:1d}", entry.Position), font.data(),
                    textpal, palette.data(), 1,
                    nes::EffectInfo::Defaults());
        }

        uint8_t c = entry.Color;
        ppux->RenderHardcodedSprite(qx + 20, qy,
                {{c, c},
                 {c, c},
                 {c, c},
                 {c, c},
                 {c, c},
                 {c, c},
                 {c, c}},
                 palette.data(), nes::EffectInfo::Defaults());

        ppux->RenderString(qx + 27, qy, entry.Name, font.data(), textpal, palette.data(), 1,
                nes::EffectInfo::Defaults());

        std::string time;
        bool putPlus = false;

        int64_t timems = entry.IntervalMS;
        if (fromLeader) {
            timems = fromLeaderTimes[entryIndex];
            //std::cout << "  " << entryIndex << " : " << timems << std::endl;
        }

        if (timems == 0) {
            if (fromLeader) {
                time = " leader";
            } else {
                time = "  gap  ";
            }
        } else if (timems < 0) {
            time = "       ";
        } else {
            putPlus = true;
            if (entry.IsFinalTime) {
                putPlus = false;
            }
            if (timems >= (10 * 60 * 1000)) {
                time = util::SimpleMillisFormat(timems, util::SimpleTimeFormatFlags::MINS);
            } else {
                time = util::SimpleMillisFormat(std::round(static_cast<double>(timems / 100.0)) * 100,
                        util::SimpleTimeFormatFlags::MSCS);
                time.pop_back();
            }
            time = fmt::format("{:>7}", time);
        }


        if (putPlus) {
            ppux->RenderString(qx + 27 + 11 * 8 - 2, qy, "+", font.data(), textpal, palette.data(), 1,
                    nes::EffectInfo::Defaults());
        }
        ppux->RenderString(qx + 27 + 11 * 8, qy, time, font.data(), textpal, palette.data(), 1,
                nes::EffectInfo::Defaults());

        ppux->StrokeOutlineO(1.0f, nes::PALETTE_ENTRY_BLACK, palette.data());
        entryIndex++;
    }
}


////////////////////////////////////////////////////////////////////////////////

//SMBCompRecordingsHelperComponent::SMBCompRecordingsHelperComponent(argos::RuntimeConfig* info, SMBComp* comp)
//    : ISMBCompSingleWindowComponent("Recordings", "recordings", true)
//    , m_Info(info)
//    , m_Competition(comp)
//    , m_FrameCount(0)
//    , m_OneFramePerStep(false)
//{
//}
//
//SMBCompRecordingsHelperComponent::~SMBCompRecordingsHelperComponent()
//{
//}
//
//void SMBCompRecordingsHelperComponent::DoControls()
//{
//    if (ImGui::Button("load prior")) {
//        std::vector<std::string> priorRecordings;
//        SMBCompPlayerWindow::UpdateRecordings2(m_Info, &priorRecordings);
//
//        std::reverse(priorRecordings.begin(), priorRecordings.end());
//        for (auto & player : m_Competition->Config.Players.Players) {
//            SMBCompFeed* feed = GetPlayerFeed(player, &m_Competition->Feeds);
//            for (auto & rec : priorRecordings) {
//                if (util::StringEndsWith(rec, player.Names.ShortName + ".rec")) {
//                    InitializeFeedRecording(feed, m_Competition->StaticData, rec);
//                    feed->MySMBSerialRecording->ResetToStartAndPause();
//                    break;
//                }
//            }
//        }
//    }
//    if (ImGui::Button("reset all to start")) {
//        ResetSMBCompTimingTower(&m_Competition->Tower);
//        for (auto & player : m_Competition->Config.Players.Players) {
//            SMBCompFeed* feed = GetPlayerFeed(player, &m_Competition->Feeds);
//            if (feed->MySMBSerialRecording) {
//                feed->MySMBSerialRecording->ResetToStartAndPause();
//            }
//        }
//        m_FrameCount = 0;
//    }
//    if (ImGui::Button("resume all")) {
//        for (auto & player : m_Competition->Config.Players.Players) {
//            SMBCompFeed* feed = GetPlayerFeed(player, &m_Competition->Feeds);
//            if (feed->MySMBSerialRecording) {
//                feed->MySMBSerialRecording->SetPaused(false);
//            }
//        }
//    }
//
//    //bool load = rgmui::InputText("file name", &m_FileName, ImGuiInputTextFlags_EnterReturnsTrue);
//    if (ImGui::Button("load comp")) { // || load) {
//        util::ForFileOfExtensionInDirectory(m_Info->ArgosDirectory + "rec/", ".rec", [&](util::fs::path p){
//            std::string recname = p.stem();
//
//            std::istringstream is(recname);
//
//            std::string ts, fname, shortname;
//            std::getline(is, ts, '_');
//
//            std::string remainder;
//            is >> remainder;
//            if (util::StringStartsWith(remainder, m_Competition->Config.Tournament.FileName)) {
//                remainder.erase(remainder.begin(), remainder.begin() + m_Competition->Config.Tournament.FileName.size() + 1);
//
//                std::istringstream is2(remainder);
//                std::string q;
//                std::getline(is2, q, '_');
//
//                is2 >> shortname;
//                if (std::stoi(q) == m_Competition->Config.Tournament.CurrentRound) {
//                    bool fnd = false;
//                    for (auto & player : m_Competition->Config.Players.Players) {
//                        if (player.Names.ShortName == shortname) {
//                            SMBCompFeed* feed = GetPlayerFeed(player, &m_Competition->Feeds);
//                            InitializeFeedRecording(feed, m_Competition->StaticData, p.string());
//                            feed->MySMBSerialRecording->ResetToStartAndPause();
//                            fnd = true;
//                            break;
//                        }
//                    }
//                    if (!fnd) {
//                        std::cout << "ERROR!: " << p.string() << std::endl;
//                    } else {
//                        std::cout << "loaded: " << recname << std::endl;
//                    }
//
//
//                }
//            }
//
//            //std::getline(is, fname, '_');
//            //std::getline(is, shortname);
//
//            //if (fname == m_FileName) {
//            //    bool fnd = false;
//            //    for (auto & player : m_Competition->Config.Players.Players) {
//            //        if (player.Names.ShortName == shortname) {
//            //            SMBCompFeed* feed = GetPlayerFeed(player, &m_Competition->Feeds);
//            //            InitializeFeedRecording(feed, m_Competition->StaticData, p.string());
//            //            feed->MySMBSerialRecording->ResetToStartAndPause();
//            //            fnd = true;
//            //            break;
//            //        }
//            //    }
//            //    if (!fnd) {
//            //        std::cout << "ERROR!: " << p.string() << std::endl;
//            //    }
//            //}
//            return true;
//        });
//        m_FrameCount = 0;
//    }
//
//    rgmui::TextFmt("{}", m_FrameCount);
//    ImGui::Checkbox("one frame per step", & m_OneFramePerStep);
//    if (ImGui::Button("seek one frame") || m_OneFramePerStep) {
//        m_FrameCount++;
//        m_Competition->FrameNumber = m_FrameCount;
//        for (auto & player : m_Competition->Config.Players.Players) {
//            SMBCompFeed* feed = GetPlayerFeed(player, &m_Competition->Feeds);
//            if (feed->MySMBSerialRecording) {
//                int64_t millis = std::round(static_cast<double>(m_FrameCount) / nes::NTSC_FPS * 1000.0);
//                feed->MySMBSerialRecording->SeekFromStartTo(millis);
//            }
//        }
//    }
//
//    if (!m_Competition->DoingRecordingOfRecordings) {
//        if (ImGui::Button("do recording of recording")) {
//            std::string out = fmt::format("{}rec/{}_{}", m_Info->ArgosDirectory, m_Competition->Config.Tournament.FileName, m_Competition->Config.Tournament.CurrentRound);
//            std::cout << out << std::endl;
//            std::string dir = out + "/";
//            util::fs::remove_all(dir);
//            util::fs::create_directory(dir);
//
//            m_Competition->DoingRecordingOfRecordings =  true;
//
//            m_OneFramePerStep = true;
//        }
//    } else {
//        if (ImGui::Button("stop recording of recording")) {
//            m_Competition->DoingRecordingOfRecordings = false;
//            std::string out = fmt::format("{}rec/{}_{}", m_Info->ArgosDirectory, m_Competition->Config.Tournament.FileName, m_Competition->Config.Tournament.CurrentRound);
//            std::cout << out << std::endl;
//            std::string dir = out + "/";
//
//            std::string cmd = fmt::format("ffmpeg -y -framerate 60 -i {}%07d.png -vcodec libx264 -crf 6 -vf format=yuv420p {}.mp4",
//                    dir, out);
//            int r = system(cmd.c_str());
//            std::cout << "ret: " << r << std::endl;
//        }
//    }
//}
//
////////////////////////////////////////////////////////////////////////////////

SMBCompPointsComponent::SMBCompPointsComponent(argos::RuntimeConfig* info, SMBComp* comp)
    : ISMBCompSingleWindowComponent("Points", "points", false)
    , m_Info(info)
    , m_Competition(comp)
{
}

SMBCompPointsComponent::~SMBCompPointsComponent()
{
}

void SMBCompPointsComponent::DoControls()
{
    std::vector<std::pair<uint32_t, int64_t>> elapsedTimes;
    if (ImGui::BeginTable("points", 5)) {
        ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("time", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("points", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("add", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("pending", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        const std::vector<SMBCompPlayer>& players = m_Competition->Config.Players.Players;
        int rowId = 0;
        for (auto & player : players) {
            if (m_Competition->Tower.Timings.find(player.UniquePlayerID) == m_Competition->Tower.Timings.end()) {
                continue;
            }

            const SMBCompPlayerTimings* timings = &m_Competition->Tower.Timings.at(player.UniquePlayerID);

            ImGui::PushID(rowId++);

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(player.Names.ShortName.c_str());

            ImGui::TableNextColumn();
            int64_t elapsedt = -1;
            std::string timetxt;
            if (TimingsToText(m_Competition, timings, player, &timetxt, &elapsedt)) {
                rgmui::GreenText(timetxt.c_str());
            } else {
                ImGui::TextUnformatted(timetxt.c_str());
            }
            elapsedTimes.emplace_back(player.UniquePlayerID, elapsedt);

            ImGui::TableNextColumn();
            int points = 0;
            auto it = m_Competition->Points.Points.find(player.UniquePlayerID);
            if (it != m_Competition->Points.Points.end()) {
                points = it->second;
            }
            rgmui::TextFmt("{}", points);

            int pending = 0;
            auto it2 = m_PendingPoints.find(player.UniquePlayerID);
            if (it2 == m_PendingPoints.end()) {
                m_PendingPoints[player.UniquePlayerID] = points;
                pending = points;
            } else {
                pending = it2->second;
            }

            ImGui::TableNextColumn();
            int add = pending - points;
            if (ImGui::InputInt("##a", &add)) {
                m_PendingPoints[player.UniquePlayerID] = points + add;
            }

            ImGui::TableNextColumn();
            if (ImGui::InputInt("##b", &pending)) {
                m_PendingPoints[player.UniquePlayerID] = pending;
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    ImGui::Checkbox("points visible", &m_Competition->Points.Visible);

    //if (ImGui::Button("set from times/increments")) {
    //    std::sort(elapsedTimes.begin(), elapsedTimes.end(), [&](const std::pair<uint32_t, int64_t>& l,
    //                const std::pair<uint32_t, int64_t>& r){
    //        return l.second < r.second;
    //    });
    //    int incrementIndex = 0;
    //    for (auto & [playerIndex, elapsed] : elapsedTimes) {
    //        if (elapsed == -1) {
    //            m_PendingPoints[playerIndex] = m_Competition->Points.Points[playerIndex];
    //        } else {
    //            int add = 0;
    //            if (incrementIndex < m_Competition->Config.Race.Increments.size()) {
    //                add = m_Competition->Config.Race.Increments[incrementIndex];
    //            }
    //            incrementIndex++;

    //            m_PendingPoints[playerIndex] = m_Competition->Points.Points[playerIndex] + add;
    //        }

    //    }
    //}
    //ImGui::SameLine();
    if (!m_Competition->Points.Visible) {
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(255, 100, 100, 255));
    }
    if (ImGui::Button("apply")) {
        m_Competition->Points.LastPoints = m_Competition->Points.Points;
        for (auto & [playerId, points] : m_PendingPoints) {
            m_Competition->Points.Points[playerId] = points;
        }
        m_Competition->Points.Countdown = POINTS_COUNTDOWN_MAX;
    }
    if (!m_Competition->Points.Visible) {
        ImGui::PopStyleColor();
    }

    if (ImGui::CollapsingHeader("draw")) {
        int w, h;
        DrawPointsSize(m_Competition, &w, &h);
        nes::PPUx ppux(w, h, nes::PPUxPriorityStatus::ENABLED);
        DrawPoints(&ppux, 0, 0, m_Competition->Config.Visuals.Palette,
                m_Competition->StaticData.Font, m_Competition);

        cv::Mat m(ppux.GetHeight(), ppux.GetWidth(), CV_8UC3, ppux.GetBGROut());
        //cv::resize(m, m, {}, 2.0, 4.0, cv::INTER_NEAREST);
        rgmui::MatAnnotator anno("pp", m);
    }
}

void argos::rgms::StepSMBCompPoints(SMBComp* comp, SMBCompPoints* points)
{
    const std::vector<SMBCompPlayer>& players = comp->Config.Players.Players;
    for (auto & player : players) {
        if (points->Points.find(player.UniquePlayerID) == points->Points.end()) {
            points->Points[player.UniquePlayerID] = 0;
            points->Countdown = 0;
        }
    }

    points->Countdown--;
    if (points->Countdown < 0) {
        points->Countdown = 0;
    }
}

void argos::rgms::DrawPointsSize(const SMBComp* comp, int* w, int* h)
{
    *w = 240;

    const std::vector<SMBCompPlayer>& players = comp->Config.Players.Players;
    *h = static_cast<int>(players.size()) * TIMING_TOWER_Y_SPACING + 4 + 10;
}

enum class PointsDrawDirection
{
    INCREASE,
    STAY_SAME,
    DECREASE
};

struct PointEntry {
    int Points;
    std::string Name;
    PointsDrawDirection Direction;
    int Y;
};

void argos::rgms::DrawPoints(nes::PPUx* ppux, int x, int y,
        const nes::Palette& palette,
        const nes::PatternTable& font,
        const SMBComp* comp)
{
    int w, h;
    DrawPointsSize(comp, &w, &h);

    DrawTowerStateFrame(ppux, x, y, w, h, palette);


    std::vector<PointEntry> entries;
    std::vector<PointEntry> lastEntries;

    const std::vector<SMBCompPlayer>& players = comp->Config.Players.Players;
    for (auto & player : players) {

        int points = 0;
        int lastPoints = 0;

        auto it = comp->Points.Points.find(player.UniquePlayerID);
        if (it != comp->Points.Points.end()) {
            points = it->second;
        }
        auto it3 = comp->Points.LastPoints.find(player.UniquePlayerID);
        if (it3 != comp->Points.LastPoints.end()) {
            lastPoints = it3->second;
        }

        PointEntry pe;
        pe.Points = points;
        pe.Name = player.Names.FullName;
        pe.Direction = PointsDrawDirection::STAY_SAME;
        pe.Y = 0;

        entries.push_back(pe);

        pe.Points = lastPoints;
        lastEntries.push_back(pe);
    }

    std::sort(entries.begin(), entries.end(), [&](const PointEntry& l, const PointEntry& r){
        if (l.Points == r.Points) {
            return l.Name < r.Name;
        }
        return l.Points > r.Points;
    });
    std::sort(lastEntries.begin(), lastEntries.end(), [&](const PointEntry& l, const PointEntry& r){
        if (l.Points == r.Points) {
            return l.Name < r.Name;
        }
        return l.Points > r.Points;
    });

    size_t n = entries.size();
    if (n <= 0) {
        return;
    }


    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            if (entries[i].Name == lastEntries[j].Name) {
                int sy = static_cast<int>(i) * TIMING_TOWER_Y_SPACING + 4;
                int ey = static_cast<int>(j) * TIMING_TOWER_Y_SPACING + 4;

                entries[i].Y = sy;
                entries[i].Points = entries[i].Points;

                if (comp->Points.Countdown != 0) {
                    entries[i].Y = std::round(util::Lerp(
                                static_cast<double>(comp->Points.Countdown),
                                    static_cast<double>(0),
                                    static_cast<double>(POINTS_COUNTDOWN_MAX),
                                static_cast<double>(sy),
                                static_cast<double>(ey)));
                    entries[i].Points = std::round(util::Lerp(
                                static_cast<double>(comp->Points.Countdown),
                                    static_cast<double>(0),
                                    static_cast<double>(POINTS_COUNTDOWN_MAX),
                                static_cast<double>(entries[i].Points),
                                static_cast<double>(lastEntries[j].Points)));
                }
                if (i < j) {
                    entries[i].Direction = PointsDrawDirection::INCREASE;
                } else if (i > j) {
                    entries[i].Direction = PointsDrawDirection::DECREASE;
                } else {
                    entries[i].Direction = PointsDrawDirection::STAY_SAME;
                }

                break;
            }

        }
    }

    std::array<uint8_t, 4> tpal = {0x00, nes::PALETTE_ENTRY_WHITE, 0x20, 0x20};
    ppux->BeginOutline();
    int l = comp->Config.Tournament.ScoreName.size() * 8;


    ppux->RenderString(x + (w - l) / 2, 3, comp->Config.Tournament.ScoreName, font.data(), tpal.data(), palette.data(), 1,
            nes::EffectInfo::Defaults());
    ppux->StrokeOutlineO(1.0f, nes::PALETTE_ENTRY_BLACK, palette.data());

    int qx = x + 8 + 16;
    for (auto & entry : entries) {
        ppux->ResetPriority();
        ppux->BeginOutline();
        int ay = entry.Y + 10;
        ppux->RenderString(qx, ay, entry.Name, font.data(), tpal.data(), palette.data(), 1,
                nes::EffectInfo::Defaults());

        ppux->RenderString(w - 40, ay, fmt::format("{:>3d}", entry.Points), font.data(), tpal.data(), palette.data(), 1,
                nes::EffectInfo::Defaults());

        int dx = qx - 16;
        int dy = ay + 1;
        if (entry.Direction == PointsDrawDirection::INCREASE) {
            uint8_t t = 0x2a;
            ppux->RenderHardcodedSprite(dx, dy,
                    {{0, 0, 0, 0, 0, t, 0, 0, 0, 0, 0},
                     {0, 0, 0, 0, t, t, t, 0, 0, 0, 0},
                     {0, 0, 0, t, t, t, t, t, 0, 0, 0},
                     {0, 0, t, t, t, t, t, t, t, 0, 0},
                     {0, t, t, t, t, t, t, t, t, t, 0},
                     {t, t, t, t, t, t, t, t, t, t, t}},
                     palette.data(), nes::EffectInfo::Defaults());
        } else if (entry.Direction == PointsDrawDirection::DECREASE) {
            uint8_t t = 0x16;
            ppux->RenderHardcodedSprite(dx, dy,
                    {{t, t, t, t, t, t, t, t, t, t, t},
                     {0, t, t, t, t, t, t, t, t, t, 0},
                     {0, 0, t, t, t, t, t, t, t, 0, 0},
                     {0, 0, 0, t, t, t, t, t, 0, 0, 0},
                     {0, 0, 0, 0, t, t, t, 0, 0, 0, 0},
                     {0, 0, 0, 0, 0, t, 0, 0, 0, 0, 0}},
                     palette.data(), nes::EffectInfo::Defaults());
        } else {
            uint8_t t = 0x2c;
            ppux->RenderHardcodedSprite(dx, dy,
                    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                     {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                     {0, t, t, t, t, t, t, t, t, t, 0},
                     {0, t, t, t, t, t, t, t, t, t, 0},
                     {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                     {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
                     palette.data(), nes::EffectInfo::Defaults());
        }

        //ppux->RenderString(w - 40, entry.Y, fmt::format("{:>3d}", comp->Points.Countdown), font.data(), tpal.data(), palette.data(), 1,
        //        nes::EffectInfo::Defaults());

        ppux->StrokeOutlineO(1.0f, nes::PALETTE_ENTRY_BLACK, palette.data());
    }
}

struct TimeEntry {
    int Position;
    int64_t ElapsedMs;
    std::string Name;
    std::string Text;
};

void argos::rgms::DrawFinalTimes(nes::PPUx* ppux, int x, int y,
        const nes::Palette& palette,
        const nes::PatternTable& font,
        SMBComp* comp)
{
    int w, h;
    DrawPointsSize(comp, &w, &h);

    DrawTowerStateFrame(ppux, x, y, w, h, palette);

    std::array<uint8_t, 4> tpal = {0x00, nes::PALETTE_ENTRY_WHITE, 0x20, 0x20};

    ppux->BeginOutline();
    int l = comp->Config.Tournament.ScoreName.size() * 8;
    ppux->RenderString(x + (w - l) / 2, 3, comp->Config.Tournament.ScoreName, font.data(), tpal.data(), palette.data(), 1,
            nes::EffectInfo::Defaults());
    ppux->StrokeOutlineO(1.0f, nes::PALETTE_ENTRY_BLACK, palette.data());

    int qx = x + 8 + 16;
    int qy = 14;
    const std::vector<SMBCompPlayer>& players = comp->Config.Players.Players;

    std::vector<TimeEntry> entries;
    for (auto & player : players) {
        TimeEntry te;
        te.Position = 0;
        te.Name = player.Names.FullName;

        const SMBCompPlayerTimings* timings = &comp->Tower.Timings.at(player.UniquePlayerID);
        bool finished = TimingsToText(comp, timings, player, &te.Text, &te.ElapsedMs);
        if (!finished) {
            te.Text = "dnf";
        }

        entries.push_back(te);

    }


    std::sort(entries.begin(), entries.end(), [&](const auto& l, const auto& r){
        return l.Text < r.Text;
    });

    int pos = 1;
    for (auto & te : entries) {
        te.Position = pos++;
        ppux->ResetPriority();
        ppux->BeginOutline();

        ppux->RenderString(qx - 16, qy, fmt::format("{}", te.Position), font.data(), tpal.data(), palette.data(), 1, nes::EffectInfo::Defaults());
        ppux->RenderString(qx, qy, te.Name, font.data(), tpal.data(), palette.data(), 1, nes::EffectInfo::Defaults());

        ppux->RenderString(w - te.Text.size() * 8 - 16, qy, te.Text, font.data(), tpal.data(), palette.data(), 1, nes::EffectInfo::Defaults());

        ppux->StrokeOutlineO(1.0f, nes::PALETTE_ENTRY_BLACK, palette.data());

        qy += TIMING_TOWER_Y_SPACING;
    }


//    std::sort(entries.begin(), entries.end(), [&](const PointEntry& l, const PointEntry& r){
//        if (l.Points == r.Points) {
//            return l.Name < r.Name;
//        }
//        return l.Points > r.Points;
//    });
//    std::sort(lastEntries.begin(), lastEntries.end(), [&](const PointEntry& l, const PointEntry& r){
//        if (l.Points == r.Points) {
//            return l.Name < r.Name;
//        }
//        return l.Points > r.Points;
//    });
//
//    size_t n = entries.size();
//    if (n <= 0) {
//        return;
//    }
//
//
//    for (size_t i = 0; i < n; i++) {
//        for (size_t j = 0; j < n; j++) {
//            if (entries[i].Name == lastEntries[j].Name) {
//                int sy = static_cast<int>(i) * TIMING_TOWER_Y_SPACING + 4;
//                int ey = static_cast<int>(j) * TIMING_TOWER_Y_SPACING + 4;
//
//                entries[i].Y = sy;
//                entries[i].Points = entries[i].Points;
//
//                if (comp->Points.Countdown != 0) {
//                    entries[i].Y = std::round(util::Lerp(
//                                static_cast<double>(comp->Points.Countdown),
//                                    static_cast<double>(0),
//                                    static_cast<double>(POINTS_COUNTDOWN_MAX),
//                                static_cast<double>(sy),
//                                static_cast<double>(ey)));
//                    entries[i].Points = std::round(util::Lerp(
//                                static_cast<double>(comp->Points.Countdown),
//                                    static_cast<double>(0),
//                                    static_cast<double>(POINTS_COUNTDOWN_MAX),
//                                static_cast<double>(entries[i].Points),
//                                static_cast<double>(lastEntries[j].Points)));
//                }
//                if (i < j) {
//                    entries[i].Direction = PointsDrawDirection::INCREASE;
//                } else if (i > j) {
//                    entries[i].Direction = PointsDrawDirection::DECREASE;
//                } else {
//                    entries[i].Direction = PointsDrawDirection::STAY_SAME;
//                }
//
//                break;
//            }
//
//        }
//    }
//
//    std::array<uint8_t, 4> tpal = {0x00, nes::PALETTE_ENTRY_WHITE, 0x20, 0x20};
//    ppux->BeginOutline();
//    int l = comp->Config.Race.ScoreName.size() * 8;
//
//
//    ppux->RenderString(x + (w - l) / 2, 3, comp->Config.Race.ScoreName, font.data(), tpal.data(), palette.data(), 1,
//            nes::EffectInfo::Defaults());
//    ppux->StrokeOutlineO(1.0f, nes::PALETTE_ENTRY_BLACK, palette.data());
//
//    int qx = x + 8 + 16;
//    for (auto & entry : entries) {
//        ppux->ResetPriority();
//        ppux->BeginOutline();
//        int ay = entry.Y + 10;
//        ppux->RenderString(qx, ay, entry.Name, font.data(), tpal.data(), palette.data(), 1,
//                nes::EffectInfo::Defaults());
//
//        ppux->RenderString(w - 40, ay, fmt::format("{:>3d}", entry.Points), font.data(), tpal.data(), palette.data(), 1,
//                nes::EffectInfo::Defaults());
//
//        int dx = qx - 16;
//        int dy = ay + 1;
//        if (entry.Direction == PointsDrawDirection::INCREASE) {
//            uint8_t t = 0x2a;
//            ppux->RenderHardcodedSprite(dx, dy,
//                    {{0, 0, 0, 0, 0, t, 0, 0, 0, 0, 0},
//                     {0, 0, 0, 0, t, t, t, 0, 0, 0, 0},
//                     {0, 0, 0, t, t, t, t, t, 0, 0, 0},
//                     {0, 0, t, t, t, t, t, t, t, 0, 0},
//                     {0, t, t, t, t, t, t, t, t, t, 0},
//                     {t, t, t, t, t, t, t, t, t, t, t}},
//                     palette.data(), nes::EffectInfo::Defaults());
//        } else if (entry.Direction == PointsDrawDirection::DECREASE) {
//            uint8_t t = 0x16;
//            ppux->RenderHardcodedSprite(dx, dy,
//                    {{t, t, t, t, t, t, t, t, t, t, t},
//                     {0, t, t, t, t, t, t, t, t, t, 0},
//                     {0, 0, t, t, t, t, t, t, t, 0, 0},
//                     {0, 0, 0, t, t, t, t, t, 0, 0, 0},
//                     {0, 0, 0, 0, t, t, t, 0, 0, 0, 0},
//                     {0, 0, 0, 0, 0, t, 0, 0, 0, 0, 0}},
//                     palette.data(), nes::EffectInfo::Defaults());
//        } else {
//            uint8_t t = 0x2c;
//            ppux->RenderHardcodedSprite(dx, dy,
//                    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
//                     {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
//                     {0, t, t, t, t, t, t, t, t, t, 0},
//                     {0, t, t, t, t, t, t, t, t, t, 0},
//                     {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
//                     {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
//                     palette.data(), nes::EffectInfo::Defaults());
//        }
//
//        //ppux->RenderString(w - 40, entry.Y, fmt::format("{:>3d}", comp->Points.Countdown), font.data(), tpal.data(), palette.data(), 1,
//        //        nes::EffectInfo::Defaults());
//
//        ppux->StrokeOutlineO(1.0f, nes::PALETTE_ENTRY_BLACK, palette.data());
//    }
}

////////////////////////////////////////////////////////////////////////////////


SMBCompAppAux::SMBCompAppAux(void* sharedMem)
    : m_SharedMemory(sharedMem)
    , m_Gluint(0)
{
    m_Mat = cv::Mat::zeros(1080, 1920, CV_8UC3);
    glGenTextures(1, &m_Gluint);
}

SMBCompAppAux::~SMBCompAppAux()
{
    glDeleteTextures(1, &m_Gluint);
}

bool SMBCompAppAux::OnFrame()
{
    uint8_t* b = reinterpret_cast<uint8_t*>(m_SharedMemory);
    if (b[SHARED_MEM_QUIT]) return false;
    memcpy(m_Mat.data, m_SharedMemory, SHARED_MEM_MAT);
    glBindTexture(GL_TEXTURE_2D, m_Gluint);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_Mat.cols, m_Mat.rows, 0,
            GL_BGR, GL_UNSIGNED_BYTE, m_Mat.data);

    auto* l = ImGui::GetForegroundDrawList();
    l->AddImage((void*)(intptr_t)(m_Gluint), ImVec2(0, 0), ImVec2(1920, 1080));
    return true;
}

//////////////////////////////////////////////////////////////////////////////

SMBCompCompetitionComponent::SMBCompCompetitionComponent(argos::RuntimeConfig* info, SMBComp* comp)
    : ISMBCompSingleWindowComponent("The Competition", "competition", true)
    , m_Info(info)
    , m_Competition(comp)
{
}

SMBCompCompetitionComponent::~SMBCompCompetitionComponent()
{
}

void SMBCompCompetitionComponent::DoControls()
{
    //auto* race = &m_Competition->Config.Race;
    //rgmui::InputText("display name", &race->DisplayName);
    //rgmui::InputText("score name", &race->ScoreName);
    //rgmui::InputText("file name", &race->FileName);

    //ImGui::Separator();

    if (ImGui::BeginTable("competition", 8)) {
        ImGui::TableSetupColumn("short name", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("full name (points)", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("color", ImGuiTableColumnFlags_WidthFixed);
        //ImGui::TableSetupColumn("input path (/dev/ttyUSB* or tcp://192.168.0.3:5555:seat1)", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("input path", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("action", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("console", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("recording status", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("time", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();

        std::vector<SMBCompPlayer>& players = m_Competition->Config.Players.Players;
        for (auto & player : players) {
            SMBCompFeed* feed = GetPlayerFeed(player, &m_Competition->Feeds);
            if (m_Competition->Tower.Timings.find(player.UniquePlayerID) == m_Competition->Tower.Timings.end()) {
                continue;
            }
            if (!feed) {
                continue;
            }

            ImGui::PushID(player.UniquePlayerID);
            const SMBCompPlayerTimings* timings = &m_Competition->Tower.Timings.at(player.UniquePlayerID);

            ImGui::TableNextColumn();
            ImGui::PushItemWidth(-1);
            rgmui::InputText("##a", &player.Names.ShortName);
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();
            ImGui::PushItemWidth(-1);
            rgmui::InputText("##b", &player.Names.FullName);
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();
            rgmui::InputPaletteIndex("##c", &player.Colors.RepresentativeColor,
                    m_Competition->Config.Visuals.Palette.data(),
                    nes::PALETTE_ENTRIES);

            ImGui::TableNextColumn();
            ImGui::PushItemWidth(-1);
            rgmui::InputText("##d", &player.Inputs.Serial.Path);
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();
            if (!feed->MySMBSerialProcessorThread && !feed->MySMBZMQRef) {
                if (feed->Source) {

                } else {
                    if (ImGui::Button("open")) {
                        feed->ErrorMessage = "";
                        InitializeFeedSerialThread(player, m_Competition->StaticData, feed);
                    }
                }
            } else {
                if (ImGui::Button("close")) {
                    feed->MySMBSerialProcessorThread.reset();
                    feed->MySMBZMQRef.reset();
                    feed->Source = nullptr;
                }
            }

            ImGui::TableNextColumn();
            auto out = GetLatestPlayerOutput(*m_Competition, player);
            if (out) {
                if (out->ConsolePoweredOn) {
                    rgmui::GreenText("ON");
                } else {
                    rgmui::RedText("OFF");
                }
            } else {
                ImGui::TextUnformatted("--");
            }

            ImGui::TableNextColumn();
            auto& thread = feed->MySMBSerialProcessorThread;
            if (thread) {
                std::string recordingPath;
                if (thread->IsRecording(&recordingPath)) {
                    rgmui::RedText("RECORDING");
                    //ImGui::SameLine();
                    //if (ImGui::Button("stop")) {
                    //    thread->StopRecording();
                    //}
                } else {
                    recordingPath = fmt::format("{}rec/{}_{}_{}.rec", m_Info->ArgosDirectory, util::GetTimestampNow(),
                            m_Competition->Config.Tournament.FileName,
                            player.Names.ShortName);
                    //if (ImGui::Button("start recording")) {
                    //    thread->StartRecording(recordingPath);
                    //}
                }
            } else {
                ImGui::TextUnformatted("--");
            }

            ImGui::TableNextColumn();
            std::string timetxt;
            if (TimingsToText(m_Competition, timings, player, &timetxt)) {
                rgmui::GreenText(timetxt.c_str());
            } else {
                ImGui::TextUnformatted(timetxt.c_str());
            }

            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    ImGui::Separator();
    if (ImGui::Button("1 open all")) {
        std::vector<SMBCompPlayer>& players = m_Competition->Config.Players.Players;
        for (auto & player : players) {
            SMBCompFeed* feed = GetPlayerFeed(player, &m_Competition->Feeds);
            feed->MySMBSerialProcessorThread.reset();
            feed->MySMBZMQRef.reset();
            feed->Source = nullptr;
            feed->ErrorMessage = "";
            InitializeFeedSerialThread(player, m_Competition->StaticData, feed);
        }
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("wait for all OFF");
    //if (ImGui::Button("2 start recording all")) {
    //    std::vector<SMBCompPlayer>& players = m_Competition->Config.Players.Players;
    //    for (auto & player : players) {
    //        SMBCompFeed* feed = GetPlayerFeed(player, &m_Competition->Feeds);
    //        if (!feed) continue;

    //        auto& thread = feed->MySMBSerialProcessorThread;
    //        if (!thread)  continue;

    //        if (thread->IsRecording()) {
    //            thread->StopRecording();
    //        }

    //        std::string recordingPath = fmt::format("{}rec/{}_{}_{}_{}.rec",
    //                m_Info->ArgosDirectory, util::GetTimestampNow(),
    //                m_Competition->Config.Tournament.FileName,
    //                m_Competition->Config.Tournament.CurrentRound,
    //                player.Names.ShortName);
    //        thread->StartRecording(recordingPath);
    //    }
    //}
    ImGui::SameLine();
    ImGui::TextUnformatted("all should be recording!");
    ImGui::TextUnformatted("consoles on in sequence, make sure on comes on in correct order");
    bool countdownok = true;
    {
        std::vector<SMBCompPlayer>& players = m_Competition->Config.Players.Players;
        for (auto & player : players) {
            SMBCompFeed* feed = GetPlayerFeed(player, &m_Competition->Feeds);
            if (feed) {
                if (!feed->MySMBSerialProcessorThread && !feed->MySMBZMQRef) {
                    countdownok = false;
                    break;
                }
                if (feed->MySMBSerialProcessorThread && !feed->MySMBSerialProcessorThread->IsRecording()) {
                    countdownok = false;
                    break;
                }
            } else {
                countdownok = false;
                break;
            }
        }
    }
    if (!countdownok) {
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(63, 9, 4, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(130, 46, 34, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(132, 35, 92, 255));
    }
    if (ImGui::Button("3 begin countdown")) {
        m_Competition->BeginCountdown = true;
    }
    if (!countdownok) {
        ImGui::PopStyleColor(3);
    }
    if (ImGui::Button("4 round complete")) {
        std::string resultspath = fmt::format("{}/results_{}_{}.txt",
                m_Info->ArgosDirectory,
                m_Competition->Config.Tournament.FileName,
                m_Competition->Config.Tournament.CurrentRound + 1);
        if (util::FileExists(resultspath)) {
            std::string resultspath2 = fmt::format("{}/results_{}_{}_{}.txt",
                    m_Info->ArgosDirectory,
                    m_Competition->Config.Tournament.FileName,
                    m_Competition->Config.Tournament.CurrentRound + 1,
                    util::GetTimestampNow());
            util::fs::copy(resultspath, resultspath2);
        }

        std::ofstream ofs(resultspath);
        std::cout << resultspath << std::endl;

        std::vector<SMBCompPlayer>& players = m_Competition->Config.Players.Players;
        for (auto & player : players) {
            std::string timetxt;
            const SMBCompPlayerTimings* timings = &m_Competition->Tower.Timings.at(player.UniquePlayerID);
            int64_t elapsedts = 0;
            bool end = TimingsToText(m_Competition, timings, player, &timetxt, &elapsedts);
            int64_t timems = static_cast<int64_t>(static_cast<double>(elapsedts) * nes::NTSC_MS_PER_M2);

            ofs << player.Names.ShortName << " " << timetxt << " " << timems << " " << static_cast<int>(end) << std::endl;


            SMBCompFeed* feed = GetPlayerFeed(player, &m_Competition->Feeds);
            if (!feed) continue;

            auto& thread = feed->MySMBSerialProcessorThread;
            if (!thread)  continue;

            if (thread->IsRecording()) {
                thread->StopRecording();
            }
        }
    }
    if (ImGui::Button("5 next round")) {
        ResetSMBCompTimingTower(&m_Competition->Tower);
        m_Competition->CombinedView.AID = smb::AreaID::GROUND_AREA_6;
        m_Competition->CombinedView.APX = 0;
        m_Competition->CombinedView.FramePalette = {
            0x22, 0x29, 0x1a, 0x0f,
            0x0f, 0x36, 0x17, 0x0f,
            0x0f, 0x30, 0x21, 0x0f,
            0x0f, 0x07, 0x17, 0x0f,

            0x22, 0x16, 0x27, 0x18,
            0x0f, 0x1a, 0x30, 0x27,
            0x0f, 0x16, 0x30, 0x27,
            0x0f, 0x0f, 0x0f, 0x0f,
        };
        m_Competition->SetToOverlay = true;
    }

}

bool argos::rgms::TimingsToText(SMBComp* comp, const SMBCompPlayerTimings* timings, const SMBCompPlayer& player,
        std::string* text, int64_t* elapsedt)
{
    if (!timings) {
        if (text) *text = "--:--.--";
        return false;
    }
    if (timings->State == TimingState::WAITING_FOR_1_1) {
        if (timings->SplitM2s.empty()) {
            if (text) *text = "--:--.--";
            return false;
        } else {
            if (elapsedt) *elapsedt = timings->SplitM2s.back() - timings->SplitM2s.front();

            int64_t timems = static_cast<int64_t>(static_cast<double>(timings->SplitM2s.back() -
                            timings->SplitM2s.front()) * nes::NTSC_MS_PER_M2);
            std::string time;
            if (timems >= (10 * 60 * 1000)) {
                time = util::SimpleMillisFormat(timems, util::SimpleTimeFormatFlags::MINS);
            } else {
                time = util::SimpleMillisFormat(std::round(static_cast<double>(timems / 100.0)) * 100,
                        util::SimpleTimeFormatFlags::MSCS);
                time.pop_back();
            }

            if (text) *text = time;
            return true;
        }
    } else {
        auto out = GetLatestPlayerOutput(*comp, player);
        if (out && !timings->SplitM2s.empty()) {
            if (elapsedt) *elapsedt = out->M2Count - timings->SplitM2s.front();
            std::string time = util::SimpleMillisFormat(
                    static_cast<int64_t>(static_cast<double>(out->M2Count -
                            timings->SplitM2s.front()) * nes::NTSC_MS_PER_M2),
                util::SimpleTimeFormatFlags::MSCS);
            if (text) *text = time;
            return false;
        } else {
            if (text) *text = "--:--.--";
            return false;
        }
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////
//
//SMBCompGhostViewComponent::SMBCompGhostViewComponent(argos::RuntimeConfig* info, SMBComp* comp)
//    : ISMBCompSingleWindowComponent("Ghost View", "ghostview", false)
//    , m_Info(info)
//    , m_Competition(comp)
//    , m_GhostIndex(0)
//    , m_FrameIndex(0)
//    , m_LastMiniX(0)
//{
//}
//
//SMBCompGhostViewComponent::~SMBCompGhostViewComponent()
//{
//}
//
//void SMBCompGhostViewComponent::Export(int ghostIndex)
//{
//    int startIndex = -1;
//    auto& ginfo = m_GhostInfo.at(ghostIndex);
//    for (int frameIndex = 0; frameIndex < ginfo.Outputs.size(); frameIndex++) {
//        if (ginfo.Outputs[frameIndex]->UserM2 > 0) {
//            startIndex = std::max(frameIndex - 200, 0);
//            break;
//        }
//    }
//
//    if (startIndex < 0) {
//        return;
//    }
//
//    std::string out = fmt::format("{}rec/{}_ghost", m_Info->ArgosDirectory, ginfo.Recording);
//    std::string dir = out + "/";
//    std::cout << dir << std::endl;
//    util::fs::remove_all(dir);
//    util::fs::create_directory(dir);
//
//    int lastMini = 0;
//    for (int outputIndex = startIndex; outputIndex < ginfo.Outputs.size(); outputIndex++) {
//        if (outputIndex % 1000 == 0) {
//            std::cout << outputIndex << std::endl;
//        }
//        cv::Mat m = CreateGhostFrame(ghostIndex, outputIndex, &lastMini);
//        cv::imwrite(fmt::format("{}{:07d}.png", dir, outputIndex - startIndex), m);
//    }
//
//
//    std::string cmd = fmt::format("ffmpeg -y -framerate 60 -i {}%07d.png -vcodec libx264 -crf 6 -vf format=yuv420p {}.mp4",
//            dir, out);
//
//    std::cout << cmd << std::endl;
//    int r = system(cmd.c_str());
//    std::cout << "ret: " << r << std::endl;
//
//    std::this_thread::sleep_for(std::chrono::seconds(1));
//    std::cout << "ret: " << r << std::endl;
//
//    util::fs::remove_all(dir);
//    std::cout << out << std::endl;
//
//}
//
//void SMBCompGhostViewComponent::DoControls()
//{
//    bool load = rgmui::InputText("filename", &m_FileName, ImGuiInputTextFlags_EnterReturnsTrue);
//    if (ImGui::Button("load filename") || load) {
//        int skippedRecs = 0;
//        m_GhostInfo.clear();
//
//        util::ForFileOfExtensionInDirectory(m_Info->ArgosDirectory + "rec/", ".rec", [&](util::fs::path p){
//            std::string recname = p.stem();
//
//            std::istringstream is(recname);
//
//            std::string ts, fname, shortname;
//            std::getline(is, ts, '_');
//            std::getline(is, fname, '_');
//            std::getline(is, shortname);
//
//            if (fname == m_FileName) {
//                std::cout << p.string() << std::endl;
//
//                m_GhostInfo.emplace_back();
//                auto& gi = m_GhostInfo.back();
//                bool fnd = false;
//                for (auto & player : m_Competition->Config.Players.Players) {
//                    if (player.Names.ShortName == shortname) {
//                        gi.Recording = recname;
//                        gi.RepresentativeColor = player.Colors.RepresentativeColor;
//                        gi.ShortName = shortname;
//                        gi.FullName = player.Names.FullName;
//                        fnd = true;
//                        break;
//                    }
//                }
//
//                if (!fnd) {
//                    m_GhostInfo.pop_back();
//                    std::cout << "ERROR!: Player not found with shortname: " << shortname << std::endl;
//                } else {
//                    smb::SMBSerialRecording rec(p.string(), &m_Competition->StaticData.Nametables);
//                    rec.GetAllOutputs(&gi.Outputs);
//                    std::cout << "Loaded: " << recname << std::endl;
//                }
//            } else {
//                skippedRecs++;
//            }
//
//            return true;
//
//        });
//    }
//    ImGui::SameLine();
//    if (ImGui::Button("export")) {
//        Export(m_GhostIndex);
//    }
//
//    if (ImGui::Button("export all")) {
//        for (int i = 0; i < m_GhostInfo.size(); i++) {
//            Export(i);
//        }
//    }
//
//
//
//    if (m_GhostInfo.empty()) {
//        ImGui::TextUnformatted("no ghosts");
//        return;
//    }
//
//    rgmui::SliderIntExt("index", &m_GhostIndex, 0, static_cast<int>(m_GhostInfo.size()) - 1);
//
//    if (m_GhostIndex >= 0 && m_GhostIndex < m_GhostInfo.size()) {
//        auto& gi = m_GhostInfo[m_GhostIndex];
//        rgmui::TextFmt("{} : 0x{:02x}   n: {}", gi.ShortName, gi.RepresentativeColor, gi.Outputs.size());
//
//        rgmui::SliderIntExt("frame", &m_FrameIndex, 0, static_cast<int>(gi.Outputs.size()) - 1);
//
//
//        if (m_FrameIndex >= 0 && m_FrameIndex < gi.Outputs.size()) {
//            cv::Mat m = CreateGhostFrame(m_GhostIndex, m_FrameIndex, &m_LastMiniX);
//            rgmui::Mat("gm2", m);
//        }
//    }
//}
//static void DrawController(nes::ControllerState cs, int x, int y, nes::Palette npal, nes::PPUx* ppux)
//{
//
//    std::string d = R"(
//.wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwmmmmmmwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww.
//wlllllllllllllllllllllllllllllllllllwwwwwwlllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllm
//wllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllm
//wllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllm
//wllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllm
//wllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllm
//wllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllm
//wllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllm
//wllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllm
//wllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllm
//wllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllm
//wllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllm
//wllllbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbllllm
//wllllbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbllllm
//wllllbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbllllm
//wllllbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbllllm
//wllllbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbllllm
//wllllbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbllllm
//wllllbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbllllm
//wllllbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbllllm
//wllllbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmbbbbbbrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrbbbbbbbbbllllm
//wllllbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmbbbbbbrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrbbbbbbbbbllllm
//wllllbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmbbbbbbrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrbbbbbbbbbllllm
//wllllbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbllllm
//wllllbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbllllm
//wllllbbbbbbbbbbbbbbbblllllllbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbllllm
//wllllbbbbbbbbbbbbbbbldddddddlbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbllllm
//wllllbbbbbbbbbbbbbbbld55555dlbbbbbbbbbbbbbbbbbbmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbllllm
//wllllbbbbbbbbbbbbbbbld55555dlbbbbbbbbbbbbbbbbbmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbllllm
//wllllbbbbbbbbbbbbbbbld55555dlbbbbbbbbbbbbbbbbbmmrrrrrrrrrrrrrrmmmrrrrrrrrrrrrrrmmbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbllllm
//wllllbbbbbbbbbbbbbbbld55555dlbbbbbbbbbbbbbbbbbmmrrrrrrrrrrrrrrmmmrrrrrrrrrrrrrrmmbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbllllm
//wllllbbbbbbbbbbbbbbbld55555dlbbbbbbbbbbbbbbbbbmmrrrrrrrrrrrrrrmmmrrrrrrrrrrrrrrmmbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbllllm
//wllllbbbbbbbbbbbbbbbld55555dlbbbbbbbbbbbbbbbbbmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbllllm
//wllllbbbbbbbbllllllldd55555ddlllllllbbbbbbbbbbbmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbllllm
//wllllbbbbbbblddddddddbbbbbbbddddddddlbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbllllm
//wllllbbbbbbbld7777777bbbbbbb8888888dlbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbllllm
//wllllbbbbbbbld7777777bbbbbbb8888888dlbbbbbbbbbblllllllllllllllllllllllllllllllllbbbbbbbbbddddddddddddbbbbbbddddddddddddbbbbbbbbbbbbllllm
//wllllbbbbbbbld7777777bbbbbbb8888888dlbbbbbbbbblddddddddddddddddddddddddddddddddwlbbbbbbbdlllllllllllldbbbbdlllllllllllldbbbbbbbbbbbllllm
//wllllbbbbbbbld7777777bbbbbbb8888888dlbbbbbbbbbldlllllllllllllllllllllllllllllllwlbbbbbbbdlll222222llldbbbbdlll111111llldbbbbbbbbbbbllllm
//wllllbbbbbbbld7777777bbbbbbb8888888dlbbbbbbbbbldlllllllllllllllllllllllllllllllwlbbbbbbbdll22222222lldbbbbdll11111111lldbbbbbbbbbbbllllm
//wllllbbbbbbblddddddddbbbbbbbddddddddlbbbbbbbbbldllll333333333lllll444444444llllwlbbbbbbbdl2222222222ldbbbbdl1111111111ldbbbbbbbbbbbllllm
//wllllbbbbbbbbllllllldd66666ddlllllllbbbbbbbbbbldlll33333333333lll44444444444lllwlbbbbbbbdl2222222222ldbbbbdl1111111111ldbbbbbbbbbbbllllm
//wllllbbbbbbbbbbbbbbbld66666dlbbbbbbbbbbbbbbbbbldlll33333333333lll44444444444lllwlbbbbbbbdl2222222222ldbbbbdl1111111111ldbbbbbbbbbbbllllm
//wllllbbbbbbbbbbbbbbbld66666dlbbbbbbbbbbbbbbbbbldlll33333333333lll44444444444lllwlbbbbbbbdl2222222222ldbbbbdl1111111111ldbbbbbbbbbbbllllm
//wllllbbbbbbbbbbbbbbbld66666dlbbbbbbbbbbbbbbbbbldlll33333333333lll44444444444lllwlbbbbbbbdl2222222222ldbbbbdl1111111111ldbbbbbbbbbbbllllm
//wllllbbbbbbbbbbbbbbbld66666dlbbbbbbbbbbbbbbbbbldllll333333333lllll444444444llllwlbbbbbbbdl2222222222ldbbbbdl1111111111ldbbbbbbbbbbbllllm
//wllllbbbbbbbbbbbbbbbld66666dlbbbbbbbbbbbbbbbbbldlllllllllllllllllllllllllllllllwlbbbbbbbdll22222222lldbbbbdll11111111lldbbbbbbbbbbbllllm
//wllllbbbbbbbbbbbbbbbld66666dlbbbbbbbbbbbbbbbbbldlllllllllllllllllllllllllllllllwlbbbbbbbdlll222222llldbbbbdlll111111llldbbbbbbbbbbbllllm
//wllllbbbbbbbbbbbbbbbldddddddlbbbbbbbbbbbbbbbbblwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwlbbbbbbbdlllllllllllldbbbbdlllllllllllldbbbbbbbbbbbllllm
//wllllbbbbbbbbbbbbbbbblllllllbbbbbbbbbbbbbbbbbbblllllllllllllllllllllllllllllllllbbbbbbbbbddddddddddddbbbbbbddddddddddddbbbbbbbbbbbbllllm
//wllllbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbllllm
//wllllbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbrrrbbbbbbbbbbbbbbbrrrbbbbbbbbbbbbllllm
//wllllbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmbbbbbbbbbbbbbbbbbbrrrbbbbbbbbbbbbbbbrrrbbbbbbbbbbbbllllm
//wllllbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmbbbbbbbbbbbbbbbbbrrrbbbbbbbbbbbbbbbrrrbbbbbbbbbbbbllllm
//wllllbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbllllm
//wllllbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbllllm
//wllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllm
//wllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllm
//wllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllm
//wllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllm
//.mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm.
//)";
//
//    int w = 136;
//    int h = 60;
//
//    int tx = x;
//    int ty = y - 1;
//    for (auto & c : d) {
//        if (c == '\n') {
//            ty++;
//            tx = x;
//            continue;
//        }
//
//        uint8_t t = 0x00;
//        if (c == 'w') t = 0x20;
//        if (c == 'r') t = 0x16;
//        if (c == 'l') t = 0x10;
//        if (c == 'm') t = 0x00;
//        if (c == 'd') t = 0x2d;
//        if (c == 'b') t = 0x0f;
//
//        if (c == '1') {
//            if (cs & 0b00000001) {
//                t = 0x21;
//            } else {
//                t = 0x16;
//            }
//        }
//        if (c == '2') {
//            if (cs & 0b00000010) {
//                t = 0x21;
//            } else {
//                t = 0x16;
//            }
//        }
//        if (c == '3') {
//            if (cs & 0b00000100) {
//                t = 0x21;
//            } else {
//                t = 0xf;
//            }
//        }
//        if (c == '4') {
//            if (cs & 0b00001000) {
//                t = 0x21;
//            } else {
//                t = 0xf;
//            }
//        }
//        if (c == '5') {
//            if (cs & 0b00010000) {
//                t = 0x21;
//            } else {
//                t = 0x0f;
//            }
//        }
//        if (c == '6') {
//            if (cs & 0b00100000) {
//                t = 0x21;
//            } else {
//                t = 0x0f;
//            }
//        }
//        if (c == '7') {
//            if (cs & 0b01000000) {
//                t = 0x21;
//            } else {
//                t = 0x0f;
//            }
//        }
//        if (c == '8') {
//            if (cs & 0b10000000) {
//                t = 0x21;
//            } else {
//                t = 0x0f;
//            }
//        }
//
//        if (c != '.') {
//            ppux->RenderPaletteData(tx, ty, 1, 1, &t, npal.data(),
//                    nes::PPUx::RenderPaletteDataFlags::RPD_PLACE_PIXELS_DIRECT,
//                    nes::EffectInfo::Defaults());
//        }
//        tx++;
//    }
//
//    //ppux->DrawBorderedBox(x, y, w, h, {0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f}, npal.data(), 0);
//
//
//}
//
//static cv::Mat OutToIndividualFrame(
//        SMBMessageProcessorOutputPtr out,
//        std::vector<SMBMessageProcessorOutputPtr>* otherOuts,
//        const smb::SMBBackgroundNametables& nametables,
//        const nes::PatternTable& font,
//        std::string name)
//{
//    nes::PPUx ppux1(256, 240, nes::PPUxPriorityStatus::ENABLED);
//
//    rgms::RenderSMBToPPUX(out->Frame, out->FramePalette, nametables, &ppux1);
//    cv::Mat m(ppux1.GetHeight(), ppux1.GetWidth(), CV_8UC3, ppux1.GetBGROut());
//
//    if (otherOuts && out->Frame.GameEngineSubroutine != 0x00) {
//        nes::PPUx ppuxq(256, 240, nes::PPUxPriorityStatus::ENABLED);
//        cv::Mat q(ppuxq.GetHeight(), ppuxq.GetWidth(), CV_8UC3, ppuxq.GetBGROut());
//        auto& nesPalette = nes::DefaultPaletteBGR();
//
//        nes::RenderInfo render;
//        render.OffX = 0;
//        render.OffY = 0;
//        render.Scale = 1;
//        render.PatternTables.push_back(rgms::smb::rom::chr0);
//        render.PaletteBGR = nesPalette.data();
//
//        std::vector<nes::OAMxEntry> oamx = out->Frame.OAMX;
//        out->Frame.OAMX.clear();
//        rgms::RenderSMBToPPUX(out->Frame, out->FramePalette, nametables, &ppuxq);
//        ppux1.ResetPriority();
//        rgms::RenderSMBToPPUX(out->Frame, out->FramePalette, nametables, &ppux1);
//        out->Frame.OAMX = oamx;
//
//        for (auto & oo : *otherOuts) {
//            if (oo->ConsolePoweredOn &&
//                oo->Frame.GameEngineSubroutine != 0x00 &&
//                oo->Frame.AP == out->Frame.AP) {
//
//                int dx = oo->Frame.APX - out->Frame.APX;
//                for (auto oamx : oo->Frame.OAMX) {
//                    oamx.X += dx;
//                    if (oamx.X > -8 && oamx.X < 256) {
//                        ppuxq.RenderOAMxEntry(oamx, render, nes::EffectInfo::Defaults());
//                    }
//                }
//            }
//        }
//
//        cv::addWeighted(m, 0.7f, q, 0.3f, 0.0, m);
//        for (auto & oamx : out->Frame.OAMX) {
//            ppux1.RenderOAMxEntry(oamx, render, nes::EffectInfo::Defaults());
//        }
//    }
//
//    cv::resize(m, m, {}, 2, 2, cv::INTER_NEAREST);
//
//
//    cv::Mat pre = m.clone();
//    nes::PPUx ppux2(512, 480, m.data, nes::PPUxPriorityStatus::ENABLED);
//
//    auto& nesPalette = nes::DefaultPaletteBGR();
//    auto DrawMyBorderedText = [&](int x, int y, bool left, const std::string text){
//        int w = text.size() * 16 + 10;
//
//        nes::PPUx ppuxt(text.size() * 8 + 2, 8 + 2, nes::PPUxPriorityStatus::ENABLED);
//        ppuxt.FillBackground(0x17, nesPalette.data());
//        ppuxt.BeginOutline();
//        std::array<uint8_t, 4> tpal = {0x00, nes::PALETTE_ENTRY_WHITE, 0x20, 0x20};
//        ppuxt.RenderString(1, 1, text, font.data(), tpal.data(), nesPalette.data(), 1, nes::EffectInfo::Defaults());
//        ppuxt.StrokeOutlineO(1.0f, nes::PALETTE_ENTRY_BLACK, nesPalette.data());
//
//        cv::Mat m3(ppuxt.GetHeight(), ppuxt.GetWidth(), CV_8UC3, ppuxt.GetBGROut());
//        cv::resize(m3, m3, {}, 2.0, 3.0, cv::INTER_NEAREST);
//
//
//        if (!left) x -= w;
//        ppux2.DrawBorderedBox(x, y, w, 38, {0x36, 0x36, 0x36,  0x36, 0x17, 0x0f,  0x0f, 0x0f, 0x0f}, nesPalette.data(), 2);
//
//        cv::Mat m2(ppux2.GetHeight(), ppux2.GetWidth(), CV_8UC3, ppux2.GetBGROut());
//        m3.copyTo(m2(cv::Rect(x + 4, y + 6, m3.cols, m3.rows)));
//    };
//
//    int by = 480-38-8;
//
//    DrawMyBorderedText(256 - (name.size() * 16 + 10) / 2, by, true, name);
//    std::string time;
//    int64_t timems = static_cast<int64_t>(std::round(static_cast<double>(out->UserM2) * nes::NTSC_MS_PER_M2));
//    if (timems >= (10 * 60 * 1000)) {
//        time = util::SimpleMillisFormat(timems, util::SimpleTimeFormatFlags::MINS);
//    } else if (timems > (99 * 60 * 1000) || timems < 0) {
//        time = "-:--.-";
//    } else {
//        time = util::SimpleMillisFormat(std::round(static_cast<double>(timems / 100.0)) * 100,
//                util::SimpleTimeFormatFlags::MSCS);
//        time.pop_back();
//    }
//    if (time.size() > 11) {
//        time = "-:--.-";
//    }
//    DrawMyBorderedText(512-8, by, false, time);
//
//    cv::Mat m2(ppux2.GetHeight(), ppux2.GetWidth(), CV_8UC3, ppux2.GetBGROut());
//
//    cv::resize(m2, m2, {}, 2, 2, cv::INTER_NEAREST);
//
//    nes::PPUx ppux3(m2.cols, m2.rows, m2.data, nes::PPUxPriorityStatus::ENABLED);
//
//    ppux3.DrawBorderedBox(16, by * 2, 152, 76,
//        {0x36, 0x36, 0x36,  0x36, 0x17, 0x0f,  0x0f, 0x0f, 0x0f}, nesPalette.data(), 4);
//
//    ppux3.ResetPriority();
//    DrawController(out->Controller, 24, by*2 + 8, nesPalette, &ppux3);
//
//    cv::resize(pre, pre, {}, 2, 2, cv::INTER_NEAREST);
//    cv::addWeighted(pre, 1.0f - 0.9f, m2, 0.9f, 0.0, m2);
//
//    return m2;
//}
//cv::Mat SMBCompGhostViewComponent::CreateGhostFrameExt(
//        const std::vector<GhostInfo>& ghostInfo, SMBComp* comp,
//        int ghostIndex, int frameIndex, int* lastMiniX)
//{
//    auto out = ghostInfo.at(ghostIndex).Outputs.at(frameIndex);
//
//    std::vector<SMBMessageProcessorOutputPtr> otherOuts;
//    std::vector<GhostInfo> otherGhosts;
//    uint8_t thisColor = 0x00;
//    std::string thisShortName = "error";
//
//    if (out->UserM2 > 0) {
//        int gi = 0;
//        for (auto & info : ghostInfo) {
//            if (gi != ghostIndex) {
//                auto& outs = info.Outputs;
//                auto it = std::upper_bound(outs.begin(), outs.end(), out->UserM2,
//                        [&](uint64_t value, const SMBMessageProcessorOutputPtr& element){
//                            return value < element->UserM2;
//                        });
//                if (it != outs.end()) {
//                    if (it != outs.begin()) {
//                        if ((out->UserM2 - (*(it - 1))->UserM2) < ((*it)->UserM2 - out->UserM2)) {
//                            it--;
//                        }
//                    }
//
//                    if (it != outs.end()) {
//                        otherOuts.push_back(*it);
//                        otherGhosts.emplace_back();
//                        otherGhosts.back().RepresentativeColor = info.RepresentativeColor;
//                        otherGhosts.back().ShortName = info.ShortName;
//                        otherGhosts.back().Outputs.push_back(*it);
//                    }
//                }
//            } else {
//                thisColor = info.RepresentativeColor;
//                thisShortName = info.ShortName;
//            }
//            gi++;
//        }
//    }
//
//    cv::Mat top = OutToIndividualFrame(out, &otherOuts, comp->StaticData.Nametables, comp->StaticData.Font,
//            ghostInfo.at(ghostIndex).FullName);
//
//
//    cv::Mat m = cv::Mat::zeros(240 * 5, 256 * 4, CV_8UC3);
//    top.copyTo(m(cv::Rect(0, 0, 256*4, 240*4)));
//
//////////////////////////////////////////////////////////////////////////////////
//
//
//    nes::PPUx mini(256*4, 240, nes::PPUxPriorityStatus::ENABLED);
//    mini.ResetPriority();
//    mini.FillBackground(nes::PALETTE_ENTRY_WHITE, comp->Config.Visuals.Palette.data());
//
//    const smb::SMBRaceCategoryInfo* catInfo = comp->StaticData.Categories.FindCategory(comp->Config.Tournament.Category);
//    const auto& route = m_Competition->StaticData.Categories.Routes.at( m_Competition->Config.Tournament.Category);
//
//
//    int minix = 0;
//    int miniw = 256*4;
//    if (lastMiniX) minix = *lastMiniX;
//    if (out->ConsolePoweredOn && out->Frame.GameEngineSubroutine != 0x00) {
//        int outx;
//        if (catInfo->InCategory(out->Frame.AP, out->Frame.APX, out->Frame.World, out->Frame.Level, &outx, nullptr)) {
//            minix = std::max(outx - (256+128), 0);
//            minix = std::min(catInfo->TotalWidth - miniw, minix);
//            if (lastMiniX) *lastMiniX = minix;
//        }
//    }
//
//
//    std::vector<smb::VisibleSection> visibleSections;
//    catInfo->RenderMinimapTo(&mini, minix,
//            smb::DefaultMinimapPalette(),
//            comp->StaticData.Nametables, &visibleSections);
//
//    nes::RenderInfo render = DefaultSMBCompRenderInfo(*comp);
//
//    // Add World Level Text
//    {
//        std::array<uint8_t, 4> tpal = {0x00, nes::PALETTE_ENTRY_WHITE, 0x20, 0x20};
//        std::string lastLevel = "";
//        for (size_t i = 0; i < visibleSections.size(); i++) {
//            auto& vsec = visibleSections[i];
//
//            std::string thisLevel = fmt::format("{}-{}", vsec.Section->World, vsec.Section->Level);
//            if (thisLevel == lastLevel) continue;
//
//
//            // Adjust text position to look nice
//            int tx = vsec.PPUxLoc;
//            if (i == 0 && tx < 16) {
//                int lastLevel = 0;
//                for (auto & sec : catInfo->Sections) {
//                    if (&sec == vsec.Section) break;
//                    lastLevel = sec.Level;
//                }
//                if (lastLevel == vsec.Section->Level) {
//                    tx = 4;
//                }
//            }
//
//            if (tx < 4) {
//                tx = 4;
//            }
//            int tw = 3 * 16;
//            if (i != visibleSections.size() - 1) {
//                if (visibleSections[i + 1].Section->Level != vsec.Section->Level) {
//                    int endx = visibleSections[i + 1].PPUxLoc - 16;
//                    if ((tx + tw) > endx) {
//                        tx = endx - tw;
//                    }
//                }
//            }
//
//            mini.BeginOutline();
//            mini.RenderString(tx, 4, thisLevel,
//                    comp->StaticData.Font.data(), tpal.data(), render.PaletteBGR, 2,
//                    nes::EffectInfo::Defaults());
//            mini.StrokeOutlineO(2.0f, nes::PALETTE_ENTRY_BLACK, render.PaletteBGR);
//
//            lastLevel = thisLevel;
//        }
//    }
//
//    std::sort(otherGhosts.begin(), otherGhosts.end(), [](const auto& l, const auto& r){
//        return l.ShortName < r.ShortName;
//    });
//    std::mt19937 gen(out->Frame.AP); // seed on ap so it changes between section kinda..
//    std::shuffle(otherGhosts.begin(), otherGhosts.end(), gen);
//
//    if (out->ConsolePoweredOn && out->Frame.GameEngineSubroutine != 0x00) {
//        otherGhosts.emplace(otherGhosts.begin());
//        otherGhosts.front().RepresentativeColor = thisColor;
//        otherGhosts.front().ShortName = thisShortName;
//        otherGhosts.front().Outputs.push_back(out);
//    }
//
//    // Now the players
//    for (auto & info : otherGhosts) {
//        std::array<uint8_t, 4> tpal = {0x00, info.RepresentativeColor,
//            info.RepresentativeColor,
//            info.RepresentativeColor};
//
//        auto& out = info.Outputs.back();
//        if (out->Frame.GameEngineSubroutine == 0x00) {
//            continue;
//        }
//
//        int categoryX = 0;
//        int sectionIndex = 0;
//        if (!catInfo->InCategory(out->Frame.AP, out->Frame.APX, out->Frame.World, out->Frame.Level, &categoryX, &sectionIndex)) continue;
//        if (comp->Config.Tournament.Category == smb::RaceCategory::WARPLESS &&
//                sectionIndex == 5 && out->Frame.GameEngineSubroutine == 0x05 && out->Frame.OperMode == 0x01) {
//            continue;
//        }
//
//        int mariox = 0, marioy = 0;
//        if (!MarioInOutput(out, &mariox, &marioy)) {
//            continue;
//        }
//
//        int ppuX = categoryX - minix + mariox;
//        auto* sec = &catInfo->Sections.at(sectionIndex);
//        nes::EffectInfo effects = nes::EffectInfo::Defaults();
//        for (auto & vsec : visibleSections) {
//            if (vsec.Section == sec) {
//                effects.CropWithin = true;
//                effects.Crop.X = vsec.PPUxLoc;
//                effects.Crop.Y = 0;
//                effects.Crop.Width = vsec.Section->Right - vsec.Section->Left;
//                effects.Crop.Height = 240;
//                break;
//            }
//        }
//
//        mini.BeginOutline();
//        // Render mario
//        for (auto oamx : out->Frame.OAMX) {
//            if (smb::IsMarioTile(oamx.TileIndex)) {
//                oamx.X += (ppuX - mariox);
//
//                oamx.TilePalette[0] = 0x00;
//                oamx.TilePalette[1] = info.RepresentativeColor;
//                oamx.TilePalette[2] = info.RepresentativeColor;
//                oamx.TilePalette[3] = info.RepresentativeColor;
//
//                mini.RenderOAMxEntry(oamx, render, effects);
//            }
//        }
//
//        // Render triangles
//        if (ppuX < 0) {
//            int trix = -8 - ppuX;
//            if (trix > 4) {
//                trix = 4;
//            }
//            RenderLeftTriangle(&mini, info.RepresentativeColor, render.PaletteBGR, trix, marioy);
//        }
//        if (ppuX > (miniw - 16)) {
//            int trix = miniw - (ppuX - (miniw - 16));
//            if (trix < (miniw - 12)) {
//                trix = miniw - 12;
//            }
//            RenderRightTriangle(&mini, info.RepresentativeColor, render.PaletteBGR, trix, marioy);
//        }
//
//        { // Render player name
//            int len = info.ShortName.size();
//            int textx = ppuX + (len - 1) * -8;
//            if (textx < 4) {
//                textx = 4;
//            }
//            int textw = len * 16;
//            if ((textx + textw) > (miniw - 4)) {
//                textx = miniw - 4 - textw;
//            }
//            effects.CropWithin = false;
//            mini.RenderString(textx, marioy - 20, info.ShortName,
//                comp->StaticData.Font.data(), tpal.data(),
//                render.PaletteBGR, 2, effects);
//        }
//
//        uint8_t outline = nes::PALETTE_ENTRY_BLACK;
//        mini.StrokeOutlineO(1.0f, outline, render.PaletteBGR);
//    }
//
//    cv::Mat bot(mini.GetHeight(), mini.GetWidth(), CV_8UC3, mini.GetBGROut());
//    bot.copyTo(m(cv::Rect(0, 240*4, 256*4, 240)));
//    return m;
//}
//
//cv::Mat SMBCompGhostViewComponent::CreateGhostFrame(int ghostIndex, int frameIndex, int* lastMiniX)
//{
//    return CreateGhostFrameExt(m_GhostInfo, m_Competition, ghostIndex, frameIndex, lastMiniX);
//}
//
//////////////////////////////////////////////////////////////////////////////////
//
//SMBCompPointTransitionComponent::SMBCompPointTransitionComponent(argos::RuntimeConfig* info, SMBComp* comp)
//    : ISMBCompSingleWindowComponent("Points Transition", "pointstransition", false)
//    , m_Info(info)
//    , m_Competition(comp)
//    , m_Points(true)
//    , m_CountdownOverride(0)
//{
//}
//
//SMBCompPointTransitionComponent::~SMBCompPointTransitionComponent()
//{
//}
//
//void SMBCompPointTransitionComponent::DoControls()
//{
//    int w, h;
//    DrawPointsSize(m_Competition, &w, &h);
//    ImGui::Checkbox("points", &m_Points);
//
//    rgmui::SliderIntExt("cntdown", &m_CountdownOverride, 0, POINTS_COUNTDOWN_MAX);
//
//    nes::PPUx ppux(w, h, nes::PPUxPriorityStatus::ENABLED);
//    if (m_Points) {
//        int cnt = m_Competition->Points.Countdown;;
//        m_Competition->Points.Countdown = m_CountdownOverride;
//        DrawPoints(&ppux, 0, 0, m_Competition->Config.Visuals.Palette, m_Competition->StaticData.Font, m_Competition);
//        m_Competition->Points.Countdown = cnt;
//    } else {
//        DrawFinalTimes(&ppux, 0, 0, m_Competition->Config.Visuals.Palette, m_Competition->StaticData.Font, m_Competition);
//    }
//
//    cv::Mat m(ppux.GetHeight(), ppux.GetWidth(), CV_8UC3, ppux.GetBGROut());
//    cv::resize(m, m, {}, 2, 4, cv::INTER_NEAREST);
//    rgmui::Mat("ppp", m);
//
//    if (ImGui::Button("save times")) {
//        std::string pth = fmt::format("{}rec/{}_time.png", m_Info->ArgosDirectory, m_Competition->Config.Tournament.FileName);
//        cv::imwrite(pth, m);
//    }
//
//    if (ImGui::Button("set begin")) {
//        m_Begin = m;
//    }
//    if (ImGui::Button("save points transition")) {
//        std::string out = fmt::format("{}rec/{}_points", m_Info->ArgosDirectory, m_Competition->Config.Tournament.FileName);
//        std::string dir = out + "/";
//
//
//        util::fs::remove_all(dir);
//        util::fs::create_directory(dir);
//
//        int f = 0;
//        for (int i = 0; i < 60; i++) {
//            cv::imwrite(fmt::format("{}{:07d}.png", dir, f++), m_Begin);
//        }
//
//        for (int i = POINTS_COUNTDOWN_MAX; i >= -60; i--) {
//            nes::PPUx ppux2(w, h, nes::PPUxPriorityStatus::ENABLED);
//            int cnt = m_Competition->Points.Countdown;;
//            m_Competition->Points.Countdown = std::max(i, 0);
//            DrawPoints(&ppux2, 0, 0, m_Competition->Config.Visuals.Palette, m_Competition->StaticData.Font, m_Competition);
//            m_Competition->Points.Countdown = cnt;
//
//            cv::Mat m2(ppux2.GetHeight(), ppux2.GetWidth(), CV_8UC3, ppux2.GetBGROut());
//            cv::resize(m2, m2, {}, 2, 4, cv::INTER_NEAREST);
//
//            cv::imwrite(fmt::format("{}{:07d}.png", dir, f++), m2);
//        }
//
//        std::string cmd = fmt::format("ffmpeg -y -framerate 60 -i {}%07d.png -vcodec libx264 -crf 6 -vf format=yuv420p {}.mp4",
//                dir, out);
//        int r = system(cmd.c_str());
//        std::cout << "ret: " << r << std::endl;
//
//
//
//    }
//
//
//    ImGui::Separator();
//
//    if (ImGui::CollapsingHeader("final times")) {
//        const std::vector<SMBCompPlayer>& players = m_Competition->Config.Players.Players;
//        for (auto & player : players) {
//            ImGui::PushID(player.UniquePlayerID);
//            auto it = m_FakeM2s.find(player.UniquePlayerID);
//            if (it == m_FakeM2s.end()) {
//                if (ImGui::Button(fmt::format("fake for {}", player.Names.ShortName).c_str())) {
//                    m_FakeM2s[player.UniquePlayerID] = 0;
//                }
//            } else {
//                if (ImGui::Button(fmt::format("stop {}", player.Names.ShortName).c_str())) {
//                    m_FakeM2s.erase(player.UniquePlayerID);
//
//                    m_Competition->Tower.Timings[player.UniquePlayerID].SplitM2s.clear();
//                    m_Competition->Tower.Timings[player.UniquePlayerID].SplitPageM2s.clear();
//                }
//                ImGui::SameLine();
//
//                uint64_t v = static_cast<uint64_t>(it->second);
//                if (ImGui::InputScalar("v", ImGuiDataType_U64, reinterpret_cast<void*>(&v))) {
//                    m_FakeM2s[player.UniquePlayerID] = v;
//                    m_Competition->Tower.Timings[player.UniquePlayerID].SplitM2s.resize(2);
//                    m_Competition->Tower.Timings[player.UniquePlayerID].SplitM2s[0] = 0;
//                    m_Competition->Tower.Timings[player.UniquePlayerID].SplitM2s[1] = v;
//                    m_Competition->Tower.Timings[player.UniquePlayerID].SplitPageM2s.resize(2);
//                    m_Competition->Tower.Timings[player.UniquePlayerID].SplitPageM2s[0] = {0};
//                    m_Competition->Tower.Timings[player.UniquePlayerID].SplitPageM2s[1] = {v};
//                }
//
//            }
//            ImGui::PopID();
//        }
//    }
//}
//
//static void InitRecreatePlayers(std::string ArgosDirectory, std::vector<RecreatePlayerInfo>* players)
//{
//    std::string data = ArgosDirectory + "recreate.json";
//    std::ifstream ifs(data);
//    nlohmann::json j;
//    ifs >> j;
//    (*players) = j;
//
//    nes::StateSequenceThreadConfig config = nes::StateSequenceThreadConfig::Defaults();
//
//    for (auto & player : *players) {
//        std::cout << player.Name <<  " " << player.PTSOffset << " " << player.Inputs.size() << std::endl;
//
//        std::unique_ptr<nes::NestopiaNESEmulator> emu = std::make_unique<nes::NestopiaNESEmulator>();
//        emu->LoadINESData(smb::rom::rom, smb::rom::size);
//        player.Emulator = std::make_unique<nes::NestopiaNESEmulator>();
//        player.Emulator->LoadINESData(smb::rom::rom, smb::rom::size);
//        player.StateThread = std::make_unique<nes::StateSequenceThread>(config, std::move(emu));
//
//        player.StateThread->InputsChange(player.Inputs);
//
//        player.InputsOptions = nesui::NESInputsComponentOptions::Defaults();
//        player.InputsState.Reset();
//        player.InputsState.OnInputChangeCallback = [&](int inputIndex, nes::ControllerState cs){
//            player.StateThread->InputChange(inputIndex, cs);
//        };
//    }
//}
//
//RecreateApp::RecreateApp(argos::RuntimeConfig* info)
//    : m_Info(info)
//    , m_PTS(0)
//{
//    InitializeSMBComp(info, &m_Competition);
//    video::StaticVideoThreadConfig cfg = video::StaticVideoThreadConfig::Defaults();
//    cfg.StaticVideoBufferCfg.BufferSize = static_cast<size_t>(1324) * static_cast<size_t>(1024 * 1024);
//    m_VideoThread = std::make_unique<video::StaticVideoThread>(std::make_unique<video::CVVideoCaptureSource>(
//                info->ArgosDirectory + "recreate.mp4"), cfg);
//
//    //m_Players.emplace_back();
//    //m_Players.back().Name = "kosmic";
//    //m_Players.back().PTSOffset = 0;
//    //m_Players.emplace_back();
//    //m_Players.back().Name = "sprsonic";
//    //m_Players.back().PTSOffset = 0;
//    //m_Players.emplace_back();
//    //m_Players.back().Name = "kriller37";
//    //m_Players.back().PTSOffset = 0;
//    //m_Players.emplace_back();
//    //m_Players.back().Name = "roopert83";
//    //m_Players.back().PTSOffset = 0;
//    //m_Players.emplace_back();
//    //m_Players.back().Name = "scalpel";
//    //m_Players.back().PTSOffset = 0;
//    //m_Players.emplace_back();
//    //m_Players.back().Name = "niftski";
//    //m_Players.back().PTSOffset = 0;
//    //m_Players.emplace_back();
//    //m_Players.back().Name = "somewes";
//    //m_Players.back().PTSOffset = 0;
//    //m_Players.emplace_back();
//    //m_Players.back().Name = "jeremy";
//    //m_Players.back().PTSOffset = 0;
//    m_ActivePlayer = nullptr;
//    InitRecreatePlayers(m_Info->ArgosDirectory, &m_Players);
//    if (!m_Players.empty()) {
//        m_ActivePlayer = &m_Players.back();
//    }
//}
//
//RecreateApp::~RecreateApp()
//{
//    std::string data = m_Info->ArgosDirectory + "recreate.json";
//    nlohmann::json j(m_Players);
//
//    std::ofstream ofs(data);
//    ofs << j << std::endl;
//}
//
//#define MAX_PTS 360000
//
//std::pair<int, int> RecreateApp::FindPreviousJump(RecreatePlayerInfo* player)
//{
//    bool foundEnd = false;
//    int start = 0;
//    int end = 0;
//    for (int i = player->InputsState.TargetIndex; i >= 0; i--) {
//        if (!foundEnd && player->Inputs[i] & nes::Button::A) {
//            end = i;
//            foundEnd = true;
//        } else if (foundEnd && !(player->Inputs[i] & nes::Button::A)) {
//            start = i + 1;
//            break;
//        }
//    }
//    if (foundEnd) {
//        while (end < (player->Inputs.size()) && player->Inputs[end] & nes::Button::A) {
//            end++;
//        }
//    }
//    return std::make_pair(start, end);
//}
//RecreateApp::InputAction RecreateApp::GetActionFromKeysPressed()
//{
//    InputAction action = InputAction::NO_ACTON;
//    if (rgmui::ShiftIsDown()) {
//        if (ImGui::IsKeyPressed(ImGuiKey_2, false) || ImGui::IsKeyPressed(ImGuiKey_4, false)) {
//            action = InputAction::SMB_REMOVE_LAST_JUMP;
//        } else if (ImGui::IsKeyPressed(ImGuiKey_3)) {
//            action = InputAction::SMB_FULL_JUMP;
//        }
//    } else if (ImGui::IsKeyPressed(ImGuiKey_1)) {
//        action = InputAction::SMB_JUMP_EARLIER;
//    } else if (ImGui::IsKeyPressed(ImGuiKey_2)) {
//        action = InputAction::SMB_JUMP_LATER;
//    } else if (ImGui::IsKeyPressed(ImGuiKey_3)) {
//        action = InputAction::SMB_JUMP;
//    } else if (ImGui::IsKeyPressed(ImGuiKey_4)) {
//        action = InputAction::SMB_JUMP_SHORTER;
//    } else if (ImGui::IsKeyPressed(ImGuiKey_5)) {
//        action = InputAction::SMB_JUMP_LONGER;
//    } else if (ImGui::IsKeyPressed(ImGuiKey_T)) {
//        action = InputAction::SMB_START;
//    }
//
//    return action;
//}
//
//void RecreateApp::DoAction(RecreatePlayerInfo* player, RecreateApp::InputAction action)
//{
//    int target = player->InputsState.TargetIndex;
//    auto ChangeInputTo = [&](int index, nes::ControllerState newState) {
//        player->InputsState.SetState(index, &player->Inputs, newState);
//    };
//    switch(action) {
//        case InputAction::SMB_JUMP_EARLIER: {
//            auto [from, to] = FindPreviousJump(player);
//            if (from != to && from > 0) {
//                ChangeInputTo(from - 1, player->Inputs[from - 1] | nes::Button::A);
//            }
//            break;
//        }
//        case InputAction::SMB_JUMP_LATER: {
//            auto [from, to] = FindPreviousJump(player);
//            if (from != to && to > (from + 1)) {
//                ChangeInputTo(from, player->Inputs[from] & ~nes::Button::A);
//            }
//            break;
//        }
//        case InputAction::SMB_JUMP: {
//            if (target >= 2) {
//                if (player->Inputs[target - 2] & nes::Button::A) {
//                    auto [from, to] = FindPreviousJump(player);
//                    ChangeInputTo(to, player->Inputs[to] | nes::Button::A);
//                } else {
//                    ChangeInputTo(target - 2, player->Inputs[target - 2] | nes::Button::A);
//                }
//            }
//            break;
//        }
//        case InputAction::SMB_JUMP_SHORTER: {
//            auto [from, to] = FindPreviousJump(player);
//            if (from != to && to > 0 && to > (from + 1)) {
//                ChangeInputTo(to - 1, player->Inputs[to - 1] & ~nes::Button::A);
//            }
//            break;
//        }
//        case InputAction::SMB_JUMP_LONGER: {
//            auto [from, to] = FindPreviousJump(player);
//            if (from != to) {
//                ChangeInputTo(to, player->Inputs[to] | nes::Button::A);
//            }
//            break;
//        }
//        case InputAction::SMB_START: {
//            if (target >= 2) {
//                int t = target - 2;
//                if (!(player->Inputs[t] & nes::Button::START)) {
//                    ChangeInputTo(t, player->Inputs[t] | nes::Button::START);
//                }
//            }
//            break;
//        }
//        case InputAction::SMB_REMOVE_LAST_JUMP: {
//            auto [from, to] = FindPreviousJump(player);
//            if (from != to) {
//                for (int i = from; i < to; i++) {
//                    ChangeInputTo(i, player->Inputs[i] & ~nes::Button::A);
//                }
//                //ConsolidateLast(to - from); TODO undo/redo
//            }
//            break;
//        }
//        case InputAction::SMB_FULL_JUMP: {
//            if (target >= 2 && player->Inputs.size() > (target + 36)) {
//                // This would be properly based on the subspeed in $0057,
//                // if frame s - 1, ram[$0057] <= 15 or >= 25:
//                //   jump 32
//                // else:
//                //   jump 35
//                // (according to slither)
//                // But I don't really have access to the emulator here
//                int s = target - 2;
//                if (player->Inputs[s] & nes::Button::A) {
//                    auto [from, to] = FindPreviousJump(player);
//                    s = from;
//                }
//
//                int nchanges = 0;
//                for (int i = 0; i < 35; i++) {
//                    if (!(player->Inputs[s + i] & nes::Button::A)) {
//                        nchanges++;
//                        ChangeInputTo(s + i, player->Inputs[s + i] | nes::Button::A);
//                    }
//                }
//                //m_UndoRedo.ConsolidateLast(nchanges); TODO undo/redo
//            }
//            break;
//        }
//        default:
//            break;
//    }
//}
//
//void RecreateApp::DoPlayerControls(RecreatePlayerInfo* player, int64_t* pts)
//{
//    int64_t targetms = *pts + player->PTSOffset;
//    int64_t targetframe = std::round(static_cast<double>(targetms) * nes::NTSC_FPS / 1000);
//    bool ignore = false;
//    if (targetframe < 0) {
//        ignore = true;
//        targetframe = 0;
//    }
//    player->InputsState.TargetIndex = targetframe;
//    player->StateThread->TargetChange(player->InputsState.TargetIndex);
//
//
//    std::string state;
//    if (player->StateThread->HasNewState(&player->InputsState.EmulatorIndex, &state) && !state.empty()) {
//        player->Emulator->LoadStateString(state);
//    }
//
//    int v = static_cast<int>(player->PTSOffset);
//    if (ImGui::InputInt("offset", &v)) {
//        player->PTSOffset = v;
//    }
//
//    nes::Frame frame;
//    player->Emulator->ScreenPeekFrame(&frame);
//    cv::Mat m = opencvext::ConstructPaletteImage(
//            frame.data(), nes::FRAME_WIDTH, nes::FRAME_HEIGHT, nes::DefaultPaletteBGR().data(),
//            opencvext::PaletteDataOrder::BGR);
//    //cv::resize(m, m, {}, 2, 2, cv::INTER_NEAREST);
//    rgmui::MatAnnotator anno("m", m);
//    if (anno.IsHovered(false)) {
//        auto& io = ImGui::GetIO();
//        if (io.MouseWheel != 0) {
//            *pts -= io.MouseWheel * 18;
//        }
//    }
//
//    int64_t pretarget = player->InputsState.TargetIndex;
//    if (nesui::NESInputsComponent::Controls(&player->Inputs, &player->InputsOptions, &player->InputsState)) {
//        m_ActivePlayer = player;
//    }
//    if (player->InputsState.TargetIndex != pretarget) {
//        *pts = player->InputsState.TargetIndex / nes::NTSC_FPS * 1000 - player->PTSOffset;
//    }
//}
//
//bool RecreateApp::OnFrame()
//{
//    if (ImGui::Begin("vid")) {
//        rgmui::TextFmt("{}: {}", m_VideoThread->HasError(), m_VideoThread->GetError());
//        ImGui::TextUnformatted(m_VideoThread->GetInputInformation().c_str());
//
//        int v = static_cast<int>(m_PTS);
//        if (rgmui::SliderIntExt("pts", &v, 0, MAX_PTS, "%d", 0, true, true, 17, 8)) {
//            m_PTS = v;
//        }
//
//        auto f = m_VideoThread->GetFramePts(m_PTS);
//        if (f) {
//            cv::Mat m(f->Height, f->Width, CV_8UC3, f->Buffer);
//            if (f->Height < 1000) {
//                //cv::resize(m, m, {}, 2, 2);
//            }
//            rgmui::MatAnnotator anno("m", m);
//            if (anno.IsHovered(false)) {
//                auto& io = ImGui::GetIO();
//                if (io.MouseWheel != 0) {
//                    m_PTS -= io.MouseWheel * 17;
//                    if (m_PTS < 0) {
//                        m_PTS = 0;
//                    }
//                    if (m_PTS > MAX_PTS) {
//                        m_PTS = MAX_PTS;
//                    }
//                }
//            }
//        }
//
//        if (m_ActivePlayer) {
//            rgmui::TextFmt("{}", m_ActivePlayer->Name);
//        }
//    }
//    ImGui::End();
//
//    if (m_ActivePlayer) {
//        DoAction(m_ActivePlayer, GetActionFromKeysPressed());
//    }
//
//
//    for (auto & player : m_Players) {
//        std::string v = "RECREATE: " + player.Name;
//        if (ImGui::Begin(v.c_str())) {
//            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
//                m_ActivePlayer = &player;
//            }
//            DoPlayerControls(&player, &m_PTS);
//        }
//        ImGui::End();
//    }
//
//    if (ImGui::Begin("replay")) {
//        int w = 6 * 16 + 16;
//        int h = 1 * 16 + 16;
//        nes::PPUx ppux(w, h, nes::PPUxPriorityStatus::ENABLED);
//        ppux.DrawBorderedBox(0, 0, w, h, {0x36, 0x36, 0x36,  0x36, 0x17, 0x0f,  0x0f, 0x0f, 0x0f},
//                m_Competition.Config.Visuals.Palette.data(), 2);
//
//        ppux.ResetPriority();
//        ppux.BeginOutline();
//        std::array<uint8_t, 4> tpal = {0x00, nes::PALETTE_ENTRY_WHITE, 0x20, 0x20};
//        ppux.RenderString(8, 8, "replay", m_Competition.StaticData.Font.data(), tpal.data(),
//                m_Competition.Config.Visuals.Palette.data(), 2, nes::EffectInfo::Defaults());
//
//        ppux.StrokeOutlineO(2.0f, nes::PALETTE_ENTRY_BLACK, m_Competition.Config.Visuals.Palette.data());
//
//        cv::Mat m(ppux.GetHeight(), ppux.GetWidth(), CV_8UC3, ppux.GetBGROut());
//        cv::resize(m, m, {}, 6, 6, cv::INTER_NEAREST);
//        rgmui::Mat("replay", m);
//        if (ImGui::Button("save")) {
//            cv::imwrite("replay.png", m);
//        }
//    }
//    ImGui::End();
//
//    return true;
//}
//
//SMBCompRecreateComponent::SMBCompRecreateComponent(argos::RuntimeConfig* info, SMBComp* comp)
//    : ISMBCompSingleWindowComponent("recreate tas", "recreatetas", false)
//    , m_Info(info)
//    , m_Competition(comp)
//    , m_GhostIndex(0)
//    , m_FrameIndex(0)
//    , m_LastMiniX(0)
//    , m_OneFramePerStep(false)
//    , m_FrameCount(0)
//{
//}
//
//SMBCompRecreateComponent::~SMBCompRecreateComponent()
//{
//}
//
//void argos::rgms::EmuToOutput(int frameIndex, int* startIndex, nes::ControllerState cs,
//        const nes::NestopiaNESEmulator& emu, smb::SMBMessageProcessorOutput* out,
//        const smb::SMBBackgroundNametables& nametables)
//{
//    using namespace smb;
//
//    out->ConsolePoweredOn = true;
//    double mult = (1.0 / nes::NTSC_FPS) * 1000 * (1 / nes::NTSC_MS_PER_M2);
//    out->M2Count = frameIndex * mult;
//    if (startIndex && *startIndex != -1) {
//        out->UserM2 = (frameIndex - *startIndex) * mult;
//    } else {
//        out->UserM2 = 0;
//    }
//
//    out->Controller = cs;
//    emu.PPUPeekFramePalette(&out->FramePalette);
//
//    AreaPointer ap = smb::AreaPointerFromAreaData(
//            emu.CPUPeek(RamAddress::AREA_DATA_LOW),
//            emu.CPUPeek(RamAddress::AREA_DATA_HIGH));
//    int apx = smb::AreaPointerXFromData(
//            emu.CPUPeek(RamAddress::SCREENEDGE_PAGELOC),
//            emu.CPUPeek(RamAddress::SCREENEDGE_X_POS),
//            ap,
//            emu.CPUPeek(RamAddress::BLOCK_BUFFER_84_DISC));
//
//    out->Frame.AP = ap;
//    out->Frame.PrevAPX = apx;
//    out->Frame.APX = apx;
//    out->Frame.IntervalTimerControl = emu.CPUPeek(RamAddress::INTERVAL_TIMER_CONTROL);
//    out->Frame.GameEngineSubroutine = emu.CPUPeek(RamAddress::GAME_ENGINE_SUBROUTINE);
//    out->Frame.OperMode = emu.CPUPeek(RamAddress::OPER_MODE);
//
//    out->Frame.Time = static_cast<int>(emu.PPUPeek8(0x2000 + 0x007a)) * 100 +
//                      static_cast<int>(emu.PPUPeek8(0x2000 + 0x007b)) * 10 +
//                      static_cast<int>(emu.PPUPeek8(0x2000 + 0x007c)) * 1;
//
//    out->Frame.World = emu.CPUPeek(RamAddress::WORLD_NUMBER) + 0x01;
//    out->Frame.Level = emu.CPUPeek(RamAddress::LEVEL_NUMBER) + 0x01;
//
//    auto PPUNT0At = [&](int x, int y){
//        return emu.PPUPeek8(0x2000 + x + y * nes::NAMETABLE_WIDTH_BYTES);
//    };
//
//    for (int i = 0; i < out->Frame.TitleScreen.ScoreTiles.size(); i++) {
//        out->Frame.TitleScreen.ScoreTiles[i] = PPUNT0At(TITLESCREEN_SCORE_X + i, TITLESCREEN_SCORE_Y);
//    }
//    out->Frame.TitleScreen.CoinTiles[0] = PPUNT0At(TITLESCREEN_COIN_X + 0, TITLESCREEN_COIN_Y);
//    out->Frame.TitleScreen.CoinTiles[1] = PPUNT0At(TITLESCREEN_COIN_X + 1, TITLESCREEN_COIN_Y);
//
//    out->Frame.TitleScreen.WorldTile = PPUNT0At(TITLESCREEN_WORLD_X, TITLESCREEN_WORLD_Y);
//    out->Frame.TitleScreen.LevelTile = PPUNT0At(TITLESCREEN_LEVEL_X, TITLESCREEN_LEVEL_Y);
//
//    out->Frame.TitleScreen.LifeTiles[0] = PPUNT0At(TITLESCREEN_LIFE_X + 0, TITLESCREEN_LIFE_Y);
//    out->Frame.TitleScreen.LifeTiles[1] = PPUNT0At(TITLESCREEN_LIFE_X + 1, TITLESCREEN_LIFE_Y);
//
//    int lpage = apx / 256;
//    int rpage = lpage + 1;
//    if ((apx % 512) >= 256) {
//        std::swap(lpage, rpage);
//    }
//
//    out->Frame.NTDiffs.clear();
//    out->Frame.TopRows.clear();
//
//    std::array<const BackgroundNametable*, 2> nts = {nullptr, nullptr};
//    nts[0] = nametables.FindNametable(ap, lpage);
//    nts[1] = nametables.FindNametable(ap, rpage);
//
//    auto ntpeek = [&](int i, int j){
//        return emu.PPUPeek8(0x2000 + 0x400 * i + j);
//    };
//
//    for (int i = 0; i < 2; i++) {
//        if (i == 0) {
//            for (int j = 0; j < 32 * 4; j++) {
//                int y = j / nes::NAMETABLE_WIDTH_BYTES;
//                int x = j % nes::NAMETABLE_WIDTH_BYTES;
//                if (x <= 1 || x >= 30 || y <= 1) {
//                    out->Frame.TopRows.push_back(36); // yolo
//                } else {
//                    out->Frame.TopRows.push_back(ntpeek(i, j));
//                }
//            }
//            for (int j = 0; j < 32; j++) {
//                out->Frame.TopRows.push_back(ntpeek(i, j + nes::NAMETABLE_ATTRIBUTE_OFFSET));
//            }
//        }
//
//        if (nts[i]) {
//            std::unordered_set<int> diffAttrs;
//            for (int j = 32*4; j < nes::NAMETABLE_ATTRIBUTE_OFFSET; j++) {
//                if (ntpeek(i, j) != nts[i]->Nametable[j]) {
//                    int y = j / nes::NAMETABLE_WIDTH_BYTES;
//                    int x = j % nes::NAMETABLE_WIDTH_BYTES;
//
//                    int tapx = (x * 8) + nts[i]->APX;
//                    if ((tapx > (apx - 8)) && (tapx < (apx + 256))) {
//                        smb::SMBNametableDiff diff;
//                        diff.NametablePage = nts[i]->Page;
//                        diff.Offset = j;
//                        diff.Value = ntpeek(i, j);
//
//                        out->Frame.NTDiffs.push_back(diff);
//
//                        int cy = y / 4;
//                        int cx = x / 4;
//
//                        int attrIndex = nes::NAMETABLE_ATTRIBUTE_OFFSET + cy * (nes::NAMETABLE_WIDTH_BYTES / 4) + cx;
//                        if (ntpeek(i, attrIndex) != nts[i]->Nametable[attrIndex]) {
//                            diffAttrs.insert(attrIndex);
//                        }
//
//                    }
//                }
//
//                for (auto & attrIndex : diffAttrs) {
//                    smb::SMBNametableDiff diff;
//                    diff.NametablePage = nts[i]->Page;
//                    diff.Offset = attrIndex;
//                    diff.Value = ntpeek(i, attrIndex);
//
//                    out->Frame.NTDiffs.push_back(diff);
//                }
//
//            }
//        }
//    }
//
//    const uint8_t* fpal = out->FramePalette.data();
//    for (int i = 0; i < nes::NUM_OAM_ENTRIES; i++) {
//        if (i == 0) { // Skip sprite zero, it's always the bottom of the coin
//            continue;
//        }
//        uint8_t y = emu.CPUPeek(RamAddress::SPRITE_DATA + (i * 4) + 0);
//        uint8_t tile_index = emu.CPUPeek(RamAddress::SPRITE_DATA + (i * 4) + 1);
//        uint8_t attributes = emu.CPUPeek(RamAddress::SPRITE_DATA + (i * 4) + 2);
//        uint8_t x = emu.CPUPeek(RamAddress::SPRITE_DATA + (i * 4) + 3);
//
//        if (y > 240) { // 'off screen'
//            continue;
//        }
//
//        nes::OAMxEntry oamx;
//        oamx.X = static_cast<int>(x);
//        oamx.Y = static_cast<int>(y);
//        oamx.TileIndex = tile_index;
//        oamx.Attributes = attributes;
//        oamx.PatternTableIndex = 0;
//
//        oamx.TilePalette[0] = fpal[16];
//        uint8_t p = attributes & nes::OAM_PALETTE;
//        for (int j = 1; j < 4; j++) {
//            oamx.TilePalette[j] = fpal[16 + p * 4 + j];
//        }
//
//        out->Frame.OAMX.push_back(oamx);
//    }
//
//    if (startIndex) {
//        if (*startIndex == -1 && ap == 0x25 && apx < 15 && out->Frame.Time == 400) {
//            *startIndex = frameIndex;
//        }
//    }
//}
//
//void SMBCompRecreateComponent::DoControls()
//{
////    if (m_Players.empty()) {
////        if (ImGui::Button("init players")) {
////            InitRecreatePlayers(m_Info->ArgosDirectory, &m_Players);
////            m_Ghosts.clear();
////
////
////            const std::vector<SMBCompPlayer>& players = m_Competition->Config.Players.Players;
////            for (auto & player : m_Players) {
////                m_Ghosts.emplace_back();
////                auto& ghost = m_Ghosts.back();
////
////                bool fnd = false;
////                for (auto & cplayer : players) {
////                    if (cplayer.Names.ShortName == player.Name) {
////
////                        ghost.Recording = "tas";
////                        ghost.RepresentativeColor = cplayer.Colors.RepresentativeColor;
////                        ghost.ShortName = cplayer.Names.ShortName;
////                        ghost.FullName = cplayer.Names.FullName;
////
////                        auto& emu = *player.Emulator.get();
////                        int startIndex = -1;
////                        for (int targetIndex = 10; targetIndex < player.Inputs.size() - 10; targetIndex++) {
////                        //for (int targetIndex = 300; targetIndex < 500; targetIndex++) {
////                            std::shared_ptr<std::string> s = nullptr;
////                            while (!s) {
////                                s = player.StateThread->GetState(targetIndex);
////                            }
////
////                            emu.LoadStateString(*s);
////
////
////                            ghost.Outputs.push_back(std::make_shared<smb::SMBMessageProcessorOutput>());
////
////                            EmuToOutput(targetIndex, &startIndex, player.Inputs.at(targetIndex),
////                                    emu, ghost.Outputs.back().get(),
////                                    m_Competition->StaticData.Nametables);
////                        }
////
////                        fnd = true;
////                        break; // hi
////                    }
////                }
////                if (!fnd) {
////                    std::cout << "error. no cplayer found for player" << std::endl;
////                }
////            }
////        }
////    } else {
////        if (ImGui::Button("init feeds")) {
////            const std::vector<SMBCompPlayer>& players = m_Competition->Config.Players.Players;
////            m_VecSources.clear();
////            m_VecSources.reserve(m_Players.size());
////            size_t i = 0;
////            for (auto & player : m_Players) {
////                for (auto & cplayer : players) {
////                    if (cplayer.Names.ShortName == player.Name) {
////                        SMBCompFeed* feed = GetPlayerFeed(cplayer, &m_Competition->Feeds);
////                        feed->ErrorMessage = "";
////                        feed->MySMBSerialProcessorThread.reset(nullptr);
////                        feed->MySMBSerialRecording.reset(nullptr);
////                        m_VecSources.emplace_back(&m_Ghosts[i].Outputs);
////                        feed->Source = &m_VecSources.back();
////                        i++;
////                        break;
////                    }
////                }
////            }
////        }
////        if (ImGui::Button("advance") || m_OneFramePerStep) {
////            m_Competition->FrameNumber = m_FrameCount++;
////            for (auto & source : m_VecSources) {
////                source.Next();
////            }
////        }
////        if (!m_Competition->DoingRecordingOfRecordings) {
////            if (ImGui::Button("do recording of recording 2")) {
////                m_FrameCount = 0;
////                std::string out = fmt::format("{}rec/{}", m_Info->ArgosDirectory, m_Competition->Config.Tournament.FileName);
////                std::cout << out << std::endl;
////                std::string dir = out + "/";
////                util::fs::remove_all(dir);
////                util::fs::create_directory(dir);
////
////                m_Competition->DoingRecordingOfRecordings =  true;
////
////                m_OneFramePerStep = true;
////            }
////        } else {
////            if (ImGui::Button("stop recording of recording") || m_FrameCount > 20600) {
////                m_Competition->DoingRecordingOfRecordings = false;
////                std::string out = fmt::format("{}rec/{}", m_Info->ArgosDirectory, m_Competition->Config.Tournament.FileName);
////                std::cout << out << std::endl;
////                std::string dir = out + "/";
////
////                std::string cmd = fmt::format("ffmpeg -y -framerate 60 -i {}%07d.png -vcodec libx264 -crf 6 -vf format=yuv420p {}.mp4",
////                        dir, out);
////                int r = system(cmd.c_str());
////                std::cout << "ret: " << r << std::endl;
////
////                m_OneFramePerStep = false;
////            }
////        }
////
////        ImGui::Separator();
////        rgmui::SliderIntExt("ghost", &m_GhostIndex, 0, m_Ghosts.size() - 1);
////        rgmui::SliderIntExt("frame", &m_FrameIndex, 0, 20000);
////
////        if (ImGui::CollapsingHeader("ghost")) {
////            if (m_GhostIndex >= 0 && m_GhostIndex < m_Ghosts.size() &&
////                m_FrameIndex >= 0 && m_FrameIndex < m_Ghosts.at(m_GhostIndex).Outputs.size())
////            {
////                auto out = m_Ghosts.at(m_GhostIndex).Outputs.at(m_FrameIndex);
////                rgmui::TextFmt("{}", out->M2Count);
////                rgmui::TextFmt("{}", out->UserM2);
////
////                int lastMiniX = 0;
////                auto m = SMBCompGhostViewComponent::CreateGhostFrameExt(m_Ghosts, m_Competition,
////                        m_GhostIndex, m_FrameIndex, &lastMiniX);
////                rgmui::Mat("g", m);
////            }
////
////
////
////            if (ImGui::Button("export")) {
////                for (int gi = 0; gi < m_Ghosts.size(); gi++) {
////
////                    std::string out = fmt::format("{}rec/tas_anyp2_{}", m_Info->ArgosDirectory, m_Ghosts[gi].ShortName);
////                    std::string dir = out + "/";
////                    std::cout << dir << std::endl;
////                    util::fs::remove_all(dir);
////                    util::fs::create_directory(dir);
////
////
////                    int lastMini = 0;
////                    for (int fi = 30; fi < m_Ghosts[gi].Outputs.size(); fi++) {
////                        if (fi % 1000 == 0) {
////                            std::cout << fi << std::endl;
////                        }
////
////                        auto m = SMBCompGhostViewComponent::CreateGhostFrameExt(m_Ghosts, m_Competition,
////                                gi, fi, &lastMini);
////
////                        cv::imwrite(fmt::format("{}{:07d}.png", dir, fi - 30), m);
////                    }
////
////                    std::string cmd = fmt::format("ffmpeg -y -framerate 60 -i {}%07d.png -vcodec libx264 -crf 6 -vf format=yuv420p {}.mp4",
////                            dir, out);
////
////                    std::cout << cmd << std::endl;
////                    int r = system(cmd.c_str());
////                    std::cout << "ret: " << r << std::endl;
////
////                    std::this_thread::sleep_for(std::chrono::seconds(1));
////                    std::cout << "ret: " << r << std::endl;
////
////                    util::fs::remove_all(dir);
////                    std::cout << out << std::endl;
////                }
////
////            }
////        }
////    }
//}
//
//VecSource::VecSource(std::vector<SMBMessageProcessorOutputPtr>* vec)
//    : m_Vec(vec)
//    , m_Index(0)
//    , m_RetIndex(1)
//{
//}
//
//VecSource::~VecSource()
//{
//}
//
//SMBMessageProcessorOutputPtr VecSource::Get(size_t index)
//{
//    if (m_Vec && index >= 0 && index < m_Vec->size()) {
//        return m_Vec->at(index);
//    }
//    if (m_Vec && index >= m_Vec->size()) {
//        return m_Vec->back();
//    }
//    return nullptr;
//}
//
//SMBMessageProcessorOutputPtr VecSource::GetLatestProcessorOutput()
//{
//    return Get(m_Index);
//}
//
//SMBMessageProcessorOutputPtr VecSource::GetNextProcessorOutput()
//{
//    if (m_RetIndex == 1 && m_Index == 0) {
//        m_RetIndex = 0;
//        return Get(0);
//    }
//    if (m_RetIndex < m_Index) {
//        m_RetIndex++;
//        return Get(m_RetIndex);
//    }
//    return nullptr;
//}
//
//void VecSource::Next()
//{
//    m_Index++;
//}
//
//////////////////////////////////////////////////////////////////////////////////
//
//SMBCompCreditsComponent::SMBCompCreditsComponent(argos::RuntimeConfig* info, SMBComp* comp)
//    : ISMBCompSingleWindowComponent("Credits", "credits", false)
//    , m_Info(info)
//    , m_Competition(comp)
//    , m_CreditY(0)
//    , m_Start(840)
//    , m_End(-2688)
//    , m_OnePerStep(0)
//    , m_Amt(7)
//    , m_RecIndex(0)
//    , m_Rec(false)
//{
//    //m_Credits = util::ReadFileToString(fmt::format("{}credits.txt",
//    m_Info->ArgosDirectory));
//}
//
//SMBCompCreditsComponent::~SMBCompCreditsComponent()
//{
//    //util::WriteStringToFile(fmt::format("{}credits.txt", m_Info->ArgosDirectory), m_Credits);
//}
//
//void SMBCompCreditsComponent::DoControls()
//{
//    std::string out = fmt::format("{}rec/credits", m_Info->ArgosDirectory);
//    std::string dir = out + "/";
//
//    rgmui::SliderIntExt("y", &m_CreditY, -4096, 4096);
//    rgmui::SliderIntExt("start", &m_Start, -4096, 4096);
//    ImGui::SameLine();
//    if (ImGui::Button("to start")) {
//        m_CreditY = m_Start;
//    }
//    rgmui::SliderIntExt("end", &m_End, -1024, 1024);
//    ImGui::SameLine();
//    if (ImGui::Button("to end")) {
//        m_CreditY = m_End;
//    }
//    ImGui::Checkbox("one per step", &m_OnePerStep);
//    ImGui::SameLine();
//    ImGui::InputInt("amt", &m_Amt);
//    if (m_OnePerStep) {
//        if (m_CreditY > m_End) m_CreditY -= m_Amt;
//    }
//    if (!m_Rec) {
//        if (ImGui::Button("start recording")) {
//            std::cout << dir << std::endl;
//            util::fs::remove_all(dir);
//            util::fs::create_directory(dir);
//            m_OnePerStep = true;
//            m_Rec = true;
//            m_RecIndex = 0;
//        }
//    } else {
//        if (ImGui::Button("stop recording")) {
//            std::string cmd = fmt::format("ffmpeg -y -framerate 60 -i {}%07d.png -vcodec libx264 -crf 6 -vf format=yuv420p {}.mp4",
//                    dir, out);
//
//            std::cout << cmd << std::endl;
//            int r = system(cmd.c_str());
//            std::cout << "ret: " << r << std::endl;
//
//            std::this_thread::sleep_for(std::chrono::seconds(1));
//            std::cout << "ret: " << r << std::endl;
//
//            util::fs::remove_all(dir);
//            std::cout << out << std::endl;
//            m_OnePerStep = false;
//            m_Rec = false;
//        }
//    }
//
//    cv::Mat m = m_Competition->CombinedView.Img.clone();
//    cv::resize(m, m, {}, 4, 4, cv::INTER_NEAREST);
//    m += cv::Scalar(30, 30, 30);
//    cv::GaussianBlur(m, m, cv::Size(13, 13), 0, 0);
//
//    nes::PPUx ppux(m.cols, m.rows, m.data, nes::PPUxPriorityStatus::ENABLED);
//    nes::RenderInfo render = DefaultSMBCompRenderInfo(*m_Competition);
//
//    std::array<uint8_t, 4> tpal = {0x00, nes::PALETTE_ENTRY_WHITE, 0x20, 0x20};
//    ppux.BeginOutline();
//    ppux.RenderStringX(0, m_CreditY, m_Credits,
//                m_Competition->StaticData.Font.data(), tpal.data(), render.PaletteBGR, 8, 8,
//                nes::EffectInfo::Defaults());
//    ppux.StrokeOutlineX(8.0f, nes::PALETTE_ENTRY_BLACK, render.PaletteBGR);
//    rgmui::Mat("creditm", m);
//
//    if (m_Rec) {
//        cv::imwrite(fmt::format("{}{:07d}.png", dir, m_RecIndex), m);
//        m_RecIndex++;
//    }
//
//
//    if (ImGui::CollapsingHeader("credit")) {
//        rgmui::InputTextMulti("credits", &m_Credits);
//    }
//}
//
static SMBCompSoundComponent* s_SoundComponent = nullptr;
static void MyMusicFinished()
{
    if (s_SoundComponent) {
        s_SoundComponent->MusicFinished();
    }
}

SMBCompSoundComponent::SMBCompSoundComponent(argos::RuntimeConfig* info, SMBComp* comp)
    : ISMBCompSingleWindowComponent("Sound", "sound", true)
    , m_Info(info)
    , m_Competition(comp)
    , m_CurrentMusic(smb::MusicTrack::SILENCE)
{
    if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
        if (SDL_Init(SDL_INIT_AUDIO) < 0) {
            throw std::runtime_error(SDL_GetError());
        }
    }

    Mix_MasterVolume(80);
    Mix_VolumeMusic(80);

    s_SoundComponent = this;
    Mix_HookMusicFinished(MyMusicFinished);
}

SMBCompSoundComponent::~SMBCompSoundComponent()
{
    //Mix_Quit();
}

void SMBCompSoundComponent::MusicFinished()
{
    m_CurrentMusic = smb::MusicTrack::SILENCE;
}

static int MusicLoops(smb::MusicTrack t)
{
    int loops = 0;
    if (t == smb::MusicTrack::PIPE_INTRO ||
        t == smb::MusicTrack::TIME_RUNNING_OUT ||
        t == smb::MusicTrack::ALT_GAME_OVER ||
        t == smb::MusicTrack::END_OF_CASTLE ||
        t == smb::MusicTrack::END_OF_LEVEL ||
        t == smb::MusicTrack::VICTORY ||
        t == smb::MusicTrack::GAME_OVER ||
        t == smb::MusicTrack::DEATH_MUSIC) {
        loops = 1;
    }
    return loops;
}

void SMBCompSoundComponent::StartMusic(smb::MusicTrack t)
{
    StopMusic();
    auto it = m_Competition->StaticData.Sounds.Musics.find(t);

    if (it == m_Competition->StaticData.Sounds.Musics.end()) return;
    Mix_Music* mus = it->second->Music;

    m_CurrentMusic = t;
    Mix_PlayMusic(mus, MusicLoops(t));
}

void SMBCompSoundComponent::StopMusic()
{
    m_CurrentMusic = smb::MusicTrack::SILENCE;
    Mix_HaltMusic();
}

void SMBCompSoundComponent::PlaySoundEffect(uint32_t playerId, smb::SoundEffect effect)
{
    if (effect == smb::SoundEffect::TIMERTICK) return;

    uint32_t target_player = m_Competition->CombinedView.PlayerID;
    //if (playerId != target_player) {
    //    if (effect == smb::SoundEffect::TIMERTICK) return;
    //}

    auto it = m_Competition->StaticData.Sounds.SoundEffects.find(effect);
    if (it == m_Competition->StaticData.Sounds.SoundEffects.end()) return;

    Mix_PlayChannel(-1, it->second->Chunk, 0);
}

static smb::MusicTrack ToMusic(char typ, uint8_t val) {
    if (val == 128) {
        return smb::MusicTrack::SILENCE;
    }
    if (typ == 'a') {
        return static_cast<smb::MusicTrack>(val);
    }
    if (typ == 'e') {
        uint32_t v = val;
        v <<= 8;
        return static_cast<smb::MusicTrack>(v);
    }
    return smb::MusicTrack::SILENCE;
}

static smb::SoundEffect ToEffect(char typ, uint8_t val) {
    uint32_t v = val;
    if (typ == 'n') {
        v <<= 16;
    } else if (typ == '2') {
        v <<= 8;
    }
    return static_cast<smb::SoundEffect>(v);
}
void SMBCompSoundComponent::OnFrameAlways()
{
    bool any_on = false;
    for (auto & player : m_Competition->Config.Players.Players) {
        if (auto out = GetLatestPlayerOutput(*m_Competition, player)) {
            if (!out->ConsolePoweredOn) {
                m_PlayerToMusic[player.UniquePlayerID] = smb::MusicTrack::SILENCE;
            } else {
                any_on = true;
            }
        }
    }




//    if (!any_on) {
//        StopMusic();
//        return;
//    }
//    uint32_t target_player = m_Competition->CombinedView.PlayerID;
//
//    auto it = m_PlayerToMusic.find(target_player);
//    if (it == m_PlayerToMusic.end()) {
//        //StopMusic();
//        return;
//    }
//    auto& requested_music = it->second;
//
//    if (m_CurrentMusic == smb::MusicTrack::SILENCE) {
//        StartMusic(requested_music);
//        return;
//    }
//
//    if (requested_music == smb::MusicTrack::SILENCE) {
//
//        bool continue_music = false;
//        for (auto & [player, music] : m_PlayerToMusic) {
//            if (player != target_player && music == m_CurrentMusic) {
//                continue_music = true;
//            }
//        }
//        if (!continue_music && MusicLoops(m_CurrentMusic) != 0) {
//            StartMusic(requested_music);
//        }
//    } else if (requested_music != m_CurrentMusic) {
//        StartMusic(requested_music);
//    }
}

void SMBCompSoundComponent::NoteOutput(const SMBCompPlayer& player, SMBMessageProcessorOutputPtr out)
{
    //std::cout << player.Names.ShortName << std::endl;
    if (out->ConsolePoweredOn == false) {
        std::cout << player.Names.ShortName << "silence" << std::endl;
        m_PlayerToMusic[player.UniquePlayerID] = smb::MusicTrack::SILENCE;
        return;
    }

    if (out->Frame.PauseSoundQueue) {
        //std::cout << player.Names.ShortName << " " << static_cast<int>(out->Frame.IntervalTimerControl) << "PauseSoundQueue: " << static_cast<int>(out->Frame.PauseSoundQueue) << std::endl;
    }

    if (out->Frame.AreaMusicQueue) {
        std::cout << player.Names.ShortName << "hello?" << std::endl;
        m_PlayerToMusic[player.UniquePlayerID] = ToMusic('a', out->Frame.AreaMusicQueue);
        //std::cout << player.Names.ShortName << " " << static_cast<int>(out->Frame.IntervalTimerControl) << "AreaMusicQueue: " << static_cast<int>(out->Frame.AreaMusicQueue) << std::endl;
    }
    if (out->Frame.EventMusicQueue) {
        std::cout << player.Names.ShortName << "hello2" << std::endl;
        m_PlayerToMusic[player.UniquePlayerID] = ToMusic('e', out->Frame.EventMusicQueue);
        //std::cout << player.Names.ShortName << " " << static_cast<int>(out->Frame.IntervalTimerControl) << "EventMusicQueue: " << static_cast<int>(out->Frame.EventMusicQueue) << std::endl;
    }


    //if (m_MusicPlayer == 0) {
    //    if (out->Frame.AreaMusicQueue && out->Frame.AreaMusicQueue != 128) {
    //        m_MusicPlayer = player.UniquePlayerID;
    //    }
    //    if (out->Frame.EventMusicQueue && out->Frame.EventMusicQueue != 128) {
    //        m_MusicPlayer = player.UniquePlayerID;
    //    }
    //}

    //if (player.UniquePlayerID == m_MusicPlayer) {
    //    if (out->Frame.AreaMusicQueue == 128) {
    //        StopMusic();
    //    } else if (out->Frame.AreaMusicQueue) {
    //        StartMusic(ToMusic('a', out->Frame.AreaMusicQueue));
    //    }
    //    if (out->Frame.EventMusicQueue == 128) {
    //        StopMusic();
    //    } else if (out->Frame.EventMusicQueue) {
    //        StartMusic(ToMusic('e', out->Frame.EventMusicQueue));
    //    }
    //}

    //if (player.UniquePlayerID == m_MusicPlayer) {

    //}

    //if (player.UniquePlayerID == m_Competition->CombinedView.PlayerID) {
    //    if (out->Frame.AreaMusicQueue == 128) {
    //        StopMusic();
    //    } else if (out->Frame.AreaMusicQueue) {
    //        StartMusic(static_cast<smb::MusicTrack>(out->Frame.AreaMusicQueue));
    //    }
    //    if (out->Frame.EventMusicQueue == 128) {
    //        StopMusic();
    //    } else if (out->Frame.EventMusicQueue) {
    //        uint32_t v = out->Frame.EventMusicQueue;
    //        v <<= 8;
    //        StartMusic(static_cast<smb::MusicTrack>(v));
    //    }
    //}
    //HERE MAKING SOUNDS
    if (out->Frame.NoiseSoundQueue) {
        PlaySoundEffect(player.UniquePlayerID, ToEffect('n', out->Frame.NoiseSoundQueue));
    }
    if (out->Frame.Square2SoundQueue) {
        PlaySoundEffect(player.UniquePlayerID, ToEffect('2', out->Frame.Square2SoundQueue));
    }
    if (out->Frame.Square1SoundQueue) {
        PlaySoundEffect(player.UniquePlayerID, ToEffect('1', out->Frame.Square1SoundQueue));
        //std::cout << player.Names.ShortName << " " << ToString(ToEffect('1', out->Frame.Square1SoundQueue)) << std::endl;
        //std::cout << player.Names.ShortName << " " << static_cast<int>(out->Frame.IntervalTimerControl) << "Square1SoundQueue: " << static_cast<int>(out->Frame.Square1SoundQueue) << std::endl;
    }
}

void SMBCompSoundComponent::DoControls()
{
    rgmui::TextFmt("{}", ToString(m_CurrentMusic));

    int vol = Mix_MasterVolume(-1);
    if (rgmui::SliderIntExt("Mix_MasterVolume", &vol, 0, MIX_MAX_VOLUME)) {
        Mix_MasterVolume(vol);
    }

    vol = Mix_VolumeMusic(-1);
    if (rgmui::SliderIntExt("Mix_VolumeMusic", &vol, 0, MIX_MAX_VOLUME)) {
        Mix_VolumeMusic(vol);
    }

    if (ImGui::CollapsingHeader("player musics")) {
        if (ImGui::Button("clear")) {
            m_PlayerToMusic.clear();
        }
        for (auto & [playerid, music] : m_PlayerToMusic) {
            //ImGui::PushID(playerid);
            //if (m_MusicPlayer == playerid) {
            //    if (ImGui::Button("reset")) {
            //        m_MusicPlayer = 0;
            //    }
            //} else {
            //    if (ImGui::Button("this one")) {
            //        StartMusic(music);
            //        m_MusicPlayer = playerid;
            //    }
            //}
            //ImGui::PopID();
            //ImGui::SameLine();
            auto* player = FindPlayer(m_Competition->Config.Players, playerid);
            if (player) {
                rgmui::TextFmt("{} {}", player->Names.ShortName, ToString(music));
            } else {
                rgmui::TextFmt("{} {}", playerid, ToString(music));
            }
        }
    }

    if (ImGui::CollapsingHeader("sounds")) {
        for (auto & [effect, chnk] : m_Competition->StaticData.Sounds.SoundEffects) {
            if (ImGui::Button(smb::ToString(effect).c_str())) {
                Mix_PlayChannel(-1, chnk->Chunk, 0);
            }
        }
    }

    if (ImGui::CollapsingHeader("music")) {
        if (ImGui::Button("Mix_HaltMusic()")) {
            Mix_HaltMusic();
        }
        for (auto & [track, mus] : m_Competition->StaticData.Sounds.Musics) {
            if (ImGui::Button(smb::ToString(track).c_str())) {
                Mix_HaltMusic();
                Mix_PlayMusic(mus->Music, 0);
            }
        }
    }

    if (ImGui::CollapsingHeader("extra")) {
        if (ImGui::Button("Mix_PauseAudio(0)")) {
            Mix_PauseAudio(0);
        }
        ImGui::SameLine();
        if (ImGui::Button("Mix_PauseAudio(1)")) {
            Mix_PauseAudio(1);
        }
        rgmui::TextFmt("Mix_AllocateChannels(-1): {}", Mix_AllocateChannels(-1));

        if (ImGui::Button("Mix_HaltChannel(-1)")) {
            Mix_HaltChannel(-1);
        }
        if (ImGui::Button("Mix_HaltMusic()")) {
            Mix_HaltMusic();
        }
        if (ImGui::Button("Mix_Pause(-1)")) {
            Mix_Pause(-1);
        }
        ImGui::SameLine();
        if (ImGui::Button("Mix_Resume(-1)")) {
            Mix_Resume(-1);
        }
        if (ImGui::Button("Mix_PauseMusic()")) {
            Mix_PauseMusic();
        }
        ImGui::SameLine();
        if (ImGui::Button("Mix_ResumeMusic()")) {
            Mix_ResumeMusic();
        }
    }

    //for (auto & player : m_Competition->Config.Players.Players) {
    //    if (auto out = GetLatestPlayerOutput(*m_Competition, player)) {
    //        if (out->Frame.PauseSoundQueue) {
    //            std::cout << static_cast<int>(out->Frame.IntervalTimerControl) << "PauseSoundQueue: " << static_cast<int>(out->Frame.PauseSoundQueue) << std::endl;
    //        }
    //        if (out->Frame.AreaMusicQueue) {
    //            std::cout << static_cast<int>(out->Frame.IntervalTimerControl) << "AreaMusicQueue: " << static_cast<int>(out->Frame.AreaMusicQueue) << std::endl;
    //        }
    //        if (out->Frame.EventMusicQueue) {
    //            std::cout << static_cast<int>(out->Frame.IntervalTimerControl) << "EventMusicQueue: " << static_cast<int>(out->Frame.EventMusicQueue) << std::endl;
    //        }
    //        if (out->Frame.NoiseSoundQueue) {
    //            std::cout << static_cast<int>(out->Frame.IntervalTimerControl) << "NoiseSoundQueue: " << static_cast<int>(out->Frame.NoiseSoundQueue) << std::endl;
    //        }
    //        if (out->Frame.Square2SoundQueue) {
    //            std::cout << static_cast<int>(out->Frame.IntervalTimerControl) << "Square2SoundQueue: " << static_cast<int>(out->Frame.Square2SoundQueue) << std::endl;
    //        }
    //        if (out->Frame.Square1SoundQueue) {
    //            std::cout << static_cast<int>(out->Frame.IntervalTimerControl) << "Square1SoundQueue: " << static_cast<int>(out->Frame.Square1SoundQueue) << std::endl;
    //        }
    //    }
    //}
}

//////////////////////////////////////////////////////////////////////////////////
//
//SMBCompFixedOverlay::SMBCompFixedOverlay(argos::RuntimeConfig* info, SMBComp* comp)
//    : ISMBCompSingleWindowComponent("Overlay", "overlay", false)
//    , m_Info(info)
//    , m_Comp(comp)
//    , m_First(true)
//{
//    FromTxt(m_Info, "aux_overlay_txt", &m_Text);
//
//    m_Mat = cv::Mat::zeros(270, 480, CV_8UC3);
//    m_ThanksMat = cv::Mat::zeros(480, 1824, CV_8UC3);
//
//    util::ForFileOfExtensionInDirectory(fmt::format("{}data/rgmty/", m_Info->ArgosDirectory), "mp4", [&](util::fs::path p){
//        m_ThanksPaths.push_back(p.string());
//        return true;
//    });
//    if (!m_ThanksPaths.empty()) {
//        m_StaticVideoThread = std::make_shared<video::StaticVideoThread>(
//                std::make_unique<video::CVVideoCaptureSource>(m_ThanksPaths.front()));
//    }
//    m_FrameIndex = 0;
//}
//
//SMBCompFixedOverlay::~SMBCompFixedOverlay()
//{
//    ToTxt(m_Info, "aux_overlay_txt", m_Text);
//}
//
//void SMBCompFixedOverlay::DoControls()
//{
//    rgmui::InputTextMulti("text", &m_Text);
//    if (ImGui::Button("redraw")) RedrawMatCompletely();
//    if (!m_StaticVideoThread) {
//        if (ImGui::Button("start")) {
//            m_StaticVideoThread = std::make_shared<video::StaticVideoThread>(
//                    std::make_unique<video::CVVideoCaptureSource>(m_ThanksPaths.front()));
//            m_FrameIndex = 0;
//        }
//    } else {
//        if (ImGui::Button("stop")) {
//            m_StaticVideoThread.reset();
//        }
//    }
//}
//
//void SMBCompFixedOverlay::RedrawMatCompletely()
//{
//    m_Mat = cv::Scalar({60, 60, 60});
//
//    nes::PPUx ppux(m_Mat.cols, m_Mat.rows, m_Mat.data, nes::PPUxPriorityStatus::ENABLED);
//
//    nes::RenderInfo render = DefaultSMBCompRenderInfo(*m_Comp);
//    std::array<uint8_t, 4> tpal = {0x00, 0x32, 0x20, 0x20};
//
//    // main flibidy
//    ppux.BeginOutline();
//    std::string head = "youtube.com/flibidydibidy";
//    ppux.RenderStringX((480 - head.length() * 2 * 8) / 2, 18, head,
//                m_Comp->StaticData.Font.data(), tpal.data(), render.PaletteBGR, 2, 2,
//                nes::EffectInfo::Defaults());
//    ppux.StrokeOutlineX(2.0f, nes::PALETTE_ENTRY_BLACK, render.PaletteBGR);
//
//    tpal = {0x00, nes::PALETTE_ENTRY_WHITE, 0x20, 0x20};
//    ppux.BeginOutline();
//    ppux.RenderStringX(0, 0, m_Text,
//                m_Comp->StaticData.Font.data(), tpal.data(), render.PaletteBGR, 1, 1,
//                nes::EffectInfo::Defaults());
//    ppux.StrokeOutlineX(1.0f, nes::PALETTE_ENTRY_BLACK, render.PaletteBGR);
//}
//void SMBCompFixedOverlay::DoDisplay(cv::Mat* m)
//{
//    if (m_First) {
//        RedrawMatCompletely();
//        m_First = false;
//    }
//    cv::resize(m_Mat, *m, {}, 4.0, 4.0, cv::INTER_NEAREST);
//
//    if (m_StaticVideoThread) {
//        int64_t n = m_StaticVideoThread->CurrentKnownNumFrames();
//        if (n) {
//            if (m_FrameIndex < n) {
//                auto f = m_StaticVideoThread->GetFrame(m_FrameIndex);
//                if (f) {
//                    memcpy(m_ThanksMat.data, f->Buffer, 1824 * 480 * 3);
//                    m_FrameIndex++;
//                }
//            } else {
//                if (m_ThanksPaths.size() > 1) {
//                    std::rotate(m_ThanksPaths.begin(), m_ThanksPaths.begin() + 1, m_ThanksPaths.end());
//                    m_StaticVideoThread = std::make_shared<video::StaticVideoThread>(
//                            std::make_unique<video::CVVideoCaptureSource>(m_ThanksPaths.front()));
//                }
//                m_FrameIndex = 0;
//            }
//        }
//    }
//    m_ThanksMat.copyTo((*m)(cv::Rect(48, 1080-480-24, 1824, 480)));
//}
//
////////////////////////////////////////////////////////////////////////////////

SMBCompReplayComponent::SMBCompReplayComponent(argos::RuntimeConfig* info, SMBComp* comp)
    : ISMBCompSingleWindowComponent("Replay", "replay", true)
    , m_Info(info)
    , m_Competition(comp)
    , m_BufferSize(1024)
    , m_PendingReplay(false)
    , m_HalfSpeed(true)
    , m_Counter(false)
{
}

SMBCompReplayComponent::~SMBCompReplayComponent()
{
}

void SMBCompReplayComponent::NoteOutput(const SMBCompPlayer& player, SMBMessageProcessorOutputPtr out)
{
    auto& deck = m_ReplayBuffer[player.UniquePlayerID];
    deck.push_back(out);
    if (deck.size() > m_BufferSize) {
        deck.pop_front();
        if (m_PendingReplay && m_PendingId == player.UniquePlayerID) {
            m_PendingIdx--;
            if (m_PendingIdx < 0) {
                m_PendingIdx = 0;
            }
        }
    }
}

void SMBCompReplayComponent::ColorPlayerDeck(const std::deque<SMBMessageProcessorOutputPtr>& deck, cv::Mat m)
{
    m = cv::Scalar({64, 64, 64});

    int s = m.cols - deck.size();
    assert(s >= 0);
    for (int i = 0; i < deck.size(); i++) {
        int j = i + s;
        if (j >= 0 && j < m.cols) {
            m.at<cv::Vec3b>(0, j) = cv::Vec3b(rgms::ColorBrewerQualitative(deck[i]->Frame.GameEngineSubroutine).data());
            m.at<cv::Vec3b>(1, j) = cv::Vec3b(rgms::ColorBrewerQualitative(deck[i]->Frame.OperMode).data());

            //m.at<cv::Vec3b>(2, j) = cv::Scalar({0, 0, 0});

            auto& frame = deck[i]->Frame;
            uint8_t v = frame.Square1SoundQueue || frame.Square2SoundQueue || frame.NoiseSoundQueue || frame.EventMusicQueue;

            auto col = rgms::ColorBrewerQualitative(v);
            auto islakitu = [](uint8_t t){
                return t == 0xb9 || t == 0xb8 || t == 0xbb || t == 0xbc;
            };
            for (auto & oamx : frame.OAMX) {
                if (islakitu(oamx.TileIndex)) {
                    col = {0, 255, 0};
                    if (oamx.Attributes & nes::OAM_FLIP_VERTICAL) {
                        col = {0, 0, 0};
                    }
                }
            }
            m.at<cv::Vec3b>(2, j) = cv::Vec3b(col.data());
        }
    }
}

void SMBCompReplayComponent::DoPlayerDeck(const SMBCompPlayer& player, const std::deque<SMBMessageProcessorOutputPtr>& deck)
{
    cv::Mat m = cv::Mat::zeros(3, m_BufferSize, CV_8UC3);
    ColorPlayerDeck(deck, m);

    nes::RenderInfo render = DefaultSMBCompRenderInfo(*m_Competition);
    std::array<uint8_t, 4> tpal = {0x00, nes::PALETTE_ENTRY_WHITE, 0x20, 0x20};

    cv::resize(m, m, {}, 1, 12, cv::INTER_NEAREST);
    nes::PPUx ppux(m.cols, m.rows, m.data, nes::PPUxPriorityStatus::ENABLED);
    ppux.BeginOutline();
    ppux.RenderStringX(2, 2, player.Names.ShortName,
                m_Competition->StaticData.Font.data(), tpal.data(), render.PaletteBGR, 2, 3,
                nes::EffectInfo::Defaults());
    ppux.StrokeOutlineX(1.0f, nes::PALETTE_ENTRY_BLACK, render.PaletteBGR);

    rgmui::MatAnnotator anno("d", m);
    auto v = anno.HoveredPixel();
    int j = anno.HoveredPixel().x;
    int s = m.cols - deck.size();
    int i = j - s;


    bool mouseOnX = (i >= 0 && i < deck.size());
    bool mouseOnY = (v.y >= 0 && v.y < m.rows);
    bool mouseOnOut = mouseOnX && mouseOnY && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    if (!m_PendingReplay && mouseOnOut) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            m_PendingReplay = true;
            m_PendingId = player.UniquePlayerID;
            m_PendingIdx = i;
        }
    }

    bool pendingReplayOnThis = (m_PendingReplay && player.UniquePlayerID == m_PendingId);

    if (pendingReplayOnThis) {
        int x = m_PendingIdx + s;
        anno.AddLine(util::Vector2F(x, 0), util::Vector2F(x, m.rows), IM_COL32_BLACK);
    }



    if ((!m_PendingReplay && mouseOnOut) || (m_PendingReplay && (player.UniquePlayerID == m_PendingId) && mouseOnX)) {
        auto out = deck.at(i);

        ImGui::BeginTooltip();
        rgmui::TextFmt("{} 0x{:02x} 0x{:02x}", player.Names.ShortName, out->Frame.GameEngineSubroutine, out->Frame.OperMode);
        cv::Mat q = cv::Mat::zeros(240, 256, CV_8UC3);
        nes::PPUx ppux(q.cols, q.rows, q.data, nes::PPUxPriorityStatus::ENABLED);

        rgms::RenderSMBToPPUX(out->Frame, out->FramePalette, m_Competition->StaticData.Nametables, &ppux, m_Competition->StaticData.ROM.rom);
        rgmui::Mat("f", q);

        ImGui::EndTooltip();
    }

    if (pendingReplayOnThis && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && mouseOnX) {
        if (i > m_PendingIdx) {
            m_PendingReplay = false;
            m_OngoingName = player.Names.FullName;
            m_OngoingReplay = std::deque<SMBMessageProcessorOutputPtr>(deck.begin() + m_PendingIdx, deck.begin() + i);
        }
    }
}

void SMBCompReplayComponent::DoControls()
{
    if (ImGui::Button("stop")) {
        m_OngoingReplay.clear();
    }
    ImGui::SameLine();
    ImGui::Checkbox("half speed", &m_HalfSpeed);
    for (auto & player : m_Competition->Config.Players.Players) {
        ImGui::PushID(player.UniquePlayerID);
        auto& deck = m_ReplayBuffer[player.UniquePlayerID];

        DoPlayerDeck(player, deck);
        ImGui::PopID();
    }
    if (m_PendingReplay && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        m_OngoingReplay.clear();
        m_PendingReplay = false;
    }
}

bool SMBCompReplayComponent::DoReplay(cv::Mat aux)
{
    if (!m_OngoingReplay.empty()) {
        auto out = m_OngoingReplay.front();
        cv::Mat q = cv::Mat::zeros(240, 256, CV_8UC3);
        nes::PPUx ppux(q.cols, q.rows, q.data, nes::PPUxPriorityStatus::ENABLED);

        rgms::RenderSMBToPPUX(out->Frame, out->FramePalette, m_Competition->StaticData.Nametables, &ppux, m_Competition->StaticData.ROM.rom);
        ppux.ResetPriority();
        ppux.BeginOutline();
        nes::RenderInfo render = DefaultSMBCompRenderInfo(*m_Competition);
        std::array<uint8_t, 4> tpal = {0x00, nes::PALETTE_ENTRY_WHITE, 0x20, 0x20};
        ppux.RenderString(2, 2, m_OngoingName + " replay",
                m_Competition->StaticData.Font.data(), tpal.data(), render.PaletteBGR, 2,
                nes::EffectInfo::Defaults());
        ppux.StrokeOutlineO(1.0f, nes::PALETTE_ENTRY_BLACK, render.PaletteBGR);

        int y = 1080 - 480 - 32;

        nes::PPUx ppux2(aux.cols, aux.rows, aux.data, nes::PPUxPriorityStatus::ENABLED);
        auto& nesPalette = nes::DefaultPaletteBGR();
        ppux2.DrawBorderedBox(16, y - 16, 512 + 32, 480 + 32, {0x36, 0x36, 0x36,  0x36, 0x17, 0x0f,  0x0f, 0x0f, 0x0f}, nesPalette.data(), 2);

        cv::resize(q, q, {}, 2, 2, cv::INTER_NEAREST);
        q.copyTo(aux(cv::Rect(32, y, 512, 480)));

        //ppux2.DrawBorderedBox(16, y - 16, 256 + 32, 240 + 32, {0x36, 0x36, 0x36,  0x36, 0x17, 0x0f,  0x0f, 0x0f, 0x0f}, nesPalette.data(), 2);
        //q.copyTo(aux(cv::Rect(32, y, 256, 240)));

        if (!m_HalfSpeed || (m_HalfSpeed && m_Counter)) {
            m_OngoingReplay.pop_front();
        }
        m_Counter = !m_Counter;
        return true;
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////
//
//SMBCompTxtDisplay::SMBCompTxtDisplay(argos::RuntimeConfig* info, SMBComp* comp)
//    : ISMBCompSingleWindowComponent("Txt", "txt", true)
//    , m_Info(info)
//    , m_Comp(comp)
//    , m_FirstTime(true)
//{
//    m_Mat = cv::Mat::zeros(270, 480, CV_8UC3);
//}
//
//SMBCompTxtDisplay::~SMBCompTxtDisplay()
//{
//    if (!m_Stem.empty()) {
//        ToTxt(m_Info, m_Stem.c_str(), m_Text);
//    }
//}
//
//void SMBCompTxtDisplay::UpdateStems()
//{
//    m_Stems.clear();
//
//    util::ForFileOfExtensionInDirectory(fmt::format("{}data/txt/", m_Info->ArgosDirectory), "txt", [&](util::fs::path p){
//        m_Stems.push_back(p.stem().string());
//        return true;
//    });
//    if (!m_Stems.empty()) {
//        std::sort(m_Stems.begin(), m_Stems.end());
//    }
//
//}
//
//void SMBCompTxtDisplay::DoControls()
//{
//    //rgmui::InputText("stem", &m_Stem);
//    if (ImGui::BeginCombo("stem", m_Stem.c_str())) {
//        if (m_FirstTime) {
//            UpdateStems();
//            m_FirstTime = false;
//        }
//
//        for (auto & stem : m_Stems) {
//            if (ImGui::Selectable(stem.c_str(), stem == m_Stem)) {
//                m_Stem = stem;
//                FromTxt(m_Info, stem.c_str(), &m_Text);
//            }
//        }
//
//        ImGui::EndCombo();
//    }
//    ImGui::SameLine();
//    if (ImGui::Button("refresh")) {
//        UpdateStems();
//    }
//    rgmui::InputTextMulti("text", &m_Text, ImVec2(-1, -1));
//}
//
//void SMBCompTxtDisplay::SetStem(const std::string& stem)
//{
//    m_Stem = stem;
//    FromTxt(m_Info, stem.c_str(), &m_Text);
//}
//
//void SMBCompTxtDisplay::DoDisplay(cv::Mat m)
//{
//    m_Mat = cv::Scalar({60, 60, 60});
//    nes::PPUx ppux(m_Mat.cols, m_Mat.rows, m_Mat.data, nes::PPUxPriorityStatus::ENABLED);
//    nes::RenderInfo render = DefaultSMBCompRenderInfo(*m_Comp);
//    std::array<uint8_t, 4> tpal = {0x00, nes::PALETTE_ENTRY_WHITE, 0x20, 0x20};
//
//    ppux.BeginOutline();
//    ppux.RenderStringX(8, 18, m_Text,
//                m_Comp->StaticData.Font.data(), tpal.data(), render.PaletteBGR, 1, 1,
//                nes::EffectInfo::Defaults());
//    ppux.StrokeOutlineX(1.0f, nes::PALETTE_ENTRY_BLACK, render.PaletteBGR);
//
//    cv::resize(m_Mat, m, {}, 4.0, 4.0, cv::INTER_NEAREST);
//}
//
////////////////////////////////////////////////////////////////////////////////

SMBCompTournamentComponent::SMBCompTournamentComponent(argos::RuntimeConfig* info, SMBComp* comp)
    : ISMBCompSimpleWindowContainerComponent("Tournament", true)
{
    RegisterWindow(std::make_shared<SMBCompTournamentSeatsWindow>(info, comp));
    RegisterWindow(std::make_shared<SMBCompTournamentPlayersWindow>(info, comp));
    RegisterWindow(std::make_shared<SMBCompTournamentScheduleWindow>(info, comp));
    m_Manager = std::make_shared<SMBCompTournamentManagerWindow>(info, comp);
    RegisterWindow(m_Manager);
    RegisterWindow(std::make_shared<SMBCompTournamentColorsWindow>(info, comp));
    RegisterWindow(std::make_shared<SMBCompTournamentResultsWindow>(info, comp));
}

SMBCompTournamentComponent::~SMBCompTournamentComponent()
{
}

void SMBCompTournamentComponent::LoadTournament(const std::string& path)
{
    m_Manager->LoadTournament(path);
}

SMBCompTournamentSeatsWindow::SMBCompTournamentSeatsWindow(argos::RuntimeConfig* info, SMBComp* comp)
    : ISMBCompSimpleWindowComponent("seats")
    , m_Info(info)
    , m_Comp(comp)
{
}

SMBCompTournamentSeatsWindow::~SMBCompTournamentSeatsWindow()
{
}

void SMBCompTournamentSeatsWindow::DoControls()
{
    if (ImGui::BeginTable("seats", 2)) {
        ImGui::TableSetupColumn("number", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("serial path (/dev/ttyUSB* or tcp://192.168.0.3:5555:seat1)", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();

        int i = 0;
        for (auto & seat : m_Comp->Config.Tournament.Seats) {
            ImGui::PushID(i);
            ImGui::TableNextColumn();
            rgmui::TextFmt("{}", i + 1);

            ImGui::TableNextColumn();
            ImGui::PushItemWidth(-1);
            rgmui::InputText("##b", &seat.Path);
            ImGui::PopItemWidth();

            ImGui::PopID();
            i++;
        }
        ImGui::EndTable();
    }
    if (ImGui::Button("add seat")) {
        SMBCompPlayerSerialInput si;
        si.Path = "/dev/ttyUSB";
        si.Baud = 40000000;

        m_Comp->Config.Tournament.Seats.push_back(si);
    }
    ImGui::SameLine();
    if (ImGui::Button("pop back")) {
        if (m_Comp->Config.Tournament.Seats.size() >= 1) {
            m_Comp->Config.Tournament.Seats.pop_back();
        }
    }
}

SMBCompTournamentPlayersWindow::SMBCompTournamentPlayersWindow(argos::RuntimeConfig* info, SMBComp* comp)
    : ISMBCompSimpleWindowComponent("players")
    , m_Info(info)
    , m_Comp(comp)
    , m_UpdatingThreeColors(true)
{
}

SMBCompTournamentPlayersWindow::~SMBCompTournamentPlayersWindow()
{
}

void SMBCompTournamentPlayersWindow::DoControls()
{
    if (ImGui::BeginTable("players", 6)) {
        ImGui::TableSetupColumn("short name", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("long name          ", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("color", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("mario colors", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("fire colors ", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("remove");
        ImGui::TableHeadersRow();


        bool remove = false;
        uint32_t toRemove;

        int i = 0;
        for (auto & player : m_Comp->Config.Tournament.Players)
        {
            ImGui::PushID(player.UniquePlayerID);

            ImGui::TableNextColumn();
            ImGui::PushItemWidth(-1);
            rgmui::InputText("##a", &player.Names.ShortName);
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();
            ImGui::PushItemWidth(-1);
            rgmui::InputText("##b", &player.Names.FullName);
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();
            if (rgmui::InputPaletteIndex("##c", &player.Colors.RepresentativeColor,
                    m_Comp->Config.Visuals.Palette.data(),
                    nes::PALETTE_ENTRIES)) {
                if (m_UpdatingThreeColors) {
                    player.Colors.MarioColors[0] = player.Colors.RepresentativeColor;
                    player.Colors.FireMarioColors[2] = player.Colors.RepresentativeColor;
                }
            }

            ImGui::TableNextColumn();
            rgmui::InputPaletteIndex("##d", &player.Colors.MarioColors[0],
                    m_Comp->Config.Visuals.Palette.data(),
                    nes::PALETTE_ENTRIES);
            ImGui::SameLine();
            rgmui::InputPaletteIndex("##e", &player.Colors.MarioColors[1],
                    m_Comp->Config.Visuals.Palette.data(),
                    nes::PALETTE_ENTRIES);
            ImGui::SameLine();
            rgmui::InputPaletteIndex("##f", &player.Colors.MarioColors[2],
                    m_Comp->Config.Visuals.Palette.data(),
                    nes::PALETTE_ENTRIES);

            ImGui::TableNextColumn();
            rgmui::InputPaletteIndex("##g", &player.Colors.FireMarioColors[0],
                    m_Comp->Config.Visuals.Palette.data(),
                    nes::PALETTE_ENTRIES);
            ImGui::SameLine();
            rgmui::InputPaletteIndex("##h", &player.Colors.FireMarioColors[1],
                    m_Comp->Config.Visuals.Palette.data(),
                    nes::PALETTE_ENTRIES);
            ImGui::SameLine();
            rgmui::InputPaletteIndex("##i", &player.Colors.FireMarioColors[2],
                    m_Comp->Config.Visuals.Palette.data(),
                    nes::PALETTE_ENTRIES);

            ImGui::TableNextColumn();
            ImGui::PushID(i);
            if (ImGui::Button("remove")) {
                remove = true;
                toRemove = player.UniquePlayerID;
            }
            ImGui::PopID();
            ImGui::PopID();
            i++;
        }

        if (remove)
        {
            std::erase_if(m_Comp->Config.Tournament.Players, [&](const SMBCompPlayer& p){
                return p.UniquePlayerID == toRemove;
            });
        }

        ImGui::EndTable();
    }
    ImGui::Separator();
    if (ImGui::Button("add player")) {
        SMBCompPlayer p;
        InitializeSMBCompPlayer(&p);
        p.UniquePlayerID = 1;
        for (auto & v : m_Comp->Config.Tournament.Players) {
            p.UniquePlayerID = std::max(p.UniquePlayerID, v.UniquePlayerID + 1);
        }
        m_Comp->Config.Tournament.Players.push_back(p);
    }
    ImGui::Checkbox("updating three colors", &m_UpdatingThreeColors);
}

SMBCompTournamentScheduleWindow::SMBCompTournamentScheduleWindow(argos::RuntimeConfig* info, SMBComp* comp)
    : ISMBCompSimpleWindowComponent("schedule")
    , m_Info(info)
    , m_Comp(comp)
    , m_Round(-1)
{
}

SMBCompTournamentScheduleWindow::~SMBCompTournamentScheduleWindow()
{
}

void SMBCompTournamentScheduleWindow::DoControls()
{
    auto& seats = m_Comp->Config.Tournament.Seats;
    auto& players = m_Comp->Config.Tournament.Players;
    auto& schedule = m_Comp->Config.Tournament.Schedule;

    if (ImGui::BeginTable("schedule", 1 + seats.size())) {
        ImGui::TableSetupColumn("round", ImGuiTableColumnFlags_WidthFixed);
        for (int i = 0; i < seats.size(); i++) {
            ImGui::TableSetupColumn(fmt::format("seat {}", i + 1).c_str(), ImGuiTableColumnFlags_WidthFixed);
        }
        ImGui::TableHeadersRow();

        for (int i = 0; i < schedule.size(); i++) {
            ImGui::PushID(i);
            if (schedule[i].size() < seats.size()) {
                schedule[i].resize(seats.size(), -1);
            }

            bool setToThis = false;

            ImGui::TableNextColumn();
            if (ImGui::Selectable(fmt::format("rnd: {:2d}", i + 1).c_str(), m_Round == i)) {
                setToThis = true;
            }
            for (int j = 0; j < seats.size(); j++) {
                int k = schedule[i][j];
                ImGui::TableNextColumn();
                if (k >= 0) {
                    if (ImGui::Selectable(players[k].Names.ShortName.c_str(), m_Round == i)) {
                        setToThis = true;
                    }
                }
            }

            if (setToThis) {
                SetToRound(i);
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }
    std::string p = fmt::format("{}/{}.txt", m_Info->ArgosDirectory, m_Comp->Config.Tournament.FileName);
    rgmui::CopyableText(p);
    if (ImGui::Button("fromtxt")) {
        if (util::FileExists(p)) {
            SetScheduleTxt(p);
        } else {
            std::cout << "ERROR: file doesn't exist: " << p << std::endl;
        }
    }
}

void SMBCompTournamentScheduleWindow::SetScheduleTxt(const std::string& p)
{
    auto& seats = m_Comp->Config.Tournament.Seats;
    auto& players = m_Comp->Config.Tournament.Players;
    auto& schedule = m_Comp->Config.Tournament.Schedule;

    std::ifstream ifs(p);
    std::string line;

    std::vector<std::vector<int>> thisSchedule;
    while (std::getline(ifs, line)) {
        std::istringstream iss(line);
        std::vector<int> theseplayers;
        for (int i = 0; i < seats.size(); i++) {
            std::string name;
            iss >> name;

            if (name.empty()) {
                std::cout << "error parsing file: " << p << std::endl;
                return;
            }

            int pno = -1;
            for (int j = 0; j < players.size(); j++) {
                if (players[j].Names.ShortName == name) {
                    pno = j;
                    break;
                }
            }
            if (pno == -1) {
                std::cout << "no player with name: " << name << std::endl;
            }

            theseplayers.push_back(pno);
        }
        thisSchedule.push_back(theseplayers);
    }

    schedule = thisSchedule;
}

void SMBCompTournamentScheduleWindow::SetToRound(int i)
{
    m_Round = i;

    m_Comp->Config.Tournament.TowerName =
        fmt::format("{} {}",
            m_Comp->Config.Tournament.DisplayName,
            i + 1);
    m_Comp->Config.Tournament.CurrentRound = i;

    auto& seats = m_Comp->Config.Tournament.Seats;
    auto& players = m_Comp->Config.Tournament.Players;
    auto& schedule = m_Comp->Config.Tournament.Schedule;

    auto& config = m_Comp->Config;
    InitializeSMBCompPlayers(&config.Players);
    int k = 0;
    for (auto & j : schedule[i]) {
        players[j].Inputs.Serial.Path = seats[k].Path;
        AddNewPlayer(&config.Players, players[j]);
        k++;
    }
}

SMBCompTournamentManagerWindow::SMBCompTournamentManagerWindow(argos::RuntimeConfig* info, SMBComp* comp)
    : ISMBCompSimpleWindowComponent("manager")
    , m_Info(info)
    , m_Comp(comp)
    , m_First(true)
{
}

SMBCompTournamentManagerWindow::~SMBCompTournamentManagerWindow()
{
}

void SMBCompTournamentManagerWindow::UpdatePaths()
{
    m_Paths.clear();
    util::ForFileOfExtensionInDirectory(fmt::format("{}", m_Info->ArgosDirectory), ".cfg", [&](util::fs::path p){
        m_Paths.push_back(p.string());
        return true;
    });

    if (!m_Paths.empty()) {
        std::sort(m_Paths.begin(), m_Paths.end());
    }
}

void SMBCompTournamentManagerWindow::LoadTournament(const std::string& path)
{
    m_TournamentToLoad = path;
}

void SMBCompTournamentManagerWindow::DoControls()
{
    if (m_TournamentToLoad != "") {
        UpdatePaths();
        m_First = false;
        for (auto & path : m_Paths) {
            if (m_TournamentToLoad == path) {
                m_CurrentPath = path;
                nlohmann::json j;

                std::ifstream ifs(path);
                ifs >> j;

                m_Comp->Config = j;
            }
        }
        m_TournamentToLoad = "";
    }
    if (ImGui::BeginCombo("tourney", m_CurrentPath.c_str())) {
        if (m_First) {
            UpdatePaths();
            m_First = false;
        }
        for (auto & path : m_Paths) {
            if (ImGui::Selectable(path.c_str(), path == m_CurrentPath) || m_TournamentToLoad == path) {
                m_TournamentToLoad = "";
                m_CurrentPath = path;
                nlohmann::json j;

                std::ifstream ifs(path);
                ifs >> j;

                m_Comp->Config = j;
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::Button("save")) {
        std::string path = fmt::format("{}/{}.cfg", m_Info->ArgosDirectory, m_Comp->Config.Tournament.FileName);
        std::ofstream of(path);
        m_CurrentPath = path;

        nlohmann::json j(m_Comp->Config);
        of << std::setw(2) << j << std::endl;

        UpdatePaths();
    }
    ImGui::Separator();
    auto* tournament = &m_Comp->Config.Tournament;
    rgmui::InputText("display name", &tournament->DisplayName);
    rgmui::InputText("tower name", &tournament->TowerName);
    rgmui::InputText("score name", &tournament->ScoreName);
    rgmui::InputText("file name", &tournament->FileName);

    if (rgmui::Combo4("category", &tournament->Category,
                m_Comp->StaticData.Categories.CategoryNames,
                m_Comp->StaticData.Categories.CategoryNames)) {
        ResetSMBCompTimingTower(&m_Comp->Tower);
    }

    if (ImGui::CollapsingHeader("increments")) {
        ImGui::InputInt("dnf", &tournament->DNFInc);
        ImGui::InputInt("dns", &tournament->DNSInc);
        rgmui::InputVectorInt("increments", &tournament->PointIncrements);
    }
}

SMBCompTournamentColorsWindow::SMBCompTournamentColorsWindow(argos::RuntimeConfig* info, SMBComp* comp)
    : ISMBCompSimpleWindowComponent("t colors")
    , m_Info(info)
    , m_Comp(comp)
{
}

SMBCompTournamentColorsWindow::~SMBCompTournamentColorsWindow()
{
}

void SMBCompTournamentColorsWindow::DoControls()
{
    nes::EffectInfo effects = nes::EffectInfo::Defaults();
    effects.Opacity = 1.0f;

    auto& visuals = m_Comp->Config.Visuals;
    nes::RenderInfo render;
    render.OffX = 0;
    render.OffY = 0;
    render.Scale = 1;
    render.PatternTables.push_back(m_Comp->StaticData.ROM.CHR0);
    render.PaletteBGR = visuals.Palette.data();

    auto& players = m_Comp->Config.Tournament.Players;
    int w = 3 * 8 * players.size() + 16;
    nes::PPUx ppux (w, 14 * 8, nes::PPUxPriorityStatus::DISABLED);
    ppux.FillBackground(0x22, render.PaletteBGR);

    for (int j = 0; j < players.size(); j++) {
        const auto& colors = players[j].Colors;
        nes::OAMxEntry oamx;
        oamx.X = 8 + j * 3 * 8;
        oamx.Y = 0;
        oamx.TileIndex = 0x00;
        oamx.Attributes = 0x00;
        oamx.PatternTableIndex = 0;
        for (int i = 0; i < 3; i++) {
            oamx.TilePalette[i + 1] = colors.MarioColors[i];
        }

        nes::NextOAMx(&oamx,  0, 0, 0x3a, 0x00); ppux.RenderOAMxEntry(oamx, render, effects);
        nes::NextOAMx(&oamx,  1, 0, 0x37, 0x00); ppux.RenderOAMxEntry(oamx, render, effects);
        nes::NextOAMx(&oamx, -1, 1, 0x4f, 0x00); ppux.RenderOAMxEntry(oamx, render, effects);
        nes::NextOAMx(&oamx,  1, 0, 0x4f, 0x40); ppux.RenderOAMxEntry(oamx, render, effects);

        nes::NextOAMx(&oamx,  -1, 2, 0x00, 0x00); ppux.RenderOAMxEntry(oamx, render, effects);
        nes::NextOAMx(&oamx,  1,  0, 0x01, 0x00); ppux.RenderOAMxEntry(oamx, render, effects);
        nes::NextOAMx(&oamx, -1,  1, 0x4c, 0x00); ppux.RenderOAMxEntry(oamx, render, effects);
        nes::NextOAMx(&oamx,  1,  0, 0x4d, 0x00); ppux.RenderOAMxEntry(oamx, render, effects);
        nes::NextOAMx(&oamx, -1,  1, 0x4a, 0x00); ppux.RenderOAMxEntry(oamx, render, effects);
        nes::NextOAMx(&oamx,  1,  0, 0x4a, 0x40); ppux.RenderOAMxEntry(oamx, render, effects);
        nes::NextOAMx(&oamx, -1,  1, 0x4b, 0x00); ppux.RenderOAMxEntry(oamx, render, effects);
        nes::NextOAMx(&oamx,  1,  0, 0x4b, 0x40); ppux.RenderOAMxEntry(oamx, render, effects);

        for (int i = 0; i < 3; i++) {
            oamx.TilePalette[i + 1] = colors.FireMarioColors[i];
        }

        nes::NextOAMx(&oamx,  -1, 2, 0x00, 0x00); ppux.RenderOAMxEntry(oamx, render, effects);
        nes::NextOAMx(&oamx,  1,  0, 0x01, 0x00); ppux.RenderOAMxEntry(oamx, render, effects);
        nes::NextOAMx(&oamx, -1,  1, 0x02, 0x00); ppux.RenderOAMxEntry(oamx, render, effects);
        nes::NextOAMx(&oamx,  1,  0, 0x03, 0x00); ppux.RenderOAMxEntry(oamx, render, effects);
        nes::NextOAMx(&oamx, -1,  1, 0x04, 0x00); ppux.RenderOAMxEntry(oamx, render, effects);
        nes::NextOAMx(&oamx,  1,  0, 0x05, 0x00); ppux.RenderOAMxEntry(oamx, render, effects);
        nes::NextOAMx(&oamx, -1,  1, 0x06, 0x00); ppux.RenderOAMxEntry(oamx, render, effects);
        nes::NextOAMx(&oamx,  1,  0, 0x07, 0x00); ppux.RenderOAMxEntry(oamx, render, effects);
    }

    cv::Mat m(ppux.GetHeight(), ppux.GetWidth(), CV_8UC3, ppux.GetBGROut());
    rgmui::Mat("m", m);
}

SMBCompTournamentResultsWindow::SMBCompTournamentResultsWindow(argos::RuntimeConfig* info, SMBComp* comp)
    : ISMBCompSimpleWindowComponent("results")
    , m_Info(info)
    , m_Comp(comp)
{
}

SMBCompTournamentResultsWindow::~SMBCompTournamentResultsWindow()
{
}

void SMBCompTournamentResultsWindow::DoControls()
{
    if (ImGui::Button("update")) {
        m_Results.clear();
        m_Totals.clear();
        for (int i = 0; i < m_Comp->Config.Tournament.Schedule.size(); i++) {
            std::string resultspath = fmt::format("{}/{}_{}.txt",
                    m_Info->ArgosDirectory,
                    m_Comp->Config.Tournament.FileName,
                    i + 1);
            if (util::FileExists(resultspath)) {
                std::ifstream ifs(resultspath);
                std::string line;
                while (std::getline(ifs, line)) {
                    std::istringstream iss(line);
                    IndividualResult res;
                    res.Round = i;
                    iss >> res.ShortName;
                    iss >> res.Time;
                    iss >> res.MS;
                    iss >> res.Completed;

                    m_Results.push_back(res);
                }
            }
        }

        if (!m_Results.empty()) {
            std::sort(m_Results.begin(), m_Results.end(), [](const IndividualResult& l, const IndividualResult& r){
                if (l.Round == r.Round) {
                    if (l.Completed != r.Completed) {
                        return l.Completed > r.Completed;
                    }
                    return l.MS < r.MS;
                } else {
                    return l.Round < r.Round;
                }
            });

            int k = 0;
            int lr = -1;
            for (int i = 0; i < m_Results.size(); i++) {
                auto& res = m_Results[i];
                if (res.Round != lr) {
                    k = 0;
                    lr = res.Round;
                }

                if (res.Completed) {
                    int pts = 0;
                    if (k >= 0 && k < m_Comp->Config.Tournament.PointIncrements.size()) {
                        pts = m_Comp->Config.Tournament.PointIncrements.at(k);
                    }
                    res.Points = pts;
                    k++;
                } else {
                    if (res.MS == 0) {
                        res.Points = m_Comp->Config.Tournament.DNSInc;
                    } else {
                        res.Points = m_Comp->Config.Tournament.DNFInc;
                    }
                }

                m_Totals[res.ShortName] += res.Points;
            }
        }

        std::vector<std::pair<int, std::string>> sortedPoints;
        for (auto & player : m_Comp->Config.Tournament.Players) {
            std::pair<int, std::string> pr;
            pr.first = m_Totals[player.Names.ShortName];
            pr.second = player.Names.FullName;
            sortedPoints.push_back(pr);
        }

        std::sort(sortedPoints.begin(), sortedPoints.end());
        std::reverse(sortedPoints.begin(), sortedPoints.end());

        std::string pointPath = fmt::format("{}/data/txt/{}_results.txt",
                m_Info->ArgosDirectory,
                m_Comp->Config.Tournament.FileName);
        std::ofstream ofs(pointPath);
        ofs << std::endl;
        for (auto & [pt, name] : sortedPoints) {
            ofs << "  " << std::setw(20) << name << "  " << pt << std::endl;
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("view")) {
        std::string pointStem = fmt::format("{}_results",
                m_Comp->Config.Tournament.FileName);

        m_Comp->SetTxtViewTo = pointStem;
    }

    if (ImGui::BeginTable("results", 6)) {
        ImGui::TableSetupColumn("rnd", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("name      ", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("time    ", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("ms    ", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("comp", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("pts", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();

        for (auto & res : m_Results) {
            ImGui::TableNextColumn();
            rgmui::TextFmt("{:3d}", res.Round);
            ImGui::TableNextColumn();
            rgmui::TextFmt("{}", res.ShortName);
            ImGui::TableNextColumn();
            rgmui::TextFmt("{}", res.Time);
            ImGui::TableNextColumn();
            rgmui::TextFmt("{}", res.MS);
            ImGui::TableNextColumn();
            rgmui::TextFmt("{}", res.Completed);
            ImGui::TableNextColumn();
            rgmui::TextFmt("{}", res.Points);
        }

        ImGui::EndTable();
    }
}

void argos::rgms::InitializeSMBRaceCategories(smb::SMBDatabase* db, SMBRaceCategories* cats)
{
    db->GetRouteNames(&cats->CategoryNames);
    for (auto & name : cats->CategoryNames) {
        auto routePtr = std::make_shared<smb::Route>();
        smb::db::route db_route;
        if (!db->GetRoute(name, &db_route)) {
            throw std::runtime_error("incomplete db?");
        }
        *routePtr = db_route.route;
        cats->Routes[name] = routePtr;
    }

}

//void argos::rgms::InitializeSMBRaceCategories(SMBRaceCategories* cats)
//{
//    if (!db || !cats) throw std::invalid_argument("invalid argument to initialize smb race categories");
//
//    db->PopulateCategories();
//    size_t n = db->categories.size();
//    std::vector<SMBRaceCategoryInfo>* outCats = &cats->Categories;
//    outCats->resize(n);
//    for (size_t i = 0; i < n; i++) {
//        outCats->at(i).Category = static_cast<RaceCategory>(db->categories[i].race_category);
//        outCats->at(i).Name = db->categories[i].name;
//
//        size_t ns = db->categories[i].category_sections.size();
//        outCats->at(i).Sections.resize(ns);
//        for (size_t j = 0; j < ns; j++) {
//            outCats->at(i).Sections.at(j).AP = db->categories[i].category_sections[j].area_pointer;
//            outCats->at(i).Sections.at(j).Left = db->categories[i].category_sections[j].left;
//            outCats->at(i).Sections.at(j).Right = db->categories[i].category_sections[j].right;
//            outCats->at(i).Sections.at(j).World = db->categories[i].category_sections[j].world;
//            outCats->at(i).Sections.at(j).Level = db->categories[i].category_sections[j].level;
//        }
//
//        outCats->at(i).TotalWidth = 0;
//        for (auto & sec : db->categories[i].category_sections) {
//            outCats->at(i).TotalWidth += (sec.right - sec.left - 1) + 16;
//        }
//        outCats->at(i).TotalWidth -= 16;
//    }
//}
//
//const SMBRaceCategoryInfo* SMBRaceCategories::FindCategory(RaceCategory category) const
//{
//    for (auto & cat : Categories) {
//        if (cat.Category == category) {
//            return &cat;
//        }
//    }
//    return nullptr;
//}


const std::vector<std::array<uint8_t, 3>>& argos::rgms::GetColorMapColors(ColorMapType cmap) {
    static std::vector<std::array<uint8_t, 3>> BREWER_RDBU = {
        {0x67, 0x00, 0x1f},
        {0xb2, 0x18, 0x2b},
        {0xd6, 0x60, 0x4d},
        {0xf4, 0xa5, 0x82},
        {0xfd, 0xdb, 0xc7},
        {0xf7, 0xf7, 0xf7},
        {0xd1, 0xe5, 0xf0},
        {0x92, 0xc5, 0xde},
        {0x43, 0x93, 0xc3},
        {0x21, 0x66, 0xac},
        {0x05, 0x30, 0x61},
    };

    static std::vector<std::array<uint8_t, 3>> BREWER_YLORBR = {
        {0xff, 0xff, 0xe5},
        {0xff, 0xf7, 0xbc},
        {0xfe, 0xe3, 0x91},
        {0xfe, 0xc4, 0x4f},
        {0xfe, 0x99, 0x29},
        {0xec, 0x70, 0x14},
        {0xcc, 0x4c, 0x02},
        {0x99, 0x34, 0x04},
        {0x66, 0x25, 0x06},
    };
    static std::vector<std::array<uint8_t, 3>> BREWER_BLUES = {
        {0xf7, 0xfb, 0xff},
        {0xde, 0xeb, 0xf7},
        {0xc6, 0xdb, 0xef},
        {0x9e, 0xca, 0xe1},
        {0x6b, 0xae, 0xd6},
        {0x42, 0x92, 0xc6},
        {0x21, 0x71, 0xb5},
        {0x08, 0x51, 0x9c},
        {0x08, 0x30, 0x6b},
    };
    static std::vector<std::array<uint8_t, 3>> BREWER_REDS = {
        {0xff, 0xf5, 0xf0},
        {0xfe, 0xe0, 0xd2},
        {0xfc, 0xbb, 0xa1},
        {0xfc, 0x92, 0x72},
        {0xfb, 0x6a, 0x4a},
        {0xef, 0x3b, 0x2c},
        {0xcb, 0x18, 0x1d},
        {0xa5, 0x0f, 0x15},
        {0x67, 0x00, 0x0d},
    };
    static std::vector<std::array<uint8_t, 3>> BREWER_GREENS = {
        {0xf7, 0xfc, 0xf5},
        {0xe5, 0xf5, 0xe0},
        {0xc7, 0xe9, 0xc0},
        {0xa1, 0xd9, 0x9b},
        {0x74, 0xc4, 0x76},
        {0x41, 0xab, 0x5d},
        {0x23, 0x8b, 0x45},
        {0x00, 0x6d, 0x2c},
        {0x00, 0x44, 0x1b},
    };

    switch (cmap) {
        case ColorMapType::BREWER_RDBU: {
            return BREWER_RDBU;
        } break;
        case ColorMapType::BREWER_YLORBR: {
            return BREWER_YLORBR;
        } break;
        case ColorMapType::BREWER_BLUES: {
            return BREWER_BLUES;
        } break;
        case ColorMapType::BREWER_REDS: {
            return BREWER_REDS;
        } break;
        case ColorMapType::BREWER_GREENS: {
            return BREWER_GREENS;
        } break;
    };

    throw std::invalid_argument("unknown color map type");
    return BREWER_YLORBR;
}

std::array<uint8_t, 3> argos::rgms::ColorMapColor(ColorMapType cmap, double v) {
    const std::vector<std::array<uint8_t, 3>>& cs = GetColorMapColors(cmap);
    if (cs.size() < 2) {
        throw std::invalid_argument("invalid color map");
    }

    if (v <= 0.0) {
        return cs.front();
    }
    if (v >= 1.0) {
        return cs.back();
    }

    std::vector<double> ls(cs.size());
    util::InplaceLinspaceBase(ls.begin(), ls.end(), 0.0, 1.0);
    auto i = std::lower_bound(ls.begin(), ls.end(), v);

    size_t li = std::distance(ls.begin(), i);

    double q = util::Lerp(v, ls[li-1], ls[li], 0.0, 1.0);

    std::array<uint8_t, 3> result;
    for (int j = 0; j < 3; j++) {
        result[j] = static_cast<uint8_t>(std::round(util::Lerp2(q,
                        static_cast<double>(cs[li-1][j]), static_cast<double>(cs[li][j]))));
    }

    return result;
}

std::array<uint8_t, 3> argos::rgms::ColorBrewerQualitative(int i)
{
    static std::vector<std::array<uint8_t, 3>> BREWER_QUALITATIVE = {
        {0xc7, 0xd3, 0x8d},
        {0xb3, 0xff, 0xff},
        {0xda, 0xba, 0xbe},
        {0x72, 0x80, 0xfb},
        {0xd3, 0xb1, 0x80},
        {0x62, 0xb4, 0xfd},
        {0x69, 0xde, 0xb3},
        {0xe5, 0xcd, 0xfc},
        {0xd9, 0xd9, 0xd9},
        {0xbd, 0x80, 0xbc},
        {0xc5, 0xeb, 0xcc},
        {0x6f, 0xed, 0xff},
    };
    int s = static_cast<int>(BREWER_QUALITATIVE.size());

    return BREWER_QUALITATIVE.at(((i % s) + s) % s);
}

void argos::rgms::RenderSMBToPPUX(const SMBFrameInfo& frame, const nes::FramePalette& fpal, smb::SMBNametableCachePtr nametables,
        nes::PPUx* ppux, nes::NESDatabase::RomSPtr rom)
{
    auto& nesPalette = nes::DefaultPaletteBGR();

    nes::RenderInfo render;
    render.OffX = 0;
    render.OffY = 0;
    render.Scale = 1;
    const uint8_t* chr1 = smb::rom_chr1(rom);
    render.PatternTables.push_back(smb::rom_chr0(rom));
    render.PatternTables.push_back(chr1);
    render.PaletteBGR = nesPalette.data();

    if (frame.GameEngineSubroutine == 0x00) {
        if (frame.OAMX.size() == 8) {
            auto PutTSTile = [&](int x, int y, uint8_t t, uint8_t a = 0x02){
                ppux->RenderNametableEntry(x * 8, y * 8, t,
                        a,
                        chr1, fpal.data(), nesPalette.data(),
                        1, nes::EffectInfo::Defaults());
            };

            for (int i = 0; i < frame.TitleScreen.ScoreTiles.size(); i++) {
                PutTSTile(rgms::TITLESCREEN_SCORE_X + i, rgms::TITLESCREEN_SCORE_Y, frame.TitleScreen.ScoreTiles[i]);
            }
            PutTSTile(rgms::TITLESCREEN_COIN_X + 0, rgms::TITLESCREEN_COIN_Y, frame.TitleScreen.CoinTiles[0]);
            PutTSTile(rgms::TITLESCREEN_COIN_X + 1, rgms::TITLESCREEN_COIN_Y, frame.TitleScreen.CoinTiles[1]);
            PutTSTile(rgms::TITLESCREEN_WORLD_X, rgms::TITLESCREEN_WORLD_Y, frame.TitleScreen.WorldTile);
            PutTSTile(rgms::TITLESCREEN_WORLD2_X, rgms::TITLESCREEN_WORLD2_Y, frame.TitleScreen.WorldTile);
            PutTSTile(rgms::TITLESCREEN_LEVEL_X, rgms::TITLESCREEN_LEVEL_Y, frame.TitleScreen.LevelTile);
            PutTSTile(rgms::TITLESCREEN_LEVEL2_X, rgms::TITLESCREEN_LEVEL2_Y, frame.TitleScreen.LevelTile);
            PutTSTile(rgms::TITLESCREEN_LIFE_X + 0, rgms::TITLESCREEN_LIFE_Y, frame.TitleScreen.LifeTiles[0]);
            PutTSTile(rgms::TITLESCREEN_LIFE_X + 1, rgms::TITLESCREEN_LIFE_Y, frame.TitleScreen.LifeTiles[1]);
            std::vector<std::tuple<int, int, uint8_t>> tls = {
                {0x03, 0x02, 0x16},
                {0x04, 0x02, 0x0a},
                {0x05, 0x02, 0x1b},
                {0x06, 0x02, 0x12},
                {0x07, 0x02, 0x18},
                {0x12, 0x02, 0x20},
                {0x13, 0x02, 0x18},
                {0x14, 0x02, 0x1b},
                {0x15, 0x02, 0x15},
                {0x16, 0x02, 0x0d},
                {0x19, 0x02, 0x1d},
                {0x1a, 0x02, 0x12},
                {0x1b, 0x02, 0x16},
                {0x1c, 0x02, 0x0e},
                {0x0c, 0x03, 0x29},
                {0x14, 0x03, 0x28},
                {0x12, 0x0a, 0x28},
                {0x0f, 0x0e, 0x29},
                {0x0b, 0x0a, 0x20},
                {0x0c, 0x0a, 0x18},
                {0x0d, 0x0a, 0x1b},
                {0x0e, 0x0a, 0x15},
                {0x0f, 0x0a, 0x0d},
            };
            for (auto [x, y, t] : tls) {
                PutTSTile(x, y, t);
            }
            PutTSTile(0x0b, 0x03, 0x2e, 0x03);

            for (auto & oamx : frame.OAMX) {
                ppux->RenderOAMxEntry(oamx, render, nes::EffectInfo::Defaults());
            }
        }
    } else {
        nametables->RenderTo(frame.AID, frame.APX, 256, ppux, 0, nesPalette,
                chr1, nullptr, fpal.data(), &frame.NTDiffs);

        if (!frame.TopRows.empty()) {
            ppux->RenderNametable(0, 0, 32, 4,
                    frame.TopRows.data(),
                    frame.TopRows.data() + 32 * 4,
                    chr1, fpal.data(), nesPalette.data(), 1, nes::EffectInfo::Defaults());
        }

        for (auto & oamx : frame.OAMX) {
            ppux->RenderOAMxEntry(oamx, render, nes::EffectInfo::Defaults());
        }
    }
}

RecReviewDB::RecReviewDB(const std::string& path)
    : SQLiteExtDB(path)
{
    ExecOrThrow(RecRecordingSchema());
}

const char* RecReviewDB::RecRecordingSchema()
{
    return R"(CREATE TABLE IF NOT EXISTS rec_recording (
    id                  INTEGER,
    import_path         TEXT NOT NULL,
    iso_timestamp       TEXT NOT NULL,
    unix_timestamp      INTEGER NOT NULL,
    offset_millis       INTEGER NOT NULL,
    elapsed_millis      INTEGER NOT NULL
);)";
}

RecReviewDB::~RecReviewDB()
{
}

void RecReviewDB::GetAllRecordings(std::vector<db::rec_recording>* recordings)
{
    sqlite3_stmt* stmt;
    sqliteext::PrepareOrThrow(m_Database, R"(
        SELECT * FROM rec_recording;
    )", &stmt);

    recordings->clear();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        db::rec_recording recording;
        recording.id = sqlite3_column_int(stmt, 0);
        recording.import_path = sqliteext::column_str(stmt, 1);
        recording.iso_timestamp = sqliteext::column_str(stmt, 2);
        recording.unix_timestamp = sqlite3_column_int(stmt, 3);
        recording.offset_millis = sqlite3_column_int(stmt, 4);
        recording.elapsed_millis = sqlite3_column_int(stmt, 5);
        recordings->push_back(recording);
    }
    sqlite3_finalize(stmt);
}

void RecReviewDB::InsertRecording(const db::rec_recording& recording)
{
    sqlite3_stmt* stmt;
    sqliteext::PrepareOrThrow(m_Database, R"(
        INSERT INTO rec_recording (import_path, iso_timestamp, unix_timestamp, offset_millis, elapsed_millis) VALUES (?, ?, ?, ?, ?);
    )", &stmt);
    sqliteext::BindStrOrThrow(stmt, 1, recording.import_path);
    sqliteext::BindStrOrThrow(stmt, 2, recording.iso_timestamp);
    sqliteext::BindInt64OrThrow(stmt, 3, recording.unix_timestamp);
    sqliteext::BindInt64OrThrow(stmt, 4, recording.offset_millis);
    sqliteext::BindInt64OrThrow(stmt, 5, recording.elapsed_millis);
    sqliteext::StepAndFinalizeOrThrow(stmt);
}

RecReviewApp::RecReviewApp(argos::RuntimeConfig* config)
    : m_Config(config)
    , m_Database(config->ArgosPathTo("argos.db"))
{
    RegisterComponent(std::make_shared<RecRecordingsComponent>(config, &m_Database));
}

RecReviewApp::~RecReviewApp()
{
}

bool RecReviewApp::OnFrame()
{
    return true;
}


RecRecordingsComponent::RecRecordingsComponent(argos::RuntimeConfig* config, RecReviewDB* db)
    : m_Config(config)
    , m_Database(db)
    , m_SMBDatabase(config->ArgosPathTo("smb.db"))
    //, m_ReplayStartTime(1667673985)
    , m_ReplayStartTime(1688620844)
    , m_InCount(0)
    , m_Context(2)
    , m_TimesWithStuffIndex(0)
{
    if (!m_SMBDatabase.IsInit()) {
        throw std::runtime_error("SMB Database is not initialized. Run 'argos smb db init'");
    }
    Init();
}

void RecRecordingsComponent::Init()
{
    m_Database->GetAllRecordings(&m_Recordings);
    //m_Timeline = cv::Mat::zeros(0, 0, CV_8UC3);
    //m_TimelineInfoIndex = cv::Mat::zeros(0, 0, CV_32SC1);
    //m_TimelineInfo.clear();
    //if (m_Recordings.empty()) {
    //    return;
    //}


    struct event {
        int64_t time;
        size_t recording_index;
        bool is_start;
    };
    std::vector<event> events(m_Recordings.size() * 2);
    {
        size_t i = 0;
        for (auto rec : m_Recordings) {
            auto& start = events[i];
            auto& end = events[i + 1];
            start.time = rec.unix_timestamp * 1000 + rec.offset_millis;
            end.time = rec.unix_timestamp * 1000 + rec.offset_millis + rec.elapsed_millis;
            start.recording_index = i / 2;
            end.recording_index = i / 2;
            start.is_start = true;
            end.is_start = false;
            i += 2;
        }
    }
    std::sort(events.begin(), events.end(), [&](const auto& a, const auto& b){
            if (a.time == b.time) {
            return a.is_start < b.is_start;
            }
        return a.time < b.time;
    });

    m_StartTime = static_cast<int>(events.front().time / 1000);
    m_EndTime = static_cast<int>(events.back().time / 1000);

    m_TimesWithStuff.clear();

    size_t max_slots = 0;
    size_t current_slots = 0;
    int64_t empty_time = 0;
    int64_t this_time = 0;
    int64_t orig_time = 0;
    std::vector<int64_t> start_times;
    for (auto & event : events) {
        if (event.is_start) {
            if (current_slots == 0 && this_time) {
                orig_time = event.time;
                empty_time += (event.time - this_time);
            }
            current_slots++;
            if (current_slots > max_slots) {
                max_slots = current_slots;
            }
        } else {
            if (current_slots == 0) {
                throw std::runtime_error("?");
            }
            current_slots--;
            if (current_slots == 0) {
                this_time = event.time;
                //std::cout << orig_time << " " << event.time << std::endl;
                m_TimesWithStuff.emplace_back(orig_time / 1000, event.time / 1000);
            }
        }
    }

    //int64_t total_time = events.back().time - events.front().time;
    //total_time -= empty_time;

    //int64_t scale = 4000;

    //size_t nx = total_time / scale + 1;
    //std::cout << nx << std::endl;
    //std::vector<size_t> slots(max_slots, events.size());

    //m_Time.resize(nx, 0);

    //int64_t current_time = events[0].time;
    //for (auto & event : events) {


    //}
}

void RecRecordingsComponent::ScanArgosDirectory()
{
    std::vector<std::string> paths;
    util::ForFileOfExtensionInDirectory(
            fmt::format("{}rec/", m_Config->ArgosDirectory),
            "rec", [&](util::fs::path p){
        paths.push_back(p.string());
        return true;
    });
    std::cout << "in directory: " << paths.size() << std::endl;
    std::unordered_set<std::string> already;
    for (auto & rec : m_Recordings) {
        already.emplace(rec.import_path);
    }
    paths.erase(std::remove_if(paths.begin(), paths.end(),
                [&](const std::string& p){
                return already.find(p) != already.end();
                }), paths.end());
    std::cout << "new ones    : " << paths.size() << std::endl;

    for (auto & path : paths) {
        std::cout << "+ " << path << std::endl;
        SMBSerialRecording recording(path, m_SMBDatabase.GetNametableCache());

        db::rec_recording rec;
        rec.import_path = path;
        rec.iso_timestamp = util::fs::path(path).stem().string().substr(0, 15);

        std::istringstream iss(rec.iso_timestamp);
        std::tm tm = {};
        iss >> std::get_time(&tm, "%Y%m%dT%H%M%S");
        std::time_t unixTime = std::mktime(&tm);

        rec.unix_timestamp = static_cast<int64_t>(unixTime);
        rec.offset_millis = 0;
        rec.elapsed_millis = recording.GetTotalElapsedMillis();

        std::cout << "   > " << rec.iso_timestamp << " [" << rec.unix_timestamp << "]" << std::endl;
        std::cout << "   > " << rec.elapsed_millis << " (" <<
            util::SimpleMillisFormat(rec.elapsed_millis, util::SimpleTimeFormatFlags::HMS) << ")" << std::endl;

        if (rec.elapsed_millis) {
            m_Database->InsertRecording(rec);
        } else {
            std::cout << "skipped..." << std::endl;
        }
    }
    std::cout << "done" << std::endl;
}

void RecRecordingsComponent::NewTime()
{
    int64_t timems = static_cast<int64_t>(m_ReplayStartTime) * 1000;
    m_InCount = 0;
    for (auto & rec : m_Recordings) {
        int64_t start = rec.unix_timestamp * 1000 + rec.offset_millis;
        int64_t end = start + rec.elapsed_millis;
        if (timems >= start && timems <= end) {
            m_InCount++;
        }
    }
}

void RecRecordingsComponent::OnFrame()
{
    if (ImGui::Begin("recordings")) {
        if (ImGui::Button("scan argos directory")) {
            ScanArgosDirectory();
            Init();
        }
        if (rgmui::SliderIntExt("replay start", &m_ReplayStartTime, m_StartTime, m_EndTime)) {
            NewTime();
        }
        if (ImGui::CollapsingHeader("stuff times")) {
            for (auto & [from, to] : m_TimesWithStuff) {
                ImGui::PushID(from);
                if (rgmui::SliderIntExt("replay start", &m_ReplayStartTime, from, to)) {
                    NewTime();
                }
                ImGui::PopID();
            }
        }

        rgmui::TextFmt("{}", m_InCount);
        if (ImGui::Button("load")) {
            int64_t timems = static_cast<int64_t>(m_ReplayStartTime) * 1000;
            m_LoadedRecordings.clear();
            for (auto & rec : m_Recordings) {
                int64_t start = rec.unix_timestamp * 1000 + rec.offset_millis;
                int64_t end = start + rec.elapsed_millis;
                if (timems >= start && timems <= end) {
                    LoadedRecording lrec;
                    lrec.Path = rec.import_path;
                    lrec.Recording = std::make_shared<SMBSerialRecording>(
                            rec.import_path, m_SMBDatabase.GetNametableCache());
                    lrec.Start = timems - start;
                    lrec.Recording->StartAt(lrec.Start);
                    lrec.Recording->SetPaused(true);
                    lrec.Target = fmt::format("tcp://localhost:{}",
                            5555 + m_LoadedRecordings.size());

                    lrec.Socket = std::make_shared<zmq::socket_t>(m_Context,
                            zmq::socket_type::pub);
                    lrec.Name = fmt::format("seat{}", m_LoadedRecordings.size() + 1);
                    lrec.Socket->bind(lrec.Target);
                    m_LoadedRecordings.push_back(lrec);
                }
            }
        }
        bool ispaused = false;
        for (auto & rec : m_LoadedRecordings) {
            ispaused = rec.Recording->GetPaused();
            rgmui::TextFmt("{} - {} {} {}", rec.Path, rec.Start,
                    rec.Recording->GetCurrentElapsedMillis(),
                    rec.Recording->GetPaused());
        }
        if (m_LoadedRecordings.size() > 0) {
            if ((ispaused && ImGui::Button("start")) || (!ispaused && ImGui::Button("stop"))) {
                for (auto & rec : m_LoadedRecordings) {
                    rec.Recording->SetPaused(!ispaused);
                }
            }
        }
    }
    ImGui::End();

    std::vector<uint8_t> buffer;
    for (auto & rec : m_LoadedRecordings) {
        bool sent_one = false;
        while (auto p = rec.Recording->GetNextProcessorOutput()) {
            if (p->Frame.NTDiffs.size() > 5000) {
                p->Frame.NTDiffs.resize(5000);
            }
            OutputToBytes(p, &buffer);
            rec.Socket->send(zmq::str_buffer("smb"), zmq::send_flags::sndmore);
            rec.Socket->send(zmq::message_t(rec.Name.data(), rec.Name.size()), zmq::send_flags::sndmore);
            rec.Socket->send(zmq::message_t(buffer.data(), buffer.size()), zmq::send_flags::none);
            //std::cout << rec.Path << std::endl;
        }
    }
}


