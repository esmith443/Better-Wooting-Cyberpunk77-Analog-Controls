#include "Analog.hpp"
#include "GameBindings.hpp"
#include "Log.hpp"
#include "Version.hpp"
#include "WootingSdk.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

#include <Windows.h>

namespace
{
std::string Bar(float aValue, int aWidth)
{
    const int filled = static_cast<int>(aValue * aWidth + 0.5f);
    std::string bar = "[";
    for (int i = 0; i < aWidth; ++i)
        bar += i < filled ? '#' : '.';
    return bar + "]";
}

std::string SignedBar(float aValue, int aHalf)
{
    std::string bar = "[";
    const int filled = static_cast<int>(std::abs(aValue) * aHalf + 0.5f);
    for (int i = aHalf; i > 0; --i)
        bar += (aValue < 0.f && i <= filled) ? '#' : '.';
    bar += '|';
    for (int i = 1; i <= aHalf; ++i)
        bar += (aValue > 0.f && i <= filled) ? '#' : '.';
    return bar + "]";
}
}

int main(int argc, char** argv)
{
    int seconds = 15;
    bool summaryOnly = false;
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--seconds") && i + 1 < argc)
            seconds = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--summary"))
            summaryOnly = true;
    }

    SetConsoleOutputCP(CP_UTF8);
    std::printf("%s %s - keyboard check\n\n", WA_MOD_NAME, WA_VERSION_STRING);

    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    const auto exeDir = std::filesystem::path(exePath).parent_path();

    WA::WootingSdk sdk;
    if (!sdk.Start({exeDir}, 500))
    {
        std::printf("SDK: FAILED - %s\n", sdk.LastError().c_str());
        return 2;
    }
    std::printf("SDK: %s (%s)\n", sdk.Version().c_str(), sdk.LoadedFrom().string().c_str());

    for (int i = 0; i < 20 && sdk.Devices().empty(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

    const auto devices = sdk.Devices();
    if (devices.empty())
    {
        std::printf("Keyboard: NOT FOUND - plug in your Wooting keyboard (and close any app that blocks it)\n");
        return 3;
    }
    for (const auto& device : devices)
    {
        const bool named = device.name.rfind(device.manufacturer, 0) == 0;
        std::printf("Keyboard: %s%s%s (%04X:%04X), Wootility gamepad mode: %s\n",
                    named ? "" : device.manufacturer.c_str(), named ? "" : " ", device.name.c_str(), device.vendorId,
                    device.productId, device.GamepadMode());
        if (!std::strcmp(device.GamepadMode(), "xbox") || !std::strcmp(device.GamepadMode(), "classic"))
            std::printf("  note: gamepad mode is on. That is fine as long as no keys are bound to gamepad inputs in\n"
                        "        Wootility; otherwise set gamepad mode to None to avoid double input.\n");
    }

    WA::Bindings bindings = WA::Bindings::Defaults();
    const auto settingsPath = WA::DefaultUserSettingsPath();
    std::string error;
    if (WA::LoadGameBindings(settingsPath, bindings, &error))
        std::printf("Game keybinds: %s\n", bindings.Describe().c_str());
    else
        std::printf("Game keybinds: not readable (%s), assuming WASD\n", error.c_str());

    if (summaryOnly || seconds <= 0)
    {
        sdk.Stop();
        return 0;
    }

    std::printf("\nPress your movement keys gently and fully. Showing live values for %d seconds...\n\n", seconds);
    WA::Tuning tuning;
    float peak = 0.f;
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    while (std::chrono::steady_clock::now() < end)
    {
        WA::KeyValues keys;
        keys.forward = sdk.Read(bindings.forward.vk);
        keys.back = sdk.Read(bindings.back.vk);
        keys.left = sdk.Read(bindings.left.vk);
        keys.right = sdk.Read(bindings.right.vk);
        keys.accelerate = sdk.Read(bindings.accelerate.vk);
        keys.brake = sdk.Read(bindings.brake.vk);
        keys.steerLeft = sdk.Read(bindings.steerLeft.vk);
        keys.steerRight = sdk.Read(bindings.steerRight.vk);
        for (float value : {keys.forward, keys.back, keys.left, keys.right})
            peak = value > peak ? value : peak;

        const auto walk = WA::Compute(keys, WA::Context::OnFoot, tuning, false);
        const auto drive = WA::Compute(keys, WA::Context::Driving, tuning, true);
        std::printf("\rfwd %s back %s | walk x %s y %s | throttle %s brake %s steer %s   ",
                    Bar(keys.forward, 8).c_str(), Bar(keys.back, 8).c_str(), SignedBar(walk.lx, 5).c_str(),
                    SignedBar(walk.ly, 5).c_str(), Bar(drive.rt, 8).c_str(), Bar(drive.lt, 8).c_str(),
                    SignedBar(drive.lx, 5).c_str());
        std::fflush(stdout);
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }

    std::printf("\n\nPeak movement key travel: %.2f (poll rate %d Hz)\n", peak, sdk.PollRate());
    if (peak <= 0.f)
        std::printf("No analog travel seen on the movement keys. If you pressed them, check that Wootility is not\n"
                    "remapping those keys, then run this again.\n");
    sdk.Stop();
    return 0;
}
