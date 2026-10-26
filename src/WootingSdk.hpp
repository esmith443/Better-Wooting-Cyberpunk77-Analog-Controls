#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace WA
{
struct WootingDevice
{
    uint16_t vendorId = 0;
    uint16_t productId = 0;
    std::string manufacturer;
    std::string name;
    uint64_t id = 0;
    int type = 0;

    const char* GamepadMode() const;
};

class WootingSdk
{
public:
    WootingSdk() = default;
    ~WootingSdk();
    WootingSdk(const WootingSdk&) = delete;
    WootingSdk& operator=(const WootingSdk&) = delete;

    bool Start(const std::vector<std::filesystem::path>& aExtraDirs, int aPollHz);
    void Stop(bool aUninitialise = true);

    bool IsLoaded() const { return m_module != nullptr; }
    bool IsInitialised() const { return m_initialised; }
    bool HasDevice() const { return m_connected.load(std::memory_order_relaxed); }

    float Read(uint16_t aVirtualKey) const
    {
        return aVirtualKey < m_values.size() ? m_values[aVirtualKey].load(std::memory_order_relaxed) : 0.f;
    }

    std::vector<WootingDevice> Devices() const;
    std::string Version() const { return m_version; }
    std::filesystem::path LoadedFrom() const { return m_path; }
    std::string LastError() const;
    int PollRate() const { return m_pollRate.load(std::memory_order_relaxed); }

private:
    bool Load(const std::vector<std::filesystem::path>& aExtraDirs);
    void Unload();
    void RefreshDevices();
    void PollLoop(int aPollHz);
    void SetError(std::string aError);

    struct Api
    {
        int (*initialise)() = nullptr;
        int (*uninitialise)() = nullptr;
        bool (*isInitialised)() = nullptr;
        int (*setKeycodeMode)(unsigned int) = nullptr;
        int (*readFullBuffer)(unsigned short*, float*, unsigned int) = nullptr;
        int (*getDevices)(void**, unsigned int) = nullptr;
        const char* (*versionSemver)() = nullptr;
        int (*setDeviceEventCb)(void (*)(int, void*)) = nullptr;
        int (*clearDeviceEventCb)() = nullptr;
    };

    void* m_module = nullptr;
    Api m_api;
    std::filesystem::path m_path;
    std::string m_version;
    bool m_initialised = false;

    std::array<std::atomic<float>, 256> m_values{};
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_devicesDirty{true};
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_threadDone{false};
    std::atomic<int> m_pollRate{0};
    std::thread m_thread;

    mutable std::mutex m_mutex;
    std::vector<WootingDevice> m_devices;
    std::string m_error;

    static void OnDeviceEvent(int aType, void* aInfo);
    static std::atomic<WootingSdk*> s_instance;
};
}
