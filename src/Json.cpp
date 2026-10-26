#include "Json.hpp"

#include <cstdlib>

namespace WA::Json
{
namespace
{
struct Parser
{
    std::string_view text;
    size_t pos = 0;
    std::string error;
    int depth = 0;

    void SkipWhitespace()
    {
        while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\n' || text[pos] == '\r'))
            ++pos;
    }

    bool Fail(const char* aWhat)
    {
        if (error.empty())
            error = std::string(aWhat) + " at offset " + std::to_string(pos);
        return false;
    }

    bool Literal(std::string_view aWord)
    {
        if (text.substr(pos, aWord.size()) != aWord)
            return Fail("unexpected token");
        pos += aWord.size();
        return true;
    }

    static void AppendUtf8(std::string& aOut, unsigned aCode)
    {
        if (aCode < 0x80)
            aOut += static_cast<char>(aCode);
        else if (aCode < 0x800)
        {
            aOut += static_cast<char>(0xC0 | (aCode >> 6));
            aOut += static_cast<char>(0x80 | (aCode & 0x3F));
        }
        else
        {
            aOut += static_cast<char>(0xE0 | (aCode >> 12));
            aOut += static_cast<char>(0x80 | ((aCode >> 6) & 0x3F));
            aOut += static_cast<char>(0x80 | (aCode & 0x3F));
        }
    }

    bool ParseString(std::string& aOut)
    {
        if (pos >= text.size() || text[pos] != '"')
            return Fail("expected string");
        ++pos;
        while (pos < text.size())
        {
            const char c = text[pos++];
            if (c == '"')
                return true;
            if (c != '\\')
            {
                aOut += c;
                continue;
            }
            if (pos >= text.size())
                break;
            const char e = text[pos++];
            switch (e)
            {
            case '"':
            case '\\':
            case '/':
                aOut += e;
                break;
            case 'b':
                aOut += '\b';
                break;
            case 'f':
                aOut += '\f';
                break;
            case 'n':
                aOut += '\n';
                break;
            case 'r':
                aOut += '\r';
                break;
            case 't':
                aOut += '\t';
                break;
            case 'u':
            {
                if (pos + 4 > text.size())
                    return Fail("bad unicode escape");
                const std::string hex(text.substr(pos, 4));
                pos += 4;
                AppendUtf8(aOut, static_cast<unsigned>(std::strtoul(hex.c_str(), nullptr, 16)));
                break;
            }
            default:
                return Fail("bad escape");
            }
        }
        return Fail("unterminated string");
    }

    bool ParseNumber(Value& aOut)
    {
        const size_t start = pos;
        if (pos < text.size() && (text[pos] == '-' || text[pos] == '+'))
            ++pos;
        while (pos < text.size() &&
               ((text[pos] >= '0' && text[pos] <= '9') || text[pos] == '.' || text[pos] == 'e' || text[pos] == 'E' ||
                text[pos] == '-' || text[pos] == '+'))
            ++pos;
        if (pos == start)
            return Fail("expected value");
        const std::string number(text.substr(start, pos - start));
        aOut.type = Value::Type::Number;
        aOut.number = std::strtod(number.c_str(), nullptr);
        return true;
    }

    bool ParseValue(Value& aOut)
    {
        if (++depth > 64)
            return Fail("nesting too deep");
        SkipWhitespace();
        if (pos >= text.size())
            return Fail("unexpected end");

        bool ok = true;
        const char c = text[pos];
        if (c == '{')
        {
            aOut.type = Value::Type::Object;
            ++pos;
            SkipWhitespace();
            if (pos < text.size() && text[pos] == '}')
                ++pos;
            else
            {
                while (true)
                {
                    SkipWhitespace();
                    std::string key;
                    if (!ParseString(key))
                        return false;
                    SkipWhitespace();
                    if (pos >= text.size() || text[pos] != ':')
                        return Fail("expected ':'");
                    ++pos;
                    Value child;
                    if (!ParseValue(child))
                        return false;
                    aOut.object.emplace_back(std::move(key), std::move(child));
                    SkipWhitespace();
                    if (pos < text.size() && text[pos] == ',')
                    {
                        ++pos;
                        continue;
                    }
                    if (pos < text.size() && text[pos] == '}')
                    {
                        ++pos;
                        break;
                    }
                    return Fail("expected ',' or '}'");
                }
            }
        }
        else if (c == '[')
        {
            aOut.type = Value::Type::Array;
            ++pos;
            SkipWhitespace();
            if (pos < text.size() && text[pos] == ']')
                ++pos;
            else
            {
                while (true)
                {
                    Value child;
                    if (!ParseValue(child))
                        return false;
                    aOut.array.emplace_back(std::move(child));
                    SkipWhitespace();
                    if (pos < text.size() && text[pos] == ',')
                    {
                        ++pos;
                        continue;
                    }
                    if (pos < text.size() && text[pos] == ']')
                    {
                        ++pos;
                        break;
                    }
                    return Fail("expected ',' or ']'");
                }
            }
        }
        else if (c == '"')
        {
            aOut.type = Value::Type::String;
            ok = ParseString(aOut.string);
        }
        else if (c == 't')
        {
            aOut.type = Value::Type::Bool;
            aOut.boolean = true;
            ok = Literal("true");
        }
        else if (c == 'f')
        {
            aOut.type = Value::Type::Bool;
            ok = Literal("false");
        }
        else if (c == 'n')
        {
            ok = Literal("null");
        }
        else
        {
            ok = ParseNumber(aOut);
        }

        --depth;
        return ok;
    }
};
}

const Value* Value::Get(std::string_view aKey) const
{
    for (const auto& [key, value] : object)
    {
        if (key == aKey)
            return &value;
    }
    return nullptr;
}

std::string_view Value::AsString() const
{
    return type == Type::String ? std::string_view(string) : std::string_view();
}

bool Parse(std::string_view aText, Value& aOut, std::string* aError)
{
    if (aText.size() >= 3 && static_cast<unsigned char>(aText[0]) == 0xEF &&
        static_cast<unsigned char>(aText[1]) == 0xBB && static_cast<unsigned char>(aText[2]) == 0xBF)
        aText.remove_prefix(3);

    Parser parser{aText};
    aOut = Value{};
    bool ok = parser.ParseValue(aOut);
    if (ok)
    {
        parser.SkipWhitespace();
        if (parser.pos != aText.size())
            ok = parser.Fail("trailing data");
    }
    if (!ok && aError)
        *aError = parser.error;
    return ok;
}
}
