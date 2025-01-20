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
//
// The RGMUI Library contains the building blocks of this particular style of
// ImGui based applications
//
// Essentially: Define an IApplication, a series of IApplicationComponents and
// then rig them all together by registering them to one another. Inter
// component communication is typically done through an EventQueue.
//
// This is a bit different than more conventional callback based applications,
// and perhaps a bit stranger. But it works?
//
////////////////////////////////////////////////////////////////////////////////

#ifndef STATIC_RGMUI_RGMUI_HEADER
#define STATIC_RGMUI_RGMUI_HEADER

#include <thread>
#include <vector>
#include <memory>
#include <functional>

#include "SDL.h"
#include "imgui.h"
#include "opencv2/opencv.hpp"
#include "fmt/fmt.h"
#include "GL/gl.h"

#include "util/vector.h"
#include "rgmui/window.h"

namespace sta::rgmui
{

////////////////////////////////////////////////////////////////////////////////


class IApplicationComponent;

struct IApplicationConfig
{
    bool CtrlWIsExit;
    bool DefaultDockspace;

    static IApplicationConfig Defaults();
};

class IApplication
{
public:
    IApplication(IApplicationConfig config = IApplicationConfig::Defaults());
    virtual ~IApplication();
    virtual bool OnSDLEventExternal(const SDL_Event& e) final;
    virtual bool OnFrameExternal() final;

protected:
    // Return true on continue running the application
    // Override these if necessary
    virtual bool OnSDLEvent(const SDL_Event& e); // Default call subcomponent
    virtual bool OnFrame();
    virtual void OnFirstFrame();

    void RegisterComponent(IApplicationComponent* component);
    void RegisterComponent(std::shared_ptr<IApplicationComponent> component);

    ImGuiID GetDefaultDockspaceID();

private:
    bool m_FirstFrame;
    IApplicationConfig m_Config;
    ImGuiID m_DockspaceID;
    std::vector<IApplicationComponent*> m_Components;
    std::vector<std::shared_ptr<IApplicationComponent>> m_OwnedComponents;
};

class IApplicationComponent {
public:
    IApplicationComponent();
    virtual ~IApplicationComponent();

    virtual void OnSDLEvent(const SDL_Event& e); // default calls sub component OnEvents
    virtual void OnFrame(); // default calls sub component OnFrames

protected:
    void RegisterSubComponent(IApplicationComponent* component);
    void RegisterSubComponent(std::shared_ptr<IApplicationComponent> component);

    void OnSubComponentEvents(const SDL_Event& e);
    void OnSubComponentFrames();

    std::vector<IApplicationComponent*> m_SubComponents;

private:
    std::vector<std::shared_ptr<IApplicationComponent>> m_OwnedComponents;
};

class ImGuiDemoComponent : public IApplicationComponent
{
public:
    ImGuiDemoComponent();
    virtual ~ImGuiDemoComponent();
    virtual void OnFrame() override final;
};

////////////////////////////////////////////////////////////////////////////////
// Event queue
////////////////////////////////////////////////////////////////////////////////
struct Event
{
    int EventType;  // Generally define your own enum
    std::shared_ptr<void> Data;
};
typedef std::function<void(const Event& e)> EventCallback;

class EventQueue
{
public:
    EventQueue();
    ~EventQueue();

    void Publish(int etype);
    void Publish(int etype, std::shared_ptr<void> data);
    void PublishI(int etype, int dataInt);
    void PublishS(int etype, std::string dataStr);
    void Publish(Event e);
    void Subscribe(int etype, EventCallback cback);
    void Subscribe(int etype, std::function<void()> cback);
    void SubscribeI(int etype, std::function<void(int v)> cback);
    void SubscribeS(int etype, std::function<void(const std::string& v)> cback);

    size_t PendingEventsSize() const;
    void PumpQueue();

private:
    std::vector<Event> m_PendingEvents;
    std::unordered_map<int, std::vector<EventCallback>> m_Callbacks;
};


////////////////////////////////////////////////////////////////////////////////
// SDL Extensions
////////////////////////////////////////////////////////////////////////////////
bool KeyUp(const SDL_Event& e, SDL_Keycode k);
bool KeyUpWithCtrl(const SDL_Event& e, SDL_Keycode k);
bool KeyDown(const SDL_Event& e, SDL_Keycode k);
bool KeyDownWithCtrl(const SDL_Event& e, SDL_Keycode k);

bool ShiftIsDown();
bool CtrlIsDown();
bool AltIsDown();

// Convert arrow key movements into +/- dx, +/- dy
// optionally multiply when shift is down
bool ArrowKeyHelper(const SDL_Event& e,
        std::function<void(int dx, int dy)> cback,
        int shiftMultiplier = 1);


////////////////////////////////////////////////////////////////////////////////
// ImGui Extensions
////////////////////////////////////////////////////////////////////////////////

bool ArrowKeyHelperInFrame(int* dx, int* dy, int shiftMultiplier);

// Favorite is : ImGuiInputTextFlags_EnterReturnsTrue, might think InputString
bool InputText(const char* label, std::string* str,
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_None);
bool InputTextMulti(const char* label, std::string* str, const ImVec2& size = ImVec2(0, 0),
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_None);

bool InputColor(const char* label, ImU32* color, bool withAlpha = false);

bool InputPaletteIndex(const char* label, uint8_t* paletteIndex,
        const uint8_t* palette, int paletteSize, // will access x3
        bool bgrOrder = true, int numcols = 16);

bool InputVectorInt(const char* label, std::vector<int>* vec);

template <typename... T>
void TextFmt(fmt::format_string<T...> fmt, T&&... args) {
    ImGui::TextUnformatted(fmt::vformat(fmt, fmt::make_format_args(args...)).c_str());
}
void CopyableText(const std::string& text);

void GreenText(const char* text);
void RedText(const char* text);


ImVec4 PaletteColorToImVec4(uint8_t paletteIndex, const uint8_t* palette, bool bgrorder);
// ImVec4 ImGui::ColorConvertU32ToFloat4(ImU32 in);
// ImU32 ImGui::ColorConvertFloat4ToU32(const ImVec4& in);

// When hovered these sliders adjust the values based on arrow keys / mouse
// wheels
bool SliderUint8Ext(const char* label, uint8_t* v, uint8_t min, uint8_t max,
        ImGuiSliderFlags flags = 0, bool allowArrowKeys = true, bool allowMouseWheel = true, uint8_t singleMove = 0x01, uint8_t shiftMult = 0x08);
bool SliderIntExt(const char* label, int* v, int min, int max,
        const char* format = "%d", ImGuiSliderFlags flags = 0,
        bool allowArrowKeys = true, bool allowMouseWheel = true,
        int singleMove = 1, int shiftMult = 8);
bool SliderFloatExt(const char* label, float* v, float min, float max,
        const char* format = "%.3f", ImGuiSliderFlags flags = 0,
        bool allowArrowKeys = true, bool allowMouseWheel = true,
        float singleMove = 0.10f, float shiftMult = 4.0f);

// Display a cv mat
void Mat(const char* label, const cv::Mat& img);
// Alternative mat api
class MatAnnotator
{
public:
    // Construct (which will display the cv::Mat)
    //    The mat must be pre-scaled!
    MatAnnotator(const char* label, const cv::Mat& img,
            float scale = 1.0f, util::Vector2F origin = {0.0f, 0.0f},
            bool clipped = true);
    ~MatAnnotator();

    // Screen positions in pixels on screen
    // Mat positions on the image (accounting for scale and origin)
    util::Vector2F MatPosToScreenPos2F(const util::Vector2F& v) const;
    ImVec2 MatPosToScreenPos(const util::Vector2F& v) const;
    util::Vector2F ScreenPosToMatPos2F(const ImVec2& p) const;
    util::Vector2I ScreenPosToMatPos2I(const ImVec2& p) const;

    // Annotate the image
    //  - these coordinates are in MAT space (so 0, 0 to mat.cols, mat.rows)
    //  Use: IM_COL32(255, 0, 255, 255) for example
    void AddLine(const util::Vector2F& p1, const util::Vector2F& p2,
                 ImU32 col, float thickness = 1.0f);
    void AddRect(const util::Vector2F& pmin, const util::Vector2F& pmax,
                 ImU32 col, float rounding = 0.0f, ImDrawFlags flags = 0, float thickness = 1.0f);
    void AddRect(const util::Rect2F& rect,
                 ImU32 col, float rounding = 0.0f, ImDrawFlags flags = 0, float thickness = 1.0f);
    void AddRectFilled(const util::Vector2F& pmin, const util::Vector2F& pmax,
                 ImU32 col, float rounding = 0.0f, ImDrawFlags flags = 0);
    void AddRectFilled(const util::Rect2F& rect,
                 ImU32 col, float rounding = 0.0f, ImDrawFlags flags = 0);
    void AddBezierCubic(const util::Vector2F& a, const util::Vector2F& b,
                        const util::Vector2F& c, const util::Vector2F& d,
                        ImU32 col, float thickness = 1.0f, int num_segments = 0);
    void AddText(const util::Vector2F& p, ImU32 col,
            const char* text_begin, const char* text_end = nullptr);



    // Use the information stored within to add additional functionality
    // (tooltips / interactivity)
    bool WasClicked(util::Vector2I* pixel);
    bool IsHovered(bool requireWindowFocus = true);
    util::Vector2I HoveredPixel() const;


private:
    void AddLineNC(const util::Vector2F& p1, const util::Vector2F& p2,
                 ImU32 col, float thickness);
    void AddRectNC(const util::Vector2F& pmin, const util::Vector2F& pmax,
                 ImU32 col, float rounding, ImDrawFlags flags, float thickness);
    void AddRectFilledNC(const util::Vector2F& pmin, const util::Vector2F& pmax,
                 ImU32 col, float rounding, ImDrawFlags flags);
    void AddBezierCubicNC(const util::Vector2F& a, const util::Vector2F& b,
                        const util::Vector2F& c, const util::Vector2F& d,
                        ImU32 col, float thickness, int num_segments);
    void AddTextNC(const util::Vector2F& p, ImU32 col,
            const char* text_begin, const char* text_end);

    class ClipHelper {
    public:
        ClipHelper(MatAnnotator* anno);
        ~ClipHelper();

    private:
        MatAnnotator* m_Anno;
    };

private:
    ImVec2 m_OriginalCursorPosition;
    ImVec2 m_BottomRight;
    const bool m_Clipped;
    ImDrawList* m_List;

    util::Vector2F m_Origin;
    float m_Scale;

    bool m_IsHovered;
    bool m_WasClicked;
};


bool IsAnyPopupOpen();
bool PopupLeftRightHelper(
        const char* left,   std::function<void()> onleft,
        const char* right,  std::function<void()> onright);
bool PopupYesNoHelper(
        std::function<void()> onYes = nullptr,
        std::function<void()> onNo = nullptr);
bool PopupOkCancelHelper(
        std::function<void()> onOK = nullptr,
        std::function<void()> onCancel = nullptr);


// easier combo boxes
template <typename T, typename Q>
bool Combo2(const char* label, T* v, const Q& container,
        std::function<std::string(const T&)> toStr) {
    bool changed = false;

    if (ImGui::BeginCombo(label, toStr(*v).c_str())) {
        for (auto & x : container) {
            bool selected = *v == x;
            if (ImGui::Selectable(toStr(x).c_str(), selected)) {
                *v = x;
                changed = true;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

template <typename T, typename Q>
bool Combo(const char* label, T* v, const Q& container) {
    auto toStr = [&](const T& t) {
        std::ostringstream os;
        os << t;
        return os.str();
    };
    return Combo2<T, Q>(label, v, container, toStr);
}

template <typename T, typename Q>
bool Combo3(const char* label, T* v, const Q& container, std::vector<std::string> strs)
{
    auto toStr = [&](const T& t) {
        int i = 0;
        for (auto & x : container) {
            if (t == x) {
                return strs.at(i);
            }
            i++;
        }
        return std::string("rgmui::Combo3 unknown?");
    };
    return Combo2<T, Q>(label, v, container, toStr);
}

bool HandleCombo4Scroll(size_t* idx, size_t size);
template <typename T>
bool Combo4(const char* label, T* v, const std::vector<T>& ts, const std::vector<std::string>& strs)
{
    assert(ts.size() == strs.size());
    bool changed = false;

    std::string init = "";
    size_t idx = ts.size();
    for (size_t i = 0; i < ts.size(); i++) {
        if (*v == ts[i]) {
            init = strs[i];
            idx = i;
            break;
        }
    }

    if (ImGui::BeginCombo(label, init.c_str())) {
        for (size_t i = 0; i < ts.size(); i++) {
            bool selected = *v == ts[i];
            if (ImGui::Selectable(strs[i].c_str(), selected)) {
                *v = ts[i];
                changed = true;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    } else {
        if (HandleCombo4Scroll(&idx, ts.size())) {
            *v = ts[idx];
            changed = true;
        }
    }
    return changed;
}

class PushPopID
{
public:
    PushPopID(int id);
    PushPopID(const char* id);
    ~PushPopID();
};

////////////////////////////////////////////////////////////////////////////////

// Following depend on calling newframe alongside ImGui::NewFrame(); (this is
// automatically done in the 'Window' class.
void NewFrame();

bool SliderExtWasHovered();
ImGuiWindowFlags SliderExtWasHoveredFlag(ImGuiWindowFlags inflags = 0);
void SliderExtIsHovered();

template <typename T>
bool HoverScroll(T* v, T min, T max) {
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        ImGui::IsItemHovered()) {

        int o = 0;
        auto& io = ImGui::GetIO();
        if (io.MouseWheel) {
            o = -io.MouseWheel;
        }
        if (o) {
            int q = static_cast<int>(*v) + o;
            if (q >= static_cast<int>(min) && q < static_cast<int>(max)) {
                *v = static_cast<size_t>(q);
                return true;
            }
        }
    }
    return false;
}

}

#endif

