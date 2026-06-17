# 3.24 kell a Signalsmith Stretch saját CMakeLists-éhez.
cmake_minimum_required(VERSION 3.24)

project(LiveDSP VERSION 0.1.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Global settings
# ---------------------------------------------------------------------------
# C++20: a Neural Amp Modeler Core C++20-at igényel (és a headerei is).
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Statikus MSVC runtime nem szükséges; a JUCE standalone önállóan fut.
if (MSVC)
    add_compile_options(/MP)        # többszálú fordítás
    # A forrásfájlok UTF-8-ban vannak, és a futtatási (execution) charset is UTF-8
    # legyen — különben az ékezetes string-literálok (pl. "Válassz módot") a
    # rendszer-kódlapon (CP1252) torzulnak el a UI-ban. A JUCE a const char*
    # literálokat UTF-8-ként értelmezi, így ez együtt helyes megjelenítést ad.
    add_compile_options(/utf-8)
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
# A saját CMakeLists-e létrehozza a 'signalsmith-stretch' INTERFACE célt, és
# FetchContent-tel behúzza a 'signalsmith-linear' függőséget. Ezt használjuk,
# NEM saját interface libet (a gyökér header a signalsmith-linear nélkül nem
# fordulna). Include: "signalsmith-stretch/signalsmith-stretch.h".
FetchContent_Declare(
    signalsmith_stretch
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(signalsmith_stretch)

# --- Rubber Band Library (pitch shifter, GPL/commercial) -------------------
# A repó Meson/Makefile alapú, nincs használható CMakeLists-e. Ezért a
# SOURCE_SUBDIR trükkel csak LETÖLTJÜK a forrást, majd a hivatalos egyfájlos
# fordítási egységből (single/RubberBandSingle.cpp) építünk statikus libet:
# ez a beépített FFT-t és resamplert használja, külső függőség nélkül,
# egyszálú módban (NO_THREADING). Include: <rubberband/RubberBandStretcher.h>.
FetchContent_Declare(
    rubberband_src
    GIT_REPOSITORY https://github.com/breakfastquay/rubberband.git
    GIT_TAG        v4.0.0
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  _do_not_build   # nem létező almappa -> nincs add_subdirectory
)
FetchContent_MakeAvailable(rubberband_src)

add_library(rubberband STATIC "${rubberband_src_SOURCE_DIR}/single/RubberBandSingle.cpp")
target_compile_features(rubberband PUBLIC cxx_std_20)
# SYSTEM: a Rubber Band third-party warningjait elnyomja a fogyasztóknál is.
# A gyökér a "rubberband/...h" publikus headerekhez ÉS a belső forrásfájlok
# "rubberband/...h" include-jaihoz is kell.
target_include_directories(rubberband SYSTEM PUBLIC "${rubberband_src_SOURCE_DIR}")
if (MSVC)
    target_compile_definitions(rubberband PRIVATE _USE_MATH_DEFINES NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# --- Neural Amp Modeler Core (MIT) ----------------------------------------
# A repó saját CMakeLists-e csak a 'tools' segédprogramokat építi (GCC/Clang
# flag-ekkel, ami MSVC-n elhasal), könyvtár-célt NEM ad. Ezért a SOURCE_SUBDIR
# trükkel csak LETÖLTJÜK a forrást (nem futtatjuk a CMakeLists-ét), majd saját
# statikus könyvtárat építünk belőle.
FetchContent_Declare(
    nam_core_src
    GIT_REPOSITORY https://github.com/sdatkinson/NeuralAmpModelerCore.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  _do_not_build   # nem létező almappa -> nincs add_subdirectory
)
FetchContent_MakeAvailable(nam_core_src)

# A NAM core forrásai: a NAM/ gyökér + a NAM/wavenet/ almappa (a2_fast, model,
# slimmable). A wav.cpp (AudioDSPTools) NEM kell, az csak a 'render' eszközhöz.
file(GLOB NAM_CORE_SOURCES
    "${nam_core_src_SOURCE_DIR}/NAM/*.cpp"
    "${nam_core_src_SOURCE_DIR}/NAM/*/*.cpp")

add_library(nam_core STATIC ${NAM_CORE_SOURCES})
target_compile_features(nam_core PUBLIC cxx_std_20)
# SYSTEM: elnyomja a NAM/Eigen third-party warningokat a fogyasztó céloknál is.
target_include_directories(nam_core SYSTEM PUBLIC
    "${nam_core_src_SOURCE_DIR}"                       # "NAM/dsp.h" elérhetősége
    "${nam_core_src_SOURCE_DIR}/Dependencies/eigen"    # <Eigen/Dense>, <unsupported/Eigen/FFT>
    "${nam_core_src_SOURCE_DIR}/Dependencies/nlohmann") # "json.hpp"

# A2 fast-path WaveNet bekapcsolása (upstream alapértelmezés ON), és a Windows
# Eigen/min-max ütközés elkerülése. PUBLIC, hogy a NAM headert include-oló
# fordítási egységeink is megkapják.
target_compile_definitions(nam_core PUBLIC NAM_ENABLE_A2_FAST)
if (WIN32)
    target_compile_definitions(nam_core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

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
    message(STATUS "LiveDSP: ASIO SDK megtalálva itt: ${GUITARDSP_ASIO_SDK_DIR}")
else()
    message(WARNING
        "LiveDSP: ASIO SDK NEM található (${GUITARDSP_ASIO_SDK_DIR}).\n"
        "  A build folytatódik ASIO NÉLKÜL (WASAPI/DirectSound fallback).\n"
        "  Az alacsony latenciás ASIO-hoz töltsd le a Steinberg ASIO SDK-t és add meg:\n"
        "  cmake -B build -DGUITARDSP_ASIO_SDK_DIR=\"C:/utvonal/asiosdk\"")
endif()

# ---------------------------------------------------------------------------
# A plugin/standalone cél
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
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(LiveDSP)

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
    # A standalone az alapértelmezett modellmappát itt keresi (fejlesztéshez):
    GUITARDSP_DEFAULT_MODELS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/models"
    # Preset ("favs") mappa az elmentett state-eknek:
    GUITARDSP_FAVS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/favs")

# ASIO bekapcsolása, ha elérhető az SDK
if (GUITARDSP_HAS_ASIO)
    target_compile_definitions(LiveDSP PUBLIC JUCE_ASIO=1)
    target_include_directories(LiveDSP PRIVATE "${GUITARDSP_ASIO_SDK_DIR}/common")
endif()

target_link_libraries(LiveDSP PRIVATE
    signalsmith-stretch
    rubberband
    # WHOLE_ARCHIVE: a NAM architektúrák file-scope statikus inicializálókkal
    # regisztrálnak (pl. SlimmableContainer a container.cpp-ben). Statikus libnél
    # az MSVC eldobná a nem hivatkozott objektumfájlokat -> a regisztráció
    # elveszne. A teljes archívum linkelése megtartja mindet.
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,nam_core>"
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
