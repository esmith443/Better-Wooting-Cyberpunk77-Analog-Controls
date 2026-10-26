#include "Log.hpp"

#include <atomic>
#include <cstdio>

namespace WA::Log
{
namespace
{
std::atomic<Sink> s_sink{nullptr};
std::atomic<Level> s_minLevel{Level::Info};

const char* Prefix(Level aLevel)
{
    switch (aLevel)
    {
    case Level::Debug:
        return "debug";
    case Level::Info:
        return "info";
    case Level::Warn:
        return "warn";
    case Level::Error:
        return "error";
    }
    return "?";
}
}

void SetSink(Sink aSink)
{
    s_sink = aSink;
}

void SetMinLevel(Level aLevel)
{
    s_minLevel = aLevel;
}

void Write(Level aLevel, const char* aFormat, ...)
{
    if (aLevel < s_minLevel.load())
        return;

    char buffer[2048];
    va_list args;
    va_start(args, aFormat);
    std::vsnprintf(buffer, sizeof(buffer), aFormat, args);
    va_end(args);

    if (auto sink = s_sink.load())
        sink(aLevel, buffer);
    else
        std::printf("[%s] %s\n", Prefix(aLevel), buffer);
}
}
