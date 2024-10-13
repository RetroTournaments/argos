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
// NES Components in the rgmui style
////////////////////////////////////////////////////////////////////////////////

#ifndef ARGOS_NES_NESUI_HEADER
#define ARGOS_NES_NESUI_HEADER

#include "nes/nes.h"
#include "nes/nesdb.h"
#include "rgmui/rgmui.h"
#include "nes/nestopiaimpl.h"

namespace argos::nesui
{
    
struct NESPaletteComponentOptions
{
    bool BGROrder;
    bool AllowEdits;

    static NESPaletteComponentOptions Defaults();
};

class NESPaletteComponent : public rgmui::IApplicationComponent
{
public:
    NESPaletteComponent(const std::string& windowName);
    NESPaletteComponent(const std::string& windowName, 
                        NESPaletteComponentOptions options,
                        nes::Palette initialPalette);
    ~NESPaletteComponent();

    virtual void OnFrame() override;

    static bool Controls(nes::Palette* palette, const NESPaletteComponentOptions& options);

    nes::Palette& GetPalette();
    const nes::Palette& GetPalette() const;

    NESPaletteComponentOptions& GetOptions();
    const NESPaletteComponentOptions& GetOptions() const;


private:
    std::string m_WindowName;
    NESPaletteComponentOptions m_Options;
    nes::Palette m_Palette;
};


struct FramePaletteComponentOptions
{
    bool BGROrder;
    bool AllowEdits;
    nes::Palette NesPalette;
    nes::Palette* NesPaletteP; // if not nullptr will use, but it's on YOU to manage lifetime

    static FramePaletteComponentOptions Defaults();
};

class FramePaletteComponent : public rgmui::IApplicationComponent
{
public:
    FramePaletteComponent(const std::string& windowName);
    FramePaletteComponent(const std::string& windowName,
                          FramePaletteComponentOptions options,
                          nes::FramePalette framePalette);
    ~FramePaletteComponent();

    virtual void OnFrame() override;

    static bool Controls(nes::FramePalette* framePalette, const FramePaletteComponentOptions& options, int* popupPaletteIndex);

    nes::FramePalette& GetFramePalette();
    const nes::FramePalette& GetFramePalette() const;

    FramePaletteComponentOptions& GetOptions();
    const FramePaletteComponentOptions& GetOptions() const;

private:
    std::string m_WindowName;
    FramePaletteComponentOptions m_Options;
    nes::FramePalette m_FramePalette;

    int m_PopupPaletteIndex;
};

////////////////////////////////////////////////////////////////////////////////

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

struct NESInputsComponentOptions
{
    int ColumnPadding;
    int ChevronColumnWidth;
    int FrameTextColumnWidth;
    int ButtonWidth;
    int FrameTextNumDigits;

    bool DisallowLROrUD;
    //bool StickyAutoScroll;
    uint8_t VisibleButtons;

    bool AllowTargetChange;
    bool KeepTargetVisible;

    ImVec4 TextColor;
    ImVec4 HighlightTextColor;
    ImVec4 ButtonColor;
    ImVec4 MarkerColor;

    static NESInputsComponentOptions Defaults();
};

struct NESInputLineSize
{
    int Height;
    int Width;

    int ChevronX;
    int FrameTextX;
    int ButtonsX;
};

enum class DraggingState
{
    NOTHING,
    BUTTON,
    INPUT_ROW
};

// TODO transfer undo/redo from graphite later... (or never?)
//enum class UndoRedoCommandType
//{
//    TOGGLE_BUTTON,
//};
//
//struct NESInputsUndoRedoCommand
//{
//    UndoRedoCommandType Type;
//    union ??
//    int InputIndex;
//    uint8_t Button;
//};
//
//struct NESInputsUndoRedo
//{
//};

struct NESInputsComponentScrollState
{
    bool AutoScroll;
    bool ForceAutoOn;
    float LastScroll;
    int FirstVisibleTarget;
    int LastVisibleTarget;

    //
    bool AtBottom();
    void UpdateScroll(const NESInputsComponentOptions* options, int target, int height);
    void DoScrollControls(const NESInputsComponentOptions* options);
    void Reset();
};

struct NESInputsComponentState
{
    std::function<void(int frameIndex, nes::ControllerState newState)> OnInputChangeCallback;
    // 
    int DraggedFromIndex;
    int DraggedToIndex;

    DraggingState DragState;
    uint8_t Button;
    nes::ControllerState State;

    int TargetIndex; // -1 to not show !! will be modified if allowed!
    int EmulatorIndex;

    bool IsDraggingTarget;
    bool MouseWasHoveringChevron;
    bool Locked;

    NESInputsComponentScrollState Scroll;

    //
    // TODO undo/redo
    bool SetState(int inputIndex, std::vector<nes::ControllerState>* inputs, nes::ControllerState newState);

    void BeginButtonDrag(int inputIndex, uint8_t button, nes::ControllerState origState);
    void BeginInputRowDrag(int inputIndex, nes::ControllerState origState);
    bool DragTo(int inputIndex, std::vector<nes::ControllerState>* inputs);
    bool EndDrag(std::vector<nes::ControllerState>* inputs); 

    bool HighlightButton(int inputIndex, uint8_t button, bool isHovered);
    bool HighlightInputRow(int inputIndex, bool isHovered);

    void Reset();
};

class NESInputsComponent : public rgmui::IApplicationComponent
{
public:
    NESInputsComponent(const std::string& windowName, 
            NESInputsComponentOptions options = NESInputsComponentOptions::Defaults());
    ~NESInputsComponent();

    virtual void OnFrame() override;
    void SetInputChangeCallback(std::function<void(int frameIndex, nes::ControllerState newState)> cback);

    static bool Controls(std::vector<nes::ControllerState>* inputs,
            NESInputsComponentOptions* options,
            NESInputsComponentState* state);
    static bool DoInputLine(
            int inputIndex, std::vector<nes::ControllerState>* inputs,
            NESInputsComponentOptions* options,
            const NESInputLineSize& size,
            NESInputsComponentState* state);

private:
    std::string m_WindowName;
    NESInputsComponentOptions m_Options;
    std::vector<nes::ControllerState> m_Inputs;
    NESInputsComponentState m_State;
};

// for smb inputs... TODO put somewhere better but I was just copy pasting around....
namespace smb {
enum class InputAction {
    NO_ACTON,
    SMB_JUMP_EARLIER,
    SMB_JUMP_LATER,
    SMB_REMOVE_LAST_JUMP,
    SMB_JUMP,
    SMB_FULL_JUMP,
    SMB_JUMP_SHORTER,
    SMB_JUMP_LONGER,
    SMB_START,
};
std::pair<int, int> FindPreviousJump(int targetindex, const std::vector<nes::ControllerState>& inputs);
InputAction GetActionFromKeysPressed(); // uses imgui context
bool DoAction(InputAction action, std::vector<nes::ControllerState>* inputs, NESInputsComponentState* state);
}


////////////////////////////////////////////////////////////////////////////////
struct NESEmuFrameComponentOptions
{
    bool AllowScrollingTarget;
    bool AllowEditOptions;

    float Scale;

    nes::Palette NesPalette;
    nes::Palette* NesPaletteP; // if not nullptr will use, but it's on YOU to manage lifetime

    //
    static NESEmuFrameComponentOptions Defaults();
};

namespace NESEmuFrameComponent {
    bool Controls(const nes::Frame& frame, NESEmuFrameComponentOptions* options, int* targetIndex);
}

////////////////////////////////////////////////////////////////////////////////
struct NESEmuNametableComponentOptions
{
    bool AllowEditOptions;

    float Scale;

    nes::Palette NesPalette;
    nes::Palette* NesPaletteP; // if not nullptr will use, but it's on YOU to manage lifetime

    //
    static NESEmuNametableComponentOptions Defaults();
};

namespace NESEmuNametableComponent {
    bool Controls(const nes::NameTable& nt, const nes::PatternTable& pt,
            const nes::FramePalette& fp, NESEmuNametableComponentOptions* options);
}

////////////////////////////////////////////////////////////////////////////////

class NESTASComponent : public rgmui::IApplicationComponent
{
public:
    NESTASComponent(const char* name);
    ~NESTASComponent();

    virtual void OnFrame() override;

    void ClearTAS();
    void SetTAS(void* user_data,
                const uint8_t* rom, size_t rom_size,
                std::string start_string,
                const std::vector<nes::ControllerState>& init_states);

private:
    std::string m_Name;
    std::vector<nes::ControllerState> m_Inputs;

    std::unique_ptr<nes::StateSequenceThread> m_StateThread;

    nes::NestopiaNESEmulator m_Emulator;
    NESEmuFrameComponentOptions m_FrameOptions;
    nes::Frame m_Frame;
    NESInputsComponentOptions m_InputsOptions;
    NESInputsComponentState m_InputsState;

    void* m_UserData;
};

}

#endif

