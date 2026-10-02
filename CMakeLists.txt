cmake_minimum_required(VERSION 3.20)
project(PRBF2_ExternalToolkit LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui")

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)

set(SOURCES
    src/main.cpp
    src/settings.cpp
    src/game_memory.cpp
    src/combat.cpp
    src/overlay.cpp
    ${IMGUI_SOURCES}
)

add_executable(PRBF2_ExternalToolkit WIN32 ${SOURCES})

target_include_directories(PRBF2_ExternalToolkit PRIVATE
    src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_link_libraries(PRBF2_ExternalToolkit PRIVATE
    d3d11
    dxgi
    dwmapi
)

if (MSVC)
    target_link_options(PRBF2_ExternalToolkit PRIVATE "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'")
endif()
