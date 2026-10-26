#pragma once

#include <cstdint>

namespace WA
{
enum class Context : int32_t
{
    Unknown = 0,
    OnFoot = 1,
    Driving = 2,
    Tank = 3,
    Neutral = 4,
};

const char* ToString(Context aContext);

struct Curve
{
    float deadzone = 0.08f;
    float full = 0.85f;
    float gamma = 1.0f;

    float Apply(float aTravel) const;
    void Sanitize();
};

struct Tuning
{
    Curve move{0.08f, 0.80f, 1.0f};
    Curve steer{0.06f, 0.90f, 1.4f};
    Curve throttle{0.06f, 0.90f, 1.0f};
    Curve brake{0.06f, 0.85f, 1.0f};

    bool compensateStickDeadzone = false;
    float stickInner = 0.05f;
    float stickOuter = 0.90f;

    void Sanitize();
};

struct KeyValues
{
    float forward = 0.f;
    float back = 0.f;
    float left = 0.f;
    float right = 0.f;
    float accelerate = 0.f;
    float brake = 0.f;
    float steerLeft = 0.f;
    float steerRight = 0.f;
};

struct PadOutput
{
    float lx = 0.f;
    float ly = 0.f;
    float lt = 0.f;
    float rt = 0.f;

    bool IsZero() const;
};

PadOutput Compute(const KeyValues& aKeys, Context aContext, const Tuning& aTuning, bool aTriggers);

float ToRawStickMagnitude(float aDesired, float aInner, float aOuter);
}
