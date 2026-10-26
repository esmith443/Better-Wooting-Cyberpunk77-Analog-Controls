#include "InputConfig.hpp"

#include <fstream>
#include <map>
#include <sstream>

#include <Windows.h>

namespace WA
{
namespace
{
struct Attribute
{
    std::string_view name;
    size_t valueBegin = 0;
    size_t valueEnd = 0;
};

bool IsSpace(char aChar)
{
    return aChar == ' ' || aChar == '\t' || aChar == '\r' || aChar == '\n';
}

size_t FindTag(std::string_view aText, std::string_view aTag, size_t aPos)
{
    while (aPos < aText.size())
    {
        const size_t lt = aText.find('<', aPos);
        if (lt == std::string_view::npos)
            return lt;
        if (aText.substr(lt, 4) == "<!--")
        {
            const size_t close = aText.find("-->", lt + 4);
            if (close == std::string_view::npos)
                return std::string_view::npos;
            aPos = close + 3;
            continue;
        }
        const size_t after = lt + 1 + aTag.size();
        if (aText.substr(lt + 1, aTag.size()) == aTag && after < aText.size() &&
            (IsSpace(aText[after]) || aText[after] == '>' || aText[after] == '/'))
            return lt;
        aPos = lt + 1;
    }
    return std::string_view::npos;
}

size_t EndOfTag(std::string_view aText, size_t aLt, bool* aSelfClosing = nullptr)
{
    char quote = 0;
    for (size_t i = aLt + 1; i < aText.size(); ++i)
    {
        const char c = aText[i];
        if (quote)
        {
            if (c == quote)
                quote = 0;
        }
        else if (c == '"' || c == '\'')
            quote = c;
        else if (c == '>')
        {
            if (aSelfClosing)
                *aSelfClosing = aText[i - 1] == '/';
            return i + 1;
        }
    }
    return std::string_view::npos;
}

std::vector<Attribute> ParseAttributes(std::string_view aStartTag)
{
    std::vector<Attribute> attributes;
    size_t i = 1;
    while (i < aStartTag.size() && !IsSpace(aStartTag[i]) && aStartTag[i] != '>' && aStartTag[i] != '/')
        ++i;
    while (i < aStartTag.size())
    {
        while (i < aStartTag.size() && IsSpace(aStartTag[i]))
            ++i;
        if (i >= aStartTag.size() || aStartTag[i] == '>' || aStartTag[i] == '/')
            break;
        const size_t nameBegin = i;
        while (i < aStartTag.size() && !IsSpace(aStartTag[i]) && aStartTag[i] != '=' && aStartTag[i] != '>')
            ++i;
        const std::string_view name = aStartTag.substr(nameBegin, i - nameBegin);
        while (i < aStartTag.size() && IsSpace(aStartTag[i]))
            ++i;
        if (i >= aStartTag.size() || aStartTag[i] != '=')
            continue;
        ++i;
        while (i < aStartTag.size() && IsSpace(aStartTag[i]))
            ++i;
        if (i >= aStartTag.size() || (aStartTag[i] != '"' && aStartTag[i] != '\''))
            break;
        const char quote = aStartTag[i++];
        const size_t valueBegin = i;
        while (i < aStartTag.size() && aStartTag[i] != quote)
            ++i;
        attributes.push_back({name, valueBegin, i});
        ++i;
    }
    return attributes;
}

bool GetAttribute(std::string_view aStartTag, std::string_view aName, std::string_view& aValue)
{
    for (const auto& attribute : ParseAttributes(aStartTag))
    {
        if (attribute.name == aName)
        {
            aValue = aStartTag.substr(attribute.valueBegin, attribute.valueEnd - attribute.valueBegin);
            return true;
        }
    }
    return false;
}

std::string SetAttribute(std::string_view aStartTag, std::string_view aName, std::string_view aValue)
{
    for (const auto& attribute : ParseAttributes(aStartTag))
    {
        if (attribute.name == aName)
        {
            std::string result(aStartTag.substr(0, attribute.valueBegin));
            result += aValue;
            result += aStartTag.substr(attribute.valueEnd);
            return result;
        }
    }
    return std::string(aStartTag);
}

struct Element
{
    size_t begin = 0;
    size_t tagEnd = 0;
    size_t closeBegin = 0;
    size_t end = 0;
};

bool FindElementAt(std::string_view aText, std::string_view aTag, size_t aLt, Element& aOut)
{
    bool selfClosing = false;
    const size_t tagEnd = EndOfTag(aText, aLt, &selfClosing);
    if (tagEnd == std::string_view::npos)
        return false;
    aOut.begin = aLt;
    aOut.tagEnd = tagEnd;
    if (selfClosing)
    {
        aOut.closeBegin = tagEnd;
        aOut.end = tagEnd;
        return true;
    }
    const std::string closing = "</" + std::string(aTag);
    size_t pos = tagEnd;
    while (true)
    {
        const size_t close = aText.find(closing, pos);
        if (close == std::string_view::npos)
            return false;
        const size_t comment = aText.find("<!--", pos);
        if (comment != std::string_view::npos && comment < close)
        {
            const size_t commentEnd = aText.find("-->", comment + 4);
            if (commentEnd == std::string_view::npos)
                return false;
            if (commentEnd > close)
            {
                pos = commentEnd + 3;
                continue;
            }
        }
        const size_t gt = aText.find('>', close);
        if (gt == std::string_view::npos)
            return false;
        aOut.closeBegin = close;
        aOut.end = gt + 1;
        return true;
    }
}

bool FindNamedElement(std::string_view aText, std::string_view aTag, std::string_view aName, Element& aOut)
{
    size_t pos = 0;
    while ((pos = FindTag(aText, aTag, pos)) != std::string_view::npos)
    {
        Element element;
        if (!FindElementAt(aText, aTag, pos, element))
            return false;
        std::string_view name;
        if (GetAttribute(aText.substr(element.begin, element.tagEnd - element.begin), "name", name) && name == aName)
        {
            aOut = element;
            return true;
        }
        pos = element.end;
    }
    return false;
}

int FilterMapping(std::string_view aText, const Element& aMapping, const std::vector<std::string_view>& aDrop,
                  std::string_view aNewName, std::string& aOut, std::string* aDroppedKeys)
{
    std::string_view startTag = aText.substr(aMapping.begin, aMapping.tagEnd - aMapping.begin);
    aOut = aNewName.empty() ? std::string(startTag) : SetAttribute(startTag, "name", aNewName);

    const std::string_view body = aText.substr(aMapping.tagEnd, aMapping.closeBegin - aMapping.tagEnd);
    int dropped = 0;
    size_t pos = 0;
    while (true)
    {
        const size_t lt = FindTag(body, "button", pos);
        if (lt == std::string_view::npos)
        {
            aOut += body.substr(pos);
            break;
        }
        Element button;
        if (!FindElementAt(body, "button", lt, button))
        {
            aOut += body.substr(pos);
            break;
        }

        std::string_view ui;
        const bool hasUi = GetAttribute(body.substr(button.begin, button.tagEnd - button.begin), "overridableUI", ui);
        bool drop = false;
        if (hasUi)
        {
            for (const auto& name : aDrop)
                drop = drop || ui == name;
        }

        if (drop)
        {
            size_t lineStart = lt;
            while (lineStart > pos && (body[lineStart - 1] == ' ' || body[lineStart - 1] == '\t'))
                --lineStart;
            aOut += body.substr(pos, lineStart - pos);
            size_t next = button.end;
            if (next < body.size() && body[next] == '\r')
                ++next;
            if (next < body.size() && body[next] == '\n')
                ++next;
            pos = next;
            ++dropped;
            if (aDroppedKeys)
            {
                std::string_view id;
                GetAttribute(body.substr(button.begin, button.tagEnd - button.begin), "id", id);
                if (!aDroppedKeys->empty())
                    *aDroppedKeys += ", ";
                *aDroppedKeys += std::string(id) + " (" + std::string(ui) + ")";
            }
        }
        else
        {
            aOut += body.substr(pos, button.end - pos);
            pos = button.end;
        }
    }

    aOut += aText.substr(aMapping.closeBegin, aMapping.end - aMapping.closeBegin);
    return dropped;
}

int RenameContextMaps(std::string_view aText, const Element& aContext, const std::map<std::string, std::string>& aRename,
                      std::string& aOut)
{
    aOut = std::string(aText.substr(aContext.begin, aContext.tagEnd - aContext.begin));
    const std::string_view body = aText.substr(aContext.tagEnd, aContext.closeBegin - aContext.tagEnd);
    int renamed = 0;
    size_t pos = 0;
    while (true)
    {
        const size_t lt = FindTag(body, "action", pos);
        if (lt == std::string_view::npos)
        {
            aOut += body.substr(pos);
            break;
        }
        Element action;
        if (!FindElementAt(body, "action", lt, action))
        {
            aOut += body.substr(pos);
            break;
        }
        aOut += body.substr(pos, lt - pos);
        const std::string_view startTag = body.substr(action.begin, action.tagEnd - action.begin);
        std::string_view map;
        const auto it = GetAttribute(startTag, "map", map) ? aRename.find(std::string(map)) : aRename.end();
        if (it != aRename.end())
        {
            aOut += SetAttribute(startTag, "map", it->second);
            aOut += body.substr(action.tagEnd, action.end - action.tagEnd);
            ++renamed;
        }
        else
        {
            aOut += body.substr(action.begin, action.end - action.begin);
        }
        pos = action.end;
    }
    aOut += aText.substr(aContext.closeBegin, aContext.end - aContext.closeBegin);
    return renamed;
}

bool ContainsKey(std::string_view aElement, std::string_view aKey)
{
    return aElement.find("\"" + std::string(aKey) + "\"") != std::string_view::npos;
}

bool FileNewerThanProcessStart(const std::filesystem::path& aPath)
{
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(aPath.c_str(), GetFileExInfoStandard, &data))
        return false;
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user))
        return false;
    return CompareFileTime(&data.ftLastWriteTime, &created) >= 0;
}

std::string ReadFile(const std::filesystem::path& aPath)
{
    std::ifstream file(aPath, std::ios::binary);
    if (!file)
        return {};
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}
}

InputXmlResult BuildInputXml(std::string_view aContextsXml, std::string_view aMappingsXml,
                             const InputXmlOptions& aOptions, std::string_view aHeaderComment)
{
    InputXmlResult result;
    std::string body;

    if (aOptions.movement)
    {
        struct Axis
        {
            const char* source;
            const char* target;
            std::vector<std::string_view> drop;
            const char* padKey;
            bool required;
        };
        const Axis axes[] = {
            {"LeftX_Axis", kMoveXMapping, {"left", "right"}, "IK_Pad_LeftAxisX", true},
            {"LeftY_Axis", kMoveYMapping, {"forward", "back"}, "IK_Pad_LeftAxisY", true},
            {"CrawlingLeftY_Axis", kCrawlMapping, {"forward"}, "IK_Pad_LeftAxisY", false},
        };

        std::string mappings;
        std::map<std::string, std::string> rename;
        bool missingRequired = false;
        for (const auto& axis : axes)
        {
            Element element;
            if (!FindNamedElement(aMappingsXml, "mapping", axis.source, element))
            {
                result.notes.push_back(std::string("mapping '") + axis.source + "' not found");
                missingRequired = missingRequired || axis.required;
                continue;
            }
            std::string filtered, dropped;
            const int count = FilterMapping(aMappingsXml, element, axis.drop, axis.target, filtered, &dropped);
            if (!ContainsKey(filtered, axis.padKey))
            {
                result.notes.push_back(std::string("mapping '") + axis.source + "' has no " + axis.padKey +
                                       ", left unchanged");
                missingRequired = missingRequired || axis.required;
                continue;
            }
            mappings += "\t" + filtered + "\n\n";
            rename[axis.source] = axis.target;
            result.notes.push_back(std::string(axis.source) + " -> " + axis.target + ": removed " +
                                   (count ? dropped : std::string("nothing")));
        }

        Element context;
        if (missingRequired)
        {
            result.notes.push_back("on-foot movement left unchanged");
        }
        else if (!FindNamedElement(aContextsXml, "context", "MovementActions", context))
        {
            result.notes.push_back("context 'MovementActions' not found, on-foot movement left unchanged");
        }
        else
        {
            std::string renamedContext;
            const int renamed = RenameContextMaps(aContextsXml, context, rename, renamedContext);
            if (renamed == 0)
            {
                result.notes.push_back("MovementActions uses none of the movement axes, left unchanged");
            }
            else
            {
                body += mappings;
                body += "\t" + renamedContext + "\n\n";
                result.changes += renamed;
                result.movement = true;
                result.notes.push_back("MovementActions: " + std::to_string(renamed) + " action(s) now read the stick");
            }
        }
    }

    auto replaceMapping = [&](const char* aName, const std::vector<std::string_view>& aDrop, const char* aPadKey) {
        Element element;
        if (!FindNamedElement(aMappingsXml, "mapping", aName, element))
        {
            result.notes.push_back(std::string("mapping '") + aName + "' not found");
            return false;
        }
        std::string filtered, dropped;
        const int count = FilterMapping(aMappingsXml, element, aDrop, {}, filtered, &dropped);
        if (count == 0)
        {
            result.notes.push_back(std::string(aName) + ": no keyboard binds to remove, left unchanged");
            return false;
        }
        if (!ContainsKey(filtered, aPadKey))
        {
            result.notes.push_back(std::string(aName) + " has no " + aPadKey + ", left unchanged");
            return false;
        }
        body += "\t" + filtered + "\n\n";
        ++result.changes;
        result.notes.push_back(std::string(aName) + ": removed " + dropped);
        return true;
    };

    if (aOptions.steering)
        result.steering =
            replaceMapping("LeftX_Axis_Vehicle", {"vehicleSteerLeft", "vehicleSteerRight"}, "IK_Pad_LeftAxisX");
    if (aOptions.throttle)
    {
        result.throttle = replaceMapping("Acceleration_Axis", {"vehicleAccelerate"}, "IK_Pad_RightTrigger");
        replaceMapping("Deceleration_Axis", {"vehicleDecelrate", "vehicleDecelerate"}, "IK_Pad_LeftTrigger");
    }

    std::string xml = "<?xml version=\"1.0\"?>\n";
    if (!aHeaderComment.empty())
        xml += "<!-- " + std::string(aHeaderComment) + " -->\n";
    xml += "<bindings>\n\n" + body + "</bindings>\n";
    result.xml = std::move(xml);
    result.ok = result.changes > 0;
    return result;
}

std::string ReadInputConfig(const std::filesystem::path& aGameRoot, const wchar_t* aFileName,
                            std::filesystem::path* aUsedPath)
{
    const auto cached = aGameRoot / L"r6" / L"cache" / aFileName;
    const auto vanilla = aGameRoot / L"r6" / L"config" / aFileName;
    const auto& path = FileNewerThanProcessStart(cached) ? cached : vanilla;
    if (aUsedPath)
        *aUsedPath = path;
    return ReadFile(path);
}

bool RegisterWithInputLoader(void* aPluginHandle, const std::filesystem::path& aGameRoot,
                             const std::filesystem::path& aXmlPath, std::string* aError)
{
    HMODULE inputLoader = GetModuleHandleW(L"input_loader.dll");
    if (!inputLoader)
    {
        const auto path = aGameRoot / L"red4ext" / L"plugins" / L"input_loader" / L"input_loader.dll";
        std::error_code ec;
        if (std::filesystem::exists(path, ec))
            inputLoader = LoadLibraryW(path.c_str());
    }
    if (!inputLoader)
    {
        if (aError)
            *aError = "Input Loader is not installed (red4ext/plugins/input_loader)";
        return false;
    }

    using AddFn = void (*)(void*, const wchar_t*);
    const auto add = reinterpret_cast<AddFn>(GetProcAddress(inputLoader, "Add"));
    if (!add)
    {
        if (aError)
            *aError = "this Input Loader version has no Add() export, please update it";
        return false;
    }
    add(aPluginHandle, aXmlPath.c_str());
    return true;
}
}
