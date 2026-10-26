#include "Natives.hpp"

#include "Core.hpp"
#include "Version.hpp"

#include <RED4ext/RED4ext.hpp>

#include <string>

namespace WA::Natives
{
namespace
{
void SetContext(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, void*, int64_t)
{
    int32_t context = 0;
    RED4ext::GetParameter(aFrame, &context);
    aFrame->code++;
    Core::SetContext(context);
}

void SetOption(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, void*, int64_t)
{
    RED4ext::CString name;
    float value = 0.f;
    RED4ext::GetParameter(aFrame, &name);
    RED4ext::GetParameter(aFrame, &value);
    aFrame->code++;
    Core::SetOption(name.c_str(), value);
}

void GetStatus(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, RED4ext::CString* aOut, int64_t)
{
    aFrame->code++;
    if (aOut)
        *aOut = RED4ext::CString(Core::GetStatus().c_str());
}

void GetDeviceName(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, RED4ext::CString* aOut, int64_t)
{
    aFrame->code++;
    if (aOut)
        *aOut = RED4ext::CString(Core::GetDeviceName().c_str());
}

void GetVersion(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, RED4ext::CString* aOut, int64_t)
{
    aFrame->code++;
    if (aOut)
        *aOut = RED4ext::CString(WA_VERSION_STRING);
}

void IsActive(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
{
    aFrame->code++;
    if (aOut)
        *aOut = Core::IsActive();
}

void TakeAnnouncement(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, RED4ext::CString* aOut, int64_t)
{
    aFrame->code++;
    if (aOut)
        *aOut = RED4ext::CString(Core::TakeAnnouncement().c_str());
}

void Log(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, void*, int64_t)
{
    RED4ext::CString message;
    RED4ext::GetParameter(aFrame, &message);
    aFrame->code++;
    Core::ScriptLog(message.c_str());
}

void IsDebug(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
{
    aFrame->code++;
    if (aOut)
        *aOut = Core::IsDebug();
}

void TakeSnapshotRequest(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
{
    aFrame->code++;
    if (aOut)
        *aOut = Core::TakeSnapshotRequest();
}

void GetValue(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, float* aOut, int64_t)
{
    int32_t index = 0;
    RED4ext::GetParameter(aFrame, &index);
    aFrame->code++;
    if (aOut)
        *aOut = Core::GetValue(index);
}
}

void Register()
{
    auto rtti = RED4ext::CRTTISystem::Get();
    const RED4ext::CBaseFunction::Flags flags = {.isNative = true, .isStatic = true};

    {
        auto func = RED4ext::CGlobalFunction::Create("WootingAnalog_SetContext", "WootingAnalog_SetContext", &SetContext);
        func->flags = flags;
        func->AddParam("Int32", "context");
        rtti->RegisterFunction(func);
    }
    {
        auto func = RED4ext::CGlobalFunction::Create("WootingAnalog_SetOption", "WootingAnalog_SetOption", &SetOption);
        func->flags = flags;
        func->AddParam("String", "name");
        func->AddParam("Float", "value");
        rtti->RegisterFunction(func);
    }
    {
        auto func = RED4ext::CGlobalFunction::Create("WootingAnalog_GetStatus", "WootingAnalog_GetStatus", &GetStatus);
        func->flags = flags;
        func->SetReturnType("String");
        rtti->RegisterFunction(func);
    }
    {
        auto func =
            RED4ext::CGlobalFunction::Create("WootingAnalog_GetDeviceName", "WootingAnalog_GetDeviceName", &GetDeviceName);
        func->flags = flags;
        func->SetReturnType("String");
        rtti->RegisterFunction(func);
    }
    {
        auto func = RED4ext::CGlobalFunction::Create("WootingAnalog_GetVersion", "WootingAnalog_GetVersion", &GetVersion);
        func->flags = flags;
        func->SetReturnType("String");
        rtti->RegisterFunction(func);
    }
    {
        auto func = RED4ext::CGlobalFunction::Create("WootingAnalog_IsActive", "WootingAnalog_IsActive", &IsActive);
        func->flags = flags;
        func->SetReturnType("Bool");
        rtti->RegisterFunction(func);
    }
    {
        auto func = RED4ext::CGlobalFunction::Create("WootingAnalog_TakeAnnouncement", "WootingAnalog_TakeAnnouncement",
                                                     &TakeAnnouncement);
        func->flags = flags;
        func->SetReturnType("String");
        rtti->RegisterFunction(func);
    }
    {
        auto func = RED4ext::CGlobalFunction::Create("WootingAnalog_Log", "WootingAnalog_Log", &Log);
        func->flags = flags;
        func->AddParam("String", "message");
        rtti->RegisterFunction(func);
    }
    {
        auto func = RED4ext::CGlobalFunction::Create("WootingAnalog_IsDebug", "WootingAnalog_IsDebug", &IsDebug);
        func->flags = flags;
        func->SetReturnType("Bool");
        rtti->RegisterFunction(func);
    }
    {
        auto func = RED4ext::CGlobalFunction::Create("WootingAnalog_TakeSnapshotRequest",
                                                     "WootingAnalog_TakeSnapshotRequest", &TakeSnapshotRequest);
        func->flags = flags;
        func->SetReturnType("Bool");
        rtti->RegisterFunction(func);
    }
    {
        auto func = RED4ext::CGlobalFunction::Create("WootingAnalog_GetValue", "WootingAnalog_GetValue", &GetValue);
        func->flags = flags;
        func->AddParam("Int32", "index");
        func->SetReturnType("Float");
        rtti->RegisterFunction(func);
    }
}
}
