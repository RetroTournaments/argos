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

#ifndef ARGOS_NES_NES_HEADER
#define ARGOS_NES_NES_HEADER

#include <array>
#include <mutex>
#include <atomic>
#include <thread>
#include <memory>
#include <vector>
#include <sstream>
#include <functional>

namespace argos::nes
{

inline constexpr int FRAME_WIDTH  = 256;
inline constexpr int FRAME_HEIGHT = 240;
inline constexpr int FRAME_SIZE = FRAME_WIDTH * FRAME_HEIGHT;
typedef std::array<uint8_t, FRAME_SIZE> Frame;

inline constexpr int NTSC_FPS_NUMERATOR = 39375000;
inline constexpr int NTSC_FPS_DENOMINATOR = 655171;
inline double NTSC_FPS = 
    static_cast<double>(NTSC_FPS_NUMERATOR) / static_cast<double>(NTSC_FPS_DENOMINATOR);
// www.nesdev.org/wiki/cpu
inline constexpr int NTSC_SUBCARRIER_FREQUENCY_NUMERATOR_MHZ = 315;
inline constexpr int NTSC_SUBCARRIER_FREQUENCY_DENOMINATOR_MHZ = 88;
inline constexpr int NTSC_CLOCK_DIVISOR = 12;
inline double NTSC_US_PER_M2 = 
    static_cast<double>(NTSC_SUBCARRIER_FREQUENCY_DENOMINATOR_MHZ * 2) /
    static_cast<double>(NTSC_SUBCARRIER_FREQUENCY_NUMERATOR_MHZ);
inline double NTSC_MS_PER_M2 = NTSC_US_PER_M2 / 1000.0;

// The full NES Palette of 64 colors
// YES I AM IGNORING EMPHASIS FOR NOW. SUE ME
inline constexpr int PALETTE_ENTRIES =  64;
inline constexpr int BYTES_PER_PALETTE_ENTRY = 3;
inline constexpr int PALETTE_SIZE = PALETTE_ENTRIES * BYTES_PER_PALETTE_ENTRY; // 0x00C0
inline constexpr uint8_t PALETTE_ENTRY_BLACK = 0x0f;
inline constexpr uint8_t PALETTE_ENTRY_WHITE = 0x20;
typedef std::array<uint8_t, PALETTE_SIZE> Palette;
const Palette& DefaultPaletteRGB();
const Palette& DefaultPaletteBGR();

// Pattern tables (typically from chr data, controlled via a mapper)
inline constexpr int PATTERNTABLE_SIZE = 0x1000;
inline constexpr int PATTERNTABLE_ENTRY_SIZE = 0x0010;
inline constexpr int PATTERNTABLE_NUM_ENTRIES = 64;
typedef std::array<uint8_t, PATTERNTABLE_SIZE> PatternTable;

// Name tables (often updated by CPU)
// The conventional nametable is 32 x 30 (256 x 240)
inline constexpr int NAMETABLE_SIZE = 0x0400; // includes attributes
inline constexpr int NAMETABLE_ATTRIBUTE_OFFSET = 0x03c0;
typedef std::array<uint8_t, NAMETABLE_SIZE> NameTable;
inline constexpr int NAMETABLE_WIDTH_BYTES = 32;
inline constexpr int NAMETABLE_HEIGHT_BYTES = 30;

// The NES does not allow the full palette to be used at once
// $0000 -       [$3f00        ]    Universal background color
// $0001 - $0003 [$3f01 - $3f03]    Background palette 0
// $0005 - $0007 [$3f05 - $3f07]    Background palette 1
// ...
// $0011 - $0013 [$3f11 - $3f13]    Sprite palette 1
inline constexpr int FRAMEPALETTE_SIZE = 0x20;
typedef std::array<uint8_t, FRAMEPALETTE_SIZE> FramePalette;
// The name is a bit of a misnomer, because the palette can actually be changed
// within a frame 

inline constexpr int RAM_SIZE = 0x0800;
typedef std::array<uint8_t, RAM_SIZE> Ram;

inline constexpr int NUM_OAM_ENTRIES = 64;
inline constexpr int OAM_SIZE = 64 * 4;
typedef std::array<uint8_t, OAM_SIZE> Oam;
struct OamEntry
{
    uint8_t y;
    uint8_t tile_index;
    uint8_t attributes;
    uint8_t x;
};
enum OAMAttributeBits : uint8_t
{
    OAM_PALETTE         = 0b00000011,
    OAM_PRIORITY        = 0b00100000, // 0 in front, 1 behind
    OAM_FLIP_HORIZONTAL = 0b01000000,
    OAM_FLIP_VERTICAL   = 0b10000000,
};


typedef uint8_t ControllerState;
enum Button : uint8_t
{
    A       = 0x01,
    B       = 0x02,
    SELECT  = 0x04,
    START   = 0x08,
    UP      = 0x10,
    DOWN    = 0x20,
    LEFT    = 0x40,
    RIGHT   = 0x80,
};
void SetControllerStateButtonPressed(ControllerState* state, Button button, bool pressed);
void ToggleControllerStateButton(ControllerState* state, Button button);

// Currently a very minimal subset of the nintendo entertainment system is supported
// for efforts on NTSC roms

// throws on error
class INESEmulator
{
public:
    INESEmulator();
    virtual ~INESEmulator();

    // The standard INES type (check nesdev.wiki)
    virtual void LoadINES(std::istream& is) = 0;

    virtual void LoadINESFile(const std::string& path) final;
    virtual void LoadINESString(const std::string& contents) final;
    virtual void LoadINESData(const unsigned char rom[], int size) final;

    // These only have to work with INESEmulators of the same type
    virtual void SaveState(std::ostream& os) const = 0;
    virtual void LoadState(std::istream& is) = 0;

    virtual void SaveStateFile(const std::string& path) const final;
    virtual void LoadStateFile(const std::string& path) final;
    virtual void SaveStateString(std::string* contents) const final;
    virtual void LoadStateString(const std::string& contents) final;

    virtual void Reset(bool isHardReset = true) = 0;
    // Advance the emulator exactly one frame
    virtual void Execute(const ControllerState& player1 = 0x00) = 0;

    // The number of frames since power on
    virtual uint64_t CurrentFrame() const = 0;

    // CPU memory map
    // $0000 - $07FF    2KB internal RAM
    // $2000 - $2007    PPU registers
    // $4000 - $4017    NES APU and I/O registers
    // $4020 - $FFFF    Cartridge space (prg rom, prg ram, mapper registers)
    // #8000 - #FFFF    common rom
    virtual uint8_t CPUPeek(uint16_t address) const = 0;
    virtual void CPUPeekMult(uint16_t address, uint16_t cnt, uint8_t* out) const;
    virtual void CPUPeekRam(Ram* ram) const;

    // PPU memory map
    // $0000 - $0fff    Pattern table 0
    // $1000 - $1fff    Pattern table 1
    // $2000 - $23ff    Nametable 0
    // $2400 - $27ff    Nametable 1
    // $2800 - $2bff    Nametable 2
    // $2c00 - $2fff    Nametable 3
    // $3f00 - $3f1f    Palette RAM indices
    virtual uint8_t PPUPeek8(uint16_t address) const = 0;
    virtual void PPUPeekPatternTable(int tableIndex, PatternTable* table) const;
    virtual void PPUPeekNameTable(int tableIndex, NameTable* table) const;
    virtual void PPUPeekFramePalette(FramePalette* fpal) const;

    virtual uint8_t ScreenPeekPixel(int x, int y) const = 0;
    virtual void ScreenPeekFrame(Frame* frame) const;

    // OAM memory map
    // Array of 64 entries:
    //  Sprite Y Coordinate
    //  Sprite tile #
    //  Sprite attribute
    //  Sprite X Coordinate
    //
    // NOTE: PREFER READING THIS INFO FROM RAM
    //  (find by looking at writes to $4014 (OAMDMA))
    // NOTE: this seems like a #badidea given that on original hardware OAM
    //  decays to randomness over time
    // NOTE: yolo
    virtual uint8_t OAMPeek8(uint8_t address) const = 0;
};

////////////////////////////////////////////////////////////////////////////////
class NESEmulatorFactory
{
public:
    NESEmulatorFactory(std::function<std::unique_ptr<INESEmulator>()> emufactory,
        std::string defaultINESString = std::string(),
        std::string defaultStateString = std::string()
    );
    ~NESEmulatorFactory();

    std::unique_ptr<INESEmulator> GetEmu(
        bool loadDefaultINES = true,
        bool loadDefaultState = true
    );

    const std::string& GetDefaultINESString() const;
    void SetDefaultINESString(const std::string& defaultINESString);

    const std::string& GetDefaultStateString() const;
    void SetDefaultStateString(const std::string& defaultStateString);

private:
    std::function<std::unique_ptr<nes::INESEmulator>()> m_EmulatorFactory;
    std::string m_DefaultINESString;
    std::string m_DefaultStateString;

};
typedef std::shared_ptr<NESEmulatorFactory> NESEmulatorFactorySPtr;

////////////////////////////////////////////////////////////////////////////////

// Idea is to have an emulator wrapper that maintains a sequence of saved states
// for the TAS Editing side of things.
struct StateSequenceConfig
{
    int SaveInterval;

    //
    static StateSequenceConfig Defaults();
};

class StateSequence
{
public:
    StateSequence(std::unique_ptr<INESEmulator>&& emulator,
        StateSequenceConfig config = StateSequenceConfig::Defaults(),
        const std::vector<ControllerState>& initialStates = std::vector<ControllerState>()
    );
    ~StateSequence();

    bool HasWork() const;
    void DoWork();

    void SetInputs(const std::vector<ControllerState>& inputs);
    const std::vector<ControllerState>& GetInputs() const;
    ControllerState GetInput(int frameIndex) const;
    void SetInput(int frameIndex, ControllerState newState);

    void SetTargetIndex(int targetIndex);
    int GetTargetIndex() const;

    int GetCurrentIndex() const;
    std::string GetCurrentStateString() const;

    // Modifies target, and does work until we have it.
    std::string GetStateString(int frameIndex);
    void SetEmu(int frameIndex, INESEmulator* emu);

private:
    void SaveCurrentState();

private:
    StateSequenceConfig m_Config;

    int m_CurrentIndex;
    std::unique_ptr<INESEmulator> m_Emulator;

    struct SaveState {
        int Index;
        std::string StateString;
    };
    std::vector<SaveState> m_SaveStates; // sorted by Frame, obviously not all states
    std::vector<ControllerState> m_Inputs;

    int m_TargetIndex;
};

// An emulator on a thread that wraps a state sequence
struct StateSequenceThreadConfig
{
    int OnWorkDelayMillis;
    int NoWorkDelayMillis;
    int TryLockDelayMicros;
    int TryLockTries;
    StateSequenceConfig StateSequenceCfg;

    static StateSequenceThreadConfig Defaults();
};

class StateSequenceThread
{
public:
    StateSequenceThread(StateSequenceThreadConfig cfg,
            std::unique_ptr<INESEmulator>&& emu,
            const std::vector<ControllerState>& initialStates = std::vector<ControllerState>());
    ~StateSequenceThread();

    void InputChange(int frameIndex, ControllerState newInput);
    void InputsChange(const std::vector<ControllerState>& inputs);
    void TargetChange(int targetFrameIndex);

    // For convenience remembers the last state returned ONLY FROM THIS FUNCTION
    bool HasNewState(int* frameIndex, std::string* state);

    // The latest is always available (might be 0 / empty)
    void GetLatestFrameIndex(int* frameIndex);
    void GetLatestState(std::string* state);

    std::shared_ptr<std::string> GetState(int frameIndex); // may be null

private:
    void SequenceThread();

private:
    StateSequenceThreadConfig m_Config;
    StateSequence m_StateSequence;

    std::atomic<int> m_OnNewStateIndex;

    std::mutex m_PendingInputsMutex;
    std::vector<std::pair<int, nes::ControllerState>> m_PendingInputs;
    std::vector<nes::ControllerState> m_PendingReplacementInputs;
    std::atomic<int> m_TargetIndex;

    mutable std::mutex m_LatestMutex;
    std::string m_LatestState;
    std::atomic<int> m_LatestIndex;
    std::atomic<bool> m_SequenceThreadShouldStop;
    std::thread m_SequenceThread;
};

////////////////////////////////////////////////////////////////////////////////

struct FM2Header
{
    int version;                // for now it is always 3
    int emuVersion;             // for now it is always 22020
    int rerecordCount;          // for now always 0
    bool palFlag;               // for now always 0
    std::string romFilename;    // ex: "Super Mario Bros. (JU) [!]"
    std::string romChecksum;    // ex: "base64:jjYwGG411HcjG/j9UOVM3Q=="
    std::string guid;           // ex: "00000000-0000-0000-0000-000000000000"
    bool fourscore;             // for now always 0
    bool microphone;            // for now always 0
    int port0;                  // for now always 1
    int port1;                  // for now always 0
    int port2;                  // for now always 0
    bool FDS;                   // for now always 0
    bool NewPPU;                // for now always 0
    std::vector<std::string> additionalLines;

    static FM2Header Defaults();
};

void ReadFM2File(std::istream& is,
        std::vector<nes::ControllerState>* inputs,
        FM2Header* header);
void WriteFM2File(std::ostream& os,
        const std::vector<nes::ControllerState>& inputs,
        const FM2Header& header);
std::string ControllerStateToFM2Line(const nes::ControllerState& state);

}

#endif

