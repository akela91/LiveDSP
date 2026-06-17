# ===========================================================================
# BuildInstaller.cmake — invoked by the LiveDSP standalone POST_BUILD step.
#
# Builds the Inno Setup installer after a Release build. Do NOT add it directly
# to the build; the POST_BUILD custom command in CMakeLists.txt passes the
# parameters:
#
#   cmake -DISCC=<ISCC.exe> -DISS=<LiveDSP.iss> -DCFG=$<CONFIG>
#         -DVER=<project version> -P cmake/BuildInstaller.cmake
#
# Runs only in the Release configuration (where the static runtime +
# distribution matter), and only when ISCC is found — otherwise it silently
# skips, so Debug/incremental builds stay fast.
# ===========================================================================

# Only build the installer in Release.
if(NOT CFG STREQUAL "Release")
    return()
endif()

if(NOT ISCC OR NOT EXISTS "${ISCC}")
    message(STATUS
        "LiveDSP: Inno Setup (ISCC) not found — installer build skipped.\n"
        "  Install with: winget install JRSoftware.InnoSetup\n"
        "  Then re-run CMake configure (find_program will pick it up).")
    return()
endif()

message(STATUS "LiveDSP: building the installer with Inno Setup (${ISS}) ...")

execute_process(
    COMMAND "${ISCC}" "/DAppVersion=${VER}" "${ISS}"
    RESULT_VARIABLE iscc_result
    OUTPUT_VARIABLE iscc_out
    ERROR_VARIABLE  iscc_err)

if(NOT iscc_result EQUAL 0)
    message(WARNING
        "LiveDSP: the Inno Setup build returned an ERROR (${iscc_result}).\n"
        "${iscc_out}\n${iscc_err}")
else()
    message(STATUS "LiveDSP: installer ready -> installer/Output/LiveDSP-Setup-${VER}.exe")
endif()
