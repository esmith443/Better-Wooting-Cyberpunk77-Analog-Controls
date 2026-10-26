#include "Core.hpp"
#include "Log.hpp"
#include "Natives.hpp"
#include "Version.hpp"

#include <RED4ext/RED4ext.hpp>

namespace
{
RED4ext::v1::PluginHandle s_handle = nullptr;
const RED4ext::v1::Sdk* s_sdk = nullptr;

void LogToRed4ext(WA::Log::Level aLevel, const char* aMessage)
{
    if (!s_sdk || !s_sdk->logger)
        return;
    switch (aLevel)
    {
    case WA::Log::Level::Debug:
        s_sdk->logger->Debug(s_handle, aMessage);
        break;
    case WA::Log::Level::Info:
        s_sdk->logger->Info(s_handle, aMessage);
        break;
    case WA::Log::Level::Warn:
        s_sdk->logger->Warn(s_handle, aMessage);
        break;
    case WA::Log::Level::Error:
        s_sdk->logger->Error(s_handle, aMessage);
        break;
    }
}

bool OnRunningUpdate(RED4ext::CGameApplication*)
{
    WA::Core::OnRunningUpdate();
    return false;
}

void PostRegisterTypes()
{
    WA::Natives::Register();
}
}

RED4EXT_C_EXPORT bool RED4EXT_CALL Main(RED4ext::v1::PluginHandle aHandle, RED4ext::v1::EMainReason aReason,
                                        const RED4ext::v1::Sdk* aSdk)
{
    switch (aReason)
    {
    case RED4ext::v1::EMainReason::Load:
    {
        s_handle = aHandle;
        s_sdk = aSdk;
        WA::Log::SetSink(&LogToRed4ext);

        RED4ext::CRTTISystem::Get()->AddPostRegisterCallback(&PostRegisterTypes);

        RED4ext::v1::GameState running{};
        running.OnUpdate = &OnRunningUpdate;
        aSdk->gameStates->Add(aHandle, RED4ext::EGameStateType::Running, &running);

        WA::Core::InitInfo info;
        info.pluginModule = aHandle;
        info.attach = [](void* aTarget, void* aDetour, void** aOriginal) {
            return s_sdk->hooking->Attach(s_handle, aTarget, aDetour, aOriginal);
        };
        WA::Core::Initialize(info);
        break;
    }
    case RED4ext::v1::EMainReason::Unload:
    {
        WA::Core::Shutdown();
        WA::Log::SetSink(nullptr);
        break;
    }
    }
    return true;
}

RED4EXT_C_EXPORT void RED4EXT_CALL Query(RED4ext::v1::PluginInfo* aInfo)
{
    aInfo->name = L"Wooting Analog";
    aInfo->author = L"cp77wooting";
    aInfo->version = RED4EXT_V1_SEMVER(WA_VERSION_MAJOR, WA_VERSION_MINOR, WA_VERSION_PATCH);
    aInfo->runtime = RED4EXT_V1_RUNTIME_VERSION_INDEPENDENT;
    aInfo->sdk = RED4EXT_V1_SDK_VERSION_CURRENT;
}

RED4EXT_C_EXPORT uint32_t RED4EXT_CALL Supports()
{
    return RED4EXT_API_VERSION_1;
}
