#include "Log.hpp"
#include "XInputHook.hpp"

#include <cstdio>

#include <Windows.h>
#include <Xinput.h>

namespace
{
int s_failures = 0;
int s_checks = 0;

#define CHECK(cond)                                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        ++s_checks;                                                                                                    \
        if (!(cond))                                                                                                   \
        {                                                                                                              \
            ++s_failures;                                                                                              \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                                \
        }                                                                                                              \
    } while (0)

WA::PadOutput s_next;
bool s_enabled = false;

bool Provide(WA::PadOutput& aOut)
{
    aOut = s_next;
    return s_enabled;
}
}

int main()
{
    XINPUT_STATE real{};
    const DWORD realResult = XInputGetState(0, &real);
    const bool realPad = realResult == ERROR_SUCCESS;
    std::printf("real controller in slot 0: %s\n", realPad ? "yes" : "no");

    CHECK(WA::XInputHook::Install(&Provide, 0, nullptr));
    CHECK(WA::XInputHook::IsInstalled());
    std::printf("hook method: %s\n", WA::XInputHook::Method());

    XINPUT_STATE state{};
    CHECK(XInputGetState(0, &state) == realResult);

    s_enabled = true;
    s_next = {0.5f, -1.f, 0.25f, 1.f};
    CHECK(XInputGetState(0, &state) == ERROR_SUCCESS);
    CHECK(WA::XInputHook::RealPadPresent() == realPad);
    if (!realPad)
    {
        CHECK(state.Gamepad.sThumbLX == 16384);
        CHECK(state.Gamepad.sThumbLY == -32768);
        CHECK(state.Gamepad.bLeftTrigger == 64);
        CHECK(state.Gamepad.bRightTrigger == 255);
        CHECK(state.Gamepad.wButtons == 0);
    }
    const DWORD packet = state.dwPacketNumber;

    CHECK(XInputGetState(0, &state) == ERROR_SUCCESS);
    CHECK(state.dwPacketNumber == packet);
    s_next = {0.f, 0.3f, 0.f, 0.f};
    CHECK(XInputGetState(0, &state) == ERROR_SUCCESS);
    CHECK(state.dwPacketNumber != packet);

    s_enabled = false;
    CHECK(XInputGetState(0, &state) == ERROR_SUCCESS);
    if (!realPad)
    {
        CHECK(state.Gamepad.sThumbLX == 0 && state.Gamepad.sThumbLY == 0);
        CHECK(state.Gamepad.bLeftTrigger == 0 && state.Gamepad.bRightTrigger == 0);
    }

    XINPUT_STATE other{};
    XINPUT_STATE otherReal{};
    WA::XInputHook::Uninstall();
    const DWORD otherExpected = XInputGetState(1, &otherReal);
    CHECK(WA::XInputHook::Install(&Provide, 0, nullptr));
    s_enabled = true;
    s_next = {1.f, 1.f, 1.f, 1.f};
    CHECK(XInputGetState(1, &other) == otherExpected);

    WA::XInputHook::Uninstall();
    CHECK(!WA::XInputHook::IsInstalled());
    CHECK(XInputGetState(0, &state) == realResult);
    CHECK(WA::XInputHook::CallCount() == 6);

    std::printf("\n%d checks, %d failed\n", s_checks, s_failures);
    return s_failures == 0 ? 0 : 1;
}
