#pragma once

#include "Analog.hpp"

#include <functional>

namespace WA::XInputHook
{
using Provider = bool (*)(PadOutput& aOut);

using AttachFn = std::function<bool(void* aTarget, void* aDetour, void** aOriginal)>;

bool Install(Provider aProvider, int aPadIndex, const AttachFn& aAttach);
void Uninstall();
bool IsInstalled();
const char* Method();
bool RealPadPresent();
unsigned long long CallCount();
}
