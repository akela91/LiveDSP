# ===========================================================================
# BuildInstaller.cmake — a LiveDSP standalone POST_BUILD lépése hívja meg.
#
# Lefordítja az Inno Setup telepítőt a Release build után. NE add hozzá
# közvetlenül a buildhez; a CMakeLists.txt POST_BUILD custom command-ja adja
# át a paramétereket:
#
#   cmake -DISCC=<ISCC.exe> -DISS=<LiveDSP.iss> -DCFG=$<CONFIG>
#         -DVER=<projekt verzió> -P cmake/BuildInstaller.cmake
#
# Csak Release konfigurációban fut (a statikus runtime + terjesztés ott
# releváns), és csak akkor, ha az ISCC megtalálható — különben csendben
# kihagyja, hogy a Debug/inkrementális buildek gyorsak maradjanak.
# ===========================================================================

# Csak Release-ben építünk telepítőt.
if(NOT CFG STREQUAL "Release")
    return()
endif()

if(NOT ISCC OR NOT EXISTS "${ISCC}")
    message(STATUS
        "LiveDSP: Inno Setup (ISCC) nem található — a telepítő-build kihagyva.\n"
        "  Telepítsd: winget install JRSoftware.InnoSetup\n"
        "  Majd konfiguráld újra a CMake-et (a find_program újra megtalálja).")
    return()
endif()

message(STATUS "LiveDSP: telepítő fordítása Inno Setup-pal (${ISS}) ...")

execute_process(
    COMMAND "${ISCC}" "/DAppVersion=${VER}" "${ISS}"
    RESULT_VARIABLE iscc_result
    OUTPUT_VARIABLE iscc_out
    ERROR_VARIABLE  iscc_err)

if(NOT iscc_result EQUAL 0)
    message(WARNING
        "LiveDSP: az Inno Setup fordítás HIBÁVAL tért vissza (${iscc_result}).\n"
        "${iscc_out}\n${iscc_err}")
else()
    message(STATUS "LiveDSP: telepítő kész → installer/Output/LiveDSP-Setup-${VER}.exe")
endif()
