#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace WA
{
struct BoundKey
{
    uint16_t vk = 0;
    std::string name;
    bool fromGame = false;
};

struct Bindings
{
    BoundKey forward;
    BoundKey back;
    BoundKey left;
    BoundKey right;
    BoundKey accelerate;
    BoundKey brake;
    BoundKey steerLeft;
    BoundKey steerRight;

    static Bindings Defaults();
    std::string Describe() const;
};

uint16_t KeyFromName(std::string_view aName);
std::string_view NameFromKey(uint16_t aKey);

uint16_t ParseKeySpec(std::string_view aSpec);

std::filesystem::path DefaultUserSettingsPath();

bool LoadGameBindings(const std::filesystem::path& aPath, Bindings& aBindings, std::string* aError = nullptr);
bool ParseGameBindings(std::string_view aJson, Bindings& aBindings, std::string* aError = nullptr);
}
