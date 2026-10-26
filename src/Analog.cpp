#include "Analog.hpp"

#include <algorithm>
#include <cmath>

namespace WA
{
namespace
{
float Clamp01(float aValue)
{
    if (!(aValue > 0.f))
        return 0.f;
    return aValue > 1.f ? 1.f : aValue;
}

void CompensateStick(float& aX, float& aY, const Tuning& aTuning)
{
    const float length = std::sqrt(aX * aX + aY * aY);
    if (length <= 0.f)
        return;

    const float raw = aTuning.compensateStickDeadzone
                          ? ToRawStickMagnitude(length, aTuning.stickInner, aTuning.stickOuter)
                          : length;
    const float scale = raw / length;
    aX *= scale;
    aY *= scale;
}

void ClampToCircle(float& aX, float& aY)
{
    const float length = std::sqrt(aX * aX + aY * aY);
    if (length > 1.f)
    {
        aX /= length;
        aY /= length;
    }
}
}

const char* ToString(Context aContext)
{
    switch (aContext)
    {
    case Context::Unknown:
        return "unknown";
    case Context::OnFoot:
        return "on foot";
    case Context::Driving:
        return "driving";
    case Context::Tank:
        return "tank";
    case Context::Neutral:
        return "neutral";
    }
    return "?";
}

float Curve::Apply(float aTravel) const
{
    const float travel = Clamp01(aTravel);
    if (travel <= deadzone)
        return 0.f;

    const float span = full - deadzone;
    const float t = span > 0.f ? Clamp01((travel - deadzone) / span) : 1.f;
    if (gamma == 1.f || t >= 1.f)
        return t;
    return std::pow(t, gamma);
}

void Curve::Sanitize()
{
    deadzone = std::clamp(std::isfinite(deadzone) ? deadzone : 0.08f, 0.f, 0.9f);
    full = std::clamp(std::isfinite(full) ? full : 0.85f, deadzone + 0.05f, 1.f);
    gamma = std::clamp(std::isfinite(gamma) ? gamma : 1.f, 0.2f, 5.f);
}

void Tuning::Sanitize()
{
    move.Sanitize();
    steer.Sanitize();
    throttle.Sanitize();
    brake.Sanitize();
    stickInner = std::clamp(std::isfinite(stickInner) ? stickInner : 0.05f, 0.f, 0.5f);
    stickOuter = std::clamp(std::isfinite(stickOuter) ? stickOuter : 0.9f, stickInner + 0.05f, 1.f);
}

bool PadOutput::IsZero() const
{
    return lx == 0.f && ly == 0.f && lt == 0.f && rt == 0.f;
}

float ToRawStickMagnitude(float aDesired, float aInner, float aOuter)
{
    const float desired = Clamp01(aDesired);
    if (desired <= 0.f)
        return 0.f;
    if (desired >= 0.999f)
        return 1.f;
    return std::min(1.f, aInner + desired * (aOuter - aInner));
}

PadOutput Compute(const KeyValues& aKeys, Context aContext, const Tuning& aTuning, bool aTriggers)
{
    PadOutput out;

    switch (aContext)
    {
    case Context::Neutral:
        return out;

    case Context::Unknown:
    case Context::OnFoot:
    {
        out.lx = aTuning.move.Apply(aKeys.right) - aTuning.move.Apply(aKeys.left);
        out.ly = aTuning.move.Apply(aKeys.forward) - aTuning.move.Apply(aKeys.back);
        ClampToCircle(out.lx, out.ly);
        CompensateStick(out.lx, out.ly, aTuning);
        return out;
    }

    case Context::Driving:
    {
        out.lx = aTuning.steer.Apply(aKeys.steerRight) - aTuning.steer.Apply(aKeys.steerLeft);
        CompensateStick(out.lx, out.ly, aTuning);
        if (aTriggers)
        {
            out.rt = aTuning.throttle.Apply(aKeys.accelerate);
            out.lt = aTuning.brake.Apply(aKeys.brake);
        }
        return out;
    }

    case Context::Tank:
    {
        out.lx = aTuning.steer.Apply(aKeys.steerRight) - aTuning.steer.Apply(aKeys.steerLeft);
        out.ly = aTuning.throttle.Apply(aKeys.accelerate) - aTuning.brake.Apply(aKeys.brake);
        ClampToCircle(out.lx, out.ly);
        CompensateStick(out.lx, out.ly, aTuning);
        return out;
    }
    }

    return out;
}
}
