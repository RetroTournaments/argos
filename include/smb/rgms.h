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
#include <thread>

#include "nes/nes.h"
#include "smb/smbdb.h"
#include "util/serial.h"
#include "util/clock.h"

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

namespace argos::rgms {

// ap and block_buffer_84_disc required to discriminate on mazes in 8-4, set ap
// to anything but 0x65-equiv to just do the normal stuff
int AreaPointerXFromData(uint8_t screenedge_pageloc, uint8_t screenedge_x_pos,
        smb::AreaID ap, uint8_t block_buffer_84_disc);

struct SMBNametableDiff
{
    int NametablePage;
    int Offset;
    uint8_t Value;
};

inline constexpr int TITLESCREEN_SCORE_X = 0x02;
inline constexpr int TITLESCREEN_SCORE_Y = 0x03;
inline constexpr int TITLESCREEN_COIN_X = 0x0d;
inline constexpr int TITLESCREEN_COIN_Y = 0x03;
inline constexpr int TITLESCREEN_WORLD_X = 0x13;
inline constexpr int TITLESCREEN_WORLD_Y = 0x03;
inline constexpr int TITLESCREEN_LEVEL_X = 0x15;
inline constexpr int TITLESCREEN_LEVEL_Y = 0x03;
inline constexpr int TITLESCREEN_WORLD2_X = 0x11;
inline constexpr int TITLESCREEN_WORLD2_Y = 0x0a;
inline constexpr int TITLESCREEN_LEVEL2_X = 0x13;
inline constexpr int TITLESCREEN_LEVEL2_Y = 0x0a;
inline constexpr int TITLESCREEN_LIFE_X = 0x11;
inline constexpr int TITLESCREEN_LIFE_Y = 0x0e;

struct SMBFrameInfo
{
    //AreaPointer AP;
    smb::AreaID AID;
    int PrevAPX; // FUCK
    int APX;

    uint8_t GameEngineSubroutine;
    uint8_t OperMode;
    uint8_t IntervalTimerControl;
    std::vector<nes::OAMxEntry> OAMX;
    std::vector<SMBNametableDiff> NTDiffs;

    std::vector<uint8_t> TopRows; // 32 * 4 bytes of nametable info, then 32 bytes of attributes.. GOD

    uint8_t World;
    uint8_t Level;

    struct {
        std::array<uint8_t, 7> ScoreTiles;
        std::array<uint8_t, 2> CoinTiles;
        uint8_t WorldTile;
        uint8_t LevelTile;
        std::array<uint8_t, 2> LifeTiles;
    } TitleScreen;

    int Time;

    uint8_t PauseSoundQueue;
    uint8_t AreaMusicQueue;
    uint8_t EventMusicQueue;
    uint8_t NoiseSoundQueue;
    uint8_t Square2SoundQueue;
    uint8_t Square1SoundQueue;
};

struct SMBMessageProcessorOutput
{
    int64_t Elapsed;

    bool ConsolePoweredOn;

    uint64_t M2Count;
    uint64_t UserM2;
    nes::ControllerState Controller;

    SMBFrameInfo Frame;
    nes::FramePalette FramePalette;
};
typedef std::shared_ptr<SMBMessageProcessorOutput> SMBMessageProcessorOutputPtr;
void ClearSMBMessageProcessorOutput(SMBMessageProcessorOutput* output);

void OutputToBytes(SMBMessageProcessorOutputPtr ptr, std::vector<uint8_t>* buffer);
SMBMessageProcessorOutputPtr BytesToOutput(const uint8_t* bytes, size_t size);

bool OutputPtrsEqual(SMBMessageProcessorOutputPtr a, SMBMessageProcessorOutputPtr b);

// Process the messages from the internesceptor into usable SMB information
class SMBMessageProcessor
{
public:
    SMBMessageProcessor(smb::SMBNametableCachePtr nametables);
    ~SMBMessageProcessor();

    void Reset();

    // The usable information
    bool OnMessage(const internesceptor::MessageParseInfo& message, int64_t elapsed);
    SMBMessageProcessorOutputPtr GetLatestProcessorOutput() const;

private:
    static void SetOutputFromNESMessageState(const internesceptor::NESMessageState& nes,
            SMBMessageProcessorOutput* output, smb::SMBNametableCachePtr backgroundNametables, const std::array<uint8_t, 6>& soundQueues);

private:
    smb::SMBNametableCachePtr m_BackgroundNametables;
    internesceptor::NESMessageState m_NESState;
    SMBMessageProcessorOutput m_Output;

    uint64_t m_LastOutM2;
    int m_PrevAPX;

    std::array<uint8_t, 6> m_SoundQueues;
};

////////////////////////////////////////////////////////////////////////////////

inline constexpr int SMB_SERIAL_BAUD = 4000000;
class SMBSerialProcessor
{
public:
    SMBSerialProcessor(smb::SMBNametableCachePtr nametables, int maxFramesStored);
    ~SMBSerialProcessor();

    void Reset();

    // Returns message count within these bytes
    int OnBytes(const uint8_t* buffer, size_t size, bool* obtainedNewOutput, int64_t* elapsed);

    int GetErrorCount() const;
    int GetMessageCount() const;

    SMBMessageProcessorOutputPtr GetLatestProcessorOutput() const;
    SMBMessageProcessorOutputPtr GetNextProcessorOutput() const;

private:
    bool OnMessage(const internesceptor::MessageParseInfo& message, int64_t elapsed);

private:
    SMBMessageProcessor m_MessageProcessor;

    int m_MaxFramesStored;
    mutable std::deque<SMBMessageProcessorOutputPtr> m_OutputDeck;

    internesceptor::MessageParseInfo m_Message;
    int m_ErrorCount;
    int m_MessageCount;
};

////////////////////////////////////////////////////////////////////////////////

struct SMBSerialProcessorThreadParameters
{
    int Baud;
    int BufferSize;
    int MaxFramesStored;

    static SMBSerialProcessorThreadParameters Defaults();
};

struct SMBSerialProcessorThreadInfo
{
    std::string InformationString;

    int ByteCount;
    double ApproxBytesPerSecond;
    int MessageCount;
    double ApproxMessagesPerSecond;
    int ErrorCount;
};

class ISMBSerialSource
{
public:
    ISMBSerialSource();
    virtual ~ISMBSerialSource();

    virtual SMBMessageProcessorOutputPtr GetLatestProcessorOutput() = 0;
    virtual SMBMessageProcessorOutputPtr GetNextProcessorOutput() = 0;
};

class SMBSerialProcessorThread : public ISMBSerialSource
{
public:
    SMBSerialProcessorThread(const std::string& path,
            smb::SMBNametableCachePtr nametables,
            SMBSerialProcessorThreadParameters params = SMBSerialProcessorThreadParameters::Defaults());
    ~SMBSerialProcessorThread();

    void GetInfo(SMBSerialProcessorThreadInfo* info);
    virtual SMBMessageProcessorOutputPtr GetLatestProcessorOutput() override;

    // Similar to LiveVideoThread but hmmm little different becase I'm not sure
    // how I want to do this yet. For now I am counting on the fact that there
    // is only one consumer that wants to see each and every frame (instead of
    // just looking at the latest which can be done by lots of people)
    virtual SMBMessageProcessorOutputPtr GetNextProcessorOutput() override;

    bool IsRecording(std::string* recordingPath = nullptr) const;
    void StartRecording(const std::string& recordingPath);
    void StopRecording();

    void SetLatency(int milliseconds);

private:
    void SerialThread();

    std::string m_InformationString;
    mutable std::mutex m_OutputMutex;
    SMBMessageProcessorOutputPtr m_OutputLatest;
    mutable std::deque<SMBMessageProcessorOutputPtr> m_OutputNext;

    std::atomic<bool> m_IsRecording;
    std::string m_RecordingPath;

    std::atomic<bool> m_ShouldStop;
    std::thread m_WatchingThread;

    std::atomic<int> m_Latency;

    argos::util::SimpleSerialPort t_SerialPort;
    std::vector<uint8_t> t_Buffer;
    SMBSerialProcessor t_SerialProcessor;
    argos::util::SimpleRateEstimator t_MessageRateEstimator;
    argos::util::SimpleRateEstimator t_ByteRateEstimator;
    std::unique_ptr<std::ofstream> t_OutputStream;
    util::mclock::time_point t_RecStart;

    std::atomic<int> m_ByteCount;
    std::atomic<int> m_ErrorCount;
    std::atomic<int> m_MessageCount;
    std::atomic<double> m_ApproxBytesPerSecond;
    std::atomic<double> m_ApproxMessagesPerSecond;
};


class SMBSerialRecording : public ISMBSerialSource
{
public:
    SMBSerialRecording(const std::string& path,
            smb::SMBNametableCachePtr nametables);
    ~SMBSerialRecording();

    void Reset();
    void ResetToStartAndPause();

    bool GetPaused();
    void SetPaused(bool pause);

    std::string GetPath() const;
    size_t GetNumBytes() const;

    virtual SMBMessageProcessorOutputPtr GetLatestProcessorOutput() override;
    virtual SMBMessageProcessorOutputPtr GetNextProcessorOutput() override;

    void GetAllOutputs(std::vector<SMBMessageProcessorOutputPtr>* outputs);

    void SeekFromStartTo(int64_t millis);
    bool Done() const;

private:
    static void PreStep(const uint8_t* data, size_t dataIndex, int64_t* elapsed, size_t* read, const uint8_t** byteData);
    static void DoStep(const uint8_t* data, size_t* dataIndex, SMBSerialProcessor* proc);
    void Seek();
    void SeekTo(int64_t millis);

private:
    std::string m_Path;
    std::vector<uint8_t> m_Data;
    size_t m_DataIndex;

    bool m_IsPaused;
    util::mclock::time_point m_Start;
    int64_t m_AddToSeek;
    int64_t m_StartMillis;

    smb::SMBNametableCachePtr m_Nametables;
    SMBSerialProcessor m_SerialProcessor;
};

class SMBZMQRef : public ISMBSerialSource
{
public:
    SMBZMQRef(const std::string& path, smb::SMBNametableCachePtr nametables);
    ~SMBZMQRef();

    virtual SMBMessageProcessorOutputPtr GetLatestProcessorOutput() override;
    virtual SMBMessageProcessorOutputPtr GetNextProcessorOutput() override;

private:
    size_t m_tag;
};




}

#endif
