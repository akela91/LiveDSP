# LiveDSP

Standalone, alacsony latenciás Windows audio alkalmazás (JUCE 8), Neural DSP /
Fortin stílusú UI-val. **Kétmódú app egyetlen `.exe`-ben:** induláskor egy
landing képernyő választatja ki a két modult — **GuitarDSP** (gitár
amp-szimulátor) vagy **VoiceDSP** (élő mikrofon csatorna). A két modul külön
panel-elrendezést és külön DSP jelutat kap; a közös téma, knob/panel elemek és a
hangoló újrahasznosulnak.

> Nem plugin és nem játszik zenei alapot — kizárólag a bemenő jelet dolgozza fel
> és küldi a kimenetre, live monitorozáshoz (Focusrite Scarlett + ASIO).

## Módok és jelutak

A bemenetnél **módonként külön kiválasztható**, hogy a Scarlett melyik bemeneti
csatornáját figyelje (a választás megmarad), így a mikrofon és a gitár nem
keveredik össze.

### GuitarDSP modul (gitár)
```
In → Input Gain → Noise Gate → Pitch (Transpose) → Overdrive
   → Amp (NAM) → Cab IR → EQ → Delay → Reverb → Output Gain → Out
```
A NAM-ig mono a jelút, utána sztereó. A NAM modell és a Cab IR közvetlenül az
**AMP** és **CAB** panel legördülőjéből választható; a **GATE** panelen egy LED
jelzi, amikor a zajzár némít. 9-sávos grafikus EQ, élő latencia-kijelző,
vizuális hangoló (tuner), preset-választó.

**Pitch motorok** (a PITCH panel motorválasztójából): Signalsmith Stretch,
RubberBand Stretcher (R3 „Finer"), és **RubberBand LiveShifter (v4)** — ez az
alapértelmezett, a legkisebb latenciára. Cél: −4 félhang power chordokra jó
minőségben, minimális késleltetéssel.

### VoiceDSP modul (ének)
`juce::dsp::ProcessorChain`, szigorúan ebben a sorrendben:
```
In → Input Gain → Low-Cut (90 Hz) → Noise Gate → Warmth (tanh)
   → Compressor → High-Shelf "Air" (6 kHz) → Delay → Reverb → Brickwall Limiter → Out
```
A GATE és a COMP panel aktivitás-LED-et kap (kigyullad némításkor / vágáskor).
| Modul | Vezérlő | Fix beállítás |
|---|---|---|
| Input Gain | GAIN 0…+24 dB | — |
| Low-Cut | — | 90 Hz high-pass |
| Noise Gate | GATE −80…−20 dB | ratio 10:1, attack 2 ms, release 150 ms |
| Warmth | WARMTH 1.0…3.0 | szinttartó tanh-szaturáció (WaveShaper) |
| Compressor | THRESH −40…0 dB, RATIO 1:1…10:1 | attack 5 ms, release 100 ms |
| High-Shelf „Air" | AIR 0…+12 dB | 6 kHz |
| Delay | TIME 50…500 ms, MIX 0…50% | feedback 0.3 |
| Reverb | MIX 0…100% | közepes hall (room/damp) |
| Limiter | — | −0.1 dB ceiling (clip-védelem) |

A teljes ének lánc **nulla-latenciás**; a beállítások (összes vokál paraméter)
megmaradnak az állapotban. A COMP/AIR/REVERB és az új GATE/WARMTH/DELAY modulok
külön be/kikapcsolhatók.

## Függőségek

A CMake `FetchContent`-tel automatikusan letölti:
- **JUCE 8.0.4**
- **Signalsmith Stretch** (pitch shifter, MIT)
- **RubberBand Library v4.0.0** (Stretcher + LiveShifter, egyfájlos build)
- **NeuralAmpModelerCore** (+ Eigen, nlohmann/json)

Kézzel beszerzendő (licenc miatt):
- **Steinberg ASIO SDK** — https://www.steinberg.net/developers/

## ASIO SDK beállítása

1. Töltsd le és csomagold ki a Steinberg ASIO SDK-t.
2. Vagy másold a `LiveDSP/ext/asiosdk/` mappába (ekkor automatikusan megtalálja),
   vagy add meg az útvonalat a konfiguráláskor:
   ```
   cmake -B build -G "Visual Studio 17 2022" -DLIVEDSP_ASIO_SDK_DIR="C:/SDKs/asiosdk"
   ```
   A mappának tartalmaznia kell a `common/iasiodrv.h` fájlt.

> ASIO nélkül is fordul (WASAPI/DirectSound fallback), de az alacsony latenciához ASIO kell.

## Build (Visual Studio 2022)

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64 -DLIVEDSP_ASIO_SDK_DIR="C:/SDKs/asiosdk"
cmake --build build --config Release
```

A standalone alkalmazás:
`build/LiveDSP_artefacts/Release/Standalone/LiveDSP.exe`

> Megjegyzés: a forrásfájlok UTF-8 kódolásúak; az MSVC `/utf-8` kapcsolóval fordul
> (az ékezetes feliratok `juce::String::fromUTF8(...)`-on keresztül jelennek meg).

## Telepítő készítése (terjesztés)

A kész alkalmazás **next-next-finish telepítőként** adható át bármely 64-bites
Windows gépnek. A telepítő egyetlen `.exe`, ami a programot a NAM modellekkel és
a gyári presetekkel együtt telepíti, és **nem igényel semmilyen futtatókörnyezetet**
a célgépen (statikus MSVC runtime → nincs Visual C++ Redistributable függőség).

### Egyszeri előfeltétel: Inno Setup

```powershell
winget install JRSoftware.InnoSetup
```

> A `winget` és az `ISCC.exe` nincs feltétlenül a PATH-on. Ha a CMake nem találja,
> teljes útvonallal hívd; tipikus per-user hely:
> `%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe`.

### Automatikus build (ajánlott)

A telepítő **minden Release build végén automatikusan újragenerálódik** — nincs
külön teendő. A CMake a konfiguráláskor megkeresi az `ISCC.exe`-t, és a standalone
build után lefordítja az `installer/LiveDSP.iss`-t.

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64 -DLIVEDSP_ASIO_SDK_DIR="C:/SDKs/asiosdk"
cmake --build build --config Release
# -> installer/Output/LiveDSP-Setup-<verzió>.exe  (~17 MB)
```

- Csak **Release**-ben fut (Debug/inkrementális build érintetlen, gyors marad).
- Ha az Inno Setup nincs telepítve, a build sikeres marad, csak a telepítőt hagyja ki.
- Kikapcsolható: `cmake -B build -DLIVEDSP_BUILD_INSTALLER=OFF`.
- A verzió a `project(LiveDSP VERSION ...)`-ból jön (a CMake adja át az `.iss`-nek).

### Kézi build (Inno Setup nélküli CMake esetén)

```powershell
& "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe" installer\LiveDSP.iss
```

### Telepítés a célgépen (végfelhasználó)

1. Másold át a `LiveDSP-Setup-<verzió>.exe`-t a másik PC-re, és futtasd → **Tovább → Tovább → Befejezés**.
   (A Program Files-ba telepítés adminjogot kér; Start menü ikon, opcionálisan asztali ikon.)
2. Indítsd a **LiveDSP**-t, a landing képernyőn válassz **GuitarDSP** vagy **VoiceDSP** modult.
3. **Audio Settings** → válaszd a hangkártya **ASIO** driverét, és állíts alacsony puffert.

> **ASIO a célgépen:** az alacsony latenciához a célgépen is kell egy ASIO driver
> (a hangkártya gyári drivere, vagy [ASIO4ALL](https://asio4all.org)). Ezt a telepítő
> nem tartalmazhatja (hardver/driver kérdés). ASIO nélkül a WASAPI fallback működik,
> nagyobb késleltetéssel.

### Mit csomagol / hogyan tölti be a modelleket

A telepítő a `models/` (NAM + IR) és `favs/` (presetek) mappát az `.exe` mellé
teszi. Az app futásidőben az [`source/AppPaths.h`](source/AppPaths.h) szerint old fel:
**előbb az exe melletti mappát** nézi (telepített eset), majd fejlesztéskor a
build-időben befordított forrásmappára (`LIVEDSP_DEFAULT_MODELS_DIR` /
`LIVEDSP_FAVS_DIR`) esik vissza — így az IDE-ből és a telepített buildből is működik.

## Modellek / IR

A `models/` mappa tartalma indításkor automatikusan elérhető (az első `.nam`
modell betöltődik). A mappa **git-ignorált** (lokális, kereskedelmi captúrák).

> A jelenlegi NAM modellek **„Full Rig"** típusúak (tartalmazzák a hangládát),
> ezért a **Cab IR alapból KI van kapcsolva** (nincs dupla cab). Külön IR-t csak
> „amp-only" (preamp/DI) NAM modellhez kapcsolj be.

## Tesztelés

1. Indítsd a standalone `.exe`-t, a landing képernyőn válassz **GuitarDSP** vagy **VoiceDSP** modult.
2. Audio Settings (Options) → válaszd a Focusrite **ASIO** drivert, állíts alacsony puffert (64/128).
3. Az INPUT panelen állítsd be a megfelelő bemeneti csatornát (gitár / mikrofon külön).
4. A „‹ MENÜ" gombbal bármikor visszaléphetsz és módot válthatsz.

## Architektúra (röviden)

- `LiveDspProcessor` (egy `juce::AudioProcessor`): mindkét DSP láncot tartalmazza,
  futás közben váltható `appMode` (none/guitar/voice) irányítja a `processBlock`-ot.
- `LiveDspEditor`: vékony shell, ami a mód szerint egy `AppView`-t mutat
  (`LandingView` / `GuitarView` / `VoiceView`).
- DSP modulok: `source/dsp/` (NoiseGate, Overdrive, PitchShifter, NamProcessor,
  CabConvolver, Equalizer, VoiceChain). UI: `source/ui/` (közös `LiveLookAndFeel` + panelek).

## Köszönet (ikonok)

- A landing gitár-ikon: [game-icons.net](https://game-icons.net) (lorc), **CC BY 3.0**.
- A landing mikrofon-ikon: [Phosphor Icons](https://phosphoricons.com) (microphone-stage), **MIT**.
