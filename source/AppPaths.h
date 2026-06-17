#pragma once

#include <juce_core/juce_core.h>

// ---------------------------------------------------------------------------
// Erőforrás-mappák feloldása futásidőben.
//
// A telepített (Inno Setup) / portable build a 'models' és 'favs' mappákat a
// futtatható mellé teszi. Fejlesztéskor viszont a build-időben befordított
// forrásmappákat (LIVEDSP_DEFAULT_MODELS_DIR / LIVEDSP_FAVS_DIR) használjuk,
// hogy az IDE-ből indított exe is megtalálja a tartalmat. A feloldás sorrendje
// ezért: 1) exe melletti mappa, 2) build-időben befordított dev-útvonal.
// ---------------------------------------------------------------------------
namespace livedsp
{
    inline juce::File resolveResourceDir (const juce::String& folderName,
                                          const char* devFallback)
    {
        // 1) Telepített / portable eset: <exe könyvtár>/<folderName>.
        const auto exeDir = juce::File::getSpecialLocation (
                                juce::File::currentExecutableFile)
                                .getParentDirectory();

        if (auto local = exeDir.getChildFile (folderName); local.isDirectory())
            return local;

        // 2) Fejlesztői eset: a build-időben befordított forrásmappa, ha létezik.
        if (devFallback != nullptr)
            if (juce::File dev { juce::String::fromUTF8 (devFallback) }; dev.isDirectory())
                return dev;

        // 3) Alapértelmezés: az exe melletti útvonal (akkor is, ha még nem létezik).
        return exeDir.getChildFile (folderName);
    }

    // NAM modellek (.nam) és IR-ek (.wav) mappája.
    inline juce::File getModelsDir()
    {
       #if defined (LIVEDSP_DEFAULT_MODELS_DIR)
        return resolveResourceDir ("models", LIVEDSP_DEFAULT_MODELS_DIR);
       #else
        return resolveResourceDir ("models", nullptr);
       #endif
    }

    // Elmentett presetek ("favs") mappája.
    inline juce::File getFavsDir()
    {
       #if defined (LIVEDSP_FAVS_DIR)
        return resolveResourceDir ("favs", LIVEDSP_FAVS_DIR);
       #else
        return resolveResourceDir ("favs", nullptr);
       #endif
    }
}
