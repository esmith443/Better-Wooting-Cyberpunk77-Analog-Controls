#include "EventInjector.hpp"

#include "Log.hpp"

#include <RED4ext/Containers/DynArray.hpp>
#include <RED4ext/Relocation.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <unordered_map>

namespace WA::EventInjector
{
namespace
{
constexpr uint32_t kProcessInputEventsHash = 673454961;

constexpr int32_t kPadFirst = 136;
constexpr int32_t kPadLeftTrigger = 150;
constexpr int32_t kPadRightTrigger = 151;
constexpr int32_t kPadLeftAxisX = 152;
constexpr int32_t kPadLeftAxisY = 153;
constexpr int32_t kPadLast = 155;
constexpr int32_t kMouseX = 228;
constexpr int32_t kMouseY = 229;
constexpr int32_t kActionPress = 1;
constexpr int32_t kActionRelease = 2;
constexpr int32_t kActionAxis = 3;

struct RawInputData
{
    int32_t key;
    int32_t action;
    float value;
    uint32_t mouseX;
    uint32_t mouseY;
    uint32_t unk14;
    uint32_t unk18;
    uint64_t unk20;
    uint64_t unk28;
    uint32_t device;
    uint32_t unk34;
    uint64_t unk38;
    uint64_t unk40;
};
static_assert(sizeof(RawInputData) == 0x48);
static_assert(offsetof(RawInputData, device) == 0x30);

struct RawInputBuffer
{
    void* vtable;
    RED4ext::DynArray<RawInputData> inputs;
};

using ProcessInputEventsFn = void (*)(void* aInkSystem, uint64_t aFrame, RawInputBuffer& aBuffer);
using Clock = std::chrono::steady_clock;

ProcessInputEventsFn s_original = nullptr;
std::atomic<Provider> s_provider{nullptr};
RealPadQuery s_realPad = nullptr;

std::atomic<bool> s_inject{false};
std::atomic<bool> s_releasePending{false};
std::atomic<bool> s_injectAfterUi{true};
std::atomic<bool> s_keyboardDevice{true};
std::atomic<bool> s_logRaw{false};
std::atomic<bool> s_debug{false};

std::atomic<unsigned long long> s_injected{0};
std::atomic<unsigned long long> s_masked{0};

RawInputData s_padTemplate{};
bool s_havePadTemplate = false;
uint32_t s_keyboardDeviceId = 0;
bool s_haveKeyboard = false;
std::unordered_map<uint64_t, int> s_loggedCombos;
int s_loggedLines = 0;
std::array<Clock::time_point, 4> s_lastSample{};

struct LastSent
{
    float lx = 0.f, ly = 0.f, lt = 0.f, rt = 0.f;
    int framesSinceRefresh = 0;
} s_last;

bool IsVirtualPadKey(int32_t aKey)
{
    return aKey >= kPadLeftTrigger && aKey <= kPadLeftAxisY;
}

bool IsKeyboardKey(const RawInputData& aInput)
{
    const int32_t key = aInput.key;
    if (key <= 3 || (key >= kPadFirst && key <= kPadLast) || key >= 193)
        return false;
    return aInput.action == kActionPress || aInput.action == kActionRelease;
}

bool IsMouse(const RawInputData& aInput)
{
    return (aInput.key >= 1 && aInput.key <= 3) || aInput.key == kMouseX || aInput.key == kMouseY;
}

void LogEvent(const char* aWhere, const RawInputData& aInput)
{
    if (s_loggedLines >= 400)
        return;
    const uint64_t combo = (static_cast<uint64_t>(aInput.key) << 40) | (static_cast<uint64_t>(aInput.action) << 32) |
                           aInput.device;
    if (++s_loggedCombos[combo] > 3)
        return;
    ++s_loggedLines;

    const auto* words = reinterpret_cast<const uint32_t*>(&aInput);
    char hex[200] = {};
    size_t used = 0;
    for (size_t i = offsetof(RawInputData, mouseX) / 4; i < sizeof(RawInputData) / 4 && used + 10 < sizeof(hex); ++i)
        used += std::snprintf(hex + used, sizeof(hex) - used, "%08X ", words[i]);
    WA_LOG_INFO("raw %s key=%d act=%d val=%.4f device=%08X | %s", aWhere, aInput.key, aInput.action, aInput.value,
                aInput.device, hex);
}

void Push(RawInputBuffer& aBuffer, int32_t aKey, float aValue)
{
    RawInputData data = s_havePadTemplate ? s_padTemplate : RawInputData{};
    data.key = aKey;
    data.action = kActionAxis;
    data.value = aValue;
    if (!s_havePadTemplate)
        data.device = 0;
    if (s_keyboardDevice.load(std::memory_order_relaxed) && s_haveKeyboard)
        data.device = s_keyboardDeviceId;
    if (s_logRaw.load(std::memory_order_relaxed))
        LogEvent("injected", data);
    aBuffer.inputs.EmplaceBack(data);
    s_injected.fetch_add(1, std::memory_order_relaxed);
}

void Inject(RawInputBuffer& aBuffer)
{
    auto provider = s_provider.load(std::memory_order_relaxed);
    if (!provider)
        return;
    PadOutput pad;
    if (!provider(pad))
        pad = {};

    const bool refresh = !pad.IsZero() && ++s_last.framesSinceRefresh >= 30;
    if (refresh)
        s_last.framesSinceRefresh = 0;

    if (refresh || pad.lx != s_last.lx)
        Push(aBuffer, kPadLeftAxisX, pad.lx);
    if (refresh || pad.ly != s_last.ly)
        Push(aBuffer, kPadLeftAxisY, pad.ly);
    if (refresh || pad.lt != s_last.lt)
        Push(aBuffer, kPadLeftTrigger, pad.lt);
    if (refresh || pad.rt != s_last.rt)
        Push(aBuffer, kPadRightTrigger, pad.rt);

    s_last.lx = pad.lx;
    s_last.ly = pad.ly;
    s_last.lt = pad.lt;
    s_last.rt = pad.rt;
}

void SamplePadEvent(const RawInputData& aInput, uint32_t aOriginalDevice)
{
    const size_t index = static_cast<size_t>(aInput.key - kPadLeftTrigger);
    const auto now = Clock::now();
    if (aInput.value == 0.f || now - s_lastSample[index] < std::chrono::seconds(1))
        return;
    s_lastSample[index] = now;
    static constexpr const char* kNames[] = {"left trigger", "right trigger", "stick x", "stick y"};
    WA_LOG_DEBUG("pad event %s = %.3f, device %08X -> %08X", kNames[index], aInput.value, aOriginalDevice,
                 aInput.device);
}

void Detour(void* aInkSystem, uint64_t aFrame, RawInputBuffer& aBuffer)
{
    if (s_provider.load(std::memory_order_relaxed))
    {
        const bool logRaw = s_logRaw.load(std::memory_order_relaxed);
        const bool debug = s_debug.load(std::memory_order_relaxed);
        const bool keyboardDevice = s_keyboardDevice.load(std::memory_order_relaxed);
        const bool realPad = s_realPad && s_realPad();

        for (auto& input : aBuffer.inputs)
        {
            if (logRaw)
                LogEvent("game", input);

            if (IsKeyboardKey(input))
            {
                if (!s_haveKeyboard || s_keyboardDeviceId != input.device)
                    WA_LOG_DEBUG("Keyboard device id %08X", input.device);
                s_keyboardDeviceId = input.device;
                s_haveKeyboard = true;
            }
            else if (!s_haveKeyboard && IsMouse(input))
            {
                s_keyboardDeviceId = input.device;
            }

            if (input.action == kActionAxis && IsVirtualPadKey(input.key))
            {
                if (!s_havePadTemplate)
                {
                    s_padTemplate = input;
                    s_havePadTemplate = true;
                }
                const uint32_t original = input.device;
                if (keyboardDevice && !realPad && (s_haveKeyboard || s_keyboardDeviceId))
                {
                    input.device = s_keyboardDeviceId;
                    s_masked.fetch_add(1, std::memory_order_relaxed);
                }
                if (debug)
                    SamplePadEvent(input, original);
            }
        }

        if (s_releasePending.exchange(false))
        {
            for (int32_t key : {kPadLeftAxisX, kPadLeftAxisY, kPadLeftTrigger, kPadRightTrigger})
                Push(aBuffer, key, 0.f);
            s_last = {};
        }

        const bool inject = s_inject.load(std::memory_order_relaxed);
        const bool after = s_injectAfterUi.load(std::memory_order_relaxed);
        if (inject && !after)
            Inject(aBuffer);
        s_original(aInkSystem, aFrame, aBuffer);
        if (inject && after)
            Inject(aBuffer);
        return;
    }

    s_original(aInkSystem, aFrame, aBuffer);
}
}

bool Install(Provider aProvider, RealPadQuery aRealPad, const AttachFn& aAttach)
{
    if (IsInstalled())
        return true;

    const auto target = reinterpret_cast<void*>(RED4ext::UniversalRelocBase::Resolve(kProcessInputEventsHash));
    if (!target)
    {
        WA_LOG_ERROR("inkSystem::ProcessInputEvents not found in the address library");
        return false;
    }
    if (!aAttach || !aAttach(target, reinterpret_cast<void*>(&Detour), reinterpret_cast<void**>(&s_original)))
    {
        WA_LOG_ERROR("Could not hook inkSystem::ProcessInputEvents");
        return false;
    }
    s_realPad = aRealPad;
    s_provider = aProvider;
    return true;
}

bool IsInstalled()
{
    return s_provider.load() != nullptr;
}

void SetOptions(const Options& aOptions)
{
    const bool wasInjecting = s_inject.exchange(aOptions.inject);
    if (wasInjecting && !aOptions.inject)
        s_releasePending = true;
    s_injectAfterUi = aOptions.injectAfterUi;
    s_keyboardDevice = aOptions.keyboardDevice;
    s_logRaw = aOptions.logRaw;
    s_debug = aOptions.debug;
}

unsigned long long InjectedCount()
{
    return s_injected.load(std::memory_order_relaxed);
}

unsigned long long MaskedCount()
{
    return s_masked.load(std::memory_order_relaxed);
}
}
