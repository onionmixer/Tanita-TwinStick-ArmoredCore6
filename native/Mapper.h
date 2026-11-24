#pragma once
#include "Bindings.h"
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

// HID hats report eight clockwise directions, with diagonals between cardinal values.
inline bool HatIncludesDirection(int hat, int cardinal) {
    return hat >= 0 && hat < 8 &&
           (hat == cardinal || (hat + 1) % 8 == cardinal || (hat + 7) % 8 == cardinal);
}

class Mapper {
public:
    using Clock = std::chrono::steady_clock;
    bool camera{};

    static bool Pressed(const PhysicalInput& input, int usage) {
        return (input.buttons & (1u << (usage - 1))) != 0;
    }

    TanitaReport Step(const PhysicalInput& input, Clock::time_point now, const Bindings& bindings = {}) {
        const bool leftTurbo = Pressed(input, Bindings::LeftTurbo);
        const bool rightTurbo = Pressed(input, Bindings::RightTurbo);
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
        for (int usage = 1; usage <= 13; ++usage) {
            if (!Bindings::IsTurbo(usage) && Pressed(input, usage))
                ApplyOutput(out, bindings.actions[usage - 1]);
        }
        for (int direction = 0; direction < 8; direction += 2) {
            if (HatIncludesDirection(input.hat, direction))
                ApplyOutput(out, bindings.actions[Bindings::HatSlot(direction)]);
        }
        if (pulse_ && now < *pulse_) out.buttons |= 0x0040;

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
