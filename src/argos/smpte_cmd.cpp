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

#include "argos/main.h"
#include "util/arg.h"
#include "ext/sdlext/sdlext.h"

using namespace argos;
using namespace argos::main;

class SMPTEApplication : public argos::rgmui::IApplication
{
public:
    SMPTEApplication();
    ~SMPTEApplication();

    void OnFirstFrame() override;
    bool OnFrame() override;

    void SetMute(bool muted);

private:
    std::unique_ptr<sdlext::SDLExtMixInit> m_MixInit;
    std::vector<uint8_t> m_AudioData;
    Mix_Chunk* m_Chunk;
    bool m_HideCursor;
    bool m_Mute;
};

SMPTEApplication::SMPTEApplication()
    : m_MixInit(std::make_unique<sdlext::SDLExtMixInit>())
    , m_Chunk(nullptr)
    , m_HideCursor(true)
    , m_Mute(false)
{
}

SMPTEApplication::~SMPTEApplication()
{
}

void SMPTEApplication::OnFirstFrame()
{
    int frequency;
    uint16_t format;
    int channels;

    if (Mix_QuerySpec(&frequency, &format, &channels)) {
        if (format == AUDIO_S16SYS) {
            m_AudioData.resize(frequency * 2);
            int16_t* p = reinterpret_cast<int16_t*>(m_AudioData.data());

            double angle = 0.0;
            for (int i = 0; i < frequency; i++) {
                *p++ = static_cast<int16_t>(std::sin(angle) * static_cast<float>(std::numeric_limits<int16_t>::max() - 1));
                angle += 1000 * M_PI / static_cast<double>(frequency);
            }
        } else if (format == AUDIO_S32SYS) {
            m_AudioData.resize(frequency * 4);
            int32_t* p = reinterpret_cast<int32_t*>(m_AudioData.data());

            double angle = 0.0;
            for (int i = 0; i < frequency; i++) {
                *p++ = static_cast<int32_t>(std::sin(angle) * static_cast<float>(std::numeric_limits<int32_t>::max() - 1));
                angle += 1000 * M_PI / static_cast<double>(frequency);
            }

        } else {
            std::cerr << "unsupported audio format: " << format << std::endl;
            return;
        }

        m_Chunk = Mix_QuickLoad_RAW(m_AudioData.data(), m_AudioData.size());
        if (!m_Mute) {
            Mix_PlayChannel(-1, m_Chunk, -1);
        }
    }
}

bool SMPTEApplication::OnFrame()
{
    auto* list = ImGui::GetForegroundDrawList();
    auto [width, height] = list->GetClipRectMax();

    float w7 = static_cast<float>(width) / 7.0f;
    float h4 = static_cast<float>(height) / 4.0f;

    for (int i = 0; i < 7; i++) {
        float fi = static_cast<float>(i);
        float sx = fi * w7;
        float ex = (fi + 1.0f) * w7;
        if (i == 6) {
            ex = width;
        }
        uint8_t b = ((i + 1) % 2) ? 0xff : 0x00;
        uint8_t r = (((i / 2) + 1) % 2) ? 0xff : 0x00;
        uint8_t g = (((i / 4) + 1) % 2) ? 0xff : 0x00;
        list->AddRectFilled({sx, 0}, {ex, height - h4}, IM_COL32(r, g, b, 0xff));
    }
    list->AddRectFilled({0, height-h4}, {width, height}, IM_COL32_BLACK);
    float sx = static_cast<float>(width) / 21.0f * 4;
    float ex = static_cast<float>(width) / 21.0f * 8;
    list->AddRectFilled({sx, height-h4}, {ex, height}, IM_COL32_WHITE);


    if (ImGui::IsKeyReleased(ImGuiKey_C)) {
        m_HideCursor = !m_HideCursor;
    }
    if (m_HideCursor) {
        SDL_ShowCursor(SDL_DISABLE);
    }
    if (ImGui::IsKeyReleased(ImGuiKey_M)) {
        SetMute(!m_Mute);
    }

    return true;
}

void SMPTEApplication::SetMute(bool mute) {
    m_Mute = mute;
    if (m_Mute) {
        Mix_HaltChannel(-1);
    } else if (m_Chunk) {
        Mix_PlayChannel(-1, m_Chunk, -1);
    }
}


REGISTER_COMMAND(smpte, "SMPTE color bars and tone",
R"(
EXAMPLES:
    argos smpte
    argos smpte --quiet

USAGE:
    argos smpte [<args>...]

DESCRIPTION:
    The 'smpte' command is to open a window with minimal other considerations
    to confirm that the main 'window' args and volume are set properly. TODO

OPTIONS:
    --mute
        Don't play the 1khz sine wave
)")
{
    bool muted = false;
    std::string arg;
    while (util::ArgReadString(&argc, &argv, &arg)) {
        if (arg == "--mute") {
            muted = true;
        } else {
            std::cerr << "error: unknown arg '" << arg << "'\n";
        }
    }

    SMPTEApplication app;
    app.SetMute(muted);
    return RunIApplication(config, "argos smpte", &app);
}

