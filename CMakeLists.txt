cmake_minimum_required(VERSION 3.22)

project(guitarDSP VERSION 0.1.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Global settings
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Statikus MSVC runtime nem szükséges; a JUCE standalone önállóan fut.
if (MSVC)
    add_compile_options(/MP)        # többszálú fordítás
endif()

# ---------------------------------------------------------------------------
# Függőségek (FetchContent)
# ---------------------------------------------------------------------------
include(FetchContent)

# --- JUCE 8 ---------------------------------------------------------------
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4          # stabil JUCE 8 tag; igény szerint frissíthető
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(juce)

# --- Signalsmith Stretch (pitch shifter, header-only, MIT) ----------------
# A repó almodulként hozza a "dsp" könyvtárat -> GIT_SUBMODULES kell.
FetchContent_Declare(
    signalsmith_stretch
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(signalsmith_stretch)
# Interface target a header-only könyvtárhoz
add_library(signalsmith_stretch_lib INTERFACE)
target_include_directories(signalsmith_stretch_lib INTERFACE
    ${signalsmith_stretch_SOURCE_DIR})

# --- Neural Amp Modeler Core (MIT) ----------------------------------------
# A repó a függőségeit (Eigen, nlohmann/json) almodulként hozza a
# Dependencies/ mappában, ezért GIT_SUBMODULES kell.
FetchContent_Declare(
    nam_core_src
    GIT_REPOSITORY https://github.com/sdatkinson/NeuralAmpModelerCore.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nam_core_src)

# A NAM core forrásait kézzel fordítjuk statikus könyvtárba, hogy ne
# függjünk a repó saját (eszközöket is építő) CMake céljaitól.
file(GLOB NAM_CORE_SOURCES
    "${nam_core_src_SOURCE_DIR}/NAM/*.cpp")

add_library(nam_core STATIC ${NAM_CORE_SOURCES})
target_compile_features(nam_core PUBLIC cxx_std_17)
target_include_directories(nam_core PUBLIC
    "${nam_core_src_SOURCE_DIR}"
    "${nam_core_src_SOURCE_DIR}/NAM"
    # Almodulként behúzott függőségek (ha a repó struktúrája változik,
    # ezeket az útvonalakat kell igazítani):
    "${nam_core_src_SOURCE_DIR}/Dependencies/eigen"
    "${nam_core_src_SOURCE_DIR}/Dependencies/nlohmann")

# A WaveNet aktivációk gyorsabbak fast-math-csal, de a determinizmus kedvéért
# alapból nem kapcsoljuk be.

# ---------------------------------------------------------------------------
# ASIO SDK (Steinberg) — licenc miatt NEM része a repónak
# ---------------------------------------------------------------------------
# Add meg a kicsomagolt ASIO SDK útvonalát:
#   cmake -B build -DGUITARDSP_ASIO_SDK_DIR="C:/SDKs/asiosdk"
# Ha nincs megadva, alapból a repo ext/asiosdk mappáját nézi.
set(GUITARDSP_ASIO_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ext/asiosdk"
    CACHE PATH "Steinberg ASIO SDK gyökérkönyvtára (a 'common' almappát tartalmazza)")

set(GUITARDSP_HAS_ASIO FALSE)
if (EXISTS "${GUITARDSP_ASIO_SDK_DIR}/common/iasiodrv.h")
    set(GUITARDSP_HAS_ASIO TRUE)
    message(STATUS "guitarDSP: ASIO SDK megtalálva itt: ${GUITARDSP_ASIO_SDK_DIR}")
else()
    message(WARNING
        "guitarDSP: ASIO SDK NEM található (${GUITARDSP_ASIO_SDK_DIR}).\n"
        "  A build folytatódik ASIO NÉLKÜL (WASAPI/DirectSound fallback).\n"
        "  Az alacsony latenciás ASIO-hoz töltsd le a Steinberg ASIO SDK-t és add meg:\n"
        "  cmake -B build -DGUITARDSP_ASIO_SDK_DIR=\"C:/utvonal/asiosdk\"")
endif()

# ---------------------------------------------------------------------------
# A plugin/standalone cél
# ---------------------------------------------------------------------------
juce_add_plugin(guitarDSP
    PRODUCT_NAME            "guitarDSP"
    COMPANY_NAME            "Qulto"
    BUNDLE_ID               "eu.qulto.guitarDSP"
    PLUGIN_MANUFACTURER_CODE Qult
    PLUGIN_CODE             Gdsp
    FORMATS                 Standalone
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(guitarDSP)

target_sources(guitarDSP PRIVATE
    source/PluginProcessor.cpp
    source/PluginEditor.cpp)

target_include_directories(guitarDSP PRIVATE
    source
    source/dsp)

target_compile_definitions(guitarDSP PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    # A standalone az alapértelmezett modellmappát itt keresi (fejlesztéshez):
    GUITARDSP_DEFAULT_MODELS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/models")

# ASIO bekapcsolása, ha elérhető az SDK
if (GUITARDSP_HAS_ASIO)
    target_compile_definitions(guitarDSP PUBLIC JUCE_ASIO=1)
    target_include_directories(guitarDSP PRIVATE "${GUITARDSP_ASIO_SDK_DIR}/common")
endif()

target_link_libraries(guitarDSP PRIVATE
    signalsmith_stretch_lib
    nam_core
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
