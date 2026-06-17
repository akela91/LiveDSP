# Harmadik féltől származó komponensek / Third-Party Notices

A **LiveDSP** a **GNU General Public License v3.0** (GPLv3) alatt érhető el —
lásd a [LICENSE](LICENSE) fájlt. Ezt a választást az alábbi (copyleft)
függőségek teszik szükségessé. A projekt az itt felsorolt komponenseket
használja; mindegyik a saját licencének feltételei szerint.

| Komponens | Verzió | Licenc | Megjegyzés |
|---|---|---|---|
| [JUCE](https://juce.com) | 8.0.4 | **GPLv3** (vagy kereskedelmi) | A GPLv3 opció alatt használjuk. |
| [Rubber Band Library](https://breakfastquay.com/rubberband/) | 4.0.0 | **GPLv2-or-later** (vagy kereskedelmi) | GPLv3-kompatibilis (or-later). |
| [Steinberg ASIO SDK](https://www.steinberg.net/developers/) | 2.3 | **GPLv3** (vagy proprietary) | A GPLv3 opció alatt használjuk és terjesztjük. |
| [NeuralAmpModelerCore](https://github.com/sdatkinson/NeuralAmpModelerCore) | — | MIT | |
| [Signalsmith Stretch](https://github.com/Signalsmith-Audio/signalsmith-stretch) | — | MIT | + signalsmith-linear (MIT). |
| [Eigen](https://eigen.tuxfamily.org) | (NAM-mal) | MPL2 (+ egyes fájlok Apache/BSD) | |
| [nlohmann/json](https://github.com/nlohmann/json) | (NAM-mal) | MIT | |

## A GPLv3 mint a teljes mű licence

A LiveDSP linkeli a fenti **GPL** komponenseket (JUCE GPLv3 opció, Rubber Band
GPLv2-or-later, ASIO SDK GPLv3). Ezért a teljes, terjesztett alkalmazás a
**GPLv3** feltételei szerint kerül kiadásra: a megfelelő forráskód elérhető a
projekt nyilvános repójában (https://github.com/akela91/LiveDSP).

## ASIO védjegy (NEM a GPL fedi)

Az „ASIO” név és a Steinberg ASIO logók a **Steinberg Media Technologies GmbH
védjegyei**, amelyekre a GPL **nem** terjed ki. A LiveDSP:

- nem használja az „ASIO” szót a termék- vagy cégnévben;
- nem terjeszti a Steinberg ASIO logó-artworköt (ezért az nincs a repóban sem);
- az „ASIO” kifejezést kizárólag termékfunkció-kontextusban (audio-driver
  választó) jeleníti meg, a Steinberg ASIO Usage Guidelines szellemében.

## ASIO4ALL (ha az installer tartalmazza)

Az [ASIO4ALL](https://asio4all.org) (© Michael Tippach) **nem** része a LiveDSP
forrásának és **nincs hozzá linkelve**; ha a telepítő tartalmazza, az
*különálló program puszta aggregációja* (a GPL értelmében), a saját
freeware-licence feltételei szerint terjesztve.

## Modellek / IR fájlok

A `models/` mappában lévő NAM-captúrák és impulzusválaszok **adatfájlok**, nem a
program részei; a saját (esetenként kereskedelmi) licencük vonatkozik rájuk,
amely független a LiveDSP GPLv3 licencétől.
