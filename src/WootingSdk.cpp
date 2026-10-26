#include "WootingSdk.hpp"

#include "Log.hpp"

#include <chrono>
#include <cstdlib>

#include <Windows.h>

namespace WA
{
namespace
{
constexpr int kResultNoDevices = -1999;
constexpr int kResultUnInitialized = -2000;

constexpr unsigned kKeycodeVirtualKey = 2;
constexpr unsigned kKeycodeVirtualKeyTranslate = 3;

struct DeviceInfoFfi
{
    uint16_t vendor_id;
    uint16_t product_id;
    char* manufacturer_name;
    char* device_name;
    uint64_t device_id;
    int device_type;
};

std::filesystem::path ProgramFiles()
{
    wchar_t buffer[MAX_PATH] = {};
    const DWORD length = GetEnvironmentVariableW(L"ProgramW6432", buffer, MAX_PATH);
    if (length > 0 && length < MAX_PATH)
        return buffer;
    if (const wchar_t* env = _wgetenv(L"ProgramFiles"))
        return env;
    return L"C:\\Program Files";
}

template<typename T>
bool Resolve(HMODULE aModule, const char* aName, T& aOut)
{
    aOut = reinterpret_cast<T>(GetProcAddress(aModule, aName));
    return aOut != nullptr;
}

class Ticker
{
public:
    Ticker()
    {
        m_timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        if (!m_timer)
            m_timer = CreateWaitableTimerW(nullptr, TRUE, nullptr);
    }
    ~Ticker()
    {
        if (m_timer)
            CloseHandle(m_timer);
    }

    void Sleep(int aMicroseconds)
    {
        if (!m_timer)
        {
            ::Sleep(static_cast<DWORD>((aMicroseconds + 999) / 1000));
            return;
        }
        LARGE_INTEGER due;
        due.QuadPart = -static_cast<LONGLONG>(aMicroseconds) * 10;
        if (SetWaitableTimer(m_timer, &due, 0, nullptr, nullptr, FALSE))
            WaitForSingleObject(m_timer, 100);
        else
            ::Sleep(1);
    }

private:
    HANDLE m_timer = nullptr;
};
}

std::atomic<WootingSdk*> WootingSdk::s_instance{nullptr};

const char* WootingDevice::GamepadMode() const
{
    if (vendorId != 0x31E3)
        return "unknown";
    switch (productId & 0x000F)
    {
    case 0:
        return "xbox";
    case 1:
        return "classic";
    case 2:
        return "none";
    default:
        return "unknown";
    }
}

WootingSdk::~WootingSdk()
{
    Stop();
}

void WootingSdk::SetError(std::string aError)
{
    std::scoped_lock lock(m_mutex);
    m_error = std::move(aError);
}

std::string WootingSdk::LastError() const
{
    std::scoped_lock lock(m_mutex);
    return m_error;
}

bool WootingSdk::Load(const std::vector<std::filesystem::path>& aExtraDirs)
{
    std::vector<std::filesystem::path> candidates;
    for (const auto& dir : aExtraDirs)
    {
        candidates.push_back(dir / L"wooting_analog_sdk_dist.dll");
        candidates.push_back(dir / L"wooting_analog_wrapper.dll");
    }
    candidates.push_back(ProgramFiles() / L"wooting-analog-sdk" / L"wooting_analog_sdk.dll");
    candidates.push_back(ProgramFiles() / L"WootingAnalogSDK" / L"wooting_analog_sdk.dll");

    std::string tried;
    for (const auto& candidate : candidates)
    {
        std::error_code ec;
        if (!std::filesystem::exists(candidate, ec))
            continue;
        HMODULE module = LoadLibraryExW(candidate.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!module)
        {
            tried += candidate.string() + " (error " + std::to_string(GetLastError()) + ") ";
            continue;
        }
        m_module = module;
        m_path = candidate;
        break;
    }

    if (!m_module)
    {
        if (HMODULE module = LoadLibraryW(L"wooting_analog_sdk.dll"))
        {
            m_module = module;
            wchar_t path[MAX_PATH] = {};
            GetModuleFileNameW(module, path, MAX_PATH);
            m_path = path;
        }
    }

    if (!m_module)
    {
        SetError("Wooting Analog SDK not found. Install Wootility (it installs the SDK) or the SDK from "
                 "github.com/WootingKb/wooting-analog-sdk/releases. " +
                 tried);
        return false;
    }

    auto module = static_cast<HMODULE>(m_module);
    const bool ok = Resolve(module, "wooting_analog_initialise", m_api.initialise) &&
                    Resolve(module, "wooting_analog_set_keycode_mode", m_api.setKeycodeMode) &&
                    Resolve(module, "wooting_analog_read_full_buffer", m_api.readFullBuffer) &&
                    Resolve(module, "wooting_analog_get_connected_devices_info", m_api.getDevices);
    Resolve(module, "wooting_analog_uninitialise", m_api.uninitialise);
    Resolve(module, "wooting_analog_is_initialised", m_api.isInitialised);
    Resolve(module, "wooting_analog_version_semver", m_api.versionSemver);
    Resolve(module, "wooting_analog_set_device_event_cb", m_api.setDeviceEventCb);
    Resolve(module, "wooting_analog_clear_device_event_cb", m_api.clearDeviceEventCb);

    if (!ok)
    {
        SetError("'" + m_path.string() + "' is missing required Wooting SDK functions");
        Unload();
        return false;
    }

    if (m_api.versionSemver)
    {
        if (const char* version = m_api.versionSemver())
            m_version = version;
    }
    return true;
}

void WootingSdk::Unload()
{
    if (m_module)
        FreeLibrary(static_cast<HMODULE>(m_module));
    m_module = nullptr;
    m_api = {};
}

bool WootingSdk::Start(const std::vector<std::filesystem::path>& aExtraDirs, int aPollHz)
{
    if (m_running)
        return true;
    if (!m_module && !Load(aExtraDirs))
        return false;

    const int result = m_api.initialise();
    if (result < 0)
    {
        SetError("wooting_analog_initialise failed with code " + std::to_string(result) +
                 (result == -1995 ? " (no device plugins found)" : ""));
        return false;
    }
    m_initialised = true;
    WA_LOG_INFO("Wooting Analog SDK %s loaded from '%s', %d device(s) at start", m_version.c_str(),
                m_path.string().c_str(), result);

    const int mode = m_api.setKeycodeMode(kKeycodeVirtualKeyTranslate);
    if (mode < 0)
    {
        WA_LOG_WARN("Layout-aware keycodes unavailable (code %d), falling back to plain virtual keys", mode);
        m_api.setKeycodeMode(kKeycodeVirtualKey);
    }

    s_instance = this;
    if (m_api.setDeviceEventCb)
        m_api.setDeviceEventCb(&WootingSdk::OnDeviceEvent);

    RefreshDevices();

    m_running = true;
    m_threadDone = false;
    m_thread = std::thread([this, aPollHz] {
        PollLoop(aPollHz);
        m_threadDone = true;
    });
    return true;
}

void WootingSdk::Stop(bool aUninitialise)
{
    if (m_running.exchange(false) && m_thread.joinable())
    {
        for (int i = 0; i < 50 && !m_threadDone.load(); ++i)
            Sleep(10);
        if (m_threadDone.load())
            m_thread.join();
        else
            m_thread.detach();
    }

    if (m_initialised && aUninitialise)
    {
        if (m_api.clearDeviceEventCb)
            m_api.clearDeviceEventCb();
        if (m_api.uninitialise)
            m_api.uninitialise();
        m_initialised = false;
    }
    s_instance = nullptr;
    m_connected = false;
    for (auto& value : m_values)
        value.store(0.f, std::memory_order_relaxed);
}

void WootingSdk::OnDeviceEvent(int aType, void*)
{
    if (auto self = s_instance.load())
    {
        self->m_devicesDirty = true;
        WA_LOG_INFO("Wooting device %s", aType == 1 ? "connected" : aType == 2 ? "disconnected" : "event");
    }
}

void WootingSdk::RefreshDevices()
{
    void* buffer[16] = {};
    const int count = m_api.getDevices(buffer, 16);

    std::vector<WootingDevice> devices;
    for (int i = 0; i < count && i < 16; ++i)
    {
        const auto* info = static_cast<const DeviceInfoFfi*>(buffer[i]);
        if (!info)
            continue;
        WootingDevice device;
        device.vendorId = info->vendor_id;
        device.productId = info->product_id;
        device.manufacturer = info->manufacturer_name ? info->manufacturer_name : "";
        device.name = info->device_name ? info->device_name : "";
        device.id = info->device_id;
        device.type = info->device_type;
        devices.push_back(std::move(device));
    }

    std::scoped_lock lock(m_mutex);
    m_devices = std::move(devices);
}

std::vector<WootingDevice> WootingSdk::Devices() const
{
    std::scoped_lock lock(m_mutex);
    return m_devices;
}

void WootingSdk::PollLoop(int aPollHz)
{
    const int periodUs = 1'000'000 / (aPollHz > 0 ? aPollHz : 500);
    Ticker ticker;

    unsigned short codes[128];
    float values[128];
    std::array<float, 256> frame{};
    auto lastDeviceRefresh = std::chrono::steady_clock::now();
    auto rateWindow = lastDeviceRefresh;
    int polls = 0;
    bool wasConnected = false;
    int lastError = 0;

    while (m_running.load(std::memory_order_relaxed))
    {
        const int count = m_api.readFullBuffer(codes, values, 128);
        frame.fill(0.f);
        bool connected = count >= 0;
        if (connected)
        {
            for (int i = 0; i < count; ++i)
            {
                if (codes[i] < frame.size())
                    frame[codes[i]] = values[i] < 0.f ? 0.f : (values[i] > 1.f ? 1.f : values[i]);
            }
        }
        else if (count != lastError)
        {
            lastError = count;
            if (count != kResultNoDevices)
                WA_LOG_WARN("Wooting read failed with code %d", count);
        }

        for (size_t key = 0; key < frame.size(); ++key)
        {
            if (m_values[key].load(std::memory_order_relaxed) != frame[key])
                m_values[key].store(frame[key], std::memory_order_relaxed);
        }
        m_connected.store(connected, std::memory_order_relaxed);

        if (connected != wasConnected)
        {
            wasConnected = connected;
            m_devicesDirty = true;
            if (connected)
                lastError = 0;
        }

        const auto now = std::chrono::steady_clock::now();
        if (m_devicesDirty.exchange(false) || now - lastDeviceRefresh > std::chrono::seconds(5))
        {
            lastDeviceRefresh = now;
            RefreshDevices();
        }

        if (count == kResultUnInitialized && m_api.initialise() >= 0)
        {
            m_api.setKeycodeMode(kKeycodeVirtualKeyTranslate);
            WA_LOG_INFO("Wooting Analog SDK re-initialised");
        }

        ++polls;
        if (now - rateWindow >= std::chrono::seconds(1))
        {
            m_pollRate.store(polls, std::memory_order_relaxed);
            polls = 0;
            rateWindow = now;
        }

        ticker.Sleep(periodUs);
    }
}
}
