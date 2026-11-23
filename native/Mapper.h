#pragma once
#include "Report.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <optional>
#include <utility>

struct PhysicalInput {
    double lx{}, ly{}, rx{}, ry{};
    unsigned buttons{};
    int hat = 8;
};

class Mapper {
public:
    using Clock = std::chrono::steady_clock;
    bool camera{};

    static bool Pressed(const PhysicalInput& input, int usage) {
        return (input.buttons & (1u << (usage - 1))) != 0;
    }

    TanitaReport Step(const PhysicalInput& input, Clock::time_point now) {
        const bool leftTurbo = Pressed(input, 7);
        const bool rightTurbo = Pressed(input, 8);
        const bool turbo = leftTurbo || rightTurbo;
        camera = leftTurbo && rightTurbo;
        if (camera) {
            chord_ = true;
            pending_.reset();
            pulse_.reset();
        } else if (chord_) {
            if (!turbo) chord_ = false;
        } else if (turbo) {
            if (!previousTurbo_) pending_ = now;
            if (pending_ && now - *pending_ >= std::chrono::milliseconds(90)) {
                pulse_ = now + std::chrono::milliseconds(85);
                pending_.reset();
            }
        } else if (pending_) {
            pulse_ = now + std::chrono::milliseconds(85);
            pending_.reset();
        }
        previousTurbo_ = turbo;

        TanitaReport out{};
        constexpr std::pair<int, int> buttons[]{
            {1, 0x4000}, {2, 0x1000}, {3, 0x2000}, {4, 0x8000},
            {9, 0x0020}, {10, 0x0010}, {11, 0x0100}, {12, 0x0200}, {13, 0x0080}
        };
        for (const auto& [usage, bit] : buttons) {
            if (Pressed(input, usage)) out.buttons |= static_cast<unsigned short>(bit);
        }
        constexpr unsigned short hats[]{1, 9, 8, 10, 2, 6, 4, 5};
        if (input.hat >= 0 && input.hat < 8) out.buttons |= hats[input.hat];
        if (pulse_ && now < *pulse_) out.buttons |= 0x0040;
        out.leftTrigger = Pressed(input, 5) ? 255 : 0;
        out.rightTrigger = Pressed(input, 6) ? 255 : 0;

        const auto left = Direction(input.lx, input.ly);
        const auto right = Direction(input.rx, input.ry);
        double x = (input.lx + input.rx) * 0.5;
        double y = (input.ly + input.ry) * 0.5;
        if (camera) {
            out.rightX = Axis(x);
            out.rightY = Axis(y);
        } else if (left == std::pair{-1, 0} && right == std::pair{1, 0}) {
            out.buttons |= 0x1000;
        } else if (!left.first && !right.first && left.second && right.second == -left.second) {
            out.rightX = Axis(left.second * 0.75);
        } else if (left == right) {
            if (left.first) out.leftX = Axis(x);
            if (left.second) out.leftY = Axis(y);
        }
        return out;
    }

private:
    static std::pair<int, int> Direction(double x, double y) {
        const auto component = [](double value) { return (value >= .6) - (value <= -.6); };
        return {component(x), component(y)};
    }

    static short Axis(double value) {
        return static_cast<short>(std::lround(std::clamp(value, -1., 1.) * 32767));
    }

    bool chord_{};
    bool previousTurbo_{};
    std::optional<Clock::time_point> pending_;
    std::optional<Clock::time_point> pulse_;
};
