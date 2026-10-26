#pragma once

#include "Analog.hpp"

#include <functional>

namespace WA::EventInjector
{
using Provider = bool (*)(PadOutput& aOut);
using AttachFn = std::function<bool(void* aTarget, void* aDetour, void** aOriginal)>;
using RealPadQuery = bool (*)();

struct Options
{
    bool inject = false;
    bool injectAfterUi = true;
    bool keyboardDevice = true;
    bool logRaw = false;
    bool debug = false;
};

bool Install(Provider aProvider, RealPadQuery aRealPad, const AttachFn& aAttach);
bool IsInstalled();
void SetOptions(const Options& aOptions);
unsigned long long InjectedCount();
unsigned long long MaskedCount();
}
