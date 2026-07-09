cmake_minimum_required(VERSION 3.24)

project(LiveDSP VERSION 0.4.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Global settings
# ---------------------------------------------------------------------------
# C++20: the Neural Amp Modeler Core requires C++20 (and so do its headers).
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static MSVC runtime: this way the .exe does not depend on the Visual C++
# Redistributable, so the installed/portable build runs on any Windows machine
# WITHOUT INSTALLATION. IMPORTANT: CMAKE_MSVC_RUNTIME_LIBRARY must be set BEFORE
# creating ANY target (juce_add_plugin, static libs) so they all get a
# consistent runtime.
if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/MP)        # multithreaded compilation
    # The source files are UTF-8, and the execution charset should also be UTF-8
    # so that any non-ASCII string literal is interpreted correctly. JUCE treats
    # const char* literals as UTF-8, so together this yields correct rendering.
    add_compile_options(/utf-8)
endif()

# ---------------------------------------------------------------------------
# Dependencies (FetchContent)
# ---------------------------------------------------------------------------
include(FetchContent)

# --- JUCE 8 ---------------------------------------------------------------
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4          # stable JUCE 8 tag; bump as needed
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(juce)

# (Pitch shifting — the guitar Transpose, the vocal Autotune and the vocal
#  "Diabolic/Mickey" effect — runs on our own header-only GranularPitchShifter,
#  so no external shifter library.)

# --- Neural Amp Modeler Core (MIT) ----------------------------------------
# Its own CMakeLists only builds the 'tools' utilities (with GCC/Clang flags
# that break on MSVC) and provides NO library target. So we use the
# SOURCE_SUBDIR trick to only DOWNLOAD the source (without running its
# CMakeLists), then build our own static library from it.
FetchContent_Declare(
    nam_core_src
    GIT_REPOSITORY https://github.com/sdatkinson/NeuralAmpModelerCore.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  _do_not_build   # non-existent subdir -> no add_subdirectory
)
FetchContent_MakeAvailable(nam_core_src)

# NAM core sources: the NAM/ root + the NAM/wavenet/ subdir (a2_fast, model,
# slimmable). wav.cpp (AudioDSPTools) is NOT needed; it is only for the 'render' tool.
file(GLOB NAM_CORE_SOURCES
    "${nam_core_src_SOURCE_DIR}/NAM/*.cpp"
    "${nam_core_src_SOURCE_DIR}/NAM/*/*.cpp")

add_library(nam_core STATIC ${NAM_CORE_SOURCES})
target_compile_features(nam_core PUBLIC cxx_std_20)
# SYSTEM: also suppresses NAM/Eigen third-party warnings in consuming targets.
target_include_directories(nam_core SYSTEM PUBLIC
    "${nam_core_src_SOURCE_DIR}"                       # makes "NAM/dsp.h" available
    "${nam_core_src_SOURCE_DIR}/Dependencies/eigen"    # <Eigen/Dense>, <unsupported/Eigen/FFT>
    "${nam_core_src_SOURCE_DIR}/Dependencies/nlohmann") # "json.hpp"

# Enable the A2 fast-path WaveNet (upstream default ON) and avoid the Windows
# Eigen/min-max clash. PUBLIC so our translation units that include the NAM
# header get it too.
target_compile_definitions(nam_core PUBLIC NAM_ENABLE_A2_FAST)
if (WIN32)
    target_compile_definitions(nam_core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# ---------------------------------------------------------------------------
# ASIO SDK (Steinberg) — bundled in the repo under its GPLv3 variant
# ---------------------------------------------------------------------------
# ASIO is a Windows-only driver model, so the whole detection is gated on
# WIN32; macOS uses CoreAudio and Linux uses ALSA/JACK natively via JUCE.
# The Steinberg ASIO SDK is GPLv3-licensed (open-source variant) and lives in
# ext/ASIOSDK/, so no manual download is needed. To point at a different SDK:
#   cmake -B build -DLIVEDSP_ASIO_SDK_DIR="C:/SDKs/asiosdk"
set(LIVEDSP_HAS_ASIO FALSE)
if (WIN32)
    set(LIVEDSP_ASIO_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ext/ASIOSDK"
        CACHE PATH "Steinberg ASIO SDK root directory (must contain the 'common' subfolder)")

    if (EXISTS "${LIVEDSP_ASIO_SDK_DIR}/common/iasiodrv.h")
        set(LIVEDSP_HAS_ASIO TRUE)
        message(STATUS "LiveDSP: ASIO SDK found at: ${LIVEDSP_ASIO_SDK_DIR}")
    else()
        message(WARNING
            "LiveDSP: ASIO SDK NOT found (${LIVEDSP_ASIO_SDK_DIR}).\n"
            "  The build continues WITHOUT ASIO (WASAPI/DirectSound fallback).\n"
            "  For low-latency ASIO, point LIVEDSP_ASIO_SDK_DIR at a Steinberg ASIO SDK:\n"
            "  cmake -B build -DLIVEDSP_ASIO_SDK_DIR=\"C:/path/asiosdk\"")
    endif()
endif()

# ---------------------------------------------------------------------------
# The plugin/standalone target
# ---------------------------------------------------------------------------
juce_add_plugin(LiveDSP
    PRODUCT_NAME            "LiveDSP"
    COMPANY_NAME            "Qulto"
    BUNDLE_ID               "eu.qulto.LiveDSP"
    PLUGIN_MANUFACTURER_CODE Qult
    PLUGIN_CODE             Gdsp
    ICON_BIG                "${CMAKE_CURRENT_SOURCE_DIR}/resources/AppIcon.png"
    FORMATS                 Standalone
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    # macOS: the standalone must request microphone access, otherwise CoreAudio
    # delivers silence. Ignored on other platforms.
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "LiveDSP needs access to your audio interface / microphone to process live audio.")

juce_generate_juce_header(LiveDSP)

# Landing-screen artwork (GuitarDSP/VoiceDSP) embedded into the binary so it is
# available at runtime without shipping loose files. Accessed via BinaryData::*.
juce_add_binary_data(LiveDSPData SOURCES
    resources/GuitarDSP.png
    resources/VoiceDSP.png)

target_sources(LiveDSP PRIVATE
    source/PluginProcessor.cpp
    source/PluginEditor.cpp
    source/ui/GuitarView.cpp)

target_include_directories(LiveDSP PRIVATE
    source
    source/dsp)

target_compile_definitions(LiveDSP PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    # Development-only fallback dirs: if these local folders exist (they are
    # gitignored and NOT shipped), the app reads models/presets from here so an
    # exe launched from the IDE keeps working. End users get the writable
    # <Documents>/LiveDSP/{models,favs} folders instead (see AppPaths.h).
    LIVEDSP_DEFAULT_MODELS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/models"
    LIVEDSP_FAVS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/favs")

# Enable ASIO if the SDK is available
if (LIVEDSP_HAS_ASIO)
    target_compile_definitions(LiveDSP PUBLIC JUCE_ASIO=1)
    target_include_directories(LiveDSP PRIVATE "${LIVEDSP_ASIO_SDK_DIR}/common")
endif()

target_link_libraries(LiveDSP PRIVATE
    LiveDSPData
    # WHOLE_ARCHIVE: the NAM architectures register themselves with file-scope
    # static initializers (e.g. SlimmableContainer in container.cpp). For a
    # static lib, MSVC would drop the unreferenced object files -> the
    # registration would be lost. Linking the whole archive keeps them all.
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,nam_core>"
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Build the installer automatically after a Release build (Inno Setup)
# ---------------------------------------------------------------------------
# At the END of every Release build, cmake/BuildInstaller.cmake runs and builds
# the installer from installer/LiveDSP.iss (into installer/Output/...). The
# script only does work in Release and only when ISCC is found, so Debug and
# incremental builds stay fast. Disable with:
#   cmake -B build -DLIVEDSP_BUILD_INSTALLER=OFF
option(LIVEDSP_BUILD_INSTALLER
    "Automatically build the Inno Setup installer on a Release build" ON)

if (WIN32 AND LIVEDSP_BUILD_INSTALLER)
    find_program(LIVEDSP_ISCC
        NAMES ISCC ISCC.exe
        PATHS
            "$ENV{LOCALAPPDATA}/Programs/Inno Setup 6"
            "$ENV{ProgramFiles}/Inno Setup 6"
            "$ENV{ProgramFiles\(x86\)}/Inno Setup 6"
        DOC "Inno Setup command-line compiler (ISCC.exe)")

    if (LIVEDSP_ISCC)
        message(STATUS "LiveDSP: Inno Setup found (${LIVEDSP_ISCC}) — installer is built automatically on Release.")
    else()
        message(STATUS "LiveDSP: Inno Setup (ISCC) not found — installer skipped. Install with: winget install JRSoftware.InnoSetup")
    endif()

    # POST_BUILD: pass the config ($<CONFIG>) and version to the script; the
    # non-Release / missing-ISCC cases are handled by the script (silent skip).
    add_custom_command(TARGET LiveDSP_Standalone POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
                "-DISCC=${LIVEDSP_ISCC}"
                "-DISS=${CMAKE_CURRENT_SOURCE_DIR}/installer/LiveDSP.iss"
                "-DCFG=$<CONFIG>"
                "-DVER=${PROJECT_VERSION}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/BuildInstaller.cmake"
        VERBATIM
        COMMENT "LiveDSP: building the installer (Release only)")
endif()
