#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace WA
{
struct InputXmlOptions
{
    bool movement = true;
    bool steering = true;
    bool throttle = false;
};

struct InputXmlResult
{
    bool ok = false;
    std::string xml;
    std::vector<std::string> notes;
    int changes = 0;
    bool movement = false;
    bool steering = false;
    bool throttle = false;
};

InputXmlResult BuildInputXml(std::string_view aContextsXml, std::string_view aMappingsXml,
                             const InputXmlOptions& aOptions, std::string_view aHeaderComment = {});

constexpr const char* kMoveXMapping = "WootingAnalog_MoveX";
constexpr const char* kMoveYMapping = "WootingAnalog_MoveY";
constexpr const char* kCrawlMapping = "WootingAnalog_Crawl";

std::string ReadInputConfig(const std::filesystem::path& aGameRoot, const wchar_t* aFileName,
                            std::filesystem::path* aUsedPath = nullptr);

bool RegisterWithInputLoader(void* aPluginHandle, const std::filesystem::path& aGameRoot,
                             const std::filesystem::path& aXmlPath, std::string* aError = nullptr);
}
