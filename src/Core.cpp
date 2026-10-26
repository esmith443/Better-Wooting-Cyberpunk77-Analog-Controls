#include "Core.hpp"

#include "Analog.hpp"
#include "Config.hpp"
#include "EventInjector.hpp"
#include "GameBindings.hpp"
#include "InputConfig.hpp"
#include "Log.hpp"
#include "Version.hpp"
#include "WootingSdk.hpp"
#include "XInputHook.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <ctime>
#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>

#include <Windows.h>

namespace WA::Core
{
namespace
{
using Clock = std::chrono::steady_clock;

enum KeySlot
{
    kForward,
    kBack,
    kLeft,
    kRight,
    kAccelerate,
    kBrake,
    kSteerLeft,
    kSteerRight,
    kKeyCount,
};

constexpr const char* kKeyLabels[kKeyCount] = {"fwd", "back", "left", "right", "acc", "brake", "steerL", "steerR"};

struct Peak
{
    std::atomic<float> value{0.f};

    void Note(float aValue)
    {
        float current = value.load(std::memory_order_relaxed);
        while (aValue > current && !value.compare_exchange_weak(current, aValue, std::memory_order_relaxed))
        {
        }
    }
    float Take() { return value.exchange(0.f, std::memory_order_relaxed); }
    float Get() const { return value.load(std::memory_order_relaxed); }
};

struct Stats
{
    std::array<Peak, kKeyCount> keys;
    Peak stickX, stickY, leftTrigger, rightTrigger;
    std::atomic<int> polls{0};
    std::atomic<int> unfocused{0};
};

struct State
{
    std::filesystem::path pluginDir;
    std::filesystem::path gameRoot;
    std::filesystem::path configPath;
    std::filesystem::path modSettingsPath;
    std::filesystem::path generatedXml;
    std::filesystem::path statusPath;
    std::filesystem::path userSettingsPath;

    Config config;
    std::unique_ptr<StateFile> stateFile;
    WootingSdk sdk;

    std::mutex mutex;
    Bindings bindings;
    std::string deviceName;
    std::string gamepadMode;
    std::string inactiveReason;
    std::string hudReason;
    std::vector<std::string> xmlNotes;
    bool movementOverridden = false;
    bool steeringOverridden = false;
    bool throttleOverridden = false;
    bool throttleNextLaunch = false;
    bool startupAnnounced = false;
    std::deque<std::string> announcements;

    std::filesystem::file_time_type userSettingsTime{};
    std::filesystem::file_time_type configTime{};
    Clock::time_point started{};
    Clock::time_point lastFileCheck{};
    Clock::time_point runningSince{};
    Clock::time_point lastStatusWrite{};
    Clock::time_point lastDebugSummary{};
    int quietSeconds = 0;
    int lastSummaryContext = -1;
    unsigned long long lastXInputCalls = 0;
    unsigned long long lastMasked = 0;
    unsigned long long lastInjected = 0;
    bool snapshotKeyDown = false;
    bool warnedNoScripts = false;
};

State* s_state = nullptr;

std::atomic<bool> s_active{false};
std::atomic<bool> s_userEnabled{true};
std::atomic<int> s_context{static_cast<int>(Context::Unknown)};
std::atomic<bool> s_analogEnabled{true};
std::atomic<bool> s_scriptsSeen{false};
std::atomic<bool> s_digitalFallback{true};
std::atomic<bool> s_requireFocus{true};
std::atomic<bool> s_triggers{false};
std::atomic<int> s_backend{static_cast<int>(Backend::XInput)};
std::atomic<bool> s_keepKeyboardPrompts{true};
std::atomic<bool> s_injectAfterUi{true};
std::atomic<bool> s_logRawInput{false};
std::atomic<bool> s_debug{false};
std::atomic<float> s_meterScale{0.6f};
std::atomic<bool> s_snapshotRequested{false};
std::atomic<bool> s_firstInputLogged{false};
std::atomic<std::shared_ptr<const Tuning>> s_tuning;
std::array<std::atomic<uint16_t>, kKeyCount> s_keys{};
Stats s_second;
Stats s_session;

constexpr int kLiveKeys = kKeyCount;
std::array<std::atomic<float>, kKeyCount + 4> s_live{};

void PublishInputOptions()
{
    EventInjector::Options options;
    options.inject = s_backend.load() == static_cast<int>(Backend::Events);
    options.injectAfterUi = s_injectAfterUi.load();
    options.keyboardDevice = s_keepKeyboardPrompts.load();
    options.logRaw = s_logRawInput.load() || s_debug.load();
    options.debug = s_debug.load();
    EventInjector::SetOptions(options);
}

void SetDebug(bool aOn)
{
    if (s_debug.exchange(aOn) != aOn)
        WA_LOG_INFO("Debug mode %s%s", aOn ? "on" : "off", aOn ? " (Ctrl+Shift+F10 saves a snapshot)" : "");
    Log::SetMinLevel(aOn ? Log::Level::Debug : Log::Level::Info);
    PublishInputOptions();
}

bool GameHasFocus()
{
    const HWND window = GetForegroundWindow();
    if (!window)
        return false;
    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    return processId == GetCurrentProcessId();
}

void ReadKeys(KeyValues& aKeys)
{
    float* fields[kKeyCount] = {&aKeys.forward,    &aKeys.back,  &aKeys.left,      &aKeys.right,
                                &aKeys.accelerate, &aKeys.brake, &aKeys.steerLeft, &aKeys.steerRight};

    const bool enabled = s_userEnabled.load(std::memory_order_relaxed) && s_analogEnabled.load(std::memory_order_relaxed);
    const bool analog = enabled && s_state->sdk.HasDevice();
    const bool digital = !analog && (!enabled || s_digitalFallback.load(std::memory_order_relaxed));
    for (int i = 0; i < kKeyCount; ++i)
    {
        const uint16_t vk = s_keys[i].load(std::memory_order_relaxed);
        if (!vk)
            continue;
        if (analog)
            *fields[i] = s_state->sdk.Read(vk);
        else if (digital)
            *fields[i] = (GetAsyncKeyState(vk) & 0x8000) ? 1.f : 0.f;
    }
}

bool Provide(PadOutput& aOut)
{
    if (!s_active.load(std::memory_order_relaxed) || !s_state)
        return false;
    if (s_requireFocus.load(std::memory_order_relaxed) && !GameHasFocus())
    {
        s_second.unfocused.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    KeyValues keys;
    ReadKeys(keys);

    const auto tuning = s_tuning.load(std::memory_order_acquire);
    if (!tuning)
        return false;
    aOut = Compute(keys, static_cast<Context>(s_context.load(std::memory_order_relaxed)), *tuning,
                   s_triggers.load(std::memory_order_relaxed));

    const float values[kKeyCount] = {keys.forward,    keys.back,  keys.left,      keys.right,
                                     keys.accelerate, keys.brake, keys.steerLeft, keys.steerRight};
    for (int i = 0; i < kKeyCount; ++i)
    {
        s_second.keys[i].Note(values[i]);
        s_session.keys[i].Note(values[i]);
        s_live[i].store(values[i], std::memory_order_relaxed);
    }
    s_live[kLiveKeys + 0].store(aOut.lx, std::memory_order_relaxed);
    s_live[kLiveKeys + 1].store(aOut.ly, std::memory_order_relaxed);
    s_live[kLiveKeys + 2].store(aOut.lt, std::memory_order_relaxed);
    s_live[kLiveKeys + 3].store(aOut.rt, std::memory_order_relaxed);
    for (Stats* stats : {&s_second, &s_session})
    {
        stats->stickX.Note(std::fabs(aOut.lx));
        stats->stickY.Note(std::fabs(aOut.ly));
        stats->leftTrigger.Note(aOut.lt);
        stats->rightTrigger.Note(aOut.rt);
    }
    s_second.polls.fetch_add(1, std::memory_order_relaxed);

    if (!s_firstInputLogged.load(std::memory_order_relaxed))
    {
        for (int i = 0; i < kKeyCount; ++i)
        {
            if (values[i] > 0.f && values[i] < 1.f && !s_firstInputLogged.exchange(true))
                WA_LOG_INFO("First analog input: %s at %.2f, so the keyboard is being read", kKeyLabels[i], values[i]);
        }
    }
    return true;
}

bool ProvideXInput(PadOutput& aOut)
{
    return s_backend.load(std::memory_order_relaxed) == static_cast<int>(Backend::XInput) && Provide(aOut);
}

bool ProvideEvents(PadOutput& aOut)
{
    return s_backend.load(std::memory_order_relaxed) == static_cast<int>(Backend::Events) && Provide(aOut);
}

bool RealPadPresent()
{
    return XInputHook::RealPadPresent();
}

std::filesystem::path ModuleDirectory(void* aModule)
{
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(static_cast<HMODULE>(aModule), path, MAX_PATH);
    return std::filesystem::path(path).parent_path();
}

std::filesystem::file_time_type WriteTime(const std::filesystem::path& aPath)
{
    std::error_code ec;
    const auto time = std::filesystem::last_write_time(aPath, ec);
    return ec ? std::filesystem::file_time_type{} : time;
}

void SelectBackend(Backend aWanted)
{
    Backend backend = aWanted;
    if (backend == Backend::Events && !EventInjector::IsInstalled() && XInputHook::IsInstalled())
        backend = Backend::XInput;
    else if (backend == Backend::XInput && !XInputHook::IsInstalled() && EventInjector::IsInstalled())
        backend = Backend::Events;
    if (backend != aWanted)
        WA_LOG_WARN("The %s backend is not available, using %s", ToString(aWanted), ToString(backend));

    const int previous = s_backend.exchange(static_cast<int>(backend));
    if (previous != static_cast<int>(backend))
        WA_LOG_INFO("Backend: %s", ToString(backend));
    PublishInputOptions();
}

void PublishConfig(const Config& aConfig)
{
    s_digitalFallback = aConfig.digitalFallback;
    s_requireFocus = aConfig.requireFocus;
    s_keepKeyboardPrompts = aConfig.keepKeyboardPrompts;
    s_injectAfterUi = aConfig.injectAfterUi;
    s_logRawInput = aConfig.logRawInput;
    s_meterScale = std::clamp(aConfig.meterScale, 0.2f, 2.f);
    auto tuning = std::make_shared<Tuning>(aConfig.tuning);
    tuning->Sanitize();
    s_tuning.store(std::move(tuning), std::memory_order_release);
    SetDebug(aConfig.debugMode);
    SelectBackend(aConfig.backend);
}

std::string DescribeTuning(const Tuning& aTuning)
{
    char text[256] = {};
    auto curve = [](const Curve& aCurve) {
        char part[64] = {};
        std::snprintf(part, sizeof(part), "%.2f/%.2f/%.2f", aCurve.deadzone, aCurve.full, aCurve.gamma);
        return std::string(part);
    };
    std::snprintf(text, sizeof(text), "move %s steer %s throttle %s brake %s (deadzone/full/curve), stick comp %s",
                  curve(aTuning.move).c_str(), curve(aTuning.steer).c_str(), curve(aTuning.throttle).c_str(),
                  curve(aTuning.brake).c_str(), aTuning.compensateStickDeadzone ? "on" : "off");
    return text;
}

void LoadSettings(State& aState, Config& aConfig)
{
    std::string error;
    if (!aConfig.Load(aState.configPath, &error))
        WA_LOG_WARN("Using default settings (%s)", error.c_str());
    const auto applied = aConfig.ApplyModSettings(aState.modSettingsPath);
    if (!applied.empty())
    {
        std::string keys;
        for (const auto& key : applied)
            keys += (keys.empty() ? "" : ", ") + key;
        WA_LOG_INFO("Mod Settings values used: %s", keys.c_str());
    }
}

void ApplyBindings(State& aState)
{
    Bindings bindings = Bindings::Defaults();
    std::string error;
    if (!LoadGameBindings(aState.userSettingsPath, bindings, &error))
        WA_LOG_WARN("Could not read keybindings from '%s' (%s), using WASD", aState.userSettingsPath.string().c_str(),
                    error.c_str());
    aState.userSettingsTime = WriteTime(aState.userSettingsPath);

    const std::pair<const std::string*, BoundKey*> overrides[] = {
        {&aState.config.keyForward, &bindings.forward},       {&aState.config.keyBack, &bindings.back},
        {&aState.config.keyLeft, &bindings.left},             {&aState.config.keyRight, &bindings.right},
        {&aState.config.keyAccelerate, &bindings.accelerate}, {&aState.config.keyBrake, &bindings.brake},
        {&aState.config.keySteerLeft, &bindings.steerLeft},   {&aState.config.keySteerRight, &bindings.steerRight},
    };
    for (const auto& [spec, key] : overrides)
    {
        if (spec->empty())
            continue;
        if (const uint16_t vk = ParseKeySpec(*spec))
        {
            key->vk = vk;
            key->name = std::string(NameFromKey(vk));
            key->fromGame = false;
        }
        else
        {
            WA_LOG_WARN("Unknown key '%s' in WootingAnalog.ini [Keys], ignored", spec->c_str());
        }
    }

    const BoundKey* ordered[kKeyCount] = {&bindings.forward,    &bindings.back,  &bindings.left,
                                          &bindings.right,      &bindings.accelerate, &bindings.brake,
                                          &bindings.steerLeft,  &bindings.steerRight};
    for (int i = 0; i < kKeyCount; ++i)
        s_keys[i] = ordered[i]->vk;

    aState.bindings = bindings;
    WA_LOG_INFO("Keys: %s", bindings.Describe().c_str());
}

std::string StatusLocked(const State& aState)
{
    std::ostringstream out;
    if (!s_active)
    {
        out << "inactive: " << (aState.inactiveReason.empty() ? "starting" : aState.inactiveReason);
        return out.str();
    }
    out << aState.deviceName << " | ";
    if (!s_userEnabled)
        out << "switched off in Mod Settings (keys act normally)";
    else if (!aState.sdk.HasDevice())
        out << (s_digitalFallback ? "keyboard disconnected, digital fallback" : "keyboard disconnected");
    else
        out << (s_analogEnabled ? "analog" : "analog off");
    out << " | " << ToString(static_cast<Backend>(s_backend.load()))
        << (s_keepKeyboardPrompts ? " + keyboard prompts" : "") << " | "
        << ToString(static_cast<Context>(s_context.load()));
    if (aState.throttleOverridden)
        out << " | analog throttle";
    else if (aState.throttleNextLaunch && s_scriptsSeen)
        out << " | analog throttle from next launch";
    else
        out << " | digital throttle";
    if (s_debug)
        out << " | debug";
    return out.str();
}

std::string PeaksLine(Stats& aStats, bool aTake)
{
    auto get = [aTake](Peak& aPeak) { return aTake ? aPeak.Take() : aPeak.Get(); };
    std::ostringstream out;
    out.setf(std::ios::fixed);
    out.precision(2);
    out << "travel";
    for (int i = 0; i < kKeyCount; ++i)
        out << " " << kKeyLabels[i] << " " << get(aStats.keys[i]);
    out << " | sent |x| " << get(aStats.stickX) << " |y| " << get(aStats.stickY) << " lt " << get(aStats.leftTrigger)
        << " rt " << get(aStats.rightTrigger);
    return out.str();
}

void WriteStatusFile(State& aState)
{
    std::ofstream out(aState.statusPath, std::ios::trunc);
    if (!out)
        return;
    const std::time_t now = std::time(nullptr);
    char stamp[64] = {};
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

    out << WA_MOD_NAME << " " << WA_VERSION_STRING << " status, written " << stamp << "\n";
    out << "active: " << (s_active ? "yes" : "no") << "\n";
    out << "status: " << StatusLocked(aState) << "\n";
    out << "sdk: " << (aState.sdk.IsLoaded() ? aState.sdk.Version() + " from " + aState.sdk.LoadedFrom().string()
                                             : std::string("not loaded"))
        << "\n";
    for (const auto& device : aState.sdk.Devices())
    {
        char ids[32] = {};
        std::snprintf(ids, sizeof(ids), "%04X:%04X", device.vendorId, device.productId);
        out << "device: " << device.name << " (" << ids << ", Wootility gamepad mode: " << device.GamepadMode()
            << ")\n";
    }
    out << "keys: " << aState.bindings.Describe() << "\n";
    if (const auto tuning = s_tuning.load())
        out << "tuning: " << DescribeTuning(*tuning) << "\n";
    out << "backend: " << ToString(static_cast<Backend>(s_backend.load()))
        << " (xinput hook: " << (XInputHook::IsInstalled() ? XInputHook::Method() : "none")
        << ", event hook: " << (EventInjector::IsInstalled() ? "yes" : "no") << ")\n";
    out << "keep keyboard prompts: " << (s_keepKeyboardPrompts ? "on" : "off") << " (pad events relabelled: "
        << EventInjector::MaskedCount() << ", events injected: " << EventInjector::InjectedCount() << ")\n";
    out << "overrides: movement=" << aState.movementOverridden << " steering=" << aState.steeringOverridden
        << " throttle=" << aState.throttleOverridden << "\n";
    out << "script layer seen: " << (s_scriptsSeen ? "yes" : "not yet") << "\n";
    out << "session peaks: " << PeaksLine(s_session, false) << "\n";
    for (const auto& note : aState.xmlNotes)
        out << "input config: " << note << "\n";
}

void Announce(State& aState, std::string aText)
{
    WA_LOG_INFO("HUD message queued: %s", aText.c_str());
    aState.announcements.push_back(std::move(aText));
}

void Deactivate(State& aState, std::string aReason, std::string aHud = "error, see log")
{
    s_active = false;
    aState.inactiveReason = std::move(aReason);
    aState.hudReason = std::move(aHud);
    WA_LOG_WARN("Inactive: %s. Movement and driving use the game's normal keyboard controls.",
                aState.inactiveReason.c_str());
}

void WriteSnapshot(State& aState, const char* aReason)
{
    KeyValues keys;
    ReadKeys(keys);
    PadOutput pad;
    const bool provided = Provide(pad);
    const float values[kKeyCount] = {keys.forward,    keys.back,  keys.left,      keys.right,
                                     keys.accelerate, keys.brake, keys.steerLeft, keys.steerRight};

    WA_LOG_INFO("===== snapshot (%s) =====", aReason);
    WA_LOG_INFO("status: %s", StatusLocked(aState).c_str());
    std::string now;
    for (int i = 0; i < kKeyCount; ++i)
    {
        char part[32] = {};
        std::snprintf(part, sizeof(part), "%s %.3f ", kKeyLabels[i], values[i]);
        now += part;
    }
    WA_LOG_INFO("key travel now: %s", now.c_str());
    WA_LOG_INFO("sending now: %s lx %.3f ly %.3f lt %.3f rt %.3f", provided ? "yes" : "no (inactive or unfocused)",
                pad.lx, pad.ly, pad.lt, pad.rt);
    if (const auto tuning = s_tuning.load())
        WA_LOG_INFO("tuning: %s", DescribeTuning(*tuning).c_str());
    WA_LOG_INFO("keys: %s", aState.bindings.Describe().c_str());
    WA_LOG_INFO("hooks: xinput %s (%llu calls, real pad %s), events %s (%llu relabelled, %llu injected)",
                XInputHook::IsInstalled() ? XInputHook::Method() : "none", XInputHook::CallCount(),
                XInputHook::RealPadPresent() ? "yes" : "no", EventInjector::IsInstalled() ? "on" : "off",
                EventInjector::MaskedCount(), EventInjector::InjectedCount());
    WA_LOG_INFO("sdk: %s, device %s, %d polls/s, focus %s", aState.sdk.Version().c_str(),
                aState.sdk.HasDevice() ? "connected" : "missing", aState.sdk.PollRate(), GameHasFocus() ? "yes" : "no");
    WA_LOG_INFO("session peaks: %s", PeaksLine(s_session, false).c_str());
    WA_LOG_INFO("===== end of snapshot =====");
}

void DebugSummary(State& aState, Clock::time_point aNow)
{
    if (aNow - aState.lastDebugSummary < std::chrono::seconds(1))
        return;
    aState.lastDebugSummary = aNow;

    const unsigned long long calls = XInputHook::CallCount();
    const unsigned long long masked = EventInjector::MaskedCount();
    const unsigned long long injected = EventInjector::InjectedCount();
    const int polls = s_second.polls.exchange(0);
    const int unfocused = s_second.unfocused.exchange(0);
    const int context = s_context.load();

    bool moving = false;
    for (auto& key : s_second.keys)
        moving = moving || key.Get() > 0.f;

    const std::string peaks = PeaksLine(s_second, true);
    if (moving || context != aState.lastSummaryContext || ++aState.quietSeconds >= 15)
    {
        aState.quietSeconds = 0;
        aState.lastSummaryContext = context;
        WA_LOG_DEBUG("1s: %s | ctx %s, %s%s, provider %d/s%s | xinput %llu/s relabelled %llu/s injected %llu/s",
                     peaks.c_str(), ToString(static_cast<Context>(context)),
                     ToString(static_cast<Backend>(s_backend.load())), s_keepKeyboardPrompts ? "+kbd prompts" : "",
                     polls, unfocused ? " (unfocused)" : "", calls - aState.lastXInputCalls,
                     masked - aState.lastMasked, injected - aState.lastInjected);
    }
    aState.lastXInputCalls = calls;
    aState.lastMasked = masked;
    aState.lastInjected = injected;
}
}

bool Initialize(const InitInfo& aInfo)
{
    s_state = new State();
    State& state = *s_state;
    std::scoped_lock lock(state.mutex);

    state.started = Clock::now();
    state.pluginDir = ModuleDirectory(aInfo.pluginModule);
    state.gameRoot = ModuleDirectory(nullptr).parent_path().parent_path();
    state.configPath = state.pluginDir / L"WootingAnalog.ini";
    state.modSettingsPath = state.gameRoot / L"red4ext" / L"plugins" / L"mod_settings" / L"user.ini";
    state.generatedXml = state.pluginDir / L"generated" / L"WootingAnalog.inputs.xml";
    state.statusPath = state.pluginDir / L"WootingAnalog.status.txt";
    state.userSettingsPath = DefaultUserSettingsPath();
    state.stateFile = std::make_unique<StateFile>(state.pluginDir / L"WootingAnalog.state.ini");

    WA_LOG_INFO("%s %s starting, game root '%s'", WA_MOD_NAME, WA_VERSION_STRING, state.gameRoot.string().c_str());

    std::error_code ec;
    if (!std::filesystem::exists(state.configPath, ec))
    {
        if (state.config.WriteDefault(state.configPath))
            WA_LOG_INFO("Wrote default settings to '%s'", state.configPath.string().c_str());
    }
    LoadSettings(state, state.config);
    state.configTime = WriteTime(state.configPath);
    s_userEnabled = state.config.enabled;
    PublishConfig(state.config);
    WA_LOG_INFO("Tuning: %s", DescribeTuning(*s_tuning.load()).c_str());

    std::filesystem::remove(state.generatedXml, ec);

    auto finish = [&state] {
        WriteStatusFile(state);
        return true;
    };

    if (!state.config.enabled)
    {
        Deactivate(state, "turned off in the settings", "turned off");
        return finish();
    }

    if (!state.sdk.Start({state.pluginDir}, state.config.pollHz))
    {
        Deactivate(state, state.sdk.LastError(), "Wooting SDK missing");
        return finish();
    }

    const auto devices = state.sdk.Devices();
    if (devices.empty())
    {
        state.sdk.Stop();
        Deactivate(state, "no Wooting keyboard found at startup (plug it in and restart the game)", "keyboard not found");
        return finish();
    }
    for (const auto& device : devices)
    {
        WA_LOG_INFO("Found %s (%04X:%04X), Wootility gamepad mode: %s", device.name.c_str(), device.vendorId,
                    device.productId, device.GamepadMode());
        if (std::string_view(device.GamepadMode()) == "xbox" || std::string_view(device.GamepadMode()) == "classic")
            WA_LOG_WARN("Wootility gamepad mode is on. If any keys are bound to gamepad inputs in Wootility, remove "
                        "those binds (or set gamepad mode to None) so the game does not get them twice.");
    }
    state.deviceName = devices.front().name.empty() ? "Wooting keyboard" : devices.front().name;
    state.gamepadMode = devices.front().GamepadMode();

    ApplyBindings(state);

    const bool scriptsSeenBefore = state.stateFile->Get("ScriptsSeen") == "1";
    const bool wantThrottle = state.config.analogThrottle == Toggle::On ||
                              (state.config.analogThrottle == Toggle::Auto && scriptsSeenBefore);
    state.throttleNextLaunch = state.config.analogThrottle == Toggle::Auto && !scriptsSeenBefore;
    if (state.throttleNextLaunch)
        WA_LOG_INFO("Analog throttle waits until the mod's script has run once (it then turns on at the next launch)");

    const bool xinput = XInputHook::Install(&ProvideXInput, state.config.padIndex, aInfo.attach);
    const bool events = EventInjector::Install(&ProvideEvents, &RealPadPresent, aInfo.attach);
    if (!xinput && !events)
    {
        state.sdk.Stop();
        Deactivate(state, "could not hook the game's input");
        return finish();
    }
    WA_LOG_INFO("Input hooks: XInput %s, engine events %s", xinput ? XInputHook::Method() : "unavailable",
                events ? "installed" : "unavailable");
    if (!events)
        WA_LOG_WARN("Without the engine event hook, button prompts may switch to controller icons");
    SelectBackend(state.config.backend);

    std::filesystem::path contextsPath, mappingsPath;
    const std::string contexts = ReadInputConfig(state.gameRoot, L"inputContexts.xml", &contextsPath);
    const std::string mappings = ReadInputConfig(state.gameRoot, L"inputUserMappings.xml", &mappingsPath);
    if (contexts.empty() || mappings.empty())
    {
        state.sdk.Stop();
        Deactivate(state, "could not read the game's input configs from r6/config");
        return finish();
    }
    WA_LOG_INFO("Input configs: '%s', '%s'", contextsPath.string().c_str(), mappingsPath.string().c_str());

    InputXmlOptions options;
    options.throttle = wantThrottle;
    const std::string header = std::string("Generated by ") + WA_MOD_NAME + " " + WA_VERSION_STRING + " for " +
                               state.deviceName + ". Rewritten at every launch, do not edit.";
    const InputXmlResult xml = BuildInputXml(contexts, mappings, options, header);
    state.xmlNotes = xml.notes;
    for (const auto& note : xml.notes)
        WA_LOG_INFO("Input config: %s", note.c_str());
    if (!xml.ok)
    {
        state.sdk.Stop();
        Deactivate(state, "the game's input configs did not have the expected movement axes");
        return finish();
    }

    std::filesystem::create_directories(state.generatedXml.parent_path(), ec);
    {
        std::ofstream out(state.generatedXml, std::ios::binary | std::ios::trunc);
        out << xml.xml;
        if (!out)
        {
            state.sdk.Stop();
            Deactivate(state, "could not write " + state.generatedXml.string());
            return finish();
        }
    }

    std::string error;
    if (!RegisterWithInputLoader(aInfo.pluginModule, state.gameRoot, state.generatedXml, &error))
    {
        std::filesystem::remove(state.generatedXml, ec);
        state.sdk.Stop();
        Deactivate(state, error, "Input Loader missing");
        return finish();
    }

    state.movementOverridden = xml.movement;
    state.steeringOverridden = xml.steering;
    state.throttleOverridden = xml.throttle;
    s_triggers = xml.throttle;
    s_active = true;
    state.inactiveReason.clear();
    WA_LOG_INFO("Active for %s: movement %s, steering %s, throttle %s", state.deviceName.c_str(),
                xml.movement ? "analog" : "unchanged", xml.steering ? "analog" : "unchanged",
                xml.throttle ? "analog" : "digital");
    return finish();
}

void Shutdown()
{
    const bool wasActive = s_active.exchange(false);
    XInputHook::Uninstall();
    if (s_state)
    {
        if (wasActive)
        {
            const auto minutes = std::chrono::duration_cast<std::chrono::minutes>(Clock::now() - s_state->started);
            WA_LOG_INFO("Session summary: %lld min, %llu pad events relabelled, %llu injected, peaks %s",
                        static_cast<long long>(minutes.count()), EventInjector::MaskedCount(),
                        EventInjector::InjectedCount(), PeaksLine(s_session, false).c_str());
        }
        s_state->sdk.Stop(false);
    }
}

void OnRunningUpdate()
{
    if (!s_state)
        return;
    State& state = *s_state;
    const auto now = Clock::now();

    std::scoped_lock lock(state.mutex);
    if (state.runningSince == Clock::time_point{})
        state.runningSince = now;

    if (s_active && s_debug)
    {
        DebugSummary(state, now);

        const bool down = GameHasFocus() && (GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
                          (GetAsyncKeyState(VK_SHIFT) & 0x8000) && (GetAsyncKeyState(VK_F10) & 0x8000);
        if (down && !state.snapshotKeyDown)
        {
            WriteSnapshot(state, "Ctrl+Shift+F10");
            s_snapshotRequested = true;
            Announce(state, "Snapshot saved");
        }
        state.snapshotKeyDown = down;
    }

    if (now - state.lastFileCheck < std::chrono::seconds(2))
        return;
    state.lastFileCheck = now;

    if (!s_active)
        return;

    if (WriteTime(state.userSettingsPath) != state.userSettingsTime)
    {
        WA_LOG_INFO("Game keybindings changed, reloading");
        ApplyBindings(state);
    }

    if (WriteTime(state.configPath) != state.configTime)
    {
        state.configTime = WriteTime(state.configPath);
        Config fresh;
        LoadSettings(state, fresh);
        fresh.enabled = state.config.enabled;
        fresh.analogThrottle = state.config.analogThrottle;
        fresh.padIndex = state.config.padIndex;
        fresh.pollHz = state.config.pollHz;
        if (fresh.backend != state.config.backend || fresh.keepKeyboardPrompts != state.config.keepKeyboardPrompts)
            WA_LOG_INFO("Switching to %s backend, keyboard prompts %s", ToString(fresh.backend),
                        fresh.keepKeyboardPrompts ? "on" : "off");
        state.config = fresh;
        PublishConfig(state.config);
        ApplyBindings(state);
        WA_LOG_INFO("Reloaded WootingAnalog.ini. Tuning: %s", DescribeTuning(*s_tuning.load()).c_str());
    }

    if (!s_scriptsSeen && !state.warnedNoScripts && now - state.runningSince > std::chrono::seconds(120))
    {
        state.warnedNoScripts = true;
        WA_LOG_WARN("The mod's redscript part has not checked in. Analog movement still works, but driving context "
                    "and in-game settings need redscript: check r6/logs/redscript_rCURRENT.log for compile errors.");
    }

    if (now - state.lastStatusWrite > std::chrono::seconds(30))
    {
        state.lastStatusWrite = now;
        WriteStatusFile(state);
    }
}

void SetContext(int aContext)
{
    if (aContext < 0 || aContext > static_cast<int>(Context::Neutral))
        aContext = static_cast<int>(Context::Unknown);
    const int previous = s_context.exchange(aContext);
    if (previous != aContext)
        WA_LOG_DEBUG("Context: %s -> %s", ToString(static_cast<Context>(previous)),
                     ToString(static_cast<Context>(aContext)));

    if (!s_scriptsSeen.exchange(true) && s_state)
    {
        std::scoped_lock lock(s_state->mutex);
        WA_LOG_INFO("Script layer is running");
        if (s_state->stateFile->Get("ScriptsSeen") != "1")
        {
            s_state->stateFile->Set("ScriptsSeen", "1");
            s_state->stateFile->Save();
            if (s_state->throttleNextLaunch)
                WA_LOG_INFO("Analog throttle and brake will be enabled from the next launch");
        }
        WriteStatusFile(*s_state);
    }
}

void SetOption(std::string_view aName, float aValue)
{
    WA_LOG_DEBUG("Option from Mod Settings: %.*s = %.3f", static_cast<int>(aName.size()), aName.data(), aValue);
    const bool on = aValue >= 0.5f;

    if (aName == "enabled")
    {
        if (s_userEnabled.exchange(on) == on || !s_state)
            return;
        std::scoped_lock lock(s_state->mutex);
        if (s_active)
        {
            WA_LOG_INFO("Switched %s in Mod Settings", on ? "on" : "off");
            Announce(*s_state, on ? "Wooting Analog on" : "Wooting Analog off");
        }
        else if (on)
        {
            Announce(*s_state, "Wooting Analog: restart to turn on");
        }
        return;
    }
    if (aName == "analog")
    {
        s_analogEnabled = on;
        return;
    }
    if (aName == "debug")
    {
        SetDebug(on);
        return;
    }
    if (aName == "meterScale")
    {
        s_meterScale = std::clamp(aValue, 0.2f, 2.f);
        return;
    }
    if (aName == "keepKeyboardPrompts")
    {
        if (s_keepKeyboardPrompts.exchange(on) != on)
            WA_LOG_INFO("Keep keyboard prompts %s", on ? "on" : "off");
        PublishInputOptions();
        return;
    }
    if (aName == "backend")
    {
        SelectBackend(on ? Backend::Events : Backend::XInput);
        return;
    }

    auto current = s_tuning.load(std::memory_order_acquire);
    if (!current)
        return;
    auto next = std::make_shared<Tuning>(*current);

    auto apply = [&](std::string_view aPrefix, Curve& aCurve) {
        if (aName.substr(0, aPrefix.size()) != aPrefix)
            return false;
        const auto field = aName.substr(aPrefix.size());
        if (field == "deadzone")
            aCurve.deadzone = aValue;
        else if (field == "fullpress")
            aCurve.full = aValue;
        else if (field == "curve")
            aCurve.gamma = aValue;
        else
            return false;
        return true;
    };

    const bool known = apply("movement.", next->move) || apply("steering.", next->steer) ||
                       apply("throttle.", next->throttle) || apply("brake.", next->brake);
    if (!known)
    {
        WA_LOG_DEBUG("Unknown option '%.*s'", static_cast<int>(aName.size()), aName.data());
        return;
    }
    next->Sanitize();
    s_tuning.store(std::move(next), std::memory_order_release);
}

std::string GetStatus()
{
    if (!s_state)
        return "not loaded";
    std::scoped_lock lock(s_state->mutex);
    return StatusLocked(*s_state);
}

bool IsActive()
{
    return s_active;
}

bool IsDebug()
{
    return s_debug;
}

std::string GetDeviceName()
{
    if (!s_state)
        return {};
    std::scoped_lock lock(s_state->mutex);
    return s_state->deviceName;
}

void ScriptLog(std::string_view aMessage)
{
    WA_LOG_INFO("[script] %.*s", static_cast<int>(aMessage.size()), aMessage.data());
}

bool TakeSnapshotRequest()
{
    return s_snapshotRequested.exchange(false);
}

float GetValue(int aIndex)
{
    if (aIndex >= 0 && aIndex < static_cast<int>(s_live.size()))
        return s_live[aIndex].load(std::memory_order_relaxed);

    const auto tuning = s_tuning.load(std::memory_order_acquire);
    const Curve* curves[4] = {};
    if (tuning)
    {
        curves[0] = &tuning->move;
        curves[1] = &tuning->steer;
        curves[2] = &tuning->throttle;
        curves[3] = &tuning->brake;
    }

    switch (aIndex)
    {
    case 12:
        return static_cast<float>(s_context.load());
    case 13:
        return static_cast<float>(s_backend.load());
    case 14:
        return s_keepKeyboardPrompts ? 1.f : 0.f;
    case 15:
        return s_active ? 1.f : 0.f;
    case 16:
        return s_state && s_state->sdk.HasDevice() ? 1.f : 0.f;
    case 17:
        return s_triggers ? 1.f : 0.f;
    case 18:
        return s_userEnabled ? 1.f : 0.f;
    case 19:
        return s_meterScale.load();
    default:
        break;
    }

    if (aIndex >= 20 && aIndex < 32 && tuning)
    {
        const Curve& curve = *curves[(aIndex - 20) / 3];
        switch ((aIndex - 20) % 3)
        {
        case 0:
            return curve.deadzone;
        case 1:
            return curve.full;
        default:
            return curve.gamma;
        }
    }
    return 0.f;
}

std::string TakeAnnouncement()
{
    if (!s_state)
        return {};
    std::scoped_lock lock(s_state->mutex);
    State& state = *s_state;

    if (!state.startupAnnounced)
    {
        state.startupAnnounced = true;
        std::string text;
        if (!s_active)
            text = state.config.enabled ? "Wooting Analog off: " + state.hudReason : std::string("Wooting Analog off");
        else if (state.throttleNextLaunch && !state.throttleOverridden)
            text = state.deviceName + " detected - restart once for analog driving";
        else
            text = state.deviceName + " detected";
        if (!text.empty())
        {
            WA_LOG_INFO("HUD message: %s", text.c_str());
            return text;
        }
    }

    if (state.announcements.empty())
        return {};
    std::string text = std::move(state.announcements.front());
    state.announcements.pop_front();
    return text;
}
}
