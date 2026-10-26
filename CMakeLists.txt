cmake_minimum_required(VERSION 3.24)

project(WootingAnalog VERSION 1.2.6 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

add_subdirectory(deps/RED4ext.SDK EXCLUDE_FROM_ALL)

set(WA_COMPILE_OPTIONS /W4 /permissive- /utf-8 /Zc:__cplusplus /EHsc)
set(WA_DEFINITIONS WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS)

add_library(wa_core STATIC
  src/Analog.cpp
  src/Config.cpp
  src/GameBindings.cpp
  src/InputConfig.cpp
  src/Json.cpp
  src/Log.cpp
  src/WootingSdk.cpp
)
target_include_directories(wa_core PUBLIC src)
target_compile_options(wa_core PRIVATE ${WA_COMPILE_OPTIONS})
target_compile_definitions(wa_core PUBLIC ${WA_DEFINITIONS})
target_link_libraries(wa_core PUBLIC shell32 ole32)

add_library(WootingAnalog SHARED
  src/Core.cpp
  src/EventInjector.cpp
  src/Main.cpp
  src/Natives.cpp
  src/XInputHook.cpp
  src/Version.rc
)
target_compile_options(WootingAnalog PRIVATE ${WA_COMPILE_OPTIONS})
target_link_libraries(WootingAnalog PRIVATE wa_core RED4ext::SDK)
target_link_options(WootingAnalog PRIVATE /DEBUG /OPT:REF /OPT:ICF)

add_executable(WootingAnalogCheck tools/WootingAnalogCheck.cpp)
target_compile_options(WootingAnalogCheck PRIVATE ${WA_COMPILE_OPTIONS})
target_link_libraries(WootingAnalogCheck PRIVATE wa_core)

add_executable(UnitTests tests/UnitTests.cpp)
target_compile_options(UnitTests PRIVATE ${WA_COMPILE_OPTIONS})
target_link_libraries(UnitTests PRIVATE wa_core)

add_executable(HookTests tests/HookTests.cpp src/XInputHook.cpp)
target_compile_options(HookTests PRIVATE ${WA_COMPILE_OPTIONS})
target_link_libraries(HookTests PRIVATE wa_core xinput9_1_0)
