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

#include <iostream>
#include <iomanip>
#include <fstream>
#include <cassert>
#include <algorithm>

#include "nes/nes.h"
#include "util/string.h"

using namespace sta::nes;

///////////////////////////////////////////////////////////////////////////////

const Palette& sta::nes::DefaultPaletteRGB()
{
    static Palette p = {
        0x52, 0x52, 0x52,  0x01, 0x1a, 0x51,
        0x0f, 0x0f, 0x65,  0x23, 0x06, 0x63,
        0x36, 0x03, 0x4b,  0x40, 0x04, 0x26,
        0x3f, 0x09, 0x04,  0x32, 0x13, 0x00,
        0x1f, 0x20, 0x00,  0x0b, 0x2a, 0x00,
        0x00, 0x2f, 0x00,  0x00, 0x2e, 0x0a,
        0x00, 0x26, 0x2d,  0x00, 0x00, 0x00,
        0x00, 0x00, 0x00,  0x00, 0x00, 0x00,
        0xa0, 0xa0, 0xa0,  0x1e, 0x4a, 0x9d,
        0x38, 0x37, 0xbc,  0x58, 0x28, 0xb8,
        0x75, 0x21, 0x94,  0x84, 0x23, 0x5c,
        0x82, 0x2e, 0x24,  0x6f, 0x3f, 0x00,
        0x51, 0x52, 0x00,  0x31, 0x63, 0x00,
        0x1a, 0x6b, 0x05,  0x0e, 0x69, 0x2e,
        0x10, 0x5c, 0x68,  0x00, 0x00, 0x00,
        0x00, 0x00, 0x00,  0x00, 0x00, 0x00,
        0xff, 0xff, 0xff,  0x69, 0x9e, 0xfc,
        0x89, 0x87, 0xff,  0xae, 0x76, 0xff,
        0xce, 0x6d, 0xf1,  0xe0, 0x70, 0xb2,
        0xde, 0x7c, 0x70,  0xc8, 0x91, 0x3e,
        0xa6, 0xa7, 0x25,  0x81, 0xba, 0x28,
        0x63, 0xc4, 0x46,  0x54, 0xc1, 0x7d,
        0x56, 0xb3, 0xc0,  0x3c, 0x3c, 0x3c,
        0x00, 0x00, 0x00,  0x00, 0x00, 0x00,
        0xff, 0xff, 0xff,  0xbe, 0xd6, 0xfd,
        0xcc, 0xcc, 0xff,  0xdd, 0xc4, 0xff,
        0xea, 0xc0, 0xf9,  0xf2, 0xc1, 0xdf,
        0xf1, 0xc7, 0xc2,  0xe8, 0xd0, 0xaa,
        0xd9, 0xda, 0x9d,  0xc9, 0xe2, 0x9e,
        0xbc, 0xe6, 0xae,  0xb4, 0xe5, 0xc7,
        0xb5, 0xdf, 0xe4,  0xa9, 0xa9, 0xa9,
        0x00, 0x00, 0x00,  0x00, 0x00, 0x00,
    };
    return p;
}

const Palette& sta::nes::DefaultPaletteBGR()
{
    static Palette p = {
        0x52, 0x52, 0x52,  0x51, 0x1a, 0x01,
        0x65, 0x0f, 0x0f,  0x63, 0x06, 0x23,
        0x4b, 0x03, 0x36,  0x26, 0x04, 0x40,
        0x04, 0x09, 0x3f,  0x00, 0x13, 0x32,
        0x00, 0x20, 0x1f,  0x00, 0x2a, 0x0b,
        0x00, 0x2f, 0x00,  0x0a, 0x2e, 0x00,
        0x2d, 0x26, 0x00,  0x00, 0x00, 0x00,
        0x00, 0x00, 0x00,  0x00, 0x00, 0x00,
        0xa0, 0xa0, 0xa0,  0x9d, 0x4a, 0x1e,
        0xbc, 0x37, 0x38,  0xb8, 0x28, 0x58,
        0x94, 0x21, 0x75,  0x5c, 0x23, 0x84,
        0x24, 0x2e, 0x82,  0x00, 0x3f, 0x6f,
        0x00, 0x52, 0x51,  0x00, 0x63, 0x31,
        0x05, 0x6b, 0x1a,  0x2e, 0x69, 0x0e,
        0x68, 0x5c, 0x10,  0x00, 0x00, 0x00,
        0x00, 0x00, 0x00,  0x00, 0x00, 0x00,
        0xff, 0xff, 0xff,  0xfc, 0x9e, 0x69,
        0xff, 0x87, 0x89,  0xff, 0x76, 0xae,
        0xf1, 0x6d, 0xce,  0xb2, 0x70, 0xe0,
        0x70, 0x7c, 0xde,  0x3e, 0x91, 0xc8,
        0x25, 0xa7, 0xa6,  0x28, 0xba, 0x81,
        0x46, 0xc4, 0x63,  0x7d, 0xc1, 0x54,
        0xc0, 0xb3, 0x56,  0x3c, 0x3c, 0x3c,
        0x00, 0x00, 0x00,  0x00, 0x00, 0x00,
        0xff, 0xff, 0xff,  0xfd, 0xd6, 0xbe,
        0xff, 0xcc, 0xcc,  0xff, 0xc4, 0xdd,
        0xf9, 0xc0, 0xea,  0xdf, 0xc1, 0xf2,
        0xc2, 0xc7, 0xf1,  0xaa, 0xd0, 0xe8,
        0x9d, 0xda, 0xd9,  0x9e, 0xe2, 0xc9,
        0xae, 0xe6, 0xbc,  0xc7, 0xe5, 0xb4,
        0xe4, 0xdf, 0xb5,  0xa9, 0xa9, 0xa9,
        0x00, 0x00, 0x00,  0x00, 0x00, 0x00,
    };
    return p;
}

///////////////////////////////////////////////////////////////////////////////

void sta::nes::SetControllerStateButtonPressed(ControllerState* state, Button button, bool pressed)
{
    *state = (*state) & ~button;
    if (pressed) {
        *state |= button;
    }
}

void sta::nes::ToggleControllerStateButton(ControllerState* state, Button button)
{
    SetControllerStateButtonPressed(state, button, !(*state & button));
}

///////////////////////////////////////////////////////////////////////////////

INESEmulator::INESEmulator()
{
}

INESEmulator::~INESEmulator()
{
}

void INESEmulator::LoadINESFile(const std::string& path)
{
    std::ifstream ifs(path);
    LoadINES(ifs);
}

void INESEmulator::SaveStateFile(const std::string& path) const
{
    std::ofstream ofs(path);
    SaveState(ofs);
}

void INESEmulator::LoadStateFile(const std::string& path)
{
    std::ifstream ifs(path);
    LoadState(ifs);
}

void INESEmulator::LoadINESString(const std::string& contents)
{
    std::istringstream iss(contents);
    LoadINES(iss);
}

void INESEmulator::LoadINESData(const unsigned char rom[], int size)
{
    // Probably shouldn't copy but I mean.. :|
    std::string q(rom, rom + size);
    LoadINESString(q);
}

void INESEmulator::SaveStateString(std::string* contents) const
{
    std::ostringstream os;
    SaveState(os);
    *contents = os.str();
}

void INESEmulator::LoadStateString(const std::string& contents)
{
    std::istringstream iss(contents);
    LoadState(iss);
}

void INESEmulator::CPUPeekRam(Ram* ram) const
{
    for (uint16_t i = 0; i < RAM_SIZE; i++) {
        (*ram)[i] = CPUPeek(i);
    }
}

void INESEmulator::CPUPeekMult(uint16_t address, uint16_t cnt, uint8_t* out) const
{
    for (uint16_t i = 0; i < cnt; i++) {
        out[i] = CPUPeek(address + i);
    }
}

void INESEmulator::PPUPeekPatternTable(int tableIndex, PatternTable* table) const
{
    if (tableIndex < 0 || tableIndex > 1) {
        throw std::invalid_argument("invalid pattern table index");
    }

    for (uint16_t i = 0; i < PATTERNTABLE_SIZE; i++) {
        (*table)[i] = PPUPeek8(i + tableIndex * 0x1000);
    }
}

void INESEmulator::PPUPeekNameTable(int tableIndex, NameTable* table) const
{
    // Is it actually though?
    if (tableIndex < 0 || tableIndex > 3) {
        throw std::invalid_argument("invalid name table index");
    }

    for (uint16_t i = 0; i < NAMETABLE_SIZE; i++) {
        (*table)[i] = PPUPeek8(0x2000 + i + tableIndex * 0x0400);
    }
}

void INESEmulator::PPUPeekFramePalette(FramePalette* fpal) const
{
    for (uint16_t i = 0; i < FRAMEPALETTE_SIZE; i++) {
        (*fpal)[i] = PPUPeek8(0x3f00 + i);
    }
}

void INESEmulator::ScreenPeekFrame(Frame* frame) const
{
    int i = 0;
    for (int y = 0; y < FRAME_HEIGHT; y++) {
        for (int x = 0; x < FRAME_WIDTH; x++) {
            (*frame)[i] = ScreenPeekPixel(x, y);
            i++;
        }
    }
}


///////////////////////////////////////////////////////////////////////////////

NESEmulatorFactory::NESEmulatorFactory(std::function<std::unique_ptr<INESEmulator>()> emufactory,
        std::string defaultINESString, std::string defaultStateString
        )
    : m_EmulatorFactory(emufactory)
    , m_DefaultINESString(defaultINESString)
    , m_DefaultStateString(defaultStateString)
{
}

NESEmulatorFactory::~NESEmulatorFactory()
{
}

std::unique_ptr<INESEmulator> NESEmulatorFactory::GetEmu(bool loadDefaultINES, bool loadDefaultState)
{
    auto emu = m_EmulatorFactory();
    if (loadDefaultINES && !m_DefaultINESString.empty()) {
        emu->LoadINESString(m_DefaultINESString);
    }
    if (loadDefaultState && !m_DefaultStateString.empty()) {
        emu->LoadStateString(m_DefaultStateString);
    }
    return std::move(emu);
}

const std::string& NESEmulatorFactory::GetDefaultINESString() const
{
    return m_DefaultINESString;
}

void NESEmulatorFactory::SetDefaultINESString(const std::string& defaultINESString)
{
    m_DefaultINESString = defaultINESString;
}

const std::string& NESEmulatorFactory::GetDefaultStateString() const
{
    return m_DefaultStateString;
}

void NESEmulatorFactory::SetDefaultStateString(const std::string& defaultStateString)
{
    m_DefaultStateString = defaultStateString;
}

///////////////////////////////////////////////////////////////////////////////

StateSequenceConfig StateSequenceConfig::Defaults()
{
    StateSequenceConfig cfg;
    cfg.SaveInterval = 12;
    return cfg;
}

StateSequence::StateSequence(
        std::unique_ptr<INESEmulator>&& emu,
        StateSequenceConfig cfg,
        const std::vector<ControllerState>& initialStates)
    : m_Emulator(std::move(emu))
    , m_Config(cfg)
    , m_TargetIndex(0)
    , m_CurrentIndex(0)
    , m_Inputs(initialStates)
{
    SaveCurrentState();
}

StateSequence::~StateSequence()
{
}

void StateSequence::SaveCurrentState()
{
    SaveState ss;
    ss.Index = m_CurrentIndex;
    m_Emulator->SaveStateString(&ss.StateString);
    m_SaveStates.emplace_back(std::move(ss));
}

void StateSequence::SetInputs(
        const std::vector<ControllerState>& inputs)
{
    assert(m_SaveStates.size() >= 1);
    m_SaveStates.resize(1);
    m_Inputs = inputs;
    m_Emulator->LoadStateString(m_SaveStates[0].StateString);
    m_CurrentIndex = 0;
}

const std::vector<ControllerState>& StateSequence::GetInputs() const
{
    return m_Inputs;
}

void StateSequence::SetInput(int frameIndex, nes::ControllerState newState)
{
    assert(m_SaveStates.size() >= 1);
    bool changed = false;
    if (frameIndex >= m_Inputs.size()) {
        m_Inputs.resize(frameIndex + 1, 0x00);
    }
    if (newState != m_Inputs[frameIndex]) {
        changed = true;
        m_Inputs[frameIndex] = newState;
    }

    if (!changed) {
        return;
    }

    auto it = std::upper_bound(m_SaveStates.begin(), m_SaveStates.end(), frameIndex,
        [&](int fi, const SaveState& ss){
            return fi < ss.Index;
        });

    // Invalidate future save states
    if (it != m_SaveStates.end()) {
        m_SaveStates.erase(it, m_SaveStates.end());
    }

    if (frameIndex <= m_CurrentIndex) {
        // Switch to latest save state
        m_Emulator->LoadStateString(m_SaveStates.back().StateString);
        m_CurrentIndex = m_SaveStates.back().Index;
    }
}

bool StateSequence::HasWork() const
{
    if (m_CurrentIndex < m_TargetIndex) {
        return true;
    }
    return false;
}

int StateSequence::GetTargetIndex() const
{
    return m_TargetIndex;
}

void StateSequence::SetTargetIndex(int targetIndex)
{
    if (targetIndex < 0) {
        throw std::invalid_argument("invalid target index");
    }
    m_TargetIndex = targetIndex;

    if (m_TargetIndex == 0) {
        m_CurrentIndex = 0;
        m_Emulator->LoadStateString(m_SaveStates.front().StateString);
    } else {
        auto it = std::upper_bound(m_SaveStates.begin(), m_SaveStates.end(), targetIndex,
            [&](int fi, const SaveState& ss){
                return fi < ss.Index;
            });
        if (it != m_SaveStates.begin()) {
            it = std::prev(it);
            if (it != m_SaveStates.begin() && it->Index == targetIndex) {
                it = std::prev(it);
            }
        }

        if (m_TargetIndex < m_CurrentIndex || m_CurrentIndex < it->Index) {
            m_CurrentIndex = it->Index;
            m_Emulator->LoadStateString(it->StateString);
        }
    }
}

int StateSequence::GetCurrentIndex() const
{
    return m_CurrentIndex;
}

std::string StateSequence::GetCurrentStateString() const
{
    std::string v;
    m_Emulator->SaveStateString(&v);
    return v;
}


ControllerState StateSequence::GetInput(int frameIndex) const
{
    if (frameIndex < m_Inputs.size()) {
        return m_Inputs[frameIndex];
    }
    return 0x00;
}

void StateSequence::DoWork()
{
    if (m_CurrentIndex < m_TargetIndex) {
        m_Emulator->Execute(GetInput(m_CurrentIndex));
        m_CurrentIndex++;

        if (m_CurrentIndex % m_Config.SaveInterval == 0 &&
                m_CurrentIndex > m_SaveStates.back().Index) {
            SaveCurrentState();
        }
    }
}

std::string StateSequence::GetStateString(int frameIndex)
{
    SetTargetIndex(frameIndex);
    while (HasWork()) {
        DoWork();
    }
    assert(GetCurrentIndex() == frameIndex);
    return GetCurrentStateString();
}

void StateSequence::SetEmu(int frameIndex, INESEmulator* emu)
{
    emu->LoadStateString(GetStateString(frameIndex));
}

////////////////////////////////////////////////////////////////////////////////

StateSequenceThreadConfig StateSequenceThreadConfig::Defaults()
{
    StateSequenceThreadConfig cfg;
    cfg.OnWorkDelayMillis = 0;
    cfg.NoWorkDelayMillis = 2;
    cfg.TryLockTries = 3;
    cfg.TryLockDelayMicros = 5;
    cfg.StateSequenceCfg = StateSequenceConfig::Defaults();
    return cfg;
}

StateSequenceThread::StateSequenceThread(StateSequenceThreadConfig cfg,
            std::unique_ptr<INESEmulator>&& emu,
            const std::vector<ControllerState>& initialStates)
    : m_Config(cfg)
    , m_StateSequence(std::move(emu), m_Config.StateSequenceCfg, initialStates)
    , m_TargetIndex(0)
    , m_LatestIndex(0)
    , m_OnNewStateIndex(-1)
    , m_SequenceThreadShouldStop(false)
{
    m_SequenceThread = std::thread(
            &StateSequenceThread::SequenceThread, this);
}

StateSequenceThread::~StateSequenceThread()
{
    m_SequenceThreadShouldStop = true;
    m_SequenceThread.join();
}

void StateSequenceThread::InputChange(int frameIndex, ControllerState newInput)
{
    std::lock_guard<std::mutex> lock(m_PendingInputsMutex);
    m_PendingInputs.emplace_back(frameIndex, newInput);
}

void StateSequenceThread::InputsChange(const std::vector<ControllerState>& inputs)
{
    std::lock_guard<std::mutex> lock(m_PendingInputsMutex);
    m_PendingReplacementInputs = inputs;
}

void StateSequenceThread::TargetChange(int targetFrameIndex)
{
    m_TargetIndex = targetFrameIndex;
}

void StateSequenceThread::GetLatestFrameIndex(int* frameIndex)
{
    if (frameIndex) {
        *frameIndex = m_LatestIndex;
    }
}

void StateSequenceThread::GetLatestState(std::string* state)
{
    if (state) {
        std::lock_guard<std::mutex> lock(m_LatestMutex);
        *state = m_LatestState;
    }
}

std::shared_ptr<std::string> StateSequenceThread::GetState(int frameIndex)
{
    TargetChange(frameIndex);
    if (m_LatestIndex == frameIndex) {
        std::lock_guard<std::mutex> lock(m_LatestMutex);
        if (m_LatestState.size() != 0) {
            return std::make_shared<std::string>(m_LatestState);
        }
    }
    return nullptr;
}

bool StateSequenceThread::HasNewState(int* frameIndex, std::string* state)
{
    bool ret = false;
    if (m_LatestIndex != m_OnNewStateIndex) {
        for (int i = 0; i < m_Config.TryLockTries; i++) {
            if (m_LatestMutex.try_lock()) {
                if (frameIndex) {
                    *frameIndex = m_LatestIndex;
                }
                if (state) {
                    *state = m_LatestState;
                }
                m_OnNewStateIndex.store(m_LatestIndex.load());
                m_LatestMutex.unlock();

                ret = true;
                break;
            } else {
                if (i != (m_Config.TryLockTries - 1)) {
                    std::this_thread::sleep_for(std::chrono::microseconds(m_Config.TryLockDelayMicros));
                }
            }
        }
    }

    return ret;
}

void StateSequenceThread::SequenceThread()
{
    while (!m_SequenceThreadShouldStop) {
        if (m_StateSequence.GetTargetIndex() != m_TargetIndex) {
            m_StateSequence.SetTargetIndex(m_TargetIndex);
            if (!m_StateSequence.HasWork()) {
                m_OnNewStateIndex = -1;
                {
                    std::lock_guard<std::mutex> lock(m_LatestMutex);
                    m_LatestIndex = m_StateSequence.GetCurrentIndex();
                    m_LatestState = m_StateSequence.GetCurrentStateString();
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock(m_PendingInputsMutex);
            if (!m_PendingInputs.empty()) {
                for (auto & [frameIndex, newState] : m_PendingInputs) {
                    m_StateSequence.SetInput(frameIndex, newState);
                }
                m_PendingInputs.clear();
            }
            if (!m_PendingReplacementInputs.empty()) {
                m_StateSequence.SetInputs(m_PendingReplacementInputs);
                m_PendingReplacementInputs.clear();
            }
        }

        if (m_StateSequence.HasWork()) {
            m_StateSequence.DoWork();
            m_OnNewStateIndex = -1;
            {
                std::lock_guard<std::mutex> lock(m_LatestMutex);
                m_LatestIndex = m_StateSequence.GetCurrentIndex();
                m_LatestState = m_StateSequence.GetCurrentStateString();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(m_Config.OnWorkDelayMillis));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(m_Config.NoWorkDelayMillis));
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

FM2Header FM2Header::Defaults()
{
    FM2Header h;
    h.version = 3;
    h.emuVersion = 22020;
    h.rerecordCount = 0;
    h.palFlag = false;
    h.romFilename = "rom";
    h.romChecksum = "base64:0000000000000/00000000==";
    h.guid = "00000000-0000-0000-0000-000000000000";
    h.fourscore = 0;
    h.microphone = 0;
    h.port0 = 1;
    h.port1 = 0;
    h.port2 = 0;
    h.FDS = false;
    h.NewPPU = false;
    return h;
}

static bool GetLine(std::istream& is, std::string& line)
{
    if (!std::getline(is, line)) {
        return false;
    }
    if (line.length() > 1) {
        if (line.back() == '\r') {
            line.pop_back();
        }
    }

    return true;
}

void sta::nes::ReadFM2File(std::istream& is,
        std::vector<nes::ControllerState>* inputs,
        FM2Header* header)
{
    if (header) {
        *header = FM2Header::Defaults();
    }

    std::string line;
    if (!GetLine(is, line) || line != "version 3") {
        throw std::invalid_argument("unsupported fm2 file");
    }

    int lineNumber = 1;
    bool readingInputs = false;
    if (inputs) {
        inputs->clear();
    }
    bool firstInput = true;
    while (GetLine(is, line)) {
        if (!readingInputs) {
            if (util::StringStartsWith(line, "|")) {
                readingInputs = true;
            } else if (header) {
                if (util::StringStartsWith(line, "emuVersion")) {
                    header->emuVersion = std::stoi(line.substr(11));
                } else if (util::StringStartsWith(line, "rerecordCount")) {
                    header->rerecordCount = std::stoi(line.substr(14));
                } else if (util::StringStartsWith(line, "palFlag")) {
                    header->palFlag = std::stoi(line.substr(8));
                } else if (util::StringStartsWith(line, "romFilename")) {
                    header->romFilename = line.substr(12);
                } else if (util::StringStartsWith(line, "romChecksum")) {
                    header->romChecksum = line.substr(12);
                } else if (util::StringStartsWith(line, "guid")) {
                    header->guid = line.substr(5);
                } else if (util::StringStartsWith(line, "fourscore")) {
                    header->fourscore = std::stoi(line.substr(10));
                } else if (util::StringStartsWith(line, "microphone")) {
                    header->microphone = std::stoi(line.substr(11));
                } else if (util::StringStartsWith(line, "port0")) {
                    header->port0 = std::stoi(line.substr(6));
                } else if (util::StringStartsWith(line, "port1")) {
                    header->port1 = std::stoi(line.substr(6));
                } else if (util::StringStartsWith(line, "port2")) {
                    header->port2 = std::stoi(line.substr(6));
                } else if (util::StringStartsWith(line, "FDS")) {
                    header->FDS = std::stoi(line.substr(4));
                } else if (util::StringStartsWith(line, "NewPPU")) {
                    header->NewPPU = std::stoi(line.substr(7));
                } else {
                    header->additionalLines.push_back(line);
                }
            }
        }

        if (readingInputs) {
            if (!(line.size() == 14 || line.size() == 22)) {
                std::ostringstream os;
                os << "invalid entry on line " << lineNumber << ": '" << line << "'";
                throw std::invalid_argument(os.str());
            } else {
                uint8_t cs = 0;
                for (int button = 0; button < 8; button++) {
                    if (line[10 - button] != '.') {
                        cs |= (1 << button);
                    }
                }

                if (inputs) {
                    if (!firstInput) {
                        inputs->push_back(static_cast<nes::ControllerState>(cs));
                    }
                    firstInput = false;
                }
            }
        }
        lineNumber++;
    }
}

std::string sta::nes::ControllerStateToFM2Line(const nes::ControllerState& state)
{
    static const std::array<char, 8> BUTTONS {
        'R', 'L', 'D', 'U', 'T', 'S', 'B', 'A',
    };
    std::string v = "|0|........|||";

    uint8_t in8 = static_cast<uint8_t>(state);
    for (int i = 0; i < 8; i++) {
        if (in8 & (1 << (7 - i))) {
            v[i + 3] = BUTTONS[i];
        }
    }
    return v;
}

void sta::nes::WriteFM2File(std::ostream& os,
        const std::vector<ControllerState>& inputs,
        const FM2Header& header)
{
    os << "version " << header.version << "\n";
    os << "emuVersion " << header.emuVersion << "\n";
    os << "rerecordCount " << header.rerecordCount << "\n";
    os << "palFlag " << static_cast<int>(header.palFlag) << "\n";
    os << "romFilename " << header.romFilename << "\n";
    os << "romChecksum " << header.romChecksum << "\n";
    os << "guid " << header.guid << "\n";
    os << "fourscore " << static_cast<int>(header.fourscore) << "\n";
    os << "microphone " << static_cast<int>(header.microphone) << "\n";
    os << "port0 " << header.port0 << "\n";
    os << "port1 " << header.port1 << "\n";
    os << "port2 " << header.port2 << "\n";
    os << "FDS " << static_cast<int>(header.FDS) << "\n";
    os << "NewPPU " << static_cast<int>(header.NewPPU) << "\n";
    for (auto & line : header.additionalLines) {
        os << line << "\n";
    }


    auto WriteInputLine = [&](const nes::ControllerState& input){
        os << ControllerStateToFM2Line(input) << "\n";
    };

    WriteInputLine(0);
    for (auto & input : inputs) {
        WriteInputLine(input);
    }
}

