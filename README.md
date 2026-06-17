# guitarDSP

Standalone Windows gitár amp-szimulátor suite (JUCE 8), Neural DSP / Fortin
stílusú jelúttal. **Fázis 1: CMake-felállás + DSP-architektúra váz.**

## Jelút

```
In → Input Gain → Noise Gate → Transpose (pitch) → Overdrive
   → Amp (NAM) → Cab IR (bypassolható) → Delay → Reverb → Output Gain → Out
```

A NAM-ig mono a jelút; utána sztereóvá szélesedik (IR/Delay/Reverb).

## Függőségek

A CMake `FetchContent`-tel automatikusan letölti:
- **JUCE 8.0.4**
- **Signalsmith Stretch** (pitch shifter, MIT)
- **NeuralAmpModelerCore** (+ Eigen, nlohmann/json almodulként)

Kézzel beszerzendő (licenc miatt):
- **Steinberg ASIO SDK** — https://www.steinberg.net/developers/

## ASIO SDK beállítása

1. Töltsd le és csomagold ki a Steinberg ASIO SDK-t.
2. Vagy másold a `guitarDSP/ext/asiosdk/` mappába (ekkor automatikusan megtalálja),
   vagy add meg az útvonalat a konfiguráláskor:
   ```
   cmake -B build -G "Visual Studio 17 2022" -DGUITARDSP_ASIO_SDK_DIR="C:/SDKs/asiosdk"
   ```
   A mappának tartalmaznia kell a `common/iasiodrv.h` fájlt.

> ASIO nélkül is fordul (WASAPI/DirectSound fallback), de az alacsony latenciához ASIO kell.

## Build (Visual Studio 2022)

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64 -DGUITARDSP_ASIO_SDK_DIR="C:/SDKs/asiosdk"
cmake --build build --config Release
```

A standalone alkalmazás:
`build/guitarDSP_artefacts/Release/Standalone/guitarDSP.exe`

## Modellek / IR

A `models/` mappa tartalma indításkor automatikusan elérhető (az első `.nam`
modell betöltődik fejlesztői kényelemből).

> A jelenlegi NAM modellek **"Full Rig"** típusúak (tartalmazzák a hangládát),
> ezért a **Cab IR alapból KI van kapcsolva** (nincs dupla cab). Külön IR-t csak
> "amp-only" (preamp/DI) NAM modellhez kapcsolj be.

## Tesztelés

1. Indítsd a standalone `.exe`-t.
2. Audio Settings → válaszd a Focusrite **ASIO** drivert, állíts alacsony puffert (64/128).
3. A GenericAudioProcessorEditor csúszkáin minden paraméter elérhető (UI későbbi fázis).
4. Játssz: a NAM amp szól; kapcsold be a Pitch-et (-2/-5 félhang), az Overdrive-ot, a Delay/Reverb-öt.

## Roadmap

- **Fázis 1 (kész):** CMake + jelút váz, NAM + IR betöltés, tesztelhető hang.
- **Fázis 2:** DSP modulok finomhangolása (gate timing, OD hangzás, pitch minőség/latencia, NAM resampling).
- **Fázis 3:** Teljes APVTS + preset mentés/betöltés + modell/IR fájltallózó.
- **Fázis 4:** Egyedi UI.
