#include "XInputHook.hpp"

#include "Log.hpp"

#include <atomic>
#include <cmath>
#include <cstring>

#include <Windows.h>
#include <Xinput.h>

namespace WA::XInputHook
{
namespace
{
using GetStateFn = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);

std::atomic<Provider> s_provider{nullptr};
GetStateFn s_original = nullptr;
int s_padIndex = 0;
void** s_patchedSlot = nullptr;
const char* s_method = "none";
std::atomic<unsigned long long> s_calls{0};
std::atomic<bool> s_realPad{false};

std::atomic<bool> s_announced{false};
std::atomic<DWORD> s_packet{1};
XINPUT_GAMEPAD s_lastPad{};

SHORT ToThumb(float aValue)
{
    const float clamped = aValue < -1.f ? -1.f : (aValue > 1.f ? 1.f : aValue);
    return static_cast<SHORT>(std::lround(clamped * (clamped < 0.f ? 32768.f : 32767.f)));
}

BYTE ToTrigger(float aValue)
{
    const float clamped = aValue < 0.f ? 0.f : (aValue > 1.f ? 1.f : aValue);
    return static_cast<BYTE>(std::lround(clamped * 255.f));
}

SHORT MergeAxis(SHORT aReal, SHORT aVirtual)
{
    return std::abs(static_cast<int>(aVirtual)) > std::abs(static_cast<int>(aReal)) ? aVirtual : aReal;
}

DWORD WINAPI GetStateDetour(DWORD aUserIndex, XINPUT_STATE* aState)
{
    s_calls.fetch_add(1, std::memory_order_relaxed);
    const DWORD result = s_original ? s_original(aUserIndex, aState) : ERROR_DEVICE_NOT_CONNECTED;
    if (static_cast<int>(aUserIndex) != s_padIndex || !aState)
        return result;
    s_realPad.store(result == ERROR_SUCCESS, std::memory_order_relaxed);

    PadOutput pad;
    const auto provider = s_provider.load(std::memory_order_relaxed);
    const bool ready = provider && provider(pad);

    if (!ready && !s_announced.load(std::memory_order_relaxed))
        return result;

    XINPUT_GAMEPAD virtualPad{};
    if (ready)
    {
        virtualPad.sThumbLX = ToThumb(pad.lx);
        virtualPad.sThumbLY = ToThumb(pad.ly);
        virtualPad.bLeftTrigger = ToTrigger(pad.lt);
        virtualPad.bRightTrigger = ToTrigger(pad.rt);
        if (!s_announced.exchange(true))
            WA_LOG_INFO("Virtual pad is now reporting to the game on XInput slot %d", s_padIndex);
    }

    XINPUT_GAMEPAD merged = virtualPad;
    DWORD basePacket = 0;
    if (result == ERROR_SUCCESS)
    {
        merged = aState->Gamepad;
        merged.sThumbLX = MergeAxis(merged.sThumbLX, virtualPad.sThumbLX);
        merged.sThumbLY = MergeAxis(merged.sThumbLY, virtualPad.sThumbLY);
        merged.bLeftTrigger = merged.bLeftTrigger > virtualPad.bLeftTrigger ? merged.bLeftTrigger : virtualPad.bLeftTrigger;
        merged.bRightTrigger =
            merged.bRightTrigger > virtualPad.bRightTrigger ? merged.bRightTrigger : virtualPad.bRightTrigger;
        basePacket = aState->dwPacketNumber;
    }

    if (std::memcmp(&merged, &s_lastPad, sizeof(merged)) != 0)
    {
        s_lastPad = merged;
        s_packet.fetch_add(1, std::memory_order_relaxed);
    }
    aState->dwPacketNumber = basePacket + s_packet.load(std::memory_order_relaxed);
    aState->Gamepad = merged;
    return ERROR_SUCCESS;
}

void** FindImportSlot(GetStateFn& aCurrent)
{
    auto base = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        return nullptr;
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    const auto& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!directory.VirtualAddress)
        return nullptr;

    for (auto desc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + directory.VirtualAddress); desc->Name; ++desc)
    {
        const char* dll = reinterpret_cast<const char*>(base + desc->Name);
        if (_strnicmp(dll, "xinput", 6) != 0)
            continue;

        HMODULE module = GetModuleHandleA(dll);
        const auto exported = module ? reinterpret_cast<void*>(GetProcAddress(module, "XInputGetState")) : nullptr;

        auto thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(base + desc->FirstThunk);
        auto names = desc->OriginalFirstThunk ? reinterpret_cast<IMAGE_THUNK_DATA*>(base + desc->OriginalFirstThunk)
                                              : nullptr;
        for (; thunk->u1.Function; ++thunk)
        {
            bool match = false;
            if (names && names->u1.AddressOfData)
            {
                if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))
                    match = IMAGE_ORDINAL(names->u1.Ordinal) == 2;
                else
                {
                    auto byName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData);
                    match = std::strcmp(reinterpret_cast<const char*>(byName->Name), "XInputGetState") == 0;
                }
            }
            else
            {
                match = exported && reinterpret_cast<void*>(thunk->u1.Function) == exported;
            }

            if (match)
            {
                aCurrent = reinterpret_cast<GetStateFn>(thunk->u1.Function);
                WA_LOG_INFO("Found XInputGetState import from %s", dll);
                return reinterpret_cast<void**>(&thunk->u1.Function);
            }
            if (names)
                ++names;
        }
    }
    return nullptr;
}

bool WriteSlot(void** aSlot, void* aValue)
{
    DWORD oldProtect = 0;
    if (!VirtualProtect(aSlot, sizeof(void*), PAGE_READWRITE, &oldProtect))
        return false;
    *aSlot = aValue;
    VirtualProtect(aSlot, sizeof(void*), oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), aSlot, sizeof(void*));
    return true;
}
}

bool Install(Provider aProvider, int aPadIndex, const AttachFn& aAttach)
{
    if (IsInstalled())
        return true;

    s_provider = aProvider;
    s_padIndex = aPadIndex;

    for (const wchar_t* dll : {L"xinput9_1_0.dll", L"xinput1_4.dll", L"xinput1_3.dll"})
    {
        HMODULE module = GetModuleHandleW(dll);
        if (!module)
            continue;
        auto target = reinterpret_cast<void*>(GetProcAddress(module, "XInputGetState"));
        if (target && aAttach &&
            aAttach(target, reinterpret_cast<void*>(&GetStateDetour), reinterpret_cast<void**>(&s_original)))
        {
            s_method = "detour";
            return true;
        }
    }

    GetStateFn current = nullptr;
    if (void** slot = FindImportSlot(current))
    {
        s_original = current;
        if (WriteSlot(slot, reinterpret_cast<void*>(&GetStateDetour)))
        {
            s_patchedSlot = slot;
            s_method = "import table";
            return true;
        }
        WA_LOG_WARN("Could not write the XInputGetState import slot (error %lu)", GetLastError());
    }

    s_provider = nullptr;
    WA_LOG_ERROR("Could not hook XInputGetState: the game does not seem to use XInput");
    return false;
}

void Uninstall()
{
    if (s_patchedSlot && s_original)
        WriteSlot(s_patchedSlot, reinterpret_cast<void*>(s_original));
    s_patchedSlot = nullptr;
    s_provider = nullptr;
    s_method = "none";
}

bool IsInstalled()
{
    return s_provider.load() != nullptr;
}

const char* Method()
{
    return s_method;
}

bool RealPadPresent()
{
    return s_realPad.load(std::memory_order_relaxed);
}

unsigned long long CallCount()
{
    return s_calls.load(std::memory_order_relaxed);
}
}
