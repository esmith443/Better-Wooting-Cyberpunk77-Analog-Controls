#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace WA::Json
{
struct Value
{
    enum class Type
    {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object,
    };

    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::vector<Value> array;
    std::vector<std::pair<std::string, Value>> object;

    const Value* Get(std::string_view aKey) const;
    std::string_view AsString() const;
    bool IsObject() const { return type == Type::Object; }
    bool IsArray() const { return type == Type::Array; }
};

bool Parse(std::string_view aText, Value& aOut, std::string* aError = nullptr);
}
