#pragma once

#include <juce_core/juce_core.h>

// ---------------------------------------------------------------------------
// Resolve resource folders at runtime.
//
// Models (NAM/IR) and presets ("favs") are NOT shipped with the app (the
// commercial captures cannot be redistributed). They live in a user-writable
// folder that is created automatically on first launch, so the in-app "Browse"
// feature can copy files there without admin rights, and users can simply
// extract downloaded rigs/IRs into a visible path:
//
//     <Documents>/LiveDSP/models      (.nam models + .wav IRs)
//     <Documents>/LiveDSP/favs         (saved presets)
//
// During development the source folders baked in at build time
// (LIVEDSP_DEFAULT_MODELS_DIR / LIVEDSP_FAVS_DIR) are used as a fallback when
// the user folder is still empty, so an exe launched from the IDE keeps working
// against the repo's local content.
// ---------------------------------------------------------------------------
namespace livedsp
{
    inline juce::File baseDir()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("LiveDSP");
    }

    // Always-created, user-writable folders. These are the Browse/import target
    // and the path the README points users to.
    inline juce::File getUserModelsDir()
    {
        auto d = baseDir().getChildFile ("models");
        d.createDirectory();
        return d;
    }

    inline juce::File getUserFavsDir()
    {
        auto d = baseDir().getChildFile ("favs");
        d.createDirectory();
        return d;
    }

    inline bool dirHasFiles (const juce::File& d, const juce::String& wildcard)
    {
        return d.isDirectory()
            && ! d.findChildFiles (juce::File::findFiles, true, wildcard).isEmpty();
    }

    // Read location for models/IRs: prefer the user folder once it has content;
    // otherwise the dev fallback (if present); otherwise the (empty) user folder.
    // The user folder is always created as a side effect, so it is findable.
    inline juce::File getModelsDir()
    {
        const auto user = getUserModelsDir();
        if (dirHasFiles (user, "*.nam") || dirHasFiles (user, "*.wav"))
            return user;

       #if defined (LIVEDSP_DEFAULT_MODELS_DIR)
        if (juce::File dev { juce::String::fromUTF8 (LIVEDSP_DEFAULT_MODELS_DIR) }; dev.isDirectory())
            return dev;
       #endif

        return user;
    }

    // Read location for presets, same strategy as getModelsDir().
    inline juce::File getFavsDir()
    {
        const auto user = getUserFavsDir();
        if (dirHasFiles (user, "*"))
            return user;

       #if defined (LIVEDSP_FAVS_DIR)
        if (juce::File dev { juce::String::fromUTF8 (LIVEDSP_FAVS_DIR) }; dev.isDirectory())
            return dev;
       #endif

        return user;
    }
}
