#include "GameBindings.hpp"

#include "Json.hpp"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include <Windows.h>
#include <ShlObj.h>

namespace WA
{
namespace
{
struct KeyName
{
    const char* name;
    uint16_t key;
};

constexpr KeyName s_keys[] = {
#include "InputKeys.inc"
};

bool EqualsNoCase(std::string_view aLeft, std::string_view aRight)
{
    if (aLeft.size() != aRight.size())
        return false;
    for (size_t i = 0; i < aLeft.size(); ++i)
    {
        if (std::tolower(static_cast<unsigned char>(aLeft[i])) != std::tolower(static_cast<unsigned char>(aRight[i])))
            return false;
    }
    return true;
}

void Assign(BoundKey& aKey, std::string_view aName)
{
    aKey.vk = KeyFromName(aName);
    aKey.name = std::string(aName);
    aKey.fromGame = true;
}

BoundKey Make(uint16_t aKey)
{
    BoundKey key;
    key.vk = aKey;
    key.name = std::string(NameFromKey(aKey));
    return key;
}
}

Bindings Bindings::Defaults()
{
    Bindings bindings;
    bindings.forward = Make('W');
    bindings.back = Make('S');
    bindings.left = Make('A');
    bindings.right = Make('D');
    bindings.accelerate = Make('W');
    bindings.brake = Make('S');
    bindings.steerLeft = Make('A');
    bindings.steerRight = Make('D');
    return bindings;
}

std::string Bindings::Describe() const
{
    auto one = [](const BoundKey& aKey) { return aKey.name.empty() ? std::string("none") : aKey.name; };
    std::ostringstream out;
    out << "on foot " << one(forward) << "/" << one(left) << "/" << one(back) << "/" << one(right) << ", driving "
        << one(accelerate) << " accelerate, " << one(brake) << " brake, " << one(steerLeft) << "/" << one(steerRight)
        << " steer";
    return out.str();
}

uint16_t KeyFromName(std::string_view aName)
{
    for (const auto& entry : s_keys)
    {
        if (EqualsNoCase(entry.name, aName))
            return entry.key;
    }
    return 0;
}

std::string_view NameFromKey(uint16_t aKey)
{
    for (const auto& entry : s_keys)
    {
        if (entry.key == aKey)
            return entry.name;
    }
    return {};
}

uint16_t ParseKeySpec(std::string_view aSpec)
{
    while (!aSpec.empty() && std::isspace(static_cast<unsigned char>(aSpec.front())))
        aSpec.remove_prefix(1);
    while (!aSpec.empty() && std::isspace(static_cast<unsigned char>(aSpec.back())))
        aSpec.remove_suffix(1);
    if (aSpec.empty())
        return 0;

    if (std::isdigit(static_cast<unsigned char>(aSpec.front())) && aSpec.size() > 1)
    {
        const std::string number(aSpec);
        const unsigned long value = std::strtoul(number.c_str(), nullptr, 0);
        return value > 0 && value < 256 ? static_cast<uint16_t>(value) : 0;
    }

    if (const uint16_t key = KeyFromName(aSpec))
        return key;
    return KeyFromName("IK_" + std::string(aSpec));
}

std::filesystem::path DefaultUserSettingsPath()
{
    std::filesystem::path base;
    PWSTR localAppData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData)) && localAppData)
        base = localAppData;
    CoTaskMemFree(localAppData);

    if (base.empty())
    {
        if (const wchar_t* env = _wgetenv(L"LOCALAPPDATA"))
            base = env;
    }
    return base / L"CD Projekt Red" / L"Cyberpunk 2077" / L"UserSettings.json";
}

bool ParseGameBindings(std::string_view aJson, Bindings& aBindings, std::string* aError)
{
    Json::Value root;
    if (!Json::Parse(aJson, root, aError))
        return false;

    const Json::Value* data = root.Get("data");
    if (!data || !data->IsArray())
    {
        if (aError)
            *aError = "no 'data' array";
        return false;
    }

    int found = 0;
    for (const auto& group : data->array)
    {
        const Json::Value* groupName = group.Get("group_name");
        const Json::Value* options = group.Get("options");
        if (!groupName || !options || !options->IsArray())
            continue;

        const auto name = groupName->AsString();
        const bool locomotion = name == "/key_bindings/SettingsLocomotion";
        const bool vehicle = name == "/key_bindings/SettingsVehicle";
        if (!locomotion && !vehicle)
            continue;

        for (const auto& option : options->array)
        {
            const Json::Value* optionName = option.Get("name");
            const Json::Value* value = option.Get("value");
            if (!optionName || !value)
                continue;

            const auto key = optionName->AsString();
            const auto bound = value->AsString();
            BoundKey* target = nullptr;
            if (locomotion)
            {
                if (key == "forward")
                    target = &aBindings.forward;
                else if (key == "back")
                    target = &aBindings.back;
                else if (key == "left")
                    target = &aBindings.left;
                else if (key == "right")
                    target = &aBindings.right;
            }
            else
            {
                if (key == "vehicleAccelerate")
                    target = &aBindings.accelerate;
                else if (key == "vehicleDecelrate" || key == "vehicleDecelerate")
                    target = &aBindings.brake;
                else if (key == "vehicleSteerLeft")
                    target = &aBindings.steerLeft;
                else if (key == "vehicleSteerRight")
                    target = &aBindings.steerRight;
            }

            if (target)
            {
                Assign(*target, bound);
                ++found;
            }
        }
    }

    if (found == 0 && aError)
        *aError = "no movement or vehicle keybindings in file";
    return found > 0;
}

bool LoadGameBindings(const std::filesystem::path& aPath, Bindings& aBindings, std::string* aError)
{
    std::ifstream file(aPath, std::ios::binary);
    if (!file)
    {
        if (aError)
            *aError = "cannot open file";
        return false;
    }
    std::ostringstream content;
    content << file.rdbuf();
    return ParseGameBindings(content.str(), aBindings, aError);
}
}
