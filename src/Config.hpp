#pragma once

#include "Analog.hpp"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace WA
{
enum class Backend
{
    XInput,
    Events,
};

enum class Toggle
{
    Auto,
    On,
    Off,
};

struct Config
{
    bool enabled = true;
    Backend backend = Backend::XInput;
    bool keepKeyboardPrompts = true;
    bool injectAfterUi = true;
    bool logRawInput = false;
    Toggle analogThrottle = Toggle::Auto;
    bool digitalFallback = true;
    bool requireFocus = true;
    int pollHz = 500;
    int padIndex = 0;
    bool debugMode = false;
    float meterScale = 0.6f;

    std::string keyForward, keyBack, keyLeft, keyRight;
    std::string keyAccelerate, keyBrake, keySteerLeft, keySteerRight;

    Tuning tuning;

    bool Load(const std::filesystem::path& aPath, std::string* aError = nullptr);
    bool WriteDefault(const std::filesystem::path& aPath) const;

    std::vector<std::string> ApplyModSettings(const std::filesystem::path& aUserIni);
};

class StateFile
{
public:
    explicit StateFile(std::filesystem::path aPath);
    std::string Get(const std::string& aKey, const std::string& aDefault = {}) const;
    void Set(const std::string& aKey, const std::string& aValue);
    bool Save() const;

private:
    std::filesystem::path m_path;
    std::map<std::string, std::string> m_values;
};

const char* ToString(Backend aBackend);
const char* ToString(Toggle aToggle);
}
