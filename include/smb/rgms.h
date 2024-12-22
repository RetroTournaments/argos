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

#include "nlohmann/json.hpp"

#include "argos/config.h"
#include "nes/nes.h"
#include "nes/nesdb.h"
#include "nes/nestopiaimpl.h"
#include "smb/smbdb.h"
#include "rgmui/rgmui.h"
#include "util/serial.h"
#include "util/clock.h"
#include "util/rect.h"
#include "ext/sdlext/sdlext.h"

namespace argos::util {

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Vector2F,
    x, y);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Rect2F,
    X, Y, Width, Height);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Rect2I,
    X, Y, Width, Height);

}

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
namespace argos::rgms {

enum RaceCategory : uint8_t
{
    ANY_PERCENT = 0x00,
    WARPLESS    = 0x01,
};
NLOHMANN_JSON_SERIALIZE_ENUM(RaceCategory, {
    {RaceCategory::ANY_PERCENT, "any_percent"},
    {RaceCategory::WARPLESS, "warpless"},
});

struct PlayerColors
{
    uint8_t RepresentativeColor;
    uint8_t OutlineColor;
    uint8_t MarioColors[3];
    uint8_t FireMarioColors[3];
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PlayerColors, RepresentativeColor, OutlineColor, MarioColors, FireMarioColors);
void InitializePlayerColors(PlayerColors* colors, bool mario = true);

bool IsMarioTile(uint8_t tileIndex);

struct ControllerColors
{
    ImVec4 Base;        // some kind of light gray
    ImVec4 BaseDark;    // some kind of light gray
    ImVec4 Dark;        // some kind of near black
    ImVec4 Accent;      // a middling gray
    ImVec4 Black;       // for the holes etc.
    // ImVec4 Text;        // a dark red TODO

    ImVec4 ABButton;    // a light red
    ImVec4 STButton;    // darkish black
    ImVec4 DPad;        // darkish black
    ImVec4 Pressed;     // maybe a green?

    static ControllerColors Defaults();
};

inline constexpr int NUM_ACCENTS = 4;
struct ControllerGeometry
{
    struct {
        float Width;
        float Height;
        float Radius;

        struct {
            float CenterX;
            float Width;
            float Height;
        } ControllerPort;
    } Boundary;

    struct {
        // Offset from the edge of the controller to three edges of the dark region
        float Offset;
        float Height;
        float Radius;
    } DarkRegion;

    struct {
        float CenterX;
        float CenterY;
        float PadDiameter;
        float PadWidth;
        float Radius;

        struct {
            float Thickness;
            float Radius;
        } Outline;
    } DPad;

    struct {
        float CenterX;
        float Width;
        float AccentRadius;
        struct {
            float TY; //
            float Height; // Last one special case
            ImDrawFlags Rounding;
        } Accent[NUM_ACCENTS];

        struct {
            float CenterY;
            float Height;

            struct {
                float OffsetX;
                float Width;
                float Height;
            } Button;
        } ButtonInset;
    } SelectStart;

    struct {
        float CenterX;
        float CenterY;
        float OutsideWidth;

        float ButtonDiameter;

        struct {
            float Width;
            float Radius;
        } Accent;
    } ABButtons;

    static ControllerGeometry Defaults();
};

struct DogboneGeometry
{
    struct {
        float Width;
        float Height;
        float BetweenCentersWidth;
        float InnerHeight;

        struct {
            float Width;
            float Height;
        } ControllerPort;
    } Boundary;

    struct {
        float PadDiameter;
        float PadWidth;
        float Radius;

        struct {
            float Thickness;
            float Radius;
        } Outline;
    } DPad;

    struct {
        struct {
            float Width;
            float Height;
            float Angle;
        } Button;

        float InterDistance;
    } SelectStart;

    struct {
        float Angle;
        float InterDistance;
        float ButtonDiameter;
    } ABButtons;

    static DogboneGeometry Defaults();
};

typedef std::pair<ImVec2, ImVec2> ButtonLocation;
typedef std::vector<ButtonLocation> ButtonLocations;
ImVec2 DrawController(
        nes::ControllerState state,
        const ControllerColors& colors,
        const ControllerGeometry& geometry,
        float x, float y, float scale,
        ImDrawList* list,
        ButtonLocations* buttons);
ImVec2 DrawDogbone(
        nes::ControllerState state,
        const ControllerColors& colors,
        const DogboneGeometry& geometry,
        float x, float y, float scale,
        ImDrawList* list,
        ButtonLocations* buttons);

struct NESControllerComponentOptions
{
    ControllerColors Colors;
    ControllerGeometry Geometry;
    DogboneGeometry DogGeometry;
    bool IsDogbone;
    float Scale;
    float ButtonPad;

    bool AllowEdits;

    static NESControllerComponentOptions Defaults();
};

enum class ControllerType
{
    BRICK,
    DOGBONE
};
NLOHMANN_JSON_SERIALIZE_ENUM(ControllerType, {
    {ControllerType::BRICK, "brick"},
    {ControllerType::DOGBONE, "dogbone"},
})

class NESControllerComponent : public rgmui::IApplicationComponent
{
public:
    NESControllerComponent(const std::string& windowName);
    ~NESControllerComponent();

    virtual void OnFrame() override;

    static bool Controls(nes::ControllerState* state, NESControllerComponentOptions* options);

    nes::ControllerState& GetState();
    const nes::ControllerState& GetState() const;

    NESControllerComponentOptions& GetOptions();
    const NESControllerComponentOptions& GetOptions() const;

private:
    std::string m_WindowName;
    NESControllerComponentOptions m_Options;
    nes::ControllerState m_ControllerState;
};

////////////////////////////////////////////////////////////////////////////////
// A fundamental part of RGMS is to take in video data (see rgmvideo!), and then
// take a /part/ of that as the area of interest. It also might be important to
// process it via a perspective or 'uncrt' transform - however that is less
// common for the actual competitions
enum class AreaOfInterestType {
    NONE,
    SCALE,
    CROP,
    PERSPECTIVE,
    UNCRT
};
NLOHMANN_JSON_SERIALIZE_ENUM(AreaOfInterestType, {
    {AreaOfInterestType::NONE, "none"},
    {AreaOfInterestType::SCALE, "scale"},
    {AreaOfInterestType::CROP, "crop"},
    {AreaOfInterestType::PERSPECTIVE, "perspective"},
    {AreaOfInterestType::UNCRT, "uncrt"},
})

struct AreaOfInterest {
    AreaOfInterestType Type;
    util::Rect2F Crop;
    util::Quadrilateral2F Quad; // In FFMPEG order!
    int Patch;
    //util::BezierPatch Patch;
    int OutWidth, OutHeight;

    static AreaOfInterest DefaultRect(int width, int height);
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AreaOfInterest,
        Type, Crop, Quad, Patch,
        OutWidth, OutHeight);
void InitAOIRect(AreaOfInterest* aoi, int width, int height);
void InitAOICrop(AreaOfInterest* aoi, int x, int y, int width, int height, int outwidth, int outheight);

//cv::Mat ExtractAOI(cv::Mat input, const AreaOfInterest& aoi, const video::PixelContributions* contrib = nullptr);

struct NamedAreaOfInterest {
    std::string Name;
    AreaOfInterest AOI;
};
int ScanDirectoryForAOIs(const std::string& directory, std::vector<NamedAreaOfInterest>* aois);

}

namespace argos::rgms {

// A simple single 'window' (ImGui::Begin/ImGui::End block) that has the 'x' in
// the corner and indicates to the owner if that x was closed.
class ISMBCompSimpleWindowComponent : public rgmui::IApplicationComponent
{
public:
    ISMBCompSimpleWindowComponent(std::string windowName);
    virtual ~ISMBCompSimpleWindowComponent();

    virtual void OnFrame() override final;
    bool WindowWasClosedLastFrame();

    virtual void DoControls() = 0;

private:

    std::string m_WindowName;
    bool m_WasClosed;
};

// A 'container' for several windows with no begin/end itself, but instead a
// toggle-able entry into the main menu
class ISMBCompSimpleWindowContainerComponent : public rgmui::IApplicationComponent
{
public:
    ISMBCompSimpleWindowContainerComponent(std::string menuName, bool startsOpen);
    ~ISMBCompSimpleWindowContainerComponent();

    virtual void OnFrame() override final;
    void DoMenuItem();

protected:
    void RegisterWindow(std::shared_ptr<ISMBCompSimpleWindowComponent> window);

private:
    std::string m_MenuName;
    bool m_IsOpen;
    std::vector<std::shared_ptr<ISMBCompSimpleWindowComponent>> m_Windows;
};

class ISMBCompSingleWindowComponent : public rgmui::IApplicationComponent
{
public:
    ISMBCompSingleWindowComponent(std::string menuName, std::string windowName, bool startsOpen);
    ~ISMBCompSingleWindowComponent();

    virtual void OnFrame() override final;
    virtual void DoMenuItem() final;
    virtual void OnFrameAlways();

    virtual void DoControls() = 0;

private:
    std::string m_WindowName;
    std::string m_MenuName;
    bool m_IsOpen;
};


////////////////////////////////////////////////////////////////////////////////

struct SMBComp;
struct SMBCompVisuals;
struct SMBCompStaticData;


struct SMBCompPlayerNames
{
    std::string ShortName;
    std::string FullName;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SMBCompPlayerNames, ShortName, FullName);

struct SMBCompPlayerVideoInput
{
    std::string Path;
    util::Rect2I Crop;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SMBCompPlayerVideoInput, Path, Crop);

struct SMBCompPlayerAudioInput
{
    std::string Path;
    std::string Format; // "PA_SAMPLE_S16LE"
    int Channels;       // 2
    int Rate;           // 44100
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SMBCompPlayerAudioInput, Path, Format, Channels, Rate);

struct SMBCompPlayerSerialInput
{
    std::string Path;
    int Baud;           // 40000000
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SMBCompPlayerSerialInput, Path, Baud);

struct SMBCompPlayerInputs
{
    SMBCompPlayerVideoInput Video;
    SMBCompPlayerAudioInput Audio;
    SMBCompPlayerSerialInput Serial;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SMBCompPlayerInputs, Video, Audio, Serial);
void InitializeSMBCompPlayerInputs(SMBCompPlayerInputs* inputs);

struct SMBCompPlayer
{
    uint32_t UniquePlayerID;

    SMBCompPlayerNames Names;
    argos::rgms::PlayerColors Colors;
    SMBCompPlayerInputs Inputs;
    argos::rgms::ControllerType ControllerType;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SMBCompPlayer, UniquePlayerID, Names, Colors, Inputs, ControllerType);
void InitializeSMBCompPlayer(SMBCompPlayer* player);

struct SMBCompPlayers
{
    std::vector<SMBCompPlayer> Players; // Prefer free functions below
    std::vector<uint32_t> InvalidPlayerIDs; // don't serialize
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SMBCompPlayers, Players);
void InitializeSMBCompPlayers(SMBCompPlayers* players);

uint32_t GetNewUniquePlayerID(const SMBCompPlayers& players); // Used when adding a new player
void AddNewPlayer(SMBCompPlayers* players, const SMBCompPlayer& player);
void RemovePlayer(SMBCompPlayers* players, uint32_t uniqueID);
const SMBCompPlayer* FindPlayer(const SMBCompPlayers& players, uint32_t uniqueID);

class SMBCompConfigurationPlayersComponent : public ISMBCompSimpleWindowComponent
{
public:
    SMBCompConfigurationPlayersComponent(argos::RuntimeConfig* info, SMBCompPlayers* players,
            const SMBCompVisuals* visuals);
    ~SMBCompConfigurationPlayersComponent();

    void DoControls() override;

    static bool PlayerNamesEditingControls(SMBCompPlayerNames* names);
    static bool PlayerColorsEditingPopup(argos::rgms::PlayerColors* colors, const SMBCompVisuals* visuals);
    typedef std::function<void(std::vector<NamedAreaOfInterest>*, bool)> GetAOIcback;
    static bool PlayerInputsVideoEditingPopup(SMBCompPlayer* player, GetAOIcback cback);
    static bool PlayerInputsEditingControls(SMBCompPlayer* player, GetAOIcback cback = nullptr);
    static bool PlayerEditingControls(const char* label, SMBCompPlayer* player, const SMBCompVisuals* visuals, GetAOIcback cback = nullptr);

private:
    argos::RuntimeConfig* m_Info;
    bool m_FirstInputs;
    std::vector<NamedAreaOfInterest> m_NamedAreasOfInterest;
    void GetNamedAreasOfInterest(std::vector<NamedAreaOfInterest>* aois, bool refresh);

private:
    SMBCompPlayers* m_Players;
    const SMBCompVisuals* m_Visuals;

    SMBCompPlayer m_PendingPlayer;
};

struct SMBCompVisuals
{
    nes::Palette Palette;
    int Scale;
    float OtherAlpha;
    float OutlineRadius;
    bool OutlineType;
    bool UsePlayerColors;
    float PlayerNameAlpha;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SMBCompVisuals, Palette, Scale, OtherAlpha, OutlineRadius, OutlineType, UsePlayerColors, PlayerNameAlpha);
void InitializeSMBCompVisuals(SMBCompVisuals* visuals);

class SMBCompConfigurationVisualsComponent : public ISMBCompSimpleWindowComponent
{
public:
    SMBCompConfigurationVisualsComponent(SMBCompVisuals* visuals);
    ~SMBCompConfigurationVisualsComponent();

    void DoControls();

private:
    SMBCompVisuals* m_Visuals;
};

////////////////////////////////////////////////////////////////////////////////

struct SMBCompTournament
{
    std::string DisplayName;
    std::string ScoreName;
    std::string FileName;

    //argos::rgms::RaceCategory Category;

    std::vector<SMBCompPlayerSerialInput> Seats;
    std::vector<SMBCompPlayer> Players;

    std::vector<int> PointIncrements; // num seats in size
    int DNFInc;
    int DNSInc;

    std::vector<std::vector<int>> Schedule;

    std::string TowerName;
    int CurrentRound;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SMBCompTournament,
    DisplayName, ScoreName, FileName,
    //Category,
    Seats, Players, PointIncrements, DNFInc, DNSInc, Schedule
);
void InitializeSMBCompTournament(SMBCompTournament* tournament);

struct SMBCompConfiguration
{
    SMBCompTournament Tournament;
    SMBCompVisuals Visuals;
    SMBCompPlayers Players;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SMBCompConfiguration, Tournament, Visuals, Players);
void InitializeSMBCompConfiguration(SMBCompConfiguration* config);

struct NamedSMBCompConfiguration {
    std::string Name;
    SMBCompConfiguration Config;
};
#define RGMS_SMB_CONFIG_EXTENSION ".smbconfig.json"
int ScanDirectoryForSMBCompConfigurations(const std::string& directory, std::vector<NamedSMBCompConfiguration>* configs);

class SMBCompConfigurationSaveLoadComponent : public ISMBCompSimpleWindowComponent
{
public:
    SMBCompConfigurationSaveLoadComponent(argos::RuntimeConfig* info, SMBCompConfiguration* config);
    ~SMBCompConfigurationSaveLoadComponent();

    void DoControls();
    void LoadNamedConfig(const std::string& name);

private:
    SMBCompConfiguration* m_Config;
    argos::RuntimeConfig* m_Info;

    std::string m_PendingName;
    std::string m_LastName;

    bool m_FirstLoad;
    std::vector<NamedSMBCompConfiguration> m_KnownConfigs;
};

class SMBCompConfigurationTournamentComponent : public ISMBCompSimpleWindowComponent
{
public:
    SMBCompConfigurationTournamentComponent(SMBComp* comp, const SMBCompStaticData* staticData);
    ~SMBCompConfigurationTournamentComponent();

    void DoControls();

private:
    SMBComp* m_Comp;
    const SMBCompStaticData* m_StaticData;
};


class SMBCompConfigurationComponent : public ISMBCompSimpleWindowContainerComponent
{
public:
    SMBCompConfigurationComponent(argos::RuntimeConfig* info, SMBComp* comp, const SMBCompStaticData* staticData);
    ~SMBCompConfigurationComponent();

    void LoadNamedConfig(const std::string& name);

private:
    std::shared_ptr<SMBCompConfigurationSaveLoadComponent> m_SaveLoadComponent;
};

////////////////////////////////////////////////////////////////////////////////

class SMBCompTournamentManagerWindow;

class SMBCompTournamentComponent : public ISMBCompSimpleWindowContainerComponent
{
public:
    SMBCompTournamentComponent(argos::RuntimeConfig* info, SMBComp* comp);
    ~SMBCompTournamentComponent();

    void LoadTournament(const std::string& path);

private:
    std::shared_ptr<SMBCompTournamentManagerWindow> m_Manager;
};

class SMBCompTournamentManagerWindow : public ISMBCompSimpleWindowComponent
{
public:
    SMBCompTournamentManagerWindow(argos::RuntimeConfig* info, SMBComp* comp);
    ~SMBCompTournamentManagerWindow();

    void DoControls();

    void LoadTournament(const std::string& path);

private:
    void UpdatePaths();
    bool m_First;
    std::string m_CurrentPath;
    std::vector<std::string> m_Paths;

    std::string m_TournamentToLoad;

    argos::RuntimeConfig* m_Info;
    SMBComp* m_Comp;
};

class SMBCompTournamentSeatsWindow : public ISMBCompSimpleWindowComponent
{
public:
    SMBCompTournamentSeatsWindow(argos::RuntimeConfig* info, SMBComp* comp);
    ~SMBCompTournamentSeatsWindow();

    void DoControls();

private:
    argos::RuntimeConfig* m_Info;
    SMBComp* m_Comp;
};

class SMBCompTournamentPlayersWindow : public ISMBCompSimpleWindowComponent
{
public:
    SMBCompTournamentPlayersWindow(argos::RuntimeConfig* info, SMBComp* comp);
    ~SMBCompTournamentPlayersWindow();

    void DoControls();

private:
    argos::RuntimeConfig* m_Info;
    SMBComp* m_Comp;
    bool m_UpdatingThreeColors;
};

class SMBCompTournamentScheduleWindow : public ISMBCompSimpleWindowComponent
{
public:
    SMBCompTournamentScheduleWindow(argos::RuntimeConfig* info, SMBComp* comp);
    ~SMBCompTournamentScheduleWindow();

    void DoControls();
    void SetToRound(int i);

private:
    int m_Round;
    void SetScheduleTxt(const std::string& p);

    argos::RuntimeConfig* m_Info;
    SMBComp* m_Comp;
};

class SMBCompTournamentColorsWindow : public ISMBCompSimpleWindowComponent
{
public:
    SMBCompTournamentColorsWindow(argos::RuntimeConfig* info, SMBComp* comp);
    ~SMBCompTournamentColorsWindow();

    void DoControls();

private:
    argos::RuntimeConfig* m_Info;
    SMBComp* m_Comp;
};

struct IndividualResult
{
    int Round;
    std::string ShortName;
    std::string Time;
    int MS;
    int Completed;
    int Points;
};

class SMBCompTournamentResultsWindow : public ISMBCompSimpleWindowComponent
{
public:
    SMBCompTournamentResultsWindow(argos::RuntimeConfig* info, SMBComp* comp);
    ~SMBCompTournamentResultsWindow();

    void DoControls();

private:
    argos::RuntimeConfig* m_Info;
    SMBComp* m_Comp;

    std::vector<IndividualResult> m_Results;
    std::unordered_map<std::string, int> m_Totals;

};

////////////////////////////////////////////////////////////////////////////////

struct SMBCompControllerData
{
    argos::rgms::ControllerColors ControllerColors;
    argos::rgms::ControllerGeometry ControllerGeometry;
    argos::rgms::DogboneGeometry DogboneGeometry;
    float DogboneScale;
    float ControllerScale;
};
void InitializeSMBCompControllerData(SMBCompControllerData* controllers);

struct SMBCompROMData
{
    argos::nes::NESDatabase::RomSPtr rom;
    const uint8_t* ROM;
    const uint8_t* PRG;
    const uint8_t* CHR0;
    const uint8_t* CHR1;
};
void InitializeSMBCompROMData(argos::smb::SMBDatabase* db, SMBCompROMData* rom);

struct SMBCompSounds
{
    std::unordered_map<smb::SoundEffect, Mix_Chunk*> SoundEffects;
    std::unordered_map<smb::MusicTrack, Mix_Music*> Musics;
};
void InitializeSMBCompSounds(const argos::RuntimeConfig* info, SMBCompSounds* sounds);

//struct AreaPointerSection
//{
//    //AreaPointer AP;
//    argos::smb::AreaID AID;
//    int Left; // >=
//    int Right; // <
//    int World;
//    int Level;
//};
//
//struct VisibleSection
//{
//    const AreaPointerSection* Section;
//    int PPUxLoc;
//};
//
//struct SMBRaceCategoryInfo
//{
//    RaceCategory Category;
//    std::string Name;
//    std::vector<AreaPointerSection> Sections;
//
//    int TotalWidth; // Includes 16 pixel buffers between zones.
//
//    //void RenderMinimapTo(nes::PPUx* ppux, int categoryX,
//    //        const MinimapPalette& minimapPalette,
//    //        const SMBBackgroundNametables& nametables,
//    //        std::vector<VisibleSection>* visibleSections) const;
//
//    //bool InCategory(AreaPointer ap, int apx, int world, int level, int* categoryX, int* sectionIndex) const;
//};
//
//struct SMBRaceCategories
//{
//    std::vector<SMBRaceCategoryInfo> Categories;
//
//    const SMBRaceCategoryInfo* FindCategory(RaceCategory category) const;
//};
//void InitializeSMBRaceCategories(SMBRaceCategories* cats);

struct SMBCompStaticData
{
    argos::smb::SMBNametableCachePtr Nametables;
    //SMBRaceCategories Categories;
    //SMBCompControllerData Controllers;
    SMBCompROMData ROM;
    nes::PatternTable Font;
    SMBCompSounds Sounds;
};
void InitializeSMBCompStaticData(const argos::RuntimeConfig* info, SMBCompStaticData* staticData);

////////////////////////////////////////////////////////////////////////////////

struct SMBCompFeed
{
    uint32_t UniquePlayerID;

    SMBMessageProcessorOutputPtr CachedOutput;

//    std::unique_ptr<video::LiveVideoThread> LiveVideoThread;
    ISMBSerialSource* Source;

    std::unique_ptr<SMBSerialProcessorThread> MySMBSerialProcessorThread;
    std::unique_ptr<SMBZMQRef> MySMBZMQRef;
    std::unique_ptr<SMBSerialRecording> MySMBSerialRecording;
    std::string ErrorMessage;
};

struct SMBCompFeeds
{

    std::unordered_map<uint32_t, std::unique_ptr<SMBCompFeed>> Feeds;
};
SMBCompFeed* GetPlayerFeed(const SMBCompPlayer& player, SMBCompFeeds* feeds);
void InitializeFeedSerialThread(const SMBCompPlayer& player, const SMBCompStaticData& data, SMBCompFeed* feed);
//void InitializeFeedLiveVideoThread(const SMBCompPlayer& player, SMBCompFeed* feed);
void InitializeFeedRecording(SMBCompFeed* feed, const SMBCompStaticData& data, const std::string& path);

////////////////////////////////////////////////////////////////////////////////

struct Lerper
{
    float Position;
    float Target;

    // State
    float LastVelocity[4];

    // Parameters
    float Acceleration;
    float DampenAmount;
    float MaxVelocity;
};
void InitializeLerper(Lerper* lerper);
void StepLerper(Lerper* lerper);

enum class MinimapFollowMethod
{
    FOLLOW_NONE,
    FOLLOW_PLAYER,
    FOLLOW_FARTHEST
};

struct SMBCompMinimap
{
    int LeftX; // The current x position of the minimap view
    int Width;

    cv::Mat Img; // UGH

    MinimapFollowMethod FollowMethod;
    uint32_t PlayerID;

    int TargetMarioX;

    //
    Lerper CameraLerp;
};
void InitializeSMBCompMinimap(SMBCompMinimap* minimap);
//void StepSMBCompMinimap(SMBCompMinimap* minimap); TODO should have done this...

////////////////////////////////////////////////////////////////////////////////

struct SMBCompPlayerLocations
{
    std::vector<uint32_t> PlayerIdsByPosition; // In timing tower
    struct PlayerScreenLocation {
        uint32_t PlayerID;

        bool OnScreen;
        int SectionIndex;
        int CategoryX;
    };
    std::vector<PlayerScreenLocation> ScreenLocations; // On Minimap
};

////////////////////////////////////////////////////////////////////////////////

#define TIMING_TOWER_Y_SPACING 12
struct TimingTowerEntry
{
    int Position;
    uint8_t Color;
    std::string Name;
    int64_t IntervalMS; // -ve for blank, 0 for 'gap' (or 'leader')
    bool IsFinalTime;
    bool InSection;
    bool IsHighlight;
    int Y; // from the top, (zero for top, n * TIMING_TOWER_Y_SPACING for the others..)
};

struct TimingTowerState
{
    std::string Title;
    std::string Subtitle;
    std::vector<TimingTowerEntry> Entries;
};

// /Before/ scaling by 2, 4
void DrawTowerStateSize(const TimingTowerState& state, int* w, int* h);
void DrawTowerState(nes::PPUx* ppux, int x, int y,
        const nes::Palette& palette,
        const nes::PatternTable& font,
        const TimingTowerState& state,
        bool fromLeader);

enum class TimingState
{
    WAITING_FOR_1_1,
    RUNNING,
};

struct SMBCompPlayerTimings
{
    TimingState State;
    std::vector<uint64_t> SplitM2s;
    std::vector<std::vector<uint64_t>> SplitPageM2s;
};
void TimingsSectionPageIndex(const SMBCompPlayerTimings& timings, int* section, int* page);
void SplitTimings(SMBCompPlayerTimings* timings, int section, int page, uint64_t m2);

void InitializeSMBCompPlayerTimings(SMBCompPlayerTimings* timings);

struct TimingTowerEntryReconciliation
{
    int PositionIndex; // in the tower from 0 to num players,
    int StartY;

};
struct TimingTowerStateReconciliation
{
    int MovingTimer;
    std::vector<TimingTowerEntryReconciliation> Entries;
};

struct SMBCompTimingTower
{
    TimingTowerState DrawState; // As actually drawn
    TimingTowerState TargetState; // The one that is based on the current
    TimingTowerStateReconciliation Reconcilation;
    bool FromLeader;

    cv::Mat Img;

    std::unordered_map<uint32_t, SMBCompPlayerTimings> Timings;
};
void InitializeSMBCompTimingTower(SMBCompTimingTower* tower);
void ResetSMBCompTimingTower(SMBCompTimingTower* tower);
//void StepSMBCompPlayerTimings(SMBCompPlayerTimings* timings, SMBMessageProcessorOutputPtr out,
//        const SMBRaceCategoryInfo* catinfo);

////////////////////////////////////////////////////////////////////////////////

enum class ViewType
{
    NO_PLAYER,
    NO_OUTPUT,
    CONSOLE_OFF,

    TITLESCREEN_MODE,
    GAMEOVER_MODE,

    PLAYING,
};

struct SMBCompCombinedViewInfo
{
    bool FollowSmart;
    struct {
        int Cnt;
    } SmartInfo;
    uint32_t PlayerID;

    cv::Mat Img; // UGH


    //
    ViewType Type;

    //smb::AreaPointer AP;
    argos::smb::AreaID AID;
    int APX;
    int Width;
    nes::FramePalette FramePalette;
};
void InitializeSMBCompCombinedViewInfo(SMBCompCombinedViewInfo* view);

#define POINTS_COUNTDOWN_MAX 30

struct SMBCompPoints
{
    std::unordered_map<uint32_t, int> LastPoints;
    std::unordered_map<uint32_t, int> Points;

    bool Visible;

    int Countdown;
};
void InitializeSMBCompPoints(SMBCompPoints* points);
void StepSMBCompPoints(SMBComp* comp, SMBCompPoints* points);

// Before scaling by 2, 4 UGHHGHAHG
void DrawPointsSize(const SMBComp* comp, int* w, int* h);
void DrawPoints(nes::PPUx* ppux, int x, int y,
        const nes::Palette& palette,
        const nes::PatternTable& font,
        const SMBComp* comp);
void DrawFinalTimes(nes::PPUx* ppux, int x, int y,
        const nes::Palette& palette,
        const nes::PatternTable& font,
        SMBComp* comp);

////////////////////////////////////////////////////////////////////////////////

struct SMBComp
{
    SMBCompStaticData StaticData; // Totally static data
    // Semi static (config and serialized data)
    SMBCompConfiguration Config;
    SMBCompPoints Points;

    // The dynamic information
    SMBCompFeeds Feeds;
    SMBCompMinimap Minimap;
    SMBCompTimingTower Tower;
    SMBCompPlayerLocations Locations;
    SMBCompCombinedViewInfo CombinedView;

    int FrameNumber;
    bool DoingRecordingOfRecordings;
    bool BeginCountdown;
    std::string SetTxtViewTo;
    bool SetToOverlay;
};
void InitializeSMBComp(const argos::RuntimeConfig* info, SMBComp* comp);
nes::RenderInfo DefaultSMBCompRenderInfo(const SMBComp& comp);
SMBMessageProcessorOutputPtr GetLatestPlayerOutput(SMBComp& comp, const SMBCompPlayer& player);

//
class SMBCompReplayComponent;
class SMBCompSoundComponent;
void StepTimingTower(SMBComp* comp, SMBCompTimingTower* tower, SMBCompPlayerLocations* locations,
        SMBCompReplayComponent* replay, SMBCompSoundComponent* sound);
void StepCombinedView(SMBComp* comp, SMBCompCombinedViewInfo* view);
bool TimingsToText(SMBComp* comp, const SMBCompPlayerTimings* timings, const SMBCompPlayer& player,
        std::string* text, int64_t* elapsedt = nullptr);

////////////////////////////////////////////////////////////////////////////////

class SMBCompPlayerWindow : public ISMBCompSimpleWindowComponent
{
public:
    SMBCompPlayerWindow(
            SMBComp* comp,
            uint32_t id, const SMBCompPlayers* players, const SMBCompVisuals* visuals,
            const SMBCompStaticData* staticData, SMBCompFeeds* feeds,
            argos::RuntimeConfig* info,
            std::vector<std::string>* priorRecordings);
    ~SMBCompPlayerWindow();

    virtual void DoControls() override;

    const SMBCompPlayer* GetPlayer();


    static void UpdateRecordings2(argos::RuntimeConfig* info, std::vector<std::string>* priorRecordings);

private:
    void DoControls(const SMBCompPlayer* player);

    void DoVideoControls(const SMBCompPlayer* player, SMBCompFeeds* feeds);

    void UpdateRecordings();

    void DoSerialControls(const SMBCompPlayer* player, SMBCompFeeds* feeds);
    void DoRecordingControls(const SMBCompPlayer* player, SMBCompFeeds* feeds);
    void DoSerialPlayerOutput(const SMBCompPlayer* player, const SMBMessageProcessorOutput* out);

    //
    SMBComp* m_Competition;
    argos::RuntimeConfig* m_Info;
    uint32_t m_PlayerID;
    const SMBCompPlayers* m_Players;
    const SMBCompVisuals* m_Visuals;
    const SMBCompStaticData* m_StaticData;
    SMBCompFeeds* m_Feeds;

    std::string m_PendingRecording;
    std::vector<std::string>* m_PriorRecordings;
};

class SMBCompPlayerWindowsComponent : public rgmui::IApplicationComponent
{
public:
    SMBCompPlayerWindowsComponent(argos::RuntimeConfig* info, SMBComp* comp);
    ~SMBCompPlayerWindowsComponent();

    virtual void OnFrame() override final;
    void DoMenuItem();

private:
    argos::RuntimeConfig* m_Info;
    SMBComp* m_Competition;
    std::vector<std::string> m_PriorRecordings;

    std::unordered_map<uint32_t, std::shared_ptr<SMBCompPlayerWindow>> m_Windows;

    bool m_IsOpen;
};

////////////////////////////////////////////////////////////////////////////////

class SMBCompIndividualViewsComponent : public ISMBCompSingleWindowComponent
{
public:
    SMBCompIndividualViewsComponent(argos::RuntimeConfig* info, SMBComp* comp);
    ~SMBCompIndividualViewsComponent();

    virtual void DoControls() override final;

private:
    argos::RuntimeConfig* m_Info;
    SMBComp* m_Competition;
    int m_Columns;
};

////////////////////////////////////////////////////////////////////////////////

class SMBCompCombinedViewComponent : public ISMBCompSingleWindowComponent
{
public:
    SMBCompCombinedViewComponent(argos::RuntimeConfig* info, SMBComp* comp);
    ~SMBCompCombinedViewComponent();

    virtual void DoControls() override final;

    static void MakeImage(SMBComp* comp, nes::PPUx* ppux);
    static bool MakeImageIndividual(SMBComp* comp, nes::PPUx* ppux, const SMBCompPlayer* player,
            bool applyVisuals, bool fullView = false,
            int* screenLeft = nullptr,
            int* screenRight = nullptr,
            int* doingOwnOAMx = nullptr,
            SMBMessageProcessorOutputPtr* output = nullptr);

private:
    argos::RuntimeConfig* m_Info;
    SMBComp* m_Competition;
};

////////////////////////////////////////////////////////////////////////////////

class SMBCompTimingTowerViewComponent : public ISMBCompSingleWindowComponent
{
public:
    SMBCompTimingTowerViewComponent(argos::RuntimeConfig* info, SMBComp* comp);
    ~SMBCompTimingTowerViewComponent();

    virtual void DoControls() override final;

private:
    argos::RuntimeConfig* m_Info;
    SMBComp* m_Competition;
};

////////////////////////////////////////////////////////////////////////////////

class SMBCompMinimapViewComponent : public ISMBCompSingleWindowComponent
{
public:
    SMBCompMinimapViewComponent(argos::RuntimeConfig* info, SMBComp* comp);
    ~SMBCompMinimapViewComponent();

    virtual void DoControls() override final;

private:
    void DoMinimapEditControls();

private:
    argos::RuntimeConfig* m_Info;
    SMBComp* m_Competition;
};

////////////////////////////////////////////////////////////////////////////////

class SMBCompRecordingsHelperComponent : public ISMBCompSingleWindowComponent
{
public:
    SMBCompRecordingsHelperComponent(argos::RuntimeConfig* info, SMBComp* comp);
    ~SMBCompRecordingsHelperComponent();

    virtual void DoControls() override final;

private:
    argos::RuntimeConfig* m_Info;
    SMBComp* m_Competition;
    std::string m_FileName;
    bool m_OneFramePerStep;
    int m_FrameCount;
};

////////////////////////////////////////////////////////////////////////////////

class SMBCompPointsComponent : public ISMBCompSingleWindowComponent
{
public:
    SMBCompPointsComponent(argos::RuntimeConfig* info, SMBComp* comp);
    ~SMBCompPointsComponent();

    virtual void DoControls() override final;

private:
    argos::RuntimeConfig* m_Info;
    SMBComp* m_Competition;

    std::unordered_map<uint32_t, int> m_PendingPoints;
};

class SMBCompCompetitionComponent : public ISMBCompSingleWindowComponent
{
public:
    SMBCompCompetitionComponent(argos::RuntimeConfig* info, SMBComp* comp);
    ~SMBCompCompetitionComponent();

    virtual void DoControls() override final;

private:
    argos::RuntimeConfig* m_Info;
    SMBComp* m_Competition;
};

struct GhostInfo
{
    std::string Recording;
    uint8_t RepresentativeColor;
    std::string ShortName;
    std::string FullName;
    std::vector<SMBMessageProcessorOutputPtr> Outputs;
};

class SMBCompGhostViewComponent : public ISMBCompSingleWindowComponent
{
public:
    SMBCompGhostViewComponent(argos::RuntimeConfig* info, SMBComp* comp);
    ~SMBCompGhostViewComponent();

    virtual void DoControls() override final;


    static cv::Mat CreateGhostFrameExt(const std::vector<GhostInfo>& ghostInfo,
            SMBComp* comp,
            int ghostIndex, int frameIndex, int* lastMiniX);

private:
    void Export(int ghostIndex);
    cv::Mat CreateGhostFrame(int ghostIndex, int frameIndex, int* lastMiniX);

private:
    argos::RuntimeConfig* m_Info;
    SMBComp* m_Competition;
    std::string m_FileName;

    std::vector<GhostInfo> m_GhostInfo;
    int m_GhostIndex;
    int m_FrameIndex;
    int m_LastMiniX;
};

class SMBCompPointTransitionComponent : public ISMBCompSingleWindowComponent
{
public:
    SMBCompPointTransitionComponent(argos::RuntimeConfig* info, SMBComp* comp);
    ~SMBCompPointTransitionComponent();

    virtual void DoControls() override final;

private:
    argos::RuntimeConfig* m_Info;
    SMBComp* m_Competition;

    bool m_Points;
    std::unordered_map<uint32_t, uint64_t> m_FakeM2s;

    int m_CountdownOverride;

    cv::Mat m_Begin;
};

//struct RecreatePlayerInfo
//{
//    std::string Name;
//    int64_t PTSOffset;
//    std::vector<nes::ControllerState> Inputs;
//
//    //
//    std::unique_ptr<nes::StateSequenceThread> StateThread;
//    std::unique_ptr<nes::NestopiaNESEmulator> Emulator;
//    nesui::NESInputsComponentOptions InputsOptions;
//    nesui::NESInputsComponentState InputsState;
//};
//NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(RecreatePlayerInfo, Name, PTSOffset, Inputs);
//
//class RecreateApp : public rgmui::IApplication
//{
//public:
//    RecreateApp(argos::RuntimeConfig* info);
//    ~RecreateApp();
//
//    bool OnFrame();
//
//private:
//    enum class InputAction {
//        NO_ACTON,
//        SMB_JUMP_EARLIER,
//        SMB_JUMP_LATER,
//        SMB_REMOVE_LAST_JUMP,
//        SMB_JUMP,
//        SMB_FULL_JUMP,
//        SMB_JUMP_SHORTER,
//        SMB_JUMP_LONGER,
//        SMB_START,
//    };
//    static std::pair<int, int> FindPreviousJump(RecreatePlayerInfo* player);
//    static InputAction GetActionFromKeysPressed();
//    static void DoAction(RecreatePlayerInfo* player, InputAction action);
//
//
//    void DoPlayerControls(RecreatePlayerInfo* player, int64_t* pts);
//
//private:
//    argos::RuntimeConfig* m_Info;
//    SMBComp m_Competition;
//    std::unique_ptr<video::StaticVideoThread> m_VideoThread;
//
//    int64_t m_PTS;
//
//    std::vector<RecreatePlayerInfo> m_Players;
//    RecreatePlayerInfo* m_ActivePlayer;
//};
//
//class VecSource : public smb::ISMBSerialSource
//{
//public:
//    VecSource(std::vector<smb::SMBMessageProcessorOutputPtr>* vec);
//    ~VecSource();
//
//    smb::SMBMessageProcessorOutputPtr Get(size_t index);
//
//    virtual smb::SMBMessageProcessorOutputPtr GetLatestProcessorOutput() override;
//    virtual smb::SMBMessageProcessorOutputPtr GetNextProcessorOutput() override;
//
//    void Next();
//
//private:
//    std::vector<smb::SMBMessageProcessorOutputPtr>* m_Vec;
//    size_t m_RetIndex;
//    size_t m_Index;
//};
//
//class SMBCompRecreateComponent : public ISMBCompSingleWindowComponent
//{
//public:
//    SMBCompRecreateComponent(argos::RuntimeConfig* info, SMBComp* comp);
//    ~SMBCompRecreateComponent();
//
//    virtual void DoControls() override final;
//
//private:
//    argos::RuntimeConfig* m_Info;
//    SMBComp* m_Competition;
//
//    std::vector<RecreatePlayerInfo> m_Players;
//    std::vector<GhostInfo> m_Ghosts;
//    std::vector<VecSource> m_VecSources;
//
//    int m_FrameCount;
//    int m_GhostIndex;
//    int m_FrameIndex;
//    int m_LastMiniX;
//    bool m_OneFramePerStep;
//};
//
class SMBCompCreditsComponent : public ISMBCompSingleWindowComponent
{
public:
    SMBCompCreditsComponent(argos::RuntimeConfig* info, SMBComp* comp);
    ~SMBCompCreditsComponent();

    virtual void DoControls() override final;

private:
    argos::RuntimeConfig* m_Info;
    SMBComp* m_Competition;

    int m_CreditY;
    int m_Start;
    int m_End;
    std::string m_Credits;

    bool m_OnePerStep;
    int m_Amt;
    int m_RecIndex;
    bool m_Rec;
};

class SMBCompSoundComponent : public ISMBCompSingleWindowComponent
{
public:
    SMBCompSoundComponent(argos::RuntimeConfig* info, SMBComp* comp);
    ~SMBCompSoundComponent();

    virtual void DoControls() override final;
    virtual void OnFrameAlways() override;

    void NoteOutput(const SMBCompPlayer& player, SMBMessageProcessorOutputPtr out);
    void MusicFinished();

private:
    void StartMusic(smb::MusicTrack t);
    void StopMusic();
    void PlaySoundEffect(uint32_t playerId, smb::SoundEffect effect);

private:
    argos::RuntimeConfig* m_Info;
    SMBComp* m_Competition;

    std::unordered_map<uint32_t, smb::MusicTrack> m_PlayerToMusic;
    smb::MusicTrack m_CurrentMusic;
};

class SMBCompReplayComponent : public ISMBCompSingleWindowComponent
{
public:
    SMBCompReplayComponent(argos::RuntimeConfig* info, SMBComp* comp);
    ~SMBCompReplayComponent();

    virtual void DoControls() override final;
    void NoteOutput(const SMBCompPlayer& player, SMBMessageProcessorOutputPtr out);

    void DoReplay(cv::Mat aux);

private:
    void DoPlayerDeck(const SMBCompPlayer& player, const std::deque<SMBMessageProcessorOutputPtr>& deck);
    void ColorPlayerDeck(const std::deque<SMBMessageProcessorOutputPtr>& deck, cv::Mat m);


private:
    argos::RuntimeConfig* m_Info;
    SMBComp* m_Competition;
    int m_BufferSize;

    bool m_PendingReplay;
    uint32_t m_PendingId;
    int m_PendingIdx;

    std::unordered_map<uint32_t, std::deque<SMBMessageProcessorOutputPtr>> m_ReplayBuffer;

    std::string m_OngoingName;
    std::deque<SMBMessageProcessorOutputPtr> m_OngoingReplay;

    bool m_HalfSpeed;
    bool m_Counter;
};

////////////////////////////////////////////////////////////////////////////////

//class SMBCompFixedOverlay : public ISMBCompSingleWindowComponent
//{
//public:
//    SMBCompFixedOverlay(argos::RuntimeConfig* info, SMBComp* comp);
//    ~SMBCompFixedOverlay();
//
//    virtual void DoControls() override final;
//
//    void DoDisplay(cv::Mat* m);
//
//
//private:
//    void LoadingThread();
//
//private:
//    void RedrawMatCompletely();
//
//    argos::RuntimeConfig* m_Info;
//    SMBComp* m_Comp;
//
//    std::vector<std::string> m_ThanksPaths;
//    std::shared_ptr<rgms::video::StaticVideoThread> m_StaticVideoThread;
//    int m_FrameIndex;
//
//    std::string m_Text;
//
//    bool m_First;
//    cv::Mat m_Mat;
//    cv::Mat m_ThanksMat;
//};
//
//class SMBCompTxtDisplay : public ISMBCompSingleWindowComponent
//{
//public:
//    SMBCompTxtDisplay(argos::RuntimeConfig* info, SMBComp* comp);
//    ~SMBCompTxtDisplay();
//
//    virtual void DoControls() override final;
//
//    void DoDisplay(cv::Mat m);
//    void SetStem(const std::string& stem);
//
//private:
//    void UpdateStems();
//
//private:
//    argos::RuntimeConfig* m_Info;
//    SMBComp* m_Comp;
//
//    std::string m_Stem;
//    std::string m_Text;
//
//    bool m_FirstTime;
//    std::vector<std::string> m_Stems;
//    cv::Mat m_Mat;
//};

////////////////////////////////////////////////////////////////////////////////

#define SHARED_MEM_SIZE 6220900
#define SHARED_MEM_MAT  6220800
#define SHARED_MEM_QUIT 6220800

class SMBCompApp : public rgmui::IApplication
{
public:
    SMBCompApp(argos::RuntimeConfig* info);
    ~SMBCompApp();

    bool OnFrame();

    void LoadNamedConfig(const std::string& name);
    void SetSharedMemory(void* sharedMem);

    void DoAuxDisplay();


private:
    argos::RuntimeConfig* m_Info;
    SMBComp m_Competition;

    cv::Mat m_AuxDisplay;
    void* m_SharedMemory;

    bool m_AuxVisibleInPrimary;
    float m_AuxVisibleScale;
    int m_AuxDisplayType;

    bool m_CountingDown;
    util::mclock::time_point m_CountdownStart;
    bool m_ShowTimer;

    // Components
    //SMBCompConfigurationComponent m_CompConfigurationComponent;
    //SMBCompPlayerWindowsComponent m_CompPlayerWindowsComponent;
    //SMBCompCombinedViewComponent m_CompCombinedViewComponent;
    //SMBCompIndividualViewsComponent m_CompIndividualViewsComponent;
    //SMBCompPointsComponent m_CompPointsComponent;
    //SMBCompMinimapViewComponent m_CompMinimapViewComponent;
    //SMBCompRecordingsHelperComponent m_CompRecordingsHelperComponent;
    //SMBCompTimingTowerViewComponent m_CompTimingTowerViewComponent;
    //SMBCompCompetitionComponent m_CompCompetitionComponent;
    //SMBCompGhostViewComponent m_CompGhostViewComponent;
    //SMBCompPointTransitionComponent m_CompPointTransitionComponent;
    ////SMBCompRecreateComponent m_CompRecreateComponent;
    //SMBCompCreditsComponent m_CompCreditsComponent;
    //SMBCompSoundComponent m_CompSoundComponent;
    //SMBCompReplayComponent m_CompReplayComponent;
    ////SMBCompFixedOverlay m_CompFixedOverlay;
    ////SMBCompTxtDisplay m_CompTxtDisplay;
    //SMBCompTournamentComponent m_CompTournamentComponent;
};

class SMBCompAppAux : public rgmui::IApplication
{
public:
    SMBCompAppAux(void* sharedMem);
    ~SMBCompAppAux();

    bool OnFrame();

private:
    void* m_SharedMemory;
    cv::Mat m_Mat;
    unsigned int m_Gluint;
};

////////////////////////////////////////////////////////////////////////////////

bool MarioInOutput(SMBMessageProcessorOutputPtr out, int* mariox, int* marioy);
//void EmuToOutput(int frameIndex, int* startIndex, nes::ControllerState cs,
//        const nes::NestopiaNESEmulator& emu, SMBMessageProcessorOutput* out,
//        const smb::SMBBackgroundNametables& nametables);


}

#endif
