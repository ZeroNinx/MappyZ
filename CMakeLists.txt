cmake_minimum_required(VERSION 3.24)

project(MappyZ
    VERSION 0.1.0
    DESCRIPTION "Modern gamepad remapping runtime"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSVC 默认使用系统代码页读取源文件，项目统一使用 UTF-8 编码
if(MSVC)
    add_compile_options(/utf-8)
endif()

# ── 第三方依赖 ──

add_subdirectory(third_party/ZeroStyle)

find_package(Qt6 REQUIRED COMPONENTS Quick)

qt_standard_project_setup(REQUIRES 6.5)

# 优先使用 vcpkg 安装的 nlohmann_json，找不到则通过 FetchContent 拉取
find_package(nlohmann_json CONFIG QUIET)

if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# SDL3 输入后端（可选）
option(MAPPYZ_ENABLE_SDL3_INPUT "构建 SDL3 输入后端" ON)

set(MAPPYZ_HAS_SDL3_INPUT OFF)

if(MAPPYZ_ENABLE_SDL3_INPUT)
    find_package(SDL3 CONFIG QUIET)

    if(SDL3_FOUND)
        set(MAPPYZ_HAS_SDL3_INPUT ON)
        message(STATUS "SDL3 已找到，SDL 输入后端已启用")
    else()
        message(WARNING "MAPPYZ_ENABLE_SDL3_INPUT=ON 但未找到 SDL3，跳过 SDL 输入后端")
    endif()
endif()

# ── Core 模块 ──

add_library(MappyZCore STATIC
    source/Core/MappingEngine.cpp
)

target_include_directories(MappyZCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/source
)

target_link_libraries(MappyZCore
    PUBLIC
        Zero::ZeroStyle
)

if(MSVC)
    target_compile_options(MappyZCore PRIVATE /W4)
else()
    target_compile_options(MappyZCore PRIVATE -Wall -Wextra -Wconversion -Wshadow)
endif()

# ── 输入后端模块 ──

set(MAPPYZ_INPUT_BACKEND_SOURCES
    source/Backends/Input/FakeInputBackend.cpp
)

if(MAPPYZ_HAS_SDL3_INPUT)
    list(APPEND MAPPYZ_INPUT_BACKEND_SOURCES
        source/Backends/Input/SdlInputBackend.cpp
    )
endif()

add_library(MappyZInputBackends STATIC
    ${MAPPYZ_INPUT_BACKEND_SOURCES}
)

target_link_libraries(MappyZInputBackends
    PUBLIC
        MappyZCore
)

if(MAPPYZ_HAS_SDL3_INPUT)
    target_link_libraries(MappyZInputBackends PRIVATE SDL3::SDL3)
    target_compile_definitions(MappyZInputBackends PUBLIC MAPPYZ_HAS_SDL3_INPUT)
endif()

if(MSVC)
    target_compile_options(MappyZInputBackends PRIVATE /W4)
else()
    target_compile_options(MappyZInputBackends PRIVATE -Wall -Wextra -Wconversion -Wshadow)
endif()

# ── 输出后端模块 ──

# Windows SendInput 输出后端（可选，仅 Windows 平台）
option(MAPPYZ_ENABLE_WINDOWS_SENDINPUT_OUTPUT "Build Windows SendInput output backend" ON)

set(MAPPYZ_HAS_WINDOWS_SENDINPUT_OUTPUT OFF)

if(MAPPYZ_ENABLE_WINDOWS_SENDINPUT_OUTPUT AND WIN32)
    set(MAPPYZ_HAS_WINDOWS_SENDINPUT_OUTPUT ON)
    message(STATUS "Windows SendInput 输出后端已启用")
elseif(MAPPYZ_ENABLE_WINDOWS_SENDINPUT_OUTPUT AND NOT WIN32)
    message(WARNING "MAPPYZ_ENABLE_WINDOWS_SENDINPUT_OUTPUT=ON 但非 Windows 平台，跳过")
endif()

set(MAPPYZ_OUTPUT_BACKEND_SOURCES
    source/Backends/Output/NullOutputBackend.cpp
)

if(MAPPYZ_HAS_WINDOWS_SENDINPUT_OUTPUT)
    list(APPEND MAPPYZ_OUTPUT_BACKEND_SOURCES
        source/Backends/Output/WindowsSendInputBackend.cpp
    )
endif()

add_library(MappyZOutputBackends STATIC
    ${MAPPYZ_OUTPUT_BACKEND_SOURCES}
)

target_link_libraries(MappyZOutputBackends
    PUBLIC
        MappyZCore
)

if(MAPPYZ_HAS_WINDOWS_SENDINPUT_OUTPUT)
    target_compile_definitions(MappyZOutputBackends PUBLIC MAPPYZ_HAS_WINDOWS_SENDINPUT_OUTPUT)
endif()

if(MSVC)
    target_compile_options(MappyZOutputBackends PRIVATE /W4)
else()
    target_compile_options(MappyZOutputBackends PRIVATE -Wall -Wextra -Wconversion -Wshadow)
endif()

# ── Runtime 模块 ──

add_library(MappyZRuntime STATIC
    source/Runtime/DeviceManager.cpp
    source/Runtime/InputRuntime.cpp
    source/Runtime/ActionDispatcher.cpp
    source/Runtime/MappingSession.cpp
    source/Runtime/ProfileManager.cpp
)

target_link_libraries(MappyZRuntime
    PUBLIC
        MappyZCore
        MappyZInputBackends
        MappyZOutputBackends
    PRIVATE
        nlohmann_json::nlohmann_json
)

if(MSVC)
    target_compile_options(MappyZRuntime PRIVATE /W4)
else()
    target_compile_options(MappyZRuntime PRIVATE -Wall -Wextra -Wconversion -Wshadow)
endif()

# ── 主应用 ──

qt_add_executable(MappyZ
    source/App/Main.cpp
)

set_target_properties(MappyZ
    PROPERTIES
        WIN32_EXECUTABLE TRUE
)

set_source_files_properties(ui/Main.qml
    PROPERTIES
        QT_RESOURCE_ALIAS Main.qml
)

qt_add_qml_module(MappyZ
    URI MappyZUI
    VERSION 1.0
    QML_FILES
        ui/Main.qml
)

target_compile_features(MappyZ PRIVATE cxx_std_20)

target_link_libraries(MappyZ
    PRIVATE
        MappyZCore
        Qt6::Quick
)

if(WIN32)
    get_filename_component(MappyZDependencyRoot "${Qt6_DIR}/../.." ABSOLUTE)
    set(MappyZQtRuntimeRoot "${MappyZDependencyRoot}/$<IF:$<CONFIG:Debug>,debug/Qt6,Qt6>")

    add_custom_command(TARGET MappyZ POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${MappyZDependencyRoot}/$<IF:$<CONFIG:Debug>,debug/bin,bin>"
            "$<TARGET_FILE_DIR:MappyZ>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:MappyZ>
            $<TARGET_FILE_DIR:MappyZ>
        COMMAND ${CMAKE_COMMAND} -E rm -rf
            "$<TARGET_FILE_DIR:MappyZ>/platforms"
            "$<TARGET_FILE_DIR:MappyZ>/Qt6/qml"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${MappyZQtRuntimeRoot}/plugins"
            "$<TARGET_FILE_DIR:MappyZ>"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${MappyZQtRuntimeRoot}/qml"
            "$<TARGET_FILE_DIR:MappyZ>/Qt6/qml"
        COMMAND_EXPAND_LISTS
    )
endif()

if(MSVC)
    target_compile_options(MappyZ PRIVATE /W4)
else()
    target_compile_options(MappyZ PRIVATE -Wall -Wextra -Wconversion -Wshadow)
endif()

# ── 测试 ──

include(CTest)

if(BUILD_TESTING)
    # 优先使用 vcpkg 安装的 Catch2，找不到则通过 FetchContent 拉取
    find_package(Catch2 3 CONFIG QUIET)

    if(NOT Catch2_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG        v3.7.1
        )
        FetchContent_MakeAvailable(Catch2)
        list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    endif()

    include(Catch)

    add_executable(MappyZCoreTests
        tests/Core/CoreTypesTests.cpp
        tests/Core/MappingProfileTests.cpp
        tests/Core/MappingEngineTests.cpp
    )

    target_link_libraries(MappyZCoreTests
        PRIVATE
            MappyZCore
            Catch2::Catch2WithMain
    )

    if(MSVC)
        target_compile_options(MappyZCoreTests PRIVATE /W4)
    else()
        target_compile_options(MappyZCoreTests PRIVATE -Wall -Wextra -Wconversion -Wshadow)
    endif()

    catch_discover_tests(MappyZCoreTests)

    # ── 输入后端测试 ──

    set(MAPPYZ_INPUT_BACKEND_TEST_SOURCES
        tests/Backends/Input/FakeInputBackendTests.cpp
    )

    if(MAPPYZ_HAS_SDL3_INPUT)
        list(APPEND MAPPYZ_INPUT_BACKEND_TEST_SOURCES
            tests/Backends/Input/SdlInputHelperTests.cpp
            tests/Backends/Input/SdlInputBackendTests.cpp
        )
    endif()

    add_executable(MappyZInputBackendTests
        ${MAPPYZ_INPUT_BACKEND_TEST_SOURCES}
    )

    target_link_libraries(MappyZInputBackendTests
        PRIVATE
            MappyZInputBackends
            Catch2::Catch2WithMain
    )

    if(MSVC)
        target_compile_options(MappyZInputBackendTests PRIVATE /W4)
    else()
        target_compile_options(MappyZInputBackendTests PRIVATE -Wall -Wextra -Wconversion -Wshadow)
    endif()

    catch_discover_tests(MappyZInputBackendTests)

    # ── 输出后端测试 ──

    set(MAPPYZ_OUTPUT_BACKEND_TEST_SOURCES
        tests/Backends/Output/NullOutputBackendTests.cpp
    )

    if(MAPPYZ_HAS_WINDOWS_SENDINPUT_OUTPUT)
        list(APPEND MAPPYZ_OUTPUT_BACKEND_TEST_SOURCES
            tests/Backends/Output/WindowsSendInputHelperTests.cpp
            tests/Backends/Output/WindowsSendInputBackendTests.cpp
        )
    endif()

    add_executable(MappyZOutputBackendTests
        ${MAPPYZ_OUTPUT_BACKEND_TEST_SOURCES}
    )

    target_link_libraries(MappyZOutputBackendTests
        PRIVATE
            MappyZOutputBackends
            Catch2::Catch2WithMain
    )

    if(MSVC)
        target_compile_options(MappyZOutputBackendTests PRIVATE /W4)
    else()
        target_compile_options(MappyZOutputBackendTests PRIVATE -Wall -Wextra -Wconversion -Wshadow)
    endif()

    catch_discover_tests(MappyZOutputBackendTests)

    # ── Runtime 测试 ──

    add_executable(MappyZRuntimeTests
        tests/Runtime/DeviceManagerTests.cpp
        tests/Runtime/InputRuntimeTests.cpp
        tests/Runtime/ActionDispatcherTests.cpp
        tests/Runtime/MappingSessionTests.cpp
        tests/Runtime/ProfileManagerTests.cpp
    )

    target_link_libraries(MappyZRuntimeTests
        PRIVATE
            MappyZRuntime
            MappyZInputBackends
            Catch2::Catch2WithMain
    )

    if(MSVC)
        target_compile_options(MappyZRuntimeTests PRIVATE /W4)
    else()
        target_compile_options(MappyZRuntimeTests PRIVATE -Wall -Wextra -Wconversion -Wshadow)
    endif()

    catch_discover_tests(MappyZRuntimeTests)
endif()
