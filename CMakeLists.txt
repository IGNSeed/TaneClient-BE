cmake_minimum_required(VERSION 3.20)

project(TaneClient LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(VENDOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor")
set(LOGO_PNG_PATH "${VENDOR_DIR}/images/TaneClient_logo.png")
set(FONT_TTF_PATH "${VENDOR_DIR}/fonts/JetBrainsMono-VariableFont_wght.ttf")
set(BOLD_FONT_TTF_PATH "${VENDOR_DIR}/fonts/JetBrainsMono-Bold.ttf")
set(ITEM_HUD_FONT_TTF_PATH "${VENDOR_DIR}/fonts/Minecraftia-Regular.ttf")
set(EFFECT_ICON_NAMES
    absorption_effect.png
    bad_omen_effect.png
    blindness_effect.png
    breath_of_the_nautilus_effect.png
    conduit_power_effect.png
    darkness_effect.png
    fire_resistance_effect.png
    haste_effect.png
    health_boost_effect.png
    hunger_effect.png
    infested_effect.png
    invisibility_effect.png
    jump_boost_effect.png
    levitation_effect.png
    mining_fatigue_effect.png
    nausea_effect.png
    night_vision_effect.png
    oozing_effect.png
    poison_effect.png
    raid_omen_effect.png
    regeneration_effect.png
    resistance_effect.png
    slow_falling_effect.png
    slowness_effect.png
    speed_effect.png
    strength_effect.png
    trial_omen_effect.png
    village_hero_effect.png
    water_breathing_effect.png
    weakness_effect.png
    weaving_effect.png
    wind_charged_effect.png
    wither_effect.png
)
set(ITEM_IMAGE_NAMES
    diamond_helmet.png
    diamond_chestplate.png
    diamond_leggings.png
    diamond_boots.png
    diamond_sword.png
    arrow.png
    potion_bottle_splash_heal.png
)
set(EFFECT_ICON_RESOURCE_LINES "")
set(EFFECT_ICON_RESOURCE_ID 3000)
foreach(EFFECT_ICON_NAME IN LISTS EFFECT_ICON_NAMES)
    string(APPEND EFFECT_ICON_RESOURCE_LINES "${EFFECT_ICON_RESOURCE_ID} RCDATA \"${VENDOR_DIR}/images/effect/${EFFECT_ICON_NAME}\"\n")
    math(EXPR EFFECT_ICON_RESOURCE_ID "${EFFECT_ICON_RESOURCE_ID} + 1")
endforeach()
set(ITEM_IMAGE_RESOURCE_LINES "")
set(ITEM_IMAGE_RESOURCE_ID 4000)
foreach(ITEM_IMAGE_NAME IN LISTS ITEM_IMAGE_NAMES)
    set(ITEM_IMAGE_PATH "${VENDOR_DIR}/images/items/${ITEM_IMAGE_NAME}")
    if(EXISTS "${ITEM_IMAGE_PATH}")
        string(APPEND ITEM_IMAGE_RESOURCE_LINES "${ITEM_IMAGE_RESOURCE_ID} RCDATA \"${ITEM_IMAGE_PATH}\"\n")
    endif()
    math(EXPR ITEM_IMAGE_RESOURCE_ID "${ITEM_IMAGE_RESOURCE_ID} + 1")
endforeach()
set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/dist")
set(INTERMEDIATE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/intermediate")

foreach(CONFIG_NAME DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_NAME} "${OUTPUT_DIR}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG_NAME} "${OUTPUT_DIR}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_NAME} "${CMAKE_BINARY_DIR}/lib")
endforeach()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

add_library(minhook STATIC
    "${VENDOR_DIR}/minhook/src/buffer.c"
    "${VENDOR_DIR}/minhook/src/hook.c"
    "${VENDOR_DIR}/minhook/src/trampoline.c"
    "${VENDOR_DIR}/minhook/src/hde/hde64.c"
)

target_include_directories(minhook PUBLIC
    "${VENDOR_DIR}/minhook/include"
)

target_include_directories(minhook PRIVATE
    "${VENDOR_DIR}/minhook/src"
    "${VENDOR_DIR}/minhook/src/hde"
)

add_library(TanePayload SHARED
    src/payload/DllMain.cpp
    src/payload/D3D11ImGuiHook.cpp
    src/GUI/CPS.cpp
    src/GUI/FPS.cpp
    src/GUI/Ping.cpp
    src/GUI/KeyStroke.cpp
    src/GUI/ControllerOverlay.cpp
    src/GUI/Tab.cpp
    src/GUI/ArrowCounter.cpp
    src/GUI/PotCounter.cpp
    src/GUI/EffectHUD.cpp
    src/GUI/ItemHUD.cpp
    src/ImGui/Menu.cpp
    src/ImGui/MenuState.cpp
    src/ImGui/MenuWidgets.cpp
    src/ImGui/InputBlock.cpp
    src/Render/NameTags.cpp
    src/Render/Fullbright.cpp
    src/Render/NoFog.cpp
    src/Render/Hitbox.cpp
    src/Render/Tracer.cpp
    src/Camera/Zoom.cpp
    src/Camera/FreeLook.cpp
    src/Movement/AutoSprint.cpp
    src/Patch/ForceCloseOreUI.cpp
    "${VENDOR_DIR}/imgui/imgui.cpp"
    "${VENDOR_DIR}/imgui/imgui_draw.cpp"
    "${VENDOR_DIR}/imgui/imgui_tables.cpp"
    "${VENDOR_DIR}/imgui/imgui_widgets.cpp"
    "${VENDOR_DIR}/imgui/backends/imgui_impl_dx11.cpp"
    "${VENDOR_DIR}/imgui/backends/imgui_impl_dx12.cpp"
    "${VENDOR_DIR}/imgui/backends/imgui_impl_win32.cpp"
    "${CMAKE_BINARY_DIR}/PayloadLogoResource.rc"
)

configure_file(
    resources/payload/PayloadLogoResource.rc.in
    "${CMAKE_BINARY_DIR}/PayloadLogoResource.rc"
    @ONLY
)

target_include_directories(TanePayload PRIVATE
    include
    src/payload
    src/GUI
    src/ImGui
    src/Render
    src/Camera
    "${VENDOR_DIR}/imgui"
    "${VENDOR_DIR}/imgui/backends"
    "${VENDOR_DIR}/minhook/include"
)

target_compile_definitions(TanePayload PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_link_libraries(TanePayload PRIVATE
    minhook
    advapi32
    d3d11
    d3d12
    d3dcompiler
    dxgi
    dwmapi
    ole32
    windowscodecs
)

set_target_properties(TanePayload PROPERTIES
    OUTPUT_NAME "TanePayload"
    SUFFIX ".payload"
    RUNTIME_OUTPUT_DIRECTORY "${INTERMEDIATE_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${INTERMEDIATE_OUTPUT_DIR}"
)

set(PAYLOAD_DLL_PATH "${INTERMEDIATE_OUTPUT_DIR}/TanePayload.payload")
set(ICON_ICO_PATH "${VENDOR_DIR}/images/TaneClient_logo.ico")
configure_file(
    resources/launcher/PayloadResource.rc.in
    "${CMAKE_BINARY_DIR}/PayloadResource.rc"
    @ONLY
)

add_executable(TaneLauncher WIN32
    src/launcher/Main.cpp
    src/launcher/LauncherApp.cpp
    "${VENDOR_DIR}/imgui/imgui.cpp"
    "${VENDOR_DIR}/imgui/imgui_draw.cpp"
    "${VENDOR_DIR}/imgui/imgui_tables.cpp"
    "${VENDOR_DIR}/imgui/imgui_widgets.cpp"
    "${VENDOR_DIR}/imgui/backends/imgui_impl_dx11.cpp"
    "${VENDOR_DIR}/imgui/backends/imgui_impl_win32.cpp"
    "${CMAKE_BINARY_DIR}/PayloadResource.rc"
)

add_dependencies(TaneLauncher TanePayload)

target_compile_definitions(TaneLauncher PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
)

target_include_directories(TaneLauncher PRIVATE
    src/launcher
    "${VENDOR_DIR}/imgui"
    "${VENDOR_DIR}/imgui/backends"
)

target_link_libraries(TaneLauncher PRIVATE
    advapi32
    d3d11
    dwmapi
    dxgi
    gdi32
    ole32
    shell32
    user32
    windowscodecs
)

set_target_properties(TaneLauncher PROPERTIES
    OUTPUT_NAME "TaneClient"
)
