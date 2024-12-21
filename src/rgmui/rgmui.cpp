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

#include "rgmui/rgmui.h"

using namespace argos::rgmui;
using namespace argos::util;

struct
{
    bool SliderExtWasHovered = false;
    bool SliderExtIsHovered = false;
} g_ImGuiExtContext;

bool argos::rgmui::SliderExtWasHovered()
{
    return g_ImGuiExtContext.SliderExtWasHovered;
}
void argos::rgmui::SliderExtIsHovered()
{
    g_ImGuiExtContext.SliderExtIsHovered = true;
}

void argos::rgmui::NewFrame()
{
    g_ImGuiExtContext.SliderExtWasHovered = g_ImGuiExtContext.SliderExtIsHovered;
    g_ImGuiExtContext.SliderExtIsHovered = false;
}

////////////////////////////////////////////////////////////////////////////////

IApplicationComponent::IApplicationComponent() {
}

IApplicationComponent::~IApplicationComponent() {
}

void IApplicationComponent::OnSDLEvent(const SDL_Event& e) {
    OnSubComponentEvents(e);
}

void IApplicationComponent::OnFrame() {
    OnSubComponentFrames();
}

void IApplicationComponent::RegisterSubComponent(IApplicationComponent* component) {
    m_SubComponents.push_back(component);
}

void IApplicationComponent::RegisterSubComponent(std::shared_ptr<IApplicationComponent> component) {
    m_OwnedComponents.push_back(component);
    m_SubComponents.push_back(component.get());
}

void IApplicationComponent::OnSubComponentEvents(const SDL_Event& e) {
    for (auto & comp : m_SubComponents) {
        comp->OnSDLEvent(e);
    }
}

void IApplicationComponent::OnSubComponentFrames() {
    for (auto & comp : m_SubComponents) {
        comp->OnFrame();
    }
}

////////////////////////////////////////////////////////////////////////////////

IApplicationConfig IApplicationConfig::Defaults() {
    IApplicationConfig cfg;
    cfg.CtrlWIsExit = true;
    cfg.DefaultDockspace = true;
    return cfg;
}

IApplication::IApplication(IApplicationConfig config)
    : m_Config(config)
    , m_DockspaceID(0)
    , m_FirstFrame(true)
{
}

IApplication::~IApplication() {
}

bool IApplication::OnSDLEventExternal(const SDL_Event& e) {
    if (m_Config.CtrlWIsExit && KeyDownWithCtrl(e, SDLK_w)) {
        return false;
    }

    bool r = OnSDLEvent(e);
    if (r) {
        for (auto & comp : m_Components) {
            comp->OnSDLEvent(e);
        }
    }
    return r;
}

ImGuiID IApplication::GetDefaultDockspaceID() {
    return m_DockspaceID;
}

bool IApplication::OnFrameExternal() {
    if (m_Config.DefaultDockspace) {
        m_DockspaceID = ImGui::DockSpaceOverViewport();
    }
    if (m_FirstFrame) {
        OnFirstFrame();
        m_FirstFrame = false;
    }
    bool r = OnFrame();

    if (r) {
        for (auto & comp : m_Components) {
            comp->OnFrame();
        }
    }
    return r;
}

bool IApplication::OnSDLEvent(const SDL_Event& e) {
    return true;
}

bool IApplication::OnFrame() {
    return true;
}

void IApplication::OnFirstFrame() {
}

void IApplication::RegisterComponent(IApplicationComponent* component) {
    m_Components.push_back(component);
}

void IApplication::RegisterComponent(std::shared_ptr<IApplicationComponent> component) {
    m_OwnedComponents.push_back(component);
    m_Components.push_back(component.get());
}

////////////////////////////////////////////////////////////////////////////////

bool argos::rgmui::KeyUp(const SDL_Event& e, SDL_Keycode k) {
    return e.type == SDL_KEYUP && e.key.keysym.sym == k;
}

bool argos::rgmui::KeyUpWithCtrl(const SDL_Event& e, SDL_Keycode k) {
    if (KeyUp(e, k)) {
        const uint8_t* keystates = SDL_GetKeyboardState(nullptr);
        if (keystates[SDL_SCANCODE_LCTRL] || keystates[SDL_SCANCODE_RCTRL]) {
            return true;
        }
    }
    return false;
}

bool argos::rgmui::KeyDown(const SDL_Event& e, SDL_Keycode k) {
    return e.type == SDL_KEYDOWN && e.key.keysym.sym == k;
}

bool argos::rgmui::KeyDownWithCtrl(const SDL_Event& e, SDL_Keycode k) {
    if (KeyDown(e, k)) {
        const uint8_t* keystates = SDL_GetKeyboardState(nullptr);
        if (keystates[SDL_SCANCODE_LCTRL] || keystates[SDL_SCANCODE_RCTRL]) {
            return true;
        }
    }
    return false;
}

bool argos::rgmui::ShiftIsDown()
{
    const uint8_t* keystates = SDL_GetKeyboardState(nullptr);
    return keystates[SDL_SCANCODE_LSHIFT] || keystates[SDL_SCANCODE_RSHIFT];
}

bool argos::rgmui::CtrlIsDown()
{
    const uint8_t* keystates = SDL_GetKeyboardState(nullptr);
    return keystates[SDL_SCANCODE_LCTRL] || keystates[SDL_SCANCODE_RCTRL];
}

bool argos::rgmui::AltIsDown()
{
    const uint8_t* keystates = SDL_GetKeyboardState(nullptr);
    return keystates[SDL_SCANCODE_LALT] || keystates[SDL_SCANCODE_RALT];
}

bool argos::rgmui::ArrowKeyHelper(const SDL_Event& e, std::function<void(int dx, int dy)> cback,
        int shiftMultiplier) {
    if (e.type == SDL_KEYDOWN) {
        int dx = 0;
        int dy = 0;
        switch (e.key.keysym.sym) {
            case SDLK_UP:
                dx = 0;
                dy = -1;
                break;
            case SDLK_DOWN:
                dx = 0;
                dy = 1;
                break;
            case SDLK_LEFT:
                dx = -1;
                dy = 0;
                break;
            case SDLK_RIGHT:
                dx = 1;
                dy = 0;
                break;
            default:
                break;
        }

        if (dx || dy) {
            if (ShiftIsDown()) {
                dx *= shiftMultiplier;
                dy *= shiftMultiplier;
            }

            cback(dx, dy);
            return true;
        }
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////

EventQueue::EventQueue() {
}

EventQueue::~EventQueue() {
}

void EventQueue::Publish(int etype) {
    Event e;
    e.EventType = etype;
    e.Data = nullptr;
    Publish(e);
}

void EventQueue::Publish(int etype, std::shared_ptr<void> data) {
    Event e;
    e.EventType = etype;
    e.Data = data;
    Publish(e);
}

void EventQueue::PublishI(int etype, int dataInt) {
    Event e;
    e.EventType = etype;
    e.Data = std::make_shared<int>(dataInt);
    Publish(e);
}

void EventQueue::PublishS(int etype, std::string dataStr) {
    Event e;
    e.EventType = etype;
    e.Data = std::make_shared<std::string>(dataStr);
    Publish(e);
}

void EventQueue::Publish(Event e) {
    m_PendingEvents.push_back(e);
}

void EventQueue::Subscribe(int etype, EventCallback cback) {
    m_Callbacks[etype].push_back(cback);
}

void EventQueue::Subscribe(int etype, std::function<void()> cback) {
    m_Callbacks[etype].push_back([cback](const Event&){
        cback();
    });
}

void EventQueue::SubscribeI(int etype, std::function<void(int v)> cback) {
    Subscribe(etype, [cback](const Event& e){
        cback(*reinterpret_cast<int*>(e.Data.get()));
    });
}

void EventQueue::SubscribeS(int etype, std::function<void(const std::string& v)> cback) {
    Subscribe(etype, [cback](const Event& e){
        cback(*reinterpret_cast<std::string*>(e.Data.get()));
    });
}

void EventQueue::PumpQueue() {
    if (!m_PendingEvents.empty()) {
        std::vector<Event> events;
        std::swap(m_PendingEvents, events);

        for (auto & e : events) {
            auto it = m_Callbacks.find(e.EventType);
            if (it != m_Callbacks.end()) {
                for (auto & v : it->second) {
                    v(e);
                }
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

bool argos::rgmui::IsAnyPopupOpen() {
    return ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId + ImGuiPopupFlags_AnyPopupLevel);
}

// See misc/cpp/imgui_stdlib
static int InputTextCallback(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        std::string* str = reinterpret_cast<std::string*>(data->UserData);
        str->resize(data->BufTextLen);
        data->Buf = const_cast<char*>(str->c_str());
    }
    return 0;
}

bool argos::rgmui::InputText(const char* label, std::string* str,
        ImGuiInputTextFlags flags)
{
    flags |= ImGuiInputTextFlags_CallbackResize;
    bool ret = ImGui::InputText(label, const_cast<char*>(str->c_str()),
        str->capacity() + 1, flags, InputTextCallback, (void*)str);
    ImGui::PushID("context");
    if (ImGui::BeginPopupContextItem(label, ImGuiPopupFlags_MouseButtonRight)) {
        if (ImGui::Selectable("copy")) {
            ImGui::SetClipboardText(str->c_str());
        }
        if (!(flags & ImGuiInputTextFlags_ReadOnly)) {
            if (ImGui::Selectable("paste")) {
                *str = ImGui::GetClipboardText();
            }
            if (ImGui::Selectable("clear")) {
                *str = "";
            }
        }
        ImGui::EndPopup();
    }
    ImGui::PopID();

    return ret;
}

bool argos::rgmui::InputTextMulti(const char* label, std::string* str, const ImVec2& size,
        ImGuiInputTextFlags flags)
{
    flags |= ImGuiInputTextFlags_CallbackResize;
    bool ret = ImGui::InputTextMultiline(label, const_cast<char*>(str->c_str()),
        str->capacity() + 1, size, flags, InputTextCallback, (void*)str);
    ImGui::PushID("context");
    if (ImGui::BeginPopupContextItem(label, ImGuiPopupFlags_MouseButtonRight)) {
        if (ImGui::Selectable("copy")) {
            ImGui::SetClipboardText(str->c_str());
        }
        if (!(flags & ImGuiInputTextFlags_ReadOnly)) {
            if (ImGui::Selectable("paste")) {
                *str = ImGui::GetClipboardText();
            }
            if (ImGui::Selectable("clear")) {
                *str = "";
            }
        }
        ImGui::EndPopup();
    }
    ImGui::PopID();

    return ret;
}

void argos::rgmui::CopyableText(const std::string& text) {
    ImGui::PushID(text.c_str());
    ImGui::InputText("##copy", const_cast<char*>(text.c_str()), text.capacity() + 1,
            ImGuiInputTextFlags_ReadOnly);
    if (ImGui::BeginPopupContextItem("##copy", ImGuiPopupFlags_MouseButtonRight)) {
        if (ImGui::Selectable("copy")) {
            ImGui::SetClipboardText(text.c_str());
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("copy")) {
        ImGui::SetClipboardText(text.c_str());
    }
    ImGui::PopID();

}

void argos::rgmui::RedText(const char* text)
{
    ImGui::TextColored({1.0f, 0.0f, 0.0f, 1.0f}, "%s", text);
}
void argos::rgmui::GreenText(const char* text)
{
    ImGui::TextColored({0.0f, 1.0f, 0.0f, 1.0f}, "%s", text);
}

bool argos::rgmui::InputColor(const char* label, ImU32* color, bool withAlpha) {
    ImVec4 c = ImGui::ColorConvertU32ToFloat4(*color);
    if (withAlpha) {
        if (ImGui::ColorEdit4(label, &c.x)) {
            *color = ImGui::ColorConvertFloat4ToU32(c);
            return true;
        }
    } else {
        if (ImGui::ColorEdit3(label, &c.x)) {
            *color = ImGui::ColorConvertFloat4ToU32(c);
            return true;
        }
    }
    return false;
}

ImVec4 argos::rgmui::PaletteColorToImVec4(uint8_t paletteIndex, const uint8_t* palette, bool bgrorder)
{
    const uint8_t* s = palette + static_cast<int>(paletteIndex) * 3;
    ImVec4 c;

    c.x = static_cast<float>(s[0]) / 255.0f;
    c.y = static_cast<float>(s[1]) / 255.0f;
    c.z = static_cast<float>(s[2]) / 255.0f;
    c.w = 1.0f;

    if (bgrorder) {
        std::swap(c.x, c.z);
    }

    return c;
}

bool argos::rgmui::InputPaletteIndex(const char* label, uint8_t* paletteIndex,
        const uint8_t* palette, int paletteSize, // will access x3
        bool bgrorder, int numcols)
{
    bool changed = false;
    int flags = ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoTooltip;
    ImVec4 color = PaletteColorToImVec4(*paletteIndex, palette, bgrorder);
    if (ImGui::ColorButton(label, color, flags)) {
        ImGui::OpenPopup(label);
    }
    if (ImGui::BeginPopup(label)) {
        int v = static_cast<int>(*paletteIndex);
        if (rgmui::SliderIntExt("Palette Entry", &v, 0, paletteSize - 1)) {
            *paletteIndex = static_cast<uint8_t>(v);
            changed = true;
        }

        if (ImGui::BeginTable("p2", numcols, ImGuiTableFlags_SizingFixedSame
                    | ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_NoPadInnerX)) {
            int tpalIndex = 0;
            for (;;) {
                if (tpalIndex >= paletteSize) break;
                ImGui::TableNextRow();
                for (int col = 0; col < numcols; col++) {
                    if (tpalIndex >= paletteSize) break;

                    ImGui::TableNextColumn();
                    ImVec4 tcolor = PaletteColorToImVec4(tpalIndex, palette, bgrorder);
                    std::string label = fmt::format("0x{:02x}", tpalIndex);

                    if (ImGui::ColorButton(label.c_str(), tcolor, flags)) {
                        *paletteIndex = static_cast<uint8_t>(tpalIndex);
                        changed = true;
                    }

                    tpalIndex++;
                }

            }
            ImGui::EndTable();
        }

        ImGui::EndPopup();
    }
    return changed;
}

template <typename T>
static bool SliderExt(T* v, T min, T max, bool allowArrowKeys, bool allowMouseWheel, T singleMove, T shiftMult) {
    bool changed = false;

    if (ImGui::IsItemHovered()) {
        if (allowMouseWheel) {
            SliderExtIsHovered();
        }

        T m = 0;
        if (allowArrowKeys &&
                ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
                m = -1;
            } else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
                m = 1;
            }
        }

        if (m == 0 && allowMouseWheel) {
            auto& io = ImGui::GetIO();
            m = static_cast<T>(-io.MouseWheel);
        }

        m *= singleMove;

        if (m && ShiftIsDown()) {
            m *= shiftMult;
        }

        if (m) {
            T nv = *v + static_cast<T>(m);
            if (nv < min) {
                nv = min;
            }
            if (nv > max) {
                nv = max;
            }

            if (nv != *v) {
                *v = nv;
                changed = true;
            }
        }
    }
    return changed;
}

bool argos::rgmui::SliderUint8Ext(const char* label, uint8_t* v, uint8_t min, uint8_t max,
        ImGuiSliderFlags flags, bool allowArrowKeys, bool allowMouseWheel, uint8_t singleMove, uint8_t shiftMult) {
    if (!v) throw std::invalid_argument("null v disallowed");
    int iv = static_cast<int>(*v);
    bool ret = SliderIntExt(label, &iv, static_cast<int>(min), static_cast<int>(max),
            "0x%02x", flags, allowArrowKeys, allowMouseWheel, static_cast<int>(singleMove), static_cast<int>(shiftMult));
    *v = static_cast<uint8_t>(iv);
    return ret;
}

bool argos::rgmui::SliderIntExt(const char* label, int* v, int min, int max,
        const char* format, ImGuiSliderFlags flags,
        bool allowArrowKeys, bool allowMouseWheel,
        int singleMove, int shiftMult) {
    bool changed = ImGui::SliderInt(label, v, min, max, format, flags);
    changed = SliderExt(v, min, max, allowArrowKeys, allowMouseWheel, singleMove, shiftMult) || changed;
    return changed;
}

bool argos::rgmui::SliderFloatExt(const char* label, float* v, float min, float max,
        const char* format, ImGuiSliderFlags flags,
        bool allowArrowKeys, bool allowMouseWheel,
        float singleMove, float shiftMult) {
    bool changed = ImGui::SliderFloat(label, v, min, max, format, flags);
    changed = SliderExt(v, min, max, allowArrowKeys, allowMouseWheel, singleMove, shiftMult) || changed;
    return changed;
}


void argos::rgmui::Mat(const char* label, const cv::Mat& img) {
    if (!img.isContinuous()) {
        cv::Mat m = img.clone();
        assert(m.isContinuous());
        Mat(label, m);
    }
    static std::unordered_map<ImGuiID, GLuint> s_GLuints;
    ImGuiID v = ImGui::GetID(label);

    auto it = s_GLuints.find(v);
    if (it != s_GLuints.end()) {
        glDeleteTextures(1, &it->second);
    }

    GLuint q = 0;
    glGenTextures(1, &q);
    s_GLuints[v] = q;
    glBindTexture(GL_TEXTURE_2D, q);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.cols, img.rows, 0,
            GL_BGR, GL_UNSIGNED_BYTE, img.data);

    ImGui::Image((void*)(intptr_t)(q), ImVec2(static_cast<float>(img.cols), static_cast<float>(img.rows)));
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

MatAnnotator::MatAnnotator(const char* label, const cv::Mat& mat, float scale, Vector2F origin,
        bool clipped)
    : m_OriginalCursorPosition(ImGui::GetCursorScreenPos())
    , m_List(ImGui::GetWindowDrawList())
    , m_Scale(scale)
    , m_Origin(origin)
    , m_Clipped(clipped)
{
    auto beforePos = ImGui::GetCursorScreenPos();
    Mat(label, mat);
    auto afterPos = ImGui::GetCursorScreenPos();
    m_BottomRight.x = m_OriginalCursorPosition.x + mat.cols * scale;
    m_BottomRight.y = m_OriginalCursorPosition.y + mat.rows * scale;

    m_IsHovered = ImGui::IsItemHovered();
    m_WasClicked = ImGui::IsItemClicked();

    ImGui::SetCursorScreenPos(beforePos);
    ImGui::InvisibleButton(fmt::format("{}_invis_button", label).c_str(),
            ImVec2(static_cast<float>(mat.cols), static_cast<float>(mat.rows)));
    ImGui::SetCursorScreenPos(afterPos);
}

MatAnnotator::~MatAnnotator() {
}


MatAnnotator::ClipHelper::ClipHelper(MatAnnotator* anno)
    : m_Anno(anno)
{
    if (m_Anno->m_Clipped) {
        m_Anno->m_List->PushClipRect(
                m_Anno->m_OriginalCursorPosition,
                m_Anno->m_BottomRight, true);
    }
}

MatAnnotator::ClipHelper::~ClipHelper() {
    if (m_Anno->m_Clipped) {
        m_Anno->m_List->PopClipRect();
    }
}

Vector2F MatAnnotator::MatPosToScreenPos2F(const Vector2F& v) const {
    Vector2F q;
    q.x = (v.x * m_Scale) + m_OriginalCursorPosition.x - m_Origin.x;
    q.y = (v.y * m_Scale) + m_OriginalCursorPosition.y - m_Origin.y;
    return q;
}

Vector2F MatAnnotator::ScreenPosToMatPos2F(const ImVec2& p) const {
    Vector2F v;
    v.x = (p.x + m_Origin.x - m_OriginalCursorPosition.x) / m_Scale;
    v.y = (p.y + m_Origin.y - m_OriginalCursorPosition.y) / m_Scale;
    return v;
}

Vector2I MatAnnotator::ScreenPosToMatPos2I(const ImVec2& p) const {
    Vector2F v = ScreenPosToMatPos2F(p);
    return Vector2I(static_cast<int>(v.x), static_cast<int>(v.y));
}

ImVec2 MatAnnotator::MatPosToScreenPos(const Vector2F& v) const {
    Vector2F q = MatPosToScreenPos2F(v);
    return ImVec2(q.x, q.y);
}

void MatAnnotator::AddLine(const Vector2F& p1, const Vector2F& p2, ImU32 col, float thickness) {
    auto clip = ClipHelper(this);
    AddLineNC(p1, p2, col, thickness);
}

void MatAnnotator::AddLineNC(const Vector2F& p1, const Vector2F& p2, ImU32 col, float thickness) {
    m_List->AddLine(
            MatPosToScreenPos(p1), MatPosToScreenPos(p2),
            col, thickness);
}

void MatAnnotator::AddBezierCubic(const Vector2F& p1, const Vector2F& p2, const Vector2F& p3, const Vector2F& p4, ImU32 col, float thickness, int num_segments) {
    auto clip = ClipHelper(this);
    AddBezierCubicNC(p1, p2, p3, p4, col, thickness, num_segments);
}

void MatAnnotator::AddBezierCubicNC(const Vector2F& p1, const Vector2F& p2, const Vector2F& p3, const Vector2F& p4, ImU32 col, float thickness, int num_segments) {
    m_List->AddBezierCubic(
            MatPosToScreenPos(p1), MatPosToScreenPos(p2),
            MatPosToScreenPos(p3), MatPosToScreenPos(p4),
            col, thickness, num_segments);
}

void MatAnnotator::AddText(const util::Vector2F& p, ImU32 col,
            const char* text_begin, const char* text_end)
{
    auto clip = ClipHelper(this);
    AddTextNC(p, col, text_begin, text_end);
}

void MatAnnotator::AddTextNC(const util::Vector2F& p, ImU32 col, const char* text_begin, const char* text_end)
{
    m_List->AddText(MatPosToScreenPos(p), col, text_begin, text_end);

}


void MatAnnotator::AddRect(const Vector2F& pmin, const Vector2F& pmax,
                 ImU32 col, float rounding, ImDrawFlags flags, float thickness) {
    auto clip = ClipHelper(this);
    AddRectNC(pmin, pmax, col, rounding, flags, thickness);
}

void MatAnnotator::AddRect(const Rect2F& rect,
                 ImU32 col, float rounding, ImDrawFlags flags, float thickness) {
    AddRect(rect.TopLeft(), rect.BottomRight(),
            col, rounding, flags, thickness);
}

void MatAnnotator::AddRectNC(const Vector2F& pmin, const Vector2F& pmax,
                 ImU32 col, float rounding, ImDrawFlags flags, float thickness) {
    m_List->AddRect(
            MatPosToScreenPos(pmin), MatPosToScreenPos(pmax),
            col, rounding, flags, thickness);
}

void MatAnnotator::AddRectFilled(const Vector2F& pmin, const Vector2F& pmax,
        ImU32 col, float rounding, ImDrawFlags flags) {
    auto clip = ClipHelper(this);
    AddRectFilledNC(pmin, pmax, col, rounding, flags);
}

void MatAnnotator::AddRectFilled(const Rect2F& rect,
        ImU32 col, float rounding, ImDrawFlags flags) {
    AddRectFilled(rect.TopLeft(), rect.BottomRight(), col, rounding, flags);
}

void MatAnnotator::AddRectFilledNC(const Vector2F& pmin, const Vector2F& pmax,
        ImU32 col, float rounding, ImDrawFlags flags) {
    m_List->AddRectFilled(
            MatPosToScreenPos(pmin), MatPosToScreenPos(pmax),
            col, rounding, flags);
}

bool MatAnnotator::IsHovered(bool requireWindowFocus)
{
    return m_IsHovered &&
        (!requireWindowFocus || ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));
}

bool MatAnnotator::WasClicked(Vector2I* pixel)
{
    if (pixel) {
        *pixel = HoveredPixel();
    }

    return m_WasClicked;
}

Vector2I MatAnnotator::HoveredPixel() const
{
    return ScreenPosToMatPos2I(ImGui::GetMousePos());
}

bool argos::rgmui::ArrowKeyHelperInFrame(int* dx, int* dy, int shiftMultiplier) {
    int tdx = 0;
    int tdy = 0;
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
        tdx = -1;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
        tdx = 1;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
        tdy = -1;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
        tdy = 1;
    }
    if (ShiftIsDown()) {
        tdx *= shiftMultiplier;
        tdy *= shiftMultiplier;
    }
    if (tdx || tdy) {
        if (dx) {
            *dx = tdx;
        }
        if (dy) {
            *dy = tdy;
        }

        return true;
    }
    return false;

}

PushPopID::PushPopID(int id)
{
    ImGui::PushID(id);
}
PushPopID::PushPopID(const char* id)
{
    ImGui::PushID(id);
}
PushPopID::~PushPopID()
{
    ImGui::PopID();
}

bool argos::rgmui::InputVectorInt(const char* label, std::vector<int>* vec)
{
    ImGui::PushID(label);
    bool changed = false;
    int q = 0;
    for (auto & v : *vec) {
        changed = ImGui::InputInt(fmt::format("{}[{}]", label, q).c_str(),
                &v) || changed;
        q++;
    }
    if (ImGui::Button("push_back")) {
        vec->push_back(0);
        changed = true;
    }
    if (ImGui::Button("pop_back")) {
        if (!vec->empty()) {
            vec->pop_back();
            changed = true;
        }
    }
    ImGui::PopID();
    return changed;
}

ImGuiWindowFlags argos::rgmui::SliderExtWasHoveredFlag(ImGuiWindowFlags flags)
{
    if (SliderExtWasHovered()) {
        flags |= ImGuiWindowFlags_NoScrollWithMouse;
    }
    return flags;
}

////////////////////////////////////////////////////////////////////////////////

bool argos::rgmui::PopupLeftRightHelper(const char* left, std::function<void()> onLeft,
        const char* right, std::function<void()> onRight)
{
    bool ret = false;

    ImVec2 space = ImGui::GetContentRegionAvail();
    float cy = ImGui::GetCursorPosY();
    float ty = cy + space.y - ImGui::GetFrameHeightWithSpacing() - 4;
    if (ty > cy) {
        ImGui::SetCursorPosY(ty);
    }

    ImGui::Separator();
    if (ImGui::BeginTable("leftright", 2)) {
        ImGui::TableNextColumn();
        if (ImGui::Button(left, ImVec2(-1, 0))) {
            if (onLeft) onLeft();
            ImGui::CloseCurrentPopup();
            ret = true;
        }

        ImGui::TableNextColumn();
        if (ImGui::Button(right, ImVec2(-1, 0))) {
            if (onRight) onRight();
            ImGui::CloseCurrentPopup();
            ret = true;
        }

        ImGui::EndTable();
    }

    //auto& io = ImGui::GetIO();
    //if (io.WantCaptureKeyboard && ImGui::IsKeyPressed(SDL_SCANCODE_RETURN)) {
    //    std::cout << "enter" << std::endl;
    //    if (onLeft) onLeft();
    //    ImGui::CloseCurrentPopup();
    //    ret = true;
    //}
    return ret;

}

bool argos::rgmui::PopupYesNoHelper(std::function<void()> onYes, std::function<void()> onNo)
{
    return PopupLeftRightHelper("Yes", onYes, "No", onNo);
}
bool argos::rgmui::PopupOkCancelHelper(std::function<void()> onOk, std::function<void()> onCancel)
{
    return PopupLeftRightHelper("OK", onOk, "Cancel", onCancel);
}

bool argos::rgmui::HandleCombo4Scroll(size_t* idx, size_t n)
{
    if (ImGui::IsItemHovered()) {
        g_ImGuiExtContext.SliderExtIsHovered = true;
        int o = 0;

        auto& io = ImGui::GetIO();
        if (io.MouseWheel) {
            o = -io.MouseWheel;
        }
        if (o) {
            if (*idx == n && n != 0) {
                *idx = 0;
                return true;
            }

            int q = static_cast<int>(*idx) + o;
            if (q >= 0 && q < static_cast<int>(n)) {
                *idx = static_cast<size_t>(q);
                return true;
            }
        }

    }
    return false;
}

ImGuiDemoComponent::ImGuiDemoComponent()
{
}

ImGuiDemoComponent::~ImGuiDemoComponent()
{
}

void ImGuiDemoComponent::OnFrame()
{
    ImGui::ShowDemoWindow();
}
