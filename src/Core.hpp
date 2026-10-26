#pragma once

#include <functional>
#include <string>
#include <string_view>

namespace WA::Core
{
struct InitInfo
{
    void* pluginModule = nullptr;
    std::function<bool(void*, void*, void**)> attach;
};

bool Initialize(const InitInfo& aInfo);
void Shutdown();

void OnRunningUpdate();

void SetContext(int aContext);
void SetOption(std::string_view aName, float aValue);
std::string GetStatus();
bool IsActive();
bool IsDebug();
std::string GetDeviceName();
std::string TakeAnnouncement();
void ScriptLog(std::string_view aMessage);
bool TakeSnapshotRequest();
float GetValue(int aIndex);
}
