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

add_library(MappyZInputBackends STATIC
    source/Backends/Input/FakeInputBackend.cpp
)

target_link_libraries(MappyZInputBackends
    PUBLIC
        MappyZCore
)

if(MSVC)
    target_compile_options(MappyZInputBackends PRIVATE /W4)
else()
    target_compile_options(MappyZInputBackends PRIVATE -Wall -Wextra -Wconversion -Wshadow)
endif()

# ── 输出后端模块 ──

add_library(MappyZOutputBackends STATIC
    source/Backends/Output/NullOutputBackend.cpp
)

target_link_libraries(MappyZOutputBackends
    PUBLIC
        MappyZCore
)

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
)

target_link_libraries(MappyZRuntime
    PUBLIC
        MappyZCore
        MappyZInputBackends
        MappyZOutputBackends
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

    add_executable(MappyZInputBackendTests
        tests/Backends/Input/FakeInputBackendTests.cpp
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

    add_executable(MappyZOutputBackendTests
        tests/Backends/Output/NullOutputBackendTests.cpp
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
