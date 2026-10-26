#pragma once

#include <cstdarg>

namespace WA::Log
{
enum class Level
{
    Debug,
    Info,
    Warn,
    Error,
};

using Sink = void (*)(Level aLevel, const char* aMessage);

void SetSink(Sink aSink);
void SetMinLevel(Level aLevel);

void Write(Level aLevel, const char* aFormat, ...);

#define WA_LOG_DEBUG(...) ::WA::Log::Write(::WA::Log::Level::Debug, __VA_ARGS__)
#define WA_LOG_INFO(...) ::WA::Log::Write(::WA::Log::Level::Info, __VA_ARGS__)
#define WA_LOG_WARN(...) ::WA::Log::Write(::WA::Log::Level::Warn, __VA_ARGS__)
#define WA_LOG_ERROR(...) ::WA::Log::Write(::WA::Log::Level::Error, __VA_ARGS__)
}
