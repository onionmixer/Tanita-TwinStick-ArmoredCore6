#pragma once

#include "Report.h"
#include <array>
#include <iterator>

enum class OutputAction : unsigned char {
    None, A, B, X, Y, LeftShoulder, RightShoulder, LeftTrigger, RightTrigger,
    LeftStick, RightStick, Up, Right, Down, Left, View, Menu, Count
};

inline constexpr const wchar_t* OutputLabels[]{
    L"Unassigned", L"Xbox A / Jump or ascend", L"Xbox B / Boost", L"Xbox X / Quick boost",
    L"Xbox Y / Shift or interact", L"Xbox LB / Left shoulder weapon", L"Xbox RB / Right shoulder weapon",
    L"Xbox LT / Left hand weapon", L"Xbox RT / Right hand weapon", L"Xbox L3 / Assault boost",
    L"Xbox R3 / Target assist", L"D-pad Up / Repair kit", L"D-pad Right / Navigate right",
    L"D-pad Down / Scan", L"D-pad Left / Navigate left", L"Xbox View / Photo mode", L"Xbox Menu / Pause"
};
static_assert(std::size(OutputLabels) == static_cast<unsigned>(OutputAction::Count));

inline void ApplyOutput(TanitaReport& report, OutputAction action) {
    constexpr unsigned short bits[]{0, 0x1000, 0x2000, 0x4000, 0x8000, 0x0100, 0x0200,
                                    0, 0, 0x0040, 0x0080, 1, 8, 2, 4, 0x0020, 0x0010};
    static_assert(std::size(bits) == std::size(OutputLabels));
    if (action == OutputAction::LeftTrigger) report.leftTrigger = 255;
    if (action == OutputAction::RightTrigger) report.rightTrigger = 255;
    report.buttons |= bits[static_cast<unsigned>(action)];
}

struct Bindings {
    static constexpr int LeftTurbo = 7;
    static constexpr int RightTurbo = 8;
    static constexpr bool IsTurbo(int usage) { return usage == LeftTurbo || usage == RightTurbo; }
    static constexpr int HatSlot(int direction) { return 14 + direction / 2; }
    // Button usages 1..14 occupy slots 0..13; the four cardinal directions follow.
    std::array<OutputAction, 18> actions{
        OutputAction::X, OutputAction::A, OutputAction::B, OutputAction::Y,
        OutputAction::LeftTrigger, OutputAction::RightTrigger, OutputAction::None, OutputAction::None,
        OutputAction::View, OutputAction::Menu, OutputAction::LeftShoulder, OutputAction::RightShoulder,
        OutputAction::RightStick, OutputAction::None, OutputAction::Up, OutputAction::Right,
        OutputAction::Down, OutputAction::Left
    };
};
