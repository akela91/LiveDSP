#pragma once

#include <juce_core/juce_core.h>

// ---------------------------------------------------------------------------
// Resolve resource folders at runtime.
//
// Models (NAM/IR) and presets ("favs") are NOT shipped with the app (the
// commercial captures cannot be redistributed). They live in a user-writable
// folder so the in-app "Browse" feature can copy files there without admin
// rights, and so users can simply extract downloaded rigs into a visible path:
//
//     <Documents>/LiveDSP/models      (.nam models + .wav IRs)
//     <Documents>/LiveDSP/favs        (saved presets)
//
// During development we instead use the source folders baked in at build time
// (LIVEDSP_DEFAULT_MODELS_DIR / LIVEDSP_FAVS_DIR) when they exist, so an exe
// launched from the IDE keeps working against the repo's local content.
//
// Resolution order: 1) dev folder baked in at build time (if it exists),
// 2) <Documents>/LiveDSP/<name> (created on demand). The returned folder is
// always writable, so it doubles as the target for the Browse/import feature.
// ---------------------------------------------------------------------------
namespace livedsp
{
    inline juce::File userResourceDir (const juce::String& folderName)
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                       .getChildFile ("LiveDSP")
                       .getChildFile (folderName);
        dir.createDirectory();   // ensure the writable target exists on first run
        return dir;
    }

    inline juce::File resolveResourceDir (const juce::String& folderName,
                                          const char* devFallback)
    {
        // 1) Development: the source folder baked in at build time, if present.
        if (devFallback != nullptr)
            if (juce::File dev { juce::String::fromUTF8 (devFallback) }; dev.isDirectory())
                return dev;

        // 2) End user: <Documents>/LiveDSP/<folderName> (writable, auto-created).
        return userResourceDir (folderName);
    }

    // Folder for NAM models (.nam) and IRs (.wav). Writable; also the Browse target.
    inline juce::File getModelsDir()
    {
       #if defined (LIVEDSP_DEFAULT_MODELS_DIR)
        return resolveResourceDir ("models", LIVEDSP_DEFAULT_MODELS_DIR);
       #else
        return resolveResourceDir ("models", nullptr);
       #endif
    }

    // Folder for saved presets ("favs"). Writable.
    inline juce::File getFavsDir()
    {
       #if defined (LIVEDSP_FAVS_DIR)
        return resolveResourceDir ("favs", LIVEDSP_FAVS_DIR);
       #else
        return resolveResourceDir ("favs", nullptr);
       #endif
    }
}
