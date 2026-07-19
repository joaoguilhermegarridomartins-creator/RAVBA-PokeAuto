#include "wx/pokeauto.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

#include <wx/event.h>
#include <wx/xrc/xmlres.h>

#include "core/base/system.h"
#include "core/gba/gbaGlobals.h"
#include "wx/wxvbam.h"

namespace pokeauto {
namespace {

using Clock = std::chrono::steady_clock;

enum class EventType {
    kReset,
    kPress,
    kRelease,
};

struct SequenceEvent {
    uint32_t at_ms;
    EventType type;
    uint32_t button;
};

// Embedded from firered_starter_sequence.json, recorded by the user at 10x.
constexpr std::array<SequenceEvent, 49> kFireRedStarterSequence = {{
    {212, EventType::kReset, 0},
    {546, EventType::kPress, KEYM_A},
    {678, EventType::kRelease, KEYM_A},
    {754, EventType::kPress, KEYM_A},
    {860, EventType::kRelease, KEYM_A},
    {918, EventType::kPress, KEYM_A},
    {1018, EventType::kRelease, KEYM_A},
    {1137, EventType::kPress, KEYM_A},
    {1299, EventType::kRelease, KEYM_A},
    {1373, EventType::kPress, KEYM_A},
    {1465, EventType::kRelease, KEYM_A},
    {1533, EventType::kPress, KEYM_A},
    {1646, EventType::kRelease, KEYM_A},
    {1698, EventType::kPress, KEYM_A},
    {1867, EventType::kRelease, KEYM_A},
    {1923, EventType::kPress, KEYM_A},
    {2025, EventType::kRelease, KEYM_A},
    {2104, EventType::kPress, KEYM_A},
    {2145, EventType::kRelease, KEYM_A},
    {2234, EventType::kPress, KEYM_A},
    {2309, EventType::kRelease, KEYM_A},
    {2384, EventType::kPress, KEYM_A},
    {2459, EventType::kRelease, KEYM_A},
    {2536, EventType::kPress, KEYM_A},
    {2609, EventType::kRelease, KEYM_A},
    {2693, EventType::kPress, KEYM_A},
    {2774, EventType::kRelease, KEYM_A},
    {2855, EventType::kPress, KEYM_A},
    {2939, EventType::kRelease, KEYM_A},
    {3118, EventType::kPress, KEYM_B},
    {3234, EventType::kRelease, KEYM_B},
    {3309, EventType::kPress, KEYM_B},
    {3394, EventType::kRelease, KEYM_B},
    {3475, EventType::kPress, KEYM_B},
    {3555, EventType::kRelease, KEYM_B},
    {3633, EventType::kPress, KEYM_B},
    {3729, EventType::kRelease, KEYM_B},
    {3805, EventType::kPress, KEYM_B},
    {3894, EventType::kRelease, KEYM_B},
    {3969, EventType::kPress, KEYM_B},
    {4055, EventType::kRelease, KEYM_B},
    {4123, EventType::kPress, KEYM_B},
    {4224, EventType::kRelease, KEYM_B},
    {4323, EventType::kPress, KEYM_B},
    {4439, EventType::kRelease, KEYM_B},
    {4508, EventType::kPress, KEYM_B},
    {4626, EventType::kRelease, KEYM_B},
    {4700, EventType::kPress, KEYM_B},
    {4796, EventType::kRelease, KEYM_B},
}};

constexpr uint32_t kPartyCountOffset = 0x24029;  // 0x02024029
constexpr uint32_t kPartyOffset = 0x24284;       // 0x02024284
constexpr uint32_t kResultTimeoutMs = 10000;
constexpr uint32_t kNextAttemptDelayMs = 250;
constexpr uint32_t kSpeedupFrameSkip = 9;        // approximately 10x

enum class Phase {
    kStopped,
    kPlayingSequence,
    kWaitingForResult,
    kCoolingDown,
};

struct State {
    Phase phase = Phase::kStopped;
    Clock::time_point phase_started{};
    Clock::time_point result_deadline{};
    Clock::time_point next_attempt{};
    size_t event_index = 0;
    uint32_t buttons = 0;
    uint64_t attempts = 0;
    uint32_t last_pid = 0;
    uint32_t last_otid = 0;
    uint16_t last_shiny_value = 0;
    bool last_was_shiny = false;
    std::string status = "PokeAuto stopped.";

    bool settings_saved = false;
    uint32_t saved_speedup_throttle = 100;
    uint32_t saved_speedup_frame_skip = 0;
    bool saved_speedup_throttle_frame_skip = false;
    bool saved_speedup_mute = true;
};

State g_state;

uint32_t ReadU32(const uint8_t* data) {
    uint32_t value = 0;
    std::memcpy(&value, data, sizeof(value));
    return value;
}

bool IsSupportedFireRed() {
    if (!g_rom || !g_workRAM)
        return false;

    // GBA header: game code at 0xAC and software version at 0xBC.
    return std::memcmp(g_rom + 0xAC, "BPRE", 4) == 0 && g_rom[0xBC] == 1;
}

void ShowStatus(const std::string& text) {
    g_state.status = text;
    systemScreenMessage(text.c_str());
}

void SaveSpeedupSettings() {
    if (g_state.settings_saved)
        return;

    g_state.saved_speedup_throttle = coreOptions.speedup_throttle;
    g_state.saved_speedup_frame_skip = coreOptions.speedup_frame_skip;
    g_state.saved_speedup_throttle_frame_skip = coreOptions.speedup_throttle_frame_skip;
    g_state.saved_speedup_mute = coreOptions.speedup_mute;
    g_state.settings_saved = true;

    coreOptions.speedup_throttle = 100;
    coreOptions.speedup_frame_skip = kSpeedupFrameSkip;
    coreOptions.speedup_throttle_frame_skip = false;
    coreOptions.speedup_mute = true;
}

void RestoreSpeedupSettings() {
    if (!g_state.settings_saved)
        return;

    coreOptions.speedup_throttle = g_state.saved_speedup_throttle;
    coreOptions.speedup_frame_skip = g_state.saved_speedup_frame_skip;
    coreOptions.speedup_throttle_frame_skip = g_state.saved_speedup_throttle_frame_skip;
    coreOptions.speedup_mute = g_state.saved_speedup_mute;
    coreOptions.speedup = false;
    g_state.settings_saved = false;
}

void QueueEmulatorReset() {
    MainFrame* frame = wxGetApp().frame;
    if (!frame)
        return;

    auto* event = new wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED, XRCID("Reset"));
    event->SetEventObject(frame);
    wxQueueEvent(frame->GetEventHandler(), event);
}

void BeginAttempt(const Clock::time_point now) {
    g_state.phase = Phase::kPlayingSequence;
    g_state.phase_started = now;
    g_state.event_index = 0;
    g_state.buttons = 0;
}

void StopInternal(const std::string& message) {
    g_state.phase = Phase::kStopped;
    g_state.buttons = 0;
    RestoreSpeedupSettings();
    ShowStatus(message);
}

void WriteFoundResult() {
    std::ofstream output("PokeAuto-found.txt", std::ios::out | std::ios::app);
    if (!output)
        return;

    output << "attempt=" << g_state.attempts
           << " pid=" << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << g_state.last_pid
           << " otid=" << std::setw(8) << g_state.last_otid
           << std::dec << " shinyValue=" << g_state.last_shiny_value
           << " shiny=YES\n";
}

void EvaluateStarter(const Clock::time_point now) {
    if (!g_workRAM) {
        StopInternal("PokeAuto stopped: GBA work RAM is unavailable.");
        return;
    }

    const uint8_t party_count = g_workRAM[kPartyCountOffset];
    if (party_count == 0) {
        if (now >= g_state.result_deadline)
            StopInternal("PokeAuto stopped: starter was not detected. Check the save position or sequence.");
        return;
    }

    g_state.last_pid = ReadU32(g_workRAM + kPartyOffset);
    g_state.last_otid = ReadU32(g_workRAM + kPartyOffset + 4);

    const uint16_t pid_low = static_cast<uint16_t>(g_state.last_pid & 0xFFFF);
    const uint16_t pid_high = static_cast<uint16_t>(g_state.last_pid >> 16);
    const uint16_t tid = static_cast<uint16_t>(g_state.last_otid & 0xFFFF);
    const uint16_t sid = static_cast<uint16_t>(g_state.last_otid >> 16);
    g_state.last_shiny_value = static_cast<uint16_t>(tid ^ sid ^ pid_low ^ pid_high);
    g_state.last_was_shiny = g_state.last_shiny_value < 8;
    ++g_state.attempts;

    std::ostringstream message;
    message << "PokeAuto attempt " << g_state.attempts
            << ": PID=" << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << g_state.last_pid
            << " OTID=" << std::setw(8) << g_state.last_otid
            << std::dec << " value=" << g_state.last_shiny_value
            << " shiny=" << (g_state.last_was_shiny ? "YES" : "NO");

    if (g_state.last_was_shiny) {
        WriteFoundResult();
        StopInternal(message.str() + " - AUTOMATION STOPPED");
        wxBell();
        return;
    }

    ShowStatus(message.str());
    g_state.buttons = 0;
    g_state.phase = Phase::kCoolingDown;
    g_state.next_attempt = now + std::chrono::milliseconds(kNextAttemptDelayMs);
}

}  // namespace

bool StartFireRedStarterHunt() {
    if (!IsSupportedFireRed()) {
        ShowStatus("PokeAuto requires Pokemon FireRed BPRE revision 1 with a loaded pre-starter save.");
        return false;
    }

    SaveSpeedupSettings();
    g_state.attempts = 0;
    g_state.last_pid = 0;
    g_state.last_otid = 0;
    g_state.last_shiny_value = 0;
    g_state.last_was_shiny = false;
    BeginAttempt(Clock::now());
    ShowStatus("PokeAuto started: FireRed starter hunt at approximately 10x.");
    return true;
}

void Stop() {
    if (g_state.phase == Phase::kStopped) {
        ShowStatus("PokeAuto is already stopped.");
        return;
    }

    StopInternal("PokeAuto stopped by user.");
}

void Tick() {
    if (g_state.phase == Phase::kStopped)
        return;

    if (!IsSupportedFireRed()) {
        StopInternal("PokeAuto stopped: the loaded ROM is not supported.");
        return;
    }

    const Clock::time_point now = Clock::now();

    if (g_state.phase == Phase::kCoolingDown) {
        if (now >= g_state.next_attempt)
            BeginAttempt(now);
        return;
    }

    if (g_state.phase == Phase::kWaitingForResult) {
        EvaluateStarter(now);
        return;
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_state.phase_started).count();
    while (g_state.event_index < kFireRedStarterSequence.size() &&
           elapsed >= kFireRedStarterSequence[g_state.event_index].at_ms) {
        const SequenceEvent& event = kFireRedStarterSequence[g_state.event_index++];
        switch (event.type) {
            case EventType::kReset:
                g_state.buttons = 0;
                QueueEmulatorReset();
                break;
            case EventType::kPress:
                g_state.buttons |= event.button;
                break;
            case EventType::kRelease:
                g_state.buttons &= ~event.button;
                break;
        }
    }

    if (g_state.event_index >= kFireRedStarterSequence.size()) {
        g_state.buttons = 0;
        g_state.phase = Phase::kWaitingForResult;
        g_state.result_deadline = now + std::chrono::milliseconds(kResultTimeoutMs);
    }
}

uint32_t GetInputMask() {
    if (g_state.phase == Phase::kStopped)
        return 0;

    // KEYM_SPEED activates VBA-M's internal speedup. No OS-level controller is created.
    return g_state.buttons | KEYM_SPEED;
}

bool IsRunning() {
    return g_state.phase != Phase::kStopped;
}

std::string GetStatusText() {
    return g_state.status;
}

}  // namespace pokeauto
