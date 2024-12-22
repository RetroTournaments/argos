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

#include "zmq.hpp"
#include "zmq_addon.hpp"
#include "spdlog/spdlog.h"

#include "smb/rgms.h"
#include "util/file.h"

using namespace argos::internesceptor;
using namespace argos::rgms;

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
                            SMBNametableDiff diff;
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
                        SMBNametableDiff diff;
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

void SMBSerialRecording::Reset()
{
    m_SerialProcessor.Reset();
    m_Start = util::Now();
    m_DataIndex = 0;
    m_AddToSeek = 0;
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
        SeekTo(util::ElapsedMillisFrom(m_Start) + m_AddToSeek);
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

