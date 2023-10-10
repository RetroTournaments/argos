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

#include <iostream>
#include <exception>
#include <sstream>

#include "nes/nestopiaimpl.h"

using namespace argos::nes;

std::string argos::nes::ResultToString(Nes::Result r) {
    switch(r) {
        case Nes::Result::RESULT_ERR_WRONG_MODE:
            return "RESULT_ERR_WRONG_MODE (-13) [NTSC/PAL region mismatch.]";
        case Nes::RESULT_ERR_MISSING_BIOS:
            return "RESULT_ERR_MISSING_BIOS (-12) [Missing FDS BIOS.]";
        case Nes::RESULT_ERR_UNSUPPORTED_MAPPER:
            return "RESULT_ERR_UNSUPPORTED_MAPPER (-11) [Unsupported or malformed mapper.]";
        case Nes::RESULT_ERR_UNSUPPORTED_VSSYSTEM:
            return "RESULT_ERR_UNSUPPORTED_VSSYSTEM (-10) [Vs DualSystem is unsupported.]";
        case Nes::RESULT_ERR_UNSUPPORTED_FILE_VERSION:
            return "RESULT_ERR_UNSUPPORTED_FILE_VERSION (-9) [File format version is no longer supported.]";
        case Nes::RESULT_ERR_UNSUPPORTED:
            return "RESULT_ERR_UNSUPPORTED (-8) [Unsupported operation.]";
        case Nes::RESULT_ERR_INVALID_CRC:
            return "RESULT_ERR_INVALID_CRC (-7) [Invalid CRC checksum.]";
        case Nes::RESULT_ERR_CORRUPT_FILE:
            return "RESULT_ERR_CORRUPT_FILE (-6) [Corrupt file.]";
        case Nes::RESULT_ERR_INVALID_FILE:
            return "RESULT_ERR_INVALID_FILE (-5) [Invalid file.]";
        case Nes::RESULT_ERR_INVALID_PARAM:
            return "RESULT_ERR_INVALID_PARAM (-4) [Invalid parameter(s).]";
        case Nes::RESULT_ERR_NOT_READY:
            return "RESULT_ERR_NOT_READY (-3) [System not ready.]";
        case Nes::RESULT_ERR_OUT_OF_MEMORY:
            return "RESULT_ERR_OUT_OF_MEMORY (-2) [Out of memory.]";
        case Nes::RESULT_ERR_GENERIC:
            return "RESULT_ERR_GENERIC (-1) [Generic error.]";
        case Nes::RESULT_OK:
            return "RESULT_OK (0) [Success.]";
        case Nes::RESULT_NOP:
            return "RESULT_NOP (1) [Success but operation had no effect.]";
        case Nes::RESULT_WARN_BAD_DUMP:
            return "RESULT_WARN_BAD_DUMP (2) [Success but image dump may be bad.]";
        case Nes::RESULT_WARN_BAD_PROM:
            return "RESULT_WARN_BAD_PROM (3) [Success but PRG-ROM may be bad.]";
        case Nes::RESULT_WARN_BAD_CROM:
            return "RESULT_WARN_BAD_CROM (4) [Success but CHR-ROM may be bad.]";
        case Nes::RESULT_WARN_BAD_FILE_HEADER:
            return "RESULT_WARN_BAD_FILE_HEADER (5) [Success but file header may have incorrect data.]";
        case Nes::RESULT_WARN_SAVEDATA_LOST:
            return "RESULT_WARN_SAVEDATA_LOST (6) [Success but save data has been lost.]";
        case Nes::RESULT_WARN_DATA_REPLACED:
            return "RESULT_WARN_DATA_REPLACED () [Success but data may have been replaced.]";
    }
    std::ostringstream os;
    os << r;
    return os.str();
}

////////////////////////////////////////////////////////////////////////////////

NestopiaNESEmulator::NestopiaNESEmulator()
    : m_Machine(m_Emulator)
{
    m_LastFrame.fill(0);
}

NestopiaNESEmulator::~NestopiaNESEmulator() {
}

void NestopiaNESEmulator::ThrowOnBadResult(const char* method, Nes::Result r) const {
    // Could use NES_FAILED macro in core/NstBase.hpp
    if (!(r == Nes::RESULT_OK || r == Nes::RESULT_NOP)) {
        std::ostringstream os;
        os << method << "  " << ResultToString(r);
        std::cout << os.str() << std::endl;
        throw std::runtime_error(os.str());
    }
}


void NestopiaNESEmulator::LoadINES(std::istream& is) {
    ThrowOnBadResult("Nes::Api::Machine::Load", m_Machine.Load(is, Nes::Api::Machine::FAVORED_NES_NTSC));
    ThrowOnBadResult("Nes::Api::Machine::Power", m_Machine.Power(true));
}

void NestopiaNESEmulator::SaveState(std::ostream& os) const {
    os.write(reinterpret_cast<const char*>(m_LastFrame.data()), m_LastFrame.size());
    ThrowOnBadResult("Nes::Api::Machine::SaveState", m_Machine.SaveState(os));
}

void NestopiaNESEmulator::LoadState(std::istream& is) {
    is.read(reinterpret_cast<char*>(m_LastFrame.data()), m_LastFrame.size());
    ThrowOnBadResult("Nes::Api::Machine::LoadState", m_Machine.LoadState(is));
}

void NestopiaNESEmulator::Reset(bool isHardReset) {
    ThrowOnBadResult("Nes::Api::Machine::Reset", m_Machine.Reset(isHardReset));
}


void NestopiaNESEmulator::Execute(const ControllerState& player1) {
    m_Controllers.pad[0].buttons = player1;
    Nes::Core::Input::Controllers* cont = &m_Controllers;

    ThrowOnBadResult("Nes::Api::Emulator::Execute", 
            m_Emulator.Execute(nullptr, nullptr, cont));

    Nes::Core::Machine& machine(const_cast<Nes::Api::Emulator&>(m_Emulator));
    Nes::Core::Ppu& ppu = machine.ppu;

    int i = 0;
    for (int y = 0; y < FRAME_HEIGHT; y++) {
        for (int x = 0; x < FRAME_WIDTH; x++) {
            m_LastFrame[i] = static_cast<uint8_t>(ppu.output.pixels[i]);
            i++;
        }
    }
}

uint64_t NestopiaNESEmulator::CurrentFrame() const {
    return m_Emulator.Frame();
}

uint8_t NestopiaNESEmulator::CPUPeek(uint16_t addr) const {
    Nes::Core::Machine& machine(const_cast<Nes::Api::Emulator&>(m_Emulator));
    Nes::Core::Cpu& cpu = machine.cpu;
    return static_cast<uint8_t>(cpu.Peek(static_cast<int>(addr)));
}

uint8_t NestopiaNESEmulator::PPUPeek8(uint16_t addr) const {
    Nes::Core::Machine& machine(const_cast<Nes::Api::Emulator&>(m_Emulator));
    Nes::Core::Ppu& ppu = machine.ppu;
    if (addr >= 0x000 && addr <= 0x1fff) {
        const Nes::Core::Ppu::ChrMem& chr = ppu.GetChrMem();
        return chr.Peek(addr);
    } else if (addr >= 0x2000 && addr <= 0x2fff) {
        const Nes::Core::Ppu::NmtMem& nmt = ppu.GetNmtMem();
        return nmt.Peek(addr - 0x2000);
    } else if (addr >= 0x3f00 && addr <= 0x3f1f) {
        const Nes::Core::Ppu::Palette& pal = ppu.palette;
        return static_cast<uint8_t>(pal.ram[addr - 0x3f00]);
    }
    throw std::runtime_error("not implemented");
    return 0x00;
}

uint8_t NestopiaNESEmulator::OAMPeek8(uint8_t addr) const {
    Nes::Core::Machine& machine(const_cast<Nes::Api::Emulator&>(m_Emulator));
    const Nes::Core::Ppu::Oam& oam = machine.ppu.oam;
    return oam.ram[addr];
}

uint8_t NestopiaNESEmulator::ScreenPeekPixel(int x, int y) const {
    if (x < 0 || x >= FRAME_WIDTH) {
        throw std::invalid_argument("invalid x");
    }
    if (y < 0 || y >= FRAME_HEIGHT) {
        throw std::invalid_argument("invalid y");
    }

    return m_LastFrame[y * FRAME_WIDTH + x];
}

