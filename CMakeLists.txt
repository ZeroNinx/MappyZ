cmake_minimum_required(VERSION 3.24)

project(MappyZ
    VERSION 0.1.0
    DESCRIPTION "Modern gamepad remapping runtime"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Quick)

qt_standard_project_setup(REQUIRES 6.5)

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
