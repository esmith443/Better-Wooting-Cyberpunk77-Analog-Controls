#include "Analog.hpp"
#include "Config.hpp"
#include "GameBindings.hpp"
#include "InputConfig.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
int s_failures = 0;
int s_checks = 0;

#define CHECK(cond)                                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        ++s_checks;                                                                                                    \
        if (!(cond))                                                                                                   \
        {                                                                                                              \
            ++s_failures;                                                                                              \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                                \
        }                                                                                                              \
    } while (0)

bool Near(float a, float b, float eps = 1e-4f)
{
    return std::fabs(a - b) <= eps;
}

size_t Count(const std::string& aText, const std::string& aNeedle)
{
    size_t count = 0;
    for (size_t pos = aText.find(aNeedle); pos != std::string::npos; pos = aText.find(aNeedle, pos + 1))
        ++count;
    return count;
}

std::string ReadAll(const std::filesystem::path& aPath)
{
    std::ifstream file(aPath, std::ios::binary);
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

void TestCurve()
{
    WA::Curve curve{0.1f, 0.9f, 1.f};
    CHECK(curve.Apply(0.f) == 0.f);
    CHECK(curve.Apply(0.1f) == 0.f);
    CHECK(Near(curve.Apply(0.5f), 0.5f));
    CHECK(curve.Apply(0.9f) == 1.f);
    CHECK(curve.Apply(1.f) == 1.f);
    CHECK(curve.Apply(-3.f) == 0.f);
    CHECK(curve.Apply(std::nanf("")) == 0.f);

    WA::Curve squared{0.f, 1.f, 2.f};
    CHECK(Near(squared.Apply(0.5f), 0.25f));

    WA::Curve broken{0.9f, 0.1f, -1.f};
    broken.Sanitize();
    CHECK(broken.full > broken.deadzone);
    CHECK(broken.gamma >= 0.2f);
}

void TestCompute()
{
    WA::Tuning tuning;
    tuning.move = {0.f, 1.f, 1.f};
    tuning.steer = {0.f, 1.f, 1.f};
    tuning.throttle = {0.f, 1.f, 1.f};
    tuning.brake = {0.f, 1.f, 1.f};
    tuning.compensateStickDeadzone = false;

    WA::KeyValues keys;
    keys.forward = 0.5f;
    keys.right = 0.25f;
    keys.accelerate = 0.5f;
    keys.brake = 0.75f;
    keys.steerRight = 0.25f;

    auto walk = WA::Compute(keys, WA::Context::OnFoot, tuning, true);
    CHECK(Near(walk.ly, 0.5f));
    CHECK(Near(walk.lx, 0.25f));
    CHECK(walk.lt == 0.f && walk.rt == 0.f);

    auto unknown = WA::Compute(keys, WA::Context::Unknown, tuning, true);
    CHECK(Near(unknown.ly, 0.5f));
    CHECK(unknown.rt == 0.f);

    auto drive = WA::Compute(keys, WA::Context::Driving, tuning, true);
    CHECK(Near(drive.rt, 0.5f));
    CHECK(Near(drive.lt, 0.75f));
    CHECK(Near(drive.lx, 0.25f));
    CHECK(drive.ly == 0.f);

    auto driveDigital = WA::Compute(keys, WA::Context::Driving, tuning, false);
    CHECK(driveDigital.rt == 0.f && driveDigital.lt == 0.f);
    CHECK(Near(driveDigital.lx, 0.25f));

    auto tank = WA::Compute(keys, WA::Context::Tank, tuning, true);
    CHECK(Near(tank.ly, -0.25f));
    CHECK(tank.rt == 0.f && tank.lt == 0.f);

    auto neutral = WA::Compute(keys, WA::Context::Neutral, tuning, true);
    CHECK(neutral.IsZero());

    WA::KeyValues diagonal;
    diagonal.forward = 1.f;
    diagonal.right = 1.f;
    auto d = WA::Compute(diagonal, WA::Context::OnFoot, tuning, false);
    CHECK(Near(std::sqrt(d.lx * d.lx + d.ly * d.ly), 1.f));

    WA::KeyValues opposite;
    opposite.forward = 1.f;
    opposite.back = 1.f;
    CHECK(WA::Compute(opposite, WA::Context::OnFoot, tuning, false).ly == 0.f);

    tuning.compensateStickDeadzone = true;
    tuning.stickInner = 0.05f;
    tuning.stickOuter = 0.9f;
    WA::KeyValues half;
    half.forward = 0.5f;
    auto compensated = WA::Compute(half, WA::Context::OnFoot, tuning, false);
    const float gameSees = (compensated.ly - tuning.stickInner) / (tuning.stickOuter - tuning.stickInner);
    CHECK(Near(gameSees, 0.5f));
    CHECK(WA::ToRawStickMagnitude(0.f, 0.05f, 0.9f) == 0.f);
    CHECK(WA::ToRawStickMagnitude(1.f, 0.05f, 0.9f) == 1.f);

    WA::Tuning shipped;
    WA::KeyValues full;
    full.forward = 1.f;
    CHECK(WA::Compute(full, WA::Context::OnFoot, shipped, false).ly == 1.f);
    WA::KeyValues firm;
    firm.forward = 0.8f;
    CHECK(WA::Compute(firm, WA::Context::OnFoot, shipped, false).ly > 0.9f);
    shipped.compensateStickDeadzone = true;
    CHECK(WA::Compute(full, WA::Context::OnFoot, shipped, false).ly == 1.f);
}

void TestKeys()
{
    CHECK(WA::KeyFromName("IK_W") == 87);
    CHECK(WA::KeyFromName("ik_w") == 87);
    CHECK(WA::KeyFromName("IK_Up") == 38);
    CHECK(WA::KeyFromName("IK_LShift") == 160);
    CHECK(WA::KeyFromName("IK_Pad_A_CROSS") == 0);
    CHECK(WA::KeyFromName("nonsense") == 0);
    CHECK(WA::NameFromKey(65) == "IK_A");
    CHECK(WA::ParseKeySpec("W") == 87);
    CHECK(WA::ParseKeySpec(" IK_S ") == 83);
    CHECK(WA::ParseKeySpec("Up") == 38);
    CHECK(WA::ParseKeySpec("87") == 87);
    CHECK(WA::ParseKeySpec("0x44") == 68);
    CHECK(WA::ParseKeySpec("8") == 56);
    CHECK(WA::ParseKeySpec("") == 0);
}

void TestBindings()
{
    const char* json = R"({
        "version": 152,
        "data": [
            {"group_name": "/controls/fppcameramouse", "options": [{"name": "forward", "type": "name", "value": "IK_Z"}]},
            {"group_name": "/key_bindings/SettingsLocomotion", "options": [
                {"name": "forward", "type": "name", "value": "IK_Up", "default_value": "IK_W"},
                {"name": "back", "type": "name", "value": "IK_S"},
                {"name": "left", "type": "name", "value": "IK_A"},
                {"name": "right", "type": "name", "value": "IK_D"},
                {"name": "jump", "type": "name", "value": "IK_Space"}
            ]},
            {"group_name": "/key_bindings/SettingsVehicle", "options": [
                {"name": "vehicleAccelerate", "type": "name", "value": "IK_I"},
                {"name": "vehicleDecelrate", "type": "name", "value": "IK_K"},
                {"name": "vehicleSteerLeft", "type": "name", "value": "IK_J"},
                {"name": "vehicleSteerRight", "type": "name", "value": "IK_L"}
            ]}
        ]
    })";
    WA::Bindings bindings = WA::Bindings::Defaults();
    std::string error;
    CHECK(WA::ParseGameBindings(json, bindings, &error));
    CHECK(bindings.forward.vk == 38);
    CHECK(bindings.back.vk == 'S');
    CHECK(bindings.accelerate.vk == 'I');
    CHECK(bindings.brake.vk == 'K');
    CHECK(bindings.steerLeft.vk == 'J');
    CHECK(bindings.steerRight.vk == 'L');

    WA::Bindings untouched = WA::Bindings::Defaults();
    CHECK(!WA::ParseGameBindings("{\"data\": []}", untouched, &error));
    CHECK(untouched.forward.vk == 'W');
    CHECK(!WA::ParseGameBindings("{ not json", untouched, &error));

    const auto path = WA::DefaultUserSettingsPath();
    if (std::filesystem::exists(path))
    {
        WA::Bindings real = WA::Bindings::Defaults();
        const bool ok = WA::LoadGameBindings(path, real, &error);
        std::printf("  real UserSettings.json: %s\n", ok ? real.Describe().c_str() : error.c_str());
        CHECK(ok);
        CHECK(real.forward.vk != 0 && real.accelerate.vk != 0);
    }
}

const char* kSampleMappings = R"(<?xml version="1.0"?>
<bindings>
	<mapping name="LeftX_Axis" type="Axis" >
        <button id="IK_Pad_LeftAxisX" />
        <button id="IK_A" val="-1.0" overridableUI="left"/>
        <button id="IK_D" val="1.0" overridableUI="right"/>
    </mapping>
    <mapping name="LeftY_Axis" type="Axis" >
        <button id="IK_Pad_LeftAxisY" />
        <!-- <button id="IK_W" val="1.0" overridableUI="forward"/> commented out stays -->
        <button id="IK_W" val="1.0" overridableUI="forward"/>
        <button id="IK_S" val="-1.0" overridableUI="back"/>
    </mapping>
	<mapping name="CrawlingLeftY_Axis" type="Axis" >
        <button id="IK_Pad_LeftAxisY" />
        <button id="IK_W" val="1.0" overridableUI="forward" />
    </mapping>
    <mapping name="Acceleration_Axis" type="Axis" >
        <button id="IK_Pad_RightTrigger" />
        <button id="IK_W" val="1.0" overridableUI="vehicleAccelerate"/>
        <button id="IK_Up" val="1.0" overridableUI="vehicleAccelerate2"/>
    </mapping>
    <mapping name="Deceleration_Axis" type="Axis" >
        <button id="IK_Pad_LeftTrigger" />
        <button id="IK_S" val="1.0" overridableUI="vehicleDecelrate"/>
	</mapping>
	<mapping name="LeftX_Axis_Vehicle" type="Axis" >
		<button id="IK_Pad_LeftAxisX" />
		<button id="IK_Right" val="1.0" overridableUI="vehicleSteerRight2"/>
		<button id="IK_D" val="1.0" overridableUI="vehicleSteerRight"/>
		<button id="IK_A" val="-1.0" overridableUI="vehicleSteerLeft"/>
	</mapping>
</bindings>
)";

const char* kSampleContexts = R"(<?xml version="1.0"?>
<bindings>
	<context name="Other" >
		<action name="MoveX" map="LeftX_Axis" />
	</context>
	<context name="MovementActions" >
		<action name="MoveX" 				map="LeftX_Axis" />
		<action name="MoveY" 				map="LeftY_Axis" />
		<action name="Forward"				map="Forward_Button" />
		<action name="Crawl" 				map="CrawlingLeftY_Axis" />
	</context>
</bindings>
)";

void TestInputXmlSample()
{
    WA::InputXmlOptions options;
    options.throttle = true;
    const auto result = WA::BuildInputXml(kSampleContexts, kSampleMappings, options, "test");
    for (const auto& note : result.notes)
        std::printf("  note: %s\n", note.c_str());
    CHECK(result.ok);
    CHECK(result.movement && result.steering && result.throttle);
    const std::string& xml = result.xml;

    CHECK(xml.find("<mapping name=\"WootingAnalog_MoveX\"") != std::string::npos);
    CHECK(xml.find("<mapping name=\"WootingAnalog_MoveY\"") != std::string::npos);
    CHECK(xml.find("<mapping name=\"WootingAnalog_Crawl\"") != std::string::npos);
    CHECK(xml.find("map=\"WootingAnalog_MoveX\"") != std::string::npos);
    CHECK(xml.find("map=\"WootingAnalog_MoveY\"") != std::string::npos);
    CHECK(xml.find("map=\"WootingAnalog_Crawl\"") != std::string::npos);
    CHECK(xml.find("map=\"Forward_Button\"") != std::string::npos);
    CHECK(xml.find("<context name=\"Other\"") == std::string::npos);
    CHECK(Count(xml, "overridableUI=\"left\"") == 0);
    CHECK(Count(xml, "overridableUI=\"forward\"") == 1);
    CHECK(Count(xml, "overridableUI=\"vehicleAccelerate\"") == 0);
    CHECK(Count(xml, "overridableUI=\"vehicleAccelerate2\"") == 1);
    CHECK(Count(xml, "overridableUI=\"vehicleDecelrate\"") == 0);
    CHECK(Count(xml, "overridableUI=\"vehicleSteerRight2\"") == 1);
    CHECK(Count(xml, "overridableUI=\"vehicleSteerLeft\"") == 0);
    CHECK(Count(xml, "<mapping ") == Count(xml, "</mapping>"));
    CHECK(Count(xml, "<context ") == Count(xml, "</context>"));
    CHECK(xml.find("<mapping name=\"LeftX_Axis\"") == std::string::npos);
    CHECK(xml.find("<mapping name=\"LeftY_Axis\"") == std::string::npos);

    WA::InputXmlOptions noThrottle;
    const auto digital = WA::BuildInputXml(kSampleContexts, kSampleMappings, noThrottle);
    CHECK(digital.ok && !digital.throttle);
    CHECK(digital.xml.find("Acceleration_Axis") == std::string::npos);

    const auto broken = WA::BuildInputXml("<bindings></bindings>", "<bindings></bindings>", options);
    CHECK(!broken.ok);
}

void TestInputXmlGame(const std::filesystem::path& aRoot)
{
    const auto contexts = ReadAll(aRoot / "r6" / "config" / "inputContexts.xml");
    const auto mappings = ReadAll(aRoot / "r6" / "config" / "inputUserMappings.xml");
    if (contexts.empty() || mappings.empty())
    {
        std::printf("  game input configs not found under %s, skipped\n", aRoot.string().c_str());
        return;
    }
    WA::InputXmlOptions options;
    options.throttle = true;
    const auto result = WA::BuildInputXml(contexts, mappings, options, "test against the installed game");
    for (const auto& note : result.notes)
        std::printf("  game: %s\n", note.c_str());
    CHECK(result.ok && result.movement && result.steering && result.throttle);
    CHECK(Count(result.xml, "<mapping ") == Count(result.xml, "</mapping>"));
    CHECK(Count(result.xml, "<context ") == 1);
    CHECK(result.xml.find("IK_Pad_LeftAxisX") != std::string::npos);
    CHECK(result.xml.find("overridableUI=\"forward\"") == std::string::npos);

    const auto out = std::filesystem::temp_directory_path() / "WootingAnalog.test.inputs.xml";
    std::ofstream(out, std::ios::binary) << result.xml;
    std::printf("  generated document written to %s\n", out.string().c_str());
}

void TestConfig()
{
    const auto path = std::filesystem::temp_directory_path() / "WootingAnalog.test.ini";
    WA::Config defaults;
    CHECK(defaults.WriteDefault(path));

    WA::Config loaded;
    CHECK(loaded.Load(path));
    CHECK(loaded.enabled);
    CHECK(loaded.backend == WA::Backend::XInput);
    CHECK(loaded.analogThrottle == WA::Toggle::Auto);
    CHECK(loaded.keepKeyboardPrompts);
    CHECK(loaded.injectAfterUi);
    CHECK(!loaded.logRawInput);
    CHECK(Near(loaded.tuning.move.deadzone, defaults.tuning.move.deadzone));
    CHECK(Near(loaded.tuning.steer.gamma, defaults.tuning.steer.gamma));

    {
        std::ofstream out(path, std::ios::trunc);
        out << "[General]\nBackend = Events ; comment\nAnalogThrottle=off\nKeepKeyboardPrompts = false\n[Keys]\nForward = Up\n"
               "[Steering]\nCurve = 2.5\n[Advanced]\nPollRateHz = 99999\nEventTiming = BeforeUI\nLogRawInput = true\n";
    }
    WA::Config custom;
    CHECK(custom.Load(path));
    CHECK(custom.backend == WA::Backend::Events);
    CHECK(!custom.keepKeyboardPrompts);
    CHECK(!custom.injectAfterUi);
    CHECK(custom.logRawInput);
    CHECK(custom.analogThrottle == WA::Toggle::Off);
    CHECK(custom.keyForward == "Up");
    CHECK(Near(custom.tuning.steer.gamma, 2.5f));
    CHECK(custom.pollHz == 2000);
    std::filesystem::remove(path);

    const auto userIni = std::filesystem::temp_directory_path() / "WootingAnalog.test.user.ini";
    {
        std::ofstream out(userIni, std::ios::trunc);
        out << "[Other.Mod]\nenabled = true\n\n[WootingAnalogSettings]\nenabled = false\nkeepKeyboardPrompts = false\n"
               "inputMethod = DirectEvents\ndebugMode = true\nmovementFullPress = 0.650000\nsteeringCurve = 2.000000\n"
               "debugMeterSize = 0.450000\n";
    }
    WA::Config fromMenu;
    const auto applied = fromMenu.ApplyModSettings(userIni);
    CHECK(applied.size() == 7);
    CHECK(Near(fromMenu.meterScale, 0.45f));
    CHECK(!fromMenu.enabled);
    CHECK(!fromMenu.keepKeyboardPrompts);
    CHECK(fromMenu.backend == WA::Backend::Events);
    CHECK(fromMenu.debugMode);
    CHECK(Near(fromMenu.tuning.move.full, 0.65f));
    CHECK(Near(fromMenu.tuning.steer.gamma, 2.f));
    WA::Config untouchedByOtherMods;
    std::filesystem::remove(userIni);
    CHECK(untouchedByOtherMods.ApplyModSettings(userIni).empty());
    CHECK(untouchedByOtherMods.enabled);

    const auto statePath = std::filesystem::temp_directory_path() / "WootingAnalog.test.state.ini";
    {
        WA::StateFile state(statePath);
        state.Set("ScriptsSeen", "1");
        CHECK(state.Save());
    }
    WA::StateFile reread(statePath);
    CHECK(reread.Get("ScriptsSeen") == "1");
    CHECK(reread.Get("Missing", "x") == "x");
    std::filesystem::remove(statePath);
}
}

int main(int argc, char** argv)
{
    std::printf("curve\n");
    TestCurve();
    std::printf("compute\n");
    TestCompute();
    std::printf("keys\n");
    TestKeys();
    std::printf("bindings\n");
    TestBindings();
    std::printf("input xml (sample)\n");
    TestInputXmlSample();
    if (argc > 1)
    {
        std::printf("input xml (game)\n");
        TestInputXmlGame(argv[1]);
    }
    std::printf("config\n");
    TestConfig();

    std::printf("\n%d checks, %d failed\n", s_checks, s_failures);
    return s_failures == 0 ? 0 : 1;
}
