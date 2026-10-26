#include "Config.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace WA
{
namespace
{
std::string Trim(std::string aText)
{
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    aText.erase(aText.begin(), std::find_if(aText.begin(), aText.end(), notSpace));
    aText.erase(std::find_if(aText.rbegin(), aText.rend(), notSpace).base(), aText.end());
    return aText;
}

std::string Lower(std::string aText)
{
    std::transform(aText.begin(), aText.end(), aText.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return aText;
}

using IniMap = std::map<std::string, std::string>;

IniMap ParseIni(std::istream& aIn)
{
    IniMap values;
    std::string section;
    std::string line;
    while (std::getline(aIn, line))
    {
        for (size_t i = 0; i < line.size(); ++i)
        {
            if ((line[i] == ';' || line[i] == '#') && (i == 0 || std::isspace(static_cast<unsigned char>(line[i - 1]))))
            {
                line.resize(i);
                break;
            }
        }
        line = Trim(line);
        if (line.empty())
            continue;
        if (line.front() == '[' && line.back() == ']')
        {
            section = Lower(Trim(line.substr(1, line.size() - 2)));
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        values[section + "." + Lower(Trim(line.substr(0, eq)))] = Trim(line.substr(eq + 1));
    }
    return values;
}

void ReadBool(const IniMap& aIni, const char* aKey, bool& aOut)
{
    const auto it = aIni.find(aKey);
    if (it == aIni.end())
        return;
    const std::string value = Lower(it->second);
    if (value == "true" || value == "1" || value == "yes" || value == "on")
        aOut = true;
    else if (value == "false" || value == "0" || value == "no" || value == "off")
        aOut = false;
}

void ReadFloat(const IniMap& aIni, const char* aKey, float& aOut)
{
    const auto it = aIni.find(aKey);
    if (it == aIni.end() || it->second.empty())
        return;
    char* end = nullptr;
    const float value = std::strtof(it->second.c_str(), &end);
    if (end != it->second.c_str())
        aOut = value;
}

void ReadInt(const IniMap& aIni, const char* aKey, int& aOut)
{
    const auto it = aIni.find(aKey);
    if (it == aIni.end() || it->second.empty())
        return;
    char* end = nullptr;
    const long value = std::strtol(it->second.c_str(), &end, 10);
    if (end != it->second.c_str())
        aOut = static_cast<int>(value);
}

void ReadString(const IniMap& aIni, const char* aKey, std::string& aOut)
{
    const auto it = aIni.find(aKey);
    if (it != aIni.end())
        aOut = it->second;
}

void ReadCurve(const IniMap& aIni, const std::string& aSection, Curve& aCurve)
{
    ReadFloat(aIni, (aSection + ".deadzone").c_str(), aCurve.deadzone);
    ReadFloat(aIni, (aSection + ".fullpress").c_str(), aCurve.full);
    ReadFloat(aIni, (aSection + ".curve").c_str(), aCurve.gamma);
}

std::string FormatFloat(float aValue)
{
    std::ostringstream out;
    out << aValue;
    return out.str();
}
}

const char* ToString(Backend aBackend)
{
    return aBackend == Backend::Events ? "events" : "xinput";
}

const char* ToString(Toggle aToggle)
{
    switch (aToggle)
    {
    case Toggle::On:
        return "on";
    case Toggle::Off:
        return "off";
    default:
        return "auto";
    }
}

bool Config::Load(const std::filesystem::path& aPath, std::string* aError)
{
    std::ifstream file(aPath);
    if (!file)
    {
        if (aError)
            *aError = "cannot open " + aPath.string();
        return false;
    }
    const IniMap ini = ParseIni(file);

    ReadBool(ini, "general.enabled", enabled);
    std::string backendName;
    ReadString(ini, "general.backend", backendName);
    backendName = Lower(backendName);
    if (backendName == "events")
        backend = Backend::Events;
    else if (backendName == "xinput")
        backend = Backend::XInput;

    std::string throttle;
    ReadString(ini, "general.analogthrottle", throttle);
    throttle = Lower(throttle);
    if (throttle == "on" || throttle == "true")
        analogThrottle = Toggle::On;
    else if (throttle == "off" || throttle == "false")
        analogThrottle = Toggle::Off;
    else if (throttle == "auto")
        analogThrottle = Toggle::Auto;

    ReadBool(ini, "general.keepkeyboardprompts", keepKeyboardPrompts);
    ReadBool(ini, "general.digitalfallback", digitalFallback);
    ReadBool(ini, "general.requirefocus", requireFocus);
    ReadBool(ini, "general.debuglog", debugMode);
    ReadBool(ini, "general.debugmode", debugMode);
    ReadFloat(ini, "general.debugmetersize", meterScale);
    ReadBool(ini, "advanced.lograwinput", logRawInput);
    std::string timing;
    ReadString(ini, "advanced.eventtiming", timing);
    timing = Lower(timing);
    if (timing == "beforeui")
        injectAfterUi = false;
    else if (timing == "afterui")
        injectAfterUi = true;
    ReadInt(ini, "advanced.pollratehz", pollHz);
    ReadInt(ini, "advanced.virtualpadindex", padIndex);
    ReadBool(ini, "advanced.compensategamedeadzone", tuning.compensateStickDeadzone);

    ReadString(ini, "keys.forward", keyForward);
    ReadString(ini, "keys.back", keyBack);
    ReadString(ini, "keys.left", keyLeft);
    ReadString(ini, "keys.right", keyRight);
    ReadString(ini, "keys.accelerate", keyAccelerate);
    ReadString(ini, "keys.brake", keyBrake);
    ReadString(ini, "keys.steerleft", keySteerLeft);
    ReadString(ini, "keys.steerright", keySteerRight);

    ReadCurve(ini, "movement", tuning.move);
    ReadCurve(ini, "steering", tuning.steer);
    ReadCurve(ini, "throttle", tuning.throttle);
    ReadCurve(ini, "brake", tuning.brake);

    pollHz = std::clamp(pollHz, 60, 2000);
    padIndex = std::clamp(padIndex, 0, 3);
    tuning.Sanitize();
    return true;
}

bool Config::WriteDefault(const std::filesystem::path& aPath) const
{
    std::ofstream out(aPath, std::ios::trunc);
    if (!out)
        return false;

    auto curve = [&out](const char* aSection, const char* aWhat, const Curve& aCurve) {
        out << "\n[" << aSection << "]\n"
            << "; " << aWhat << "\n"
            << "Deadzone = " << FormatFloat(aCurve.deadzone) << "    ; key travel ignored at the top (0-1)\n"
            << "FullPress = " << FormatFloat(aCurve.full) << "   ; key travel that already counts as fully pressed\n"
            << "Curve = " << FormatFloat(aCurve.gamma) << "       ; 1 = linear, above 1 = finer control on light presses\n";
    };

    out << "; Wooting Analog for Cyberpunk 2077\n"
           "; Most of these can also be changed in game: Settings > Mods > Wooting Analog (needs Mod Settings).\n"
           "; In-game values win over this file while the game runs.\n"
           "\n[General]\n"
           "Enabled = true\n"
           "; How the analog values reach the game. Can be changed while playing.\n"
           "; xinput = virtual gamepad only the game can see (recommended)\n"
           "; events = experimental: feeds axis events straight into the engine, no gamepad at all\n"
           "Backend = xinput\n"
           "; Keep keyboard button prompts instead of switching to Xbox icons while you move.\n"
           "KeepKeyboardPrompts = true\n"
           "; Analog throttle and brake while driving. auto = enabled once the mod's script has run\n"
           "; successfully at least once (needs redscript); on / off to force it.\n"
           "AnalogThrottle = auto\n"
           "; If the keyboard disconnects mid-game, keep the movement keys working as normal digital keys.\n"
           "DigitalFallback = true\n"
           "; Ignore the keyboard while the game window is not focused.\n"
           "RequireFocus = true\n"
           "; Detailed diagnostics in red4ext/logs/WootingAnalog-*.log. While it is on, Ctrl+Shift+F10\n"
           "; saves a snapshot of everything the mod sees - press it when something feels wrong.\n"
           "DebugMode = false\n"
           "; Size of the debug meters in the top left corner (0.2 - 2).\n"
           "DebugMeterSize = 0.6\n"
           "\n[Keys]\n"
           "; Leave empty to use your in-game keybindings (recommended). Accepts W, IK_W, or a key code.\n"
           "Forward =\nBack =\nLeft =\nRight =\nAccelerate =\nBrake =\nSteerLeft =\nSteerRight =\n";

    curve("Movement", "Walking and running", tuning.move);
    curve("Steering", "Steering left and right", tuning.steer);
    curve("Throttle", "Accelerating", tuning.throttle);
    curve("Brake", "Braking and reversing", tuning.brake);

    out << "\n[Advanced]\n"
           "; Pre-scale the virtual stick for the game's radial deadzone (r6/config/inputDeadzones.xml).\n"
           "; Leave off: the game reads the stick value directly and only sprints above 0.9.\n"
           "CompensateGameDeadzone = false\n"
           "PollRateHz = "
        << pollHz
        << "\n"
           "; Which XInput slot the virtual pad uses (0-3). It merges with a real pad in that slot.\n"
           "VirtualPadIndex = "
        << padIndex
        << "\n"
           "; Events backend: afterUI (default) or beforeUI - when the axis events join the engine's input.\n"
           "EventTiming = afterUI\n"
           "; Diagnostics: write the engine's raw input events to the log (first few of each kind).\n"
           "LogRawInput = false\n";
    return static_cast<bool>(out);
}

std::vector<std::string> Config::ApplyModSettings(const std::filesystem::path& aUserIni)
{
    std::vector<std::string> applied;
    std::ifstream file(aUserIni);
    if (!file)
        return applied;
    const IniMap ini = ParseIni(file);
    const std::string section = "wootinganalogsettings.";

    auto has = [&](const char* aKey) { return ini.count(section + aKey) > 0; };
    auto boolean = [&](const char* aKey, bool& aOut) {
        if (!has(aKey))
            return;
        ReadBool(ini, (section + aKey).c_str(), aOut);
        applied.push_back(aKey);
    };
    auto number = [&](const char* aKey, float& aOut) {
        if (!has(aKey))
            return;
        ReadFloat(ini, (section + aKey).c_str(), aOut);
        applied.push_back(aKey);
    };

    boolean("enabled", enabled);
    boolean("keepkeyboardprompts", keepKeyboardPrompts);
    boolean("debugmode", debugMode);
    number("debugmetersize", meterScale);
    if (has("inputmethod"))
    {
        const std::string method = Lower(ini.at(section + "inputmethod"));
        backend = method == "directevents" || method == "1" ? Backend::Events : Backend::XInput;
        applied.push_back("inputmethod");
    }
    number("movementdeadzone", tuning.move.deadzone);
    number("movementfullpress", tuning.move.full);
    number("movementcurve", tuning.move.gamma);
    number("steeringdeadzone", tuning.steer.deadzone);
    number("steeringfullpress", tuning.steer.full);
    number("steeringcurve", tuning.steer.gamma);
    number("throttledeadzone", tuning.throttle.deadzone);
    number("throttlefullpress", tuning.throttle.full);
    number("throttlecurve", tuning.throttle.gamma);
    number("brakedeadzone", tuning.brake.deadzone);
    number("brakefullpress", tuning.brake.full);
    number("brakecurve", tuning.brake.gamma);
    tuning.Sanitize();
    return applied;
}

StateFile::StateFile(std::filesystem::path aPath)
    : m_path(std::move(aPath))
{
    std::ifstream file(m_path);
    if (!file)
        return;
    for (const auto& [key, value] : ParseIni(file))
    {
        m_values[key.size() > 1 && key.front() == '.' ? key.substr(1) : key] = value;
    }
}

std::string StateFile::Get(const std::string& aKey, const std::string& aDefault) const
{
    const auto it = m_values.find(Lower(aKey));
    return it == m_values.end() ? aDefault : it->second;
}

void StateFile::Set(const std::string& aKey, const std::string& aValue)
{
    m_values[Lower(aKey)] = aValue;
}

bool StateFile::Save() const
{
    std::ofstream out(m_path, std::ios::trunc);
    if (!out)
        return false;
    out << "; Written by Wooting Analog. Safe to delete.\n";
    for (const auto& [key, value] : m_values)
        out << key << " = " << value << "\n";
    return static_cast<bool>(out);
}
}
