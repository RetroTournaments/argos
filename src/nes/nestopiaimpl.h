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

#ifndef ARGOS_NES_NESTOPIAIMPL_HEADER
#define ARGOS_NES_NESTOPIAIMPL_HEADER

#include "NstApiEmulator.hpp"
#include "NstApiMachine.hpp"
#include "NstApiVideo.hpp"
#include "NstApiInput.hpp"
#include "NstApiCartridge.hpp"
#include "NstMachine.hpp"

#include "nes/nes.h"

namespace argos::nes {

std::string ResultToString(Nes::Result r);

class NestopiaNESEmulator : public INESEmulator
{
public:
    NestopiaNESEmulator();
    ~NestopiaNESEmulator();

    virtual void LoadINES(std::istream& is) override;
    virtual void SaveState(std::ostream& os) const override;
    virtual void LoadState(std::istream& is) override;

    virtual void Reset(bool isHardReset = true) override;
    virtual void Execute(const ControllerState& player1 = 0x00) override;
    virtual uint64_t CurrentFrame() const override;

    virtual uint8_t CPUPeek(uint16_t addr) const override;
    virtual uint8_t PPUPeek8(uint16_t addr) const override;
    virtual uint8_t OAMPeek8(uint8_t addr) const override;
    virtual uint8_t ScreenPeekPixel(int x, int y) const override;

private:
    void InitVideoOutput();
    void PopulateFrame(Frame* frame);
    void ThrowOnBadResult(const char* method, Nes::Result r) const;

private:
    Nes::Api::Emulator m_Emulator;
    Nes::Api::Machine m_Machine;

    Nes::Core::Input::Controllers m_Controllers;
    Frame m_LastFrame;
};

}

#endif

