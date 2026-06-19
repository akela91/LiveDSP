# Third-Party Notices

**LiveDSP** is distributed under the **GNU General Public License v3.0** (GPLv3)
— see the [LICENSE](LICENSE) file. That choice is required by the (copyleft)
dependencies below. The project uses the components listed here; each under the
terms of its own license.

| Component | Version | License | Note |
|---|---|---|---|
| [JUCE](https://juce.com) | 8.0.4 | **GPLv3** (or commercial) | Used under the GPLv3 option. |
| [Rubber Band Library](https://breakfastquay.com/rubberband/) | 4.0.0 | **GPLv2-or-later** (or commercial) | GPLv3-compatible (or-later). |
| [Steinberg ASIO SDK](https://www.steinberg.net/developers/) | 2.3 | **GPLv3** (or proprietary) | Used and redistributed under the GPLv3 option. |
| [NeuralAmpModelerCore](https://github.com/sdatkinson/NeuralAmpModelerCore) | — | MIT | |
| [Eigen](https://eigen.tuxfamily.org) | (with NAM) | MPL2 (+ some files Apache/BSD) | |
| [nlohmann/json](https://github.com/nlohmann/json) | (with NAM) | MIT | |

## GPLv3 as the license of the whole work

LiveDSP links the GPL components above (JUCE under its GPLv3 option, Rubber Band
GPLv2-or-later, ASIO SDK GPLv3). Therefore the complete, distributed application
is released under the terms of **GPLv3**: the corresponding source code is
available in the project's public repository
(https://github.com/akela91/LiveDSP).

## ASIO trademark (NOT covered by the GPL)

The "ASIO" name and the Steinberg ASIO logos are **trademarks of Steinberg Media
Technologies GmbH** and are **not** covered by the GPL. LiveDSP:

- does not use the word "ASIO" in the product or company name;
- does not redistribute the Steinberg ASIO logo artwork (which is therefore not
  in the repository either);
- displays "ASIO" only in a product-feature context (the audio-driver selector),
  in the spirit of the Steinberg ASIO Usage Guidelines.

## ASIO4ALL (if the installer offers it)

[ASIO4ALL](https://asio4all.org) (© Michael Tippach) is **not** part of the
LiveDSP source and is **not** linked into it. The installer does not bundle it;
it only offers to open the official download page. ASIO4ALL is distributed under
the terms of its own freeware license.

## Models / IR files

The NAM captures and impulse responses are **data files**, not part of the
program; they are covered by their own (sometimes commercial) licenses, which
are independent of LiveDSP's GPLv3 license. They are neither version-controlled
nor shipped with the app.
