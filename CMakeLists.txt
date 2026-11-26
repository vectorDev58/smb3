cmake_minimum_required(VERSION 3.20)
project(SMB3PC VERSION 0.2.0 LANGUAGES C CXX)

option(SMB3_NATIVE_RECOMP "Recompile a local ROM at build time for development" OFF)
set(SMB3_ROM_SHA256 "6ea0777ca520ba7ad7b0ea0f6452140d59aa0b1dffe5045cb1e8020dde10c267")
set(SMB3_PAYLOAD_SHA256 "d77d17d34af24871d7ce1160ccd3330555835c8e940b7100e095ac38973d927a")

set(NESRECOMP_ROOT "${CMAKE_SOURCE_DIR}/.deps/nesrecomp")
set(NESRECOMP_RECOMP_UI "${CMAKE_SOURCE_DIR}/.deps/recomp-ui" CACHE STRING "Launcher")
set(NESRECOMP_GAME_VERSION "${PROJECT_VERSION}" CACHE STRING "Game version")
if(NOT EXISTS "${NESRECOMP_ROOT}/runner/cyc/project.cmake" OR NOT EXISTS "${CMAKE_SOURCE_DIR}/.deps/libwebp/CMakeLists.txt")
    message(FATAL_ERROR "Descarga las dependencias: python3 tools/smb3.py build")
endif()
if(NOT NESRECOMP_HEADLESS)
    if(WIN32)
        list(APPEND CMAKE_PREFIX_PATH "${NESRECOMP_ROOT}/runner/external/SDL2/cmake")
    endif()
    find_package(SDL2 CONFIG REQUIRED)
endif()
include("${NESRECOMP_ROOT}/runner/cyc/project.cmake")
if(SMB3_NATIVE_RECOMP)
    set(SMB3_ROM "${CMAKE_SOURCE_DIR}/private/baserom.nes" CACHE FILEPATH "SMB3 USA original ROM")
    file(SHA256 "${SMB3_ROM}" rom_sha256)
    if(NOT rom_sha256 STREQUAL SMB3_ROM_SHA256)
        message(FATAL_ERROR "Esta version necesita Super Mario Bros. 3 (USA), PRG0. Consulta README.md.")
    endif()
    nesrecomp_add_cycle_game(smb3 ROM "${SMB3_ROM}"
        GAME_CONFIG "${CMAKE_SOURCE_DIR}/game.toml" PLAYERS 2 REGION NTSC
        HOST_EXTRAS src/remaster.cpp)
else()
    include("${NESRECOMP_ROOT}/runner/cyc/cyc.cmake")
    find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
    set(hook_table "${CMAKE_CURRENT_BINARY_DIR}/runtime_hooks.inc")
    add_custom_command(OUTPUT "${hook_table}"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/runtime_hooks.py"
                "${CMAKE_CURRENT_SOURCE_DIR}/game.toml" "${hook_table}"
        DEPENDS game.toml tools/runtime_hooks.py VERBATIM)
    add_executable(smb3 ${NESRECOMP_CYC_SOURCES} src/remaster.cpp src/runtime_rom.c "${hook_table}")
    target_include_directories(smb3 PRIVATE ${NESRECOMP_CYC_INCLUDE_DIRS} "${CMAKE_CURRENT_BINARY_DIR}")
    target_link_libraries(smb3 PRIVATE ${NESRECOMP_CYC_LIBRARIES})
    target_compile_features(smb3 PRIVATE c_std_11)
    target_compile_definitions(smb3 PRIVATE CYC_GAME_PLAYERS=2 NESRECOMP_GAME_PLAYERS=2 CYC_GAME_REGION=0
        NESRECOMP_NATIVE_INTERVALS=1 SMB3_RUNTIME_ROM=1 _CRT_SECURE_NO_WARNINGS
        "CYC_BUILD_VERSION=\"${NESRECOMP_GAME_VERSION}\"" "CYC_BUILD_REVISION=\"${NESRECOMP_BUILD_REVISION}\"")
    if(NOT NESRECOMP_HEADLESS)
        nesrecomp_cyc_enable_sdl(smb3)
        if(NESRECOMP_CYC_RECOMP_UI_ROOT)
            nesrecomp_cyc_enable_recomp_ui(smb3 ROM_SHA256 "${SMB3_PAYLOAD_SHA256}")
        endif()
    endif()
endif()

# Apply full image validation to both file selection and the command line.
set_property(SOURCE "${NESRECOMP_ROOT}/runner/cyc/cyc_host.c" APPEND PROPERTY
    COMPILE_DEFINITIONS cyc_load_ines=smb3_load_ines)
target_sources(smb3 PRIVATE src/rom_validation.c)
target_compile_definitions(smb3 PRIVATE "SMB3_ROM_SHA256=\"${SMB3_ROM_SHA256}\"")
set(sha256_dir "${CMAKE_SOURCE_DIR}/.deps/recomp-ui/src/common")
target_include_directories(smb3 PRIVATE "${sha256_dir}")
if(NESRECOMP_HEADLESS OR NOT NESRECOMP_CYC_RECOMP_UI_ROOT)
    target_sources(smb3 PRIVATE "${sha256_dir}/sha256.c")
endif()
target_compile_features(smb3 PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(smb3 PRIVATE /utf-8)
endif()

# The expanded character sheet is published as WebP; only the decoder is built.
foreach(webp_tool ANIM_UTILS CWEBP DWEBP GIF2WEBP IMG2WEBP VWEBP WEBPINFO LIBWEBPMUX WEBPMUX EXTRAS)
    set(WEBP_BUILD_${webp_tool} OFF)
endforeach()
set(WEBP_USE_THREAD OFF)
add_subdirectory("${CMAKE_SOURCE_DIR}/.deps/libwebp" libwebp EXCLUDE_FROM_ALL)
target_include_directories(smb3 PRIVATE "${CMAKE_SOURCE_DIR}/.deps/libwebp/src")
target_link_libraries(smb3 PRIVATE webpdecoder)
target_include_directories(smb3 PRIVATE "${NESRECOMP_ROOT}/runner/src")
if(NOT NESRECOMP_HEADLESS)
    target_sources(smb3 PRIVATE src/window.cpp src/ui_host.c)
    # Capture the existing menu host so the game's screens share its bindings and persistence.
    set_property(SOURCE "${NESRECOMP_ROOT}/runner/cyc/cyc_ui_menu.c" APPEND PROPERTY
        COMPILE_DEFINITIONS cyc_ui_menu_create=smb3_create_runtime_menu)
    set_property(SOURCE "${NESRECOMP_ROOT}/runner/cyc/cyc_ui_launcher.c" APPEND PROPERTY
        COMPILE_DEFINITIONS recomp_launcher_run_window=smb3_launcher_run_window)
    if(NESRECOMP_CYC_RECOMP_UI_ROOT)
        set_property(SOURCE "${NESRECOMP_CYC_RECOMP_UI_ROOT}/src/common/backends/imgui/launcher_imgui.cpp" APPEND PROPERTY
            COMPILE_DEFINITIONS launcher_i18n_text=smb3_launcher_i18n_text)
    endif()
    set_property(SOURCE "${NESRECOMP_ROOT}/runner/cyc/cyc_ui_imgui.cpp" APPEND PROPERTY
        COMPILE_DEFINITIONS recomp_runtime_ui_render_imgui=smb3_render_runtime_menu cyc_state_ui_render=smb3_render_state_menu)
    # Use the game's style for the existing save and rewind browser, including its thumbnails and input.
    set(state_menu "${NESRECOMP_ROOT}/runner/cyc/cyc_state_ui.cpp")
    set_property(SOURCE "${state_menu}" APPEND PROPERTY COMPILE_DEFINITIONS SMB3_STATE_MENU_STYLE)
    # Keep the dependency checkout intact; adapt only the host's picture copy.
    set_property(SOURCE "${NESRECOMP_ROOT}/runner/cyc/cyc_sdl.c" APPEND PROPERTY
        COMPILE_DEFINITIONS SMB3_HOST_COPY)
    if(MSVC)
        set_property(SOURCE "${state_menu}" APPEND PROPERTY
            COMPILE_OPTIONS "/FI${CMAKE_CURRENT_SOURCE_DIR}/src/state_menu_style.h")
        set_property(SOURCE "${NESRECOMP_ROOT}/runner/cyc/cyc_sdl.c" APPEND PROPERTY
            COMPILE_OPTIONS "/FI${CMAKE_CURRENT_SOURCE_DIR}/src/window.h")
    else()
        set_property(SOURCE "${state_menu}" APPEND PROPERTY
            COMPILE_OPTIONS -include "${CMAKE_CURRENT_SOURCE_DIR}/src/state_menu_style.h")
        set_property(SOURCE "${NESRECOMP_ROOT}/runner/cyc/cyc_sdl.c" APPEND PROPERTY
            COMPILE_OPTIONS -include "${CMAKE_CURRENT_SOURCE_DIR}/src/window.h")
    endif()
endif()

enable_testing()
add_executable(render_math tests/render_math.cpp)
target_include_directories(render_math PRIVATE src)
target_compile_features(render_math PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(render_math PRIVATE /UNDEBUG)
else()
    target_compile_options(render_math PRIVATE -UNDEBUG)
endif()
add_test(NAME render_math COMMAND render_math)
if(NOT NESRECOMP_HEADLESS)
    add_executable(window_viewport tests/window_viewport.cpp src/window.cpp)
    target_include_directories(window_viewport PRIVATE src)
    target_link_libraries(window_viewport PRIVATE SDL2::SDL2)
    target_compile_features(window_viewport PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(window_viewport PRIVATE /UNDEBUG)
    else()
        target_compile_options(window_viewport PRIVATE -UNDEBUG)
    endif()
    add_test(NAME window_viewport COMMAND window_viewport)
endif()

add_custom_command(TARGET smb3 POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_SOURCE_DIR}/config.default.ini" "$<TARGET_FILE_DIR:smb3>/config.default.ini")

if(NOT SMB3_NATIVE_RECOMP AND NOT NESRECOMP_HEADLESS AND NESRECOMP_CYC_RECOMP_UI_ROOT
    AND CMAKE_SIZEOF_VOID_P EQUAL 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$"
    AND (WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux"))
    install(TARGETS smb3 RUNTIME DESTINATION . COMPONENT SMB3Runtime)
    install(FILES config.default.ini DESTINATION . RENAME config.ini COMPONENT SMB3Runtime)
    install(FILES README.md LICENSE THIRD_PARTY.md DESTINATION . COMPONENT SMB3Runtime)
    install(DIRECTORY licenses/ DESTINATION licenses COMPONENT SMB3Runtime
        FILES_MATCHING PATTERN "*.txt" PATTERN "*.md")

    if(WIN32)
        install(FILES "$<TARGET_FILE:SDL2::SDL2>" DESTINATION . COMPONENT SMB3Runtime)
        set(CPACK_GENERATOR "ZIP")
        set(smb3_release_platform windows-x86_64)
    else()
        set(CPACK_GENERATOR "TGZ")
        set(smb3_release_platform linux-x86_64)
    endif()

    # Package only the ROM-loading runtime and its notices, including on a reused build tree.
    set(CPACK_INSTALL_CMAKE_PROJECTS "${CMAKE_BINARY_DIR};${PROJECT_NAME};SMB3Runtime;/")
    set(CPACK_PACKAGE_DIRECTORY "${CMAKE_SOURCE_DIR}/dist")
    set(CPACK_PACKAGE_FILE_NAME "smb3-pc-${smb3_release_platform}")
    include(CPack)
endif()
