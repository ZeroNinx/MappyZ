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

# 启用 ZeroStyle 短宏（NODISCARD 等），项目代码统一使用短名。
add_compile_definitions(ZERO_ENABLE_SHORT_MACROS)

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
    source/Runtime/BackendEventQueue.cpp
    source/Runtime/RuntimeEventPump.cpp
    source/Runtime/RuntimeHost.cpp
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

# ── App 模块 ──

add_library(MappyZApp STATIC
    source/App/ApplicationBootstrap.cpp
)

target_link_libraries(MappyZApp
    PUBLIC
        MappyZRuntime
        MappyZInputBackends
        MappyZOutputBackends
)

if(MSVC)
    target_compile_options(MappyZApp PRIVATE /W4)
else()
    target_compile_options(MappyZApp PRIVATE -Wall -Wextra -Wconversion -Wshadow)
endif()

# ── UI Bridge 模块 ──

add_library(MappyZUIBridge STATIC
    source/UI/Bridge/ActionCatalogModel.cpp
    source/UI/Bridge/AppController.cpp
    source/UI/Bridge/DeviceModel.cpp
    source/UI/Bridge/InputCaptureModel.cpp
    source/UI/Bridge/InputStateModel.cpp
    source/UI/Bridge/LogModel.cpp
    source/UI/Bridge/MappingRuleModel.cpp
)

target_link_libraries(MappyZUIBridge
    PUBLIC
        Qt6::Core
        MappyZApp
)

if(MSVC)
    target_compile_options(MappyZUIBridge PRIVATE /W4)
else()
    target_compile_options(MappyZUIBridge PRIVATE -Wall -Wextra -Wconversion -Wshadow)
endif()

# ── QML 模块 ──
# 将 QML 资源提取为独立 STATIC 库，供主应用和 smoke test 共同链接

qt_add_library(MappyZQml STATIC)

set_source_files_properties(ui/Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)
set_source_files_properties(ui/theme/Theme.qml PROPERTIES QT_RESOURCE_ALIAS Theme.qml)
set_source_files_properties(ui/common/Panel.qml PROPERTIES QT_RESOURCE_ALIAS Panel.qml)
set_source_files_properties(ui/common/ActionButton.qml PROPERTIES QT_RESOURCE_ALIAS ActionButton.qml)
set_source_files_properties(ui/common/Tag.qml PROPERTIES QT_RESOURCE_ALIAS Tag.qml)
set_source_files_properties(ui/common/FieldLabel.qml PROPERTIES QT_RESOURCE_ALIAS FieldLabel.qml)
set_source_files_properties(ui/common/ValueText.qml PROPERTIES QT_RESOURCE_ALIAS ValueText.qml)
set_source_files_properties(ui/gamepad/InputControlState.qml PROPERTIES QT_RESOURCE_ALIAS InputControlState.qml)
set_source_files_properties(ui/gamepad/ControlDot.qml PROPERTIES QT_RESOURCE_ALIAS ControlDot.qml)
set_source_files_properties(ui/panels/TopBar.qml PROPERTIES QT_RESOURCE_ALIAS TopBar.qml)
set_source_files_properties(ui/panels/DevicesPanel.qml PROPERTIES QT_RESOURCE_ALIAS DevicesPanel.qml)
set_source_files_properties(ui/gamepad/GamepadView.qml PROPERTIES QT_RESOURCE_ALIAS GamepadView.qml)
set_source_files_properties(ui/panels/BindingEditor.qml PROPERTIES QT_RESOURCE_ALIAS BindingEditor.qml)
set_source_files_properties(ui/panels/EventLogPanel.qml PROPERTIES QT_RESOURCE_ALIAS EventLogPanel.qml)
set_source_files_properties(ui/panels/StatusBar.qml PROPERTIES QT_RESOURCE_ALIAS StatusBar.qml)
set_source_files_properties(ui/picker/PickerKey.qml PROPERTIES QT_RESOURCE_ALIAS PickerKey.qml)
set_source_files_properties(ui/picker/PickerTabs.qml PROPERTIES QT_RESOURCE_ALIAS PickerTabs.qml)
set_source_files_properties(ui/picker/KeyboardPicker.qml PROPERTIES QT_RESOURCE_ALIAS KeyboardPicker.qml)
set_source_files_properties(ui/picker/MousePicker.qml PROPERTIES QT_RESOURCE_ALIAS MousePicker.qml)
set_source_files_properties(ui/picker/DInputPicker.qml PROPERTIES QT_RESOURCE_ALIAS DInputPicker.qml)
set_source_files_properties(ui/picker/MappingPickerDialog.qml PROPERTIES QT_RESOURCE_ALIAS MappingPickerDialog.qml)
set_source_files_properties(ui/common/InlineMessage.qml PROPERTIES QT_RESOURCE_ALIAS InlineMessage.qml)

qt_add_qml_module(MappyZQml
    URI MappyZUI
    VERSION 1.0
    QML_FILES
        ui/Main.qml
        ui/theme/Theme.qml
        ui/common/Panel.qml
        ui/common/ActionButton.qml
        ui/common/Tag.qml
        ui/common/FieldLabel.qml
        ui/common/ValueText.qml
        ui/gamepad/InputControlState.qml
        ui/common/InlineMessage.qml
        ui/gamepad/ControlDot.qml
        ui/panels/TopBar.qml
        ui/panels/DevicesPanel.qml
        ui/gamepad/GamepadView.qml
        ui/panels/BindingEditor.qml
        ui/panels/EventLogPanel.qml
        ui/panels/StatusBar.qml
        ui/picker/PickerKey.qml
        ui/picker/PickerTabs.qml
        ui/picker/KeyboardPicker.qml
        ui/picker/MousePicker.qml
        ui/picker/DInputPicker.qml
        ui/picker/MappingPickerDialog.qml
)

# ── 主应用 ──

qt_add_executable(MappyZ
    source/App/Main.cpp
)

set_target_properties(MappyZ
    PROPERTIES
        WIN32_EXECUTABLE TRUE
)

target_compile_features(MappyZ PRIVATE cxx_std_20)

target_link_libraries(MappyZ
    PRIVATE
        MappyZUIBridge
        MappyZApp
        MappyZCore
        MappyZQml
        MappyZQmlplugin
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
        tests/Runtime/BackendEventQueueTests.cpp
        tests/Runtime/RuntimeEventPumpTests.cpp
        tests/Runtime/RuntimeHostTests.cpp
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

    # ── App 测试 ──

    add_executable(MappyZAppTests
        tests/App/ApplicationBootstrapTests.cpp
    )

    target_link_libraries(MappyZAppTests
        PRIVATE
            MappyZApp
            MappyZInputBackends
            MappyZOutputBackends
            Catch2::Catch2WithMain
    )

    if(MSVC)
        target_compile_options(MappyZAppTests PRIVATE /W4)
    else()
        target_compile_options(MappyZAppTests PRIVATE -Wall -Wextra -Wconversion -Wshadow)
    endif()

    catch_discover_tests(MappyZAppTests)

    # ── UI Bridge 测试 ──

    find_package(Qt6 REQUIRED COMPONENTS Test)

    add_executable(MappyZUIBridgeTests
        tests/UI/Bridge/Main.cpp
        tests/UI/Bridge/ActionCatalogModelTests.cpp
        tests/UI/Bridge/AppControllerTests.cpp
        tests/UI/Bridge/DeviceModelTests.cpp
        tests/UI/Bridge/InputCaptureModelTests.cpp
        tests/UI/Bridge/InputStateModelTests.cpp
        tests/UI/Bridge/LogModelTests.cpp
        tests/UI/Bridge/MappingRuleModelTests.cpp
    )

    target_link_libraries(MappyZUIBridgeTests
        PRIVATE
            MappyZUIBridge
            MappyZInputBackends
            MappyZOutputBackends
            Qt6::Test
            Catch2::Catch2
    )

    if(MSVC)
        target_compile_options(MappyZUIBridgeTests PRIVATE /W4)
    else()
        target_compile_options(MappyZUIBridgeTests PRIVATE -Wall -Wextra -Wconversion -Wshadow)
    endif()

    catch_discover_tests(MappyZUIBridgeTests)

    # ── QML 离屏冒烟测试 ──
    # 使用 offscreen 平台加载 QML 模块，验证无 binding/import/property 警告

    add_executable(MappyZQmlSmokeTests
        tests/UI/QmlSmokeTests.cpp
    )

    target_link_libraries(MappyZQmlSmokeTests
        PRIVATE
            MappyZQml
            MappyZQmlplugin
            MappyZUIBridge
            MappyZInputBackends
            MappyZOutputBackends
            Qt6::Quick
            Catch2::Catch2
    )

    if(MSVC)
        target_compile_options(MappyZQmlSmokeTests PRIVATE /W4)
    else()
        target_compile_options(MappyZQmlSmokeTests PRIVATE -Wall -Wextra -Wconversion -Wshadow)
    endif()

    catch_discover_tests(MappyZQmlSmokeTests)
endif()
